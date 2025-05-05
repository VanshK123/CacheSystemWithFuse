 #include <stddef.h>
 #include <errno.h>
 #include <sys/types.h> 
 #include <unistd.h> 
 
 
 extern "C" {
 ssize_t  cache_store_file(const char*, const char*, size_t, off_t);
 ssize_t  cache_read_file (const char*, char*,       size_t, off_t);
 int      cache_evict_gb  (double);
 void     cache_flush_all (void);
 }
 
 __attribute__((weak)) int  cache_init   (const char*, int);
 __attribute__((weak)) void cache_cleanup(void);
 
 bool cache_has_valid_entry(const char*) { return false; }
 void* cache_get_entry(const char*) { return NULL;  }
 int  cache_apply_eviction(void) {
    return reinterpret_cast<void*>(cache_evict_gb) ? cache_evict_gb(1.0) : 0;
}

 
 