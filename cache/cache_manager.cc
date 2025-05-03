#include "cache_manager.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <libgen.h>

#define CACHE_HASH_SIZE   CACHE_TABLE_SIZE
#define COPY_BUFFER_SIZE  4096

static char*        cache_directory = NULL;
static int          cache_timeout   = 3600;
static cache_entry* cache_table[CACHE_HASH_SIZE] = { nullptr };

static unsigned int hash_path(const char* path) {
    unsigned int hash = 5381;
    int c;
    while ((c = *path++))
        hash = ((hash << 5) + hash) + c;
    return hash % CACHE_HASH_SIZE;
}

static int mkdir_p(const char *path) {
    char tmp[1024];
    char *p;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len-1] == '/') tmp[len-1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static cache_entry* create_entry(const char* path) {
    cache_entry* e = (cache_entry*)malloc(sizeof(cache_entry));
    if (!e) return nullptr;

    e->path = strdup(path);

    unsigned int h = hash_path(path);
    char lp[1024];
    snprintf(lp, sizeof(lp), "%s/%u_%s",
             cache_directory,
             h,
             strrchr(path,'/')?strrchr(path,'/')+1:path);

    e->local_path     = strdup(lp);
    e->size           = 0;
    e->timestamp      = time(NULL);
    e->last_accessed  = e->timestamp;
    e->dirty          = false;
    e->next           = nullptr;
    return e;
}

static int write_back_entry(cache_entry* entry) {
    char dst[1024];
    snprintf(dst, sizeof(dst), "%s%s", cache_directory, entry->path);

    char* dup = strdup(dst);
    char* dir = dirname(dup);
    mkdir_p(dir);
    free(dup);

    int fd_src = open(entry->local_path, O_RDONLY);
    if (fd_src < 0) {
        perror("write_back: open local cache");
        return -1;
    }
    int fd_dst = open(dst, O_WRONLY | O_CREAT, 0644);
    if (fd_dst < 0) {
        perror("write_back: open backing store");
        close(fd_src);
        return -1;
    }
    char buf[COPY_BUFFER_SIZE];
    ssize_t n;
    while ((n = read(fd_src, buf, sizeof(buf))) > 0) {
        if (write(fd_dst, buf, n) != n) {
            perror("write_back: write");
            close(fd_src);
            close(fd_dst);
            return -1;
        }
    }
    close(fd_src);
    close(fd_dst);
    if (n < 0) perror("write_back: read");
    return (n < 0 ? -1 : 0);
}

static int populate_from_backing(cache_entry* entry) {
    char src[1024];
    snprintf(src, sizeof(src), "%s%s", cache_directory, entry->path);

    int fd_src = open(src, O_RDONLY);
    if (fd_src < 0) {
        perror("cache_read_file: open backing store");
        return -1;
    }
    int fd_dst = open(entry->local_path,
                      O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd_dst < 0) {
        perror("cache_read_file: create local cache");
        close(fd_src);
        return -1;
    }

    char buf[COPY_BUFFER_SIZE];
    ssize_t n, total = 0;
    while ((n = read(fd_src, buf, sizeof(buf))) > 0) {
        if (write(fd_dst, buf, n) != n) {
            perror("cache_read_file: write cache");
            close(fd_src);
            close(fd_dst);
            return -1;
        }
        total += n;
    }
    if (n < 0) perror("cache_read_file: read backing");
    close(fd_src);
    close(fd_dst);

    entry->dirty = false;
    entry->size  = (size_t)total;
    return (n < 0 ? -1 : 0);
}

int cache_init(const char* backing_dir, int timeout) {
    cache_directory = strdup(backing_dir);
    cache_timeout   = timeout;

    struct stat st{};
    if (stat(cache_directory, &st) == -1) {
        if (mkdir_p(cache_directory) != 0) {
            perror("cache_init: mkdir_p");
            return -1;
        }
    }
    for (int i = 0; i < CACHE_HASH_SIZE; ++i)
        cache_table[i] = nullptr;
    return 0;
}

bool cache_has_valid_entry(const char* path) {
    unsigned int h = hash_path(path);
    cache_entry* e = cache_table[h];
    time_t now = time(NULL);
    while (e) {
        if (strcmp(e->path, path) == 0) {
            return difftime(now, e->last_accessed) <= cache_timeout;
        }
        e = e->next;
    }
    return false;
}

cache_entry* cache_get_entry(const char* path) {
    unsigned int h = hash_path(path);
    cache_entry* e = cache_table[h];
    while (e) {
        if (strcmp(e->path, path) == 0) {
            e->last_accessed = time(NULL);
            return e;
        }
        e = e->next;
    }
    return nullptr;
}

int cache_store_file(const char* path,
                     const char* data,
                     size_t       size,
                     off_t        offset)
{
    unsigned int h = hash_path(path);
    cache_entry* e = cache_get_entry(path);
    if (!e) {
        e = create_entry(path);
        if (!e) return -1;
        e->next = cache_table[h];
        cache_table[h] = e;
    }

    int fd = open(e->local_path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        perror("cache_store_file: open");
        return -1;
    }
    if (lseek(fd, offset, SEEK_SET) < 0) {
        perror("cache_store_file: lseek");
        close(fd);
        return -1;
    }
    if (write(fd, data, size) != (ssize_t)size) {
        perror("cache_store_file: write");
        close(fd);
        return -1;
    }
    close(fd);

    e->size           = offset + size;
    e->dirty          = true;
    e->last_accessed  = time(NULL);
    return 0;
}

ssize_t cache_read_file(const char* path,
                        char*        buffer,
                        size_t       size,
                        off_t        offset)
{
    unsigned int h = hash_path(path);
    cache_entry* e = cache_get_entry(path);
    if (!e) {
        e = create_entry(path);
        if (!e) return -1;
        e->next = cache_table[h];
        cache_table[h] = e;
        if (populate_from_backing(e) != 0) {
            return -1;
        }
    }

    int fd = open(e->local_path, O_RDONLY);
    if (fd < 0) {
        perror("cache_read_file: open cache");
        return -1;
    }
    if (lseek(fd, offset, SEEK_SET) < 0) {
        perror("cache_read_file: lseek");
        close(fd);
        return -1;
    }
    ssize_t n = read(fd, buffer, size);
    if (n < 0) perror("cache_read_file: read");
    e->last_accessed = time(NULL);
    close(fd);
    return n;
}

int cache_apply_eviction(void) {
    time_t now = time(NULL);
    for (int i = 0; i < CACHE_HASH_SIZE; ++i) {
        cache_entry* prev = nullptr;
        cache_entry* e    = cache_table[i];
        while (e) {
            cache_entry* nxt = e->next;
            if (difftime(now, e->last_accessed) > cache_timeout) {
                if (e->dirty) write_back_entry(e);
                unlink(e->local_path);
                if (prev) prev->next = nxt;
                else        cache_table[i] = nxt;
                free(e->path);
                free(e->local_path);
                free(e);
            } else {
                prev = e;
            }
            e = nxt;
        }
    }
    return 0;
}

void cache_cleanup(void) {
    for (int i = 0; i < CACHE_HASH_SIZE; ++i) {
        cache_entry* e = cache_table[i];
        while (e) {
            cache_entry* nxt = e->next;
            if (e->dirty) write_back_entry(e);
            unlink(e->local_path);
            free(e->path);
            free(e->local_path);
            free(e);
            e = nxt;
        }
        cache_table[i] = nullptr;
    }
    free(cache_directory);
}
