#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

typedef struct {
    char* path;
    char* local_path;
    size_t size;
    time_t timestamp;
    time_t last_accessed;
    bool dirty;
} cache_entry;

int cache_init(const char* cache_dir, int timeout);

bool cache_has_valid_entry(const char* path);

cache_entry* cache_get_entry(const char* path);

int cache_store_file(const char* path, const char* buffer, size_t size, off_t offset);

int cache_read_file(const char* path, char* buffer, size_t size, off_t offset);

int cache_apply_eviction();

int cache_mark_dirty(const char* path);

int cache_sync_dirty();

void cache_cleanup();

#endif