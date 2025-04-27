#include "cache_manager.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#define CACHE_HASH_SIZE 1024

static char* cache_directory = NULL;
static int cache_timeout = 3600;
static cache_entry* cache_table[CACHE_HASH_SIZE] = {NULL};

static unsigned int hash_path(const char* path) {
    unsigned int hash = 0;
    while (*path) {
        hash = (hash * 31) + (*path++);
    }
    return hash % CACHE_HASH_SIZE;
}

static cache_entry* create_entry(const char* path) {
    cache_entry* entry = (cache_entry*)malloc(sizeof(cache_entry));
    if (!entry) return NULL;
    
    entry->path = strdup(path);
    
    char local_path[1024];
    unsigned int hash = hash_path(path);
    snprintf(local_path, sizeof(local_path), "%s/%u_%s", 
             cache_directory, hash, strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
    
    entry->local_path = strdup(local_path);
    entry->size = 0;
    entry->timestamp = time(NULL);
    entry->last_accessed = entry->timestamp;
    entry->dirty = false;
    
    return entry;
}

int cache_init(const char* cache_dir, int timeout) {
    cache_directory = strdup(cache_dir);
    cache_timeout = timeout;
    
    struct stat st = {0};
    if (stat(cache_directory, &st) == -1) {
        if (mkdir(cache_directory, 0755) == -1) {
            fprintf(stderr, "Failed to create cache directory: %s\n", strerror(errno));
            return -1;
        }
    }
    
    memset(cache_table, 0, sizeof(cache_table));
    
    return 0;
}

bool cache_has_valid_entry(const char* path) {
    unsigned int hash = hash_path(path);
    cache_entry* entry = cache_table[hash];
    
    while (entry) {
        if (strcmp(entry->path, path) == 0) {
            time_t now = time(NULL);
            if (now - entry->timestamp <= cache_timeout) {
                entry->last_accessed = now;
                return true;
            }
            return false;
        }
        entry = (cache_entry*)entry->next;
    }
    
    return false;
}

cache_entry* cache_get_entry(const char* path) {
    unsigned int hash = hash_path(path);
    cache_entry* entry = cache_table[hash];
    
    while (entry) {
        if (strcmp(entry->path, path) == 0) {
            entry->last_accessed = time(NULL);
            return entry;
        }
        entry = (cache_entry*)entry->next;
    }
    
    return NULL;
}

int cache_store_file(const char* path, const char* buffer, size_t size, off_t offset) {
    unsigned int hash = hash_path(path);
    cache_entry* entry = cache_table[hash];
    
    while (entry) {
        if (strcmp(entry->path, path) == 0) {
            break;
        }
        entry = (cache_entry*)entry->next;
    }
    
    if (!entry) {
        entry = create_entry(path);
        if (!entry) return -1;
        
        entry->next = (struct cache_entry*)cache_table[hash];
        cache_table[hash] = entry;
    }
    
    int fd = open(entry->local_path, O_WRONLY | O_CREAT, 0644);
    if (fd == -1) {
        fprintf(stderr, "Failed to open cache file: %s\n", strerror(errno));
        return -1;
    }
    
    if (lseek(fd, offset, SEEK_SET) == -1) {
        fprintf(stderr, "Failed to seek in cache file: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    
    ssize_t written = write(fd, buffer, size);
    close(fd);
    
    if (written == -1) {
        fprintf(stderr, "Failed to write to cache file: %s\n", strerror(errno));
        return -1;
    }
    
    entry->timestamp = time(NULL);
    entry->last_accessed = entry->timestamp;
    if (offset + size > entry->size) {
        entry->size = offset + size;
    }
    
    return written;
}

int cache_read_file(const char* path, char* buffer, size_t size, off_t offset) {
    cache_entry* entry = cache_get_entry(path);
    if (!entry) {
        return -1;
    }
    
    int fd = open(entry->local_path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open cache file: %s\n", strerror(errno));
        return -1;
    }
    
    if (lseek(fd, offset, SEEK_SET) == -1) {
        fprintf(stderr, "Failed to seek in cache file: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    
    ssize_t bytes_read = read(fd, buffer, size);
    close(fd);
    
    if (bytes_read == -1) {
        fprintf(stderr, "Failed to read from cache file: %s\n", strerror(errno));
        return -1;
    }
    
    entry->last_accessed = time(NULL);
    return bytes_read;
}

int cache_apply_eviction() {
    time_t oldest_time = time(NULL);
    cache_entry* oldest_entry = NULL;
    unsigned int oldest_hash = 0;
    
    for (int i = 0; i < CACHE_HASH_SIZE; i++) {
        cache_entry* entry = cache_table[i];
        while (entry) {
            if (entry->last_accessed < oldest_time && !entry->dirty) {
                oldest_time = entry->last_accessed;
                oldest_entry = entry;
                oldest_hash = i;
            }
            entry = (cache_entry*)entry->next;
        }
    }
    
    if (oldest_entry) {
        if (cache_table[oldest_hash] == oldest_entry) {
            cache_table[oldest_hash] = (cache_entry*)oldest_entry->next;
        } else {
            cache_entry* prev = cache_table[oldest_hash];
            while (prev && prev->next != (struct cache_entry*)oldest_entry) {
                prev = (cache_entry*)prev->next;
            }
            if (prev) {
                prev->next = oldest_entry->next;
            }
        }
        
        unlink(oldest_entry->local_path);
        
        free(oldest_entry->path);
        free(oldest_entry->local_path);
        free(oldest_entry);
        return 0;
    }
    
    return -1;
}

void cache_cleanup() {
    for (int i = 0; i < CACHE_HASH_SIZE; i++) {
        cache_entry* entry = cache_table[i];
        while (entry) {
            cache_entry* next = (cache_entry*)entry->next;
            free(entry->path);
            free(entry->local_path);
            free(entry);
            entry = next;
        }
        cache_table[i] = NULL;
    }
    
    if (cache_directory) {
        free(cache_directory);
        cache_directory = NULL;
    }
}