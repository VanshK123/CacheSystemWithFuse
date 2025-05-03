#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <sys/types.h>

#define CACHE_TABLE_SIZE 1024

typedef struct cache_entry {
    char*               path;
    char*               local_path;
    size_t              size;
    time_t              timestamp;
    time_t              last_accessed;
    bool                dirty;
    struct cache_entry* next;
} cache_entry;

int cache_init(const char* backing_dir, int timeout);

bool cache_has_valid_entry(const char* path);

cache_entry* cache_get_entry(const char* path);

int cache_store_file(const char* path, const char* data, size_t size, off_t offset);

ssize_t cache_read_file(const char* path, char* buffer, size_t size, off_t offset);

int cache_apply_eviction(void);

void cache_cleanup(void);

#endif
