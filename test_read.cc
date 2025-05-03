#include <iostream>
#include <cstring>
#include <unistd.h>
#include "cache/cache_manager.h"

int main() {
    const char* backing_dir = "./cache_dir";
    const char* path        = "/foo.txt";
    const char* data        = "Hello, Cache Read!";
    const int   timeout     = 1;

    if (cache_init(backing_dir, timeout) != 0) {
        std::cerr << "cache_init failed\n";
        return 1;
    }
    std::cout << "cache_init OK\n";

    if (cache_store_file(path, data, strlen(data), 0) != 0) {
        std::cerr << "cache_store_file failed\n";
        return 1;
    }
    std::cout << "cache_store_file OK\n";

    {
        char buf[64] = {0};
        ssize_t n = cache_read_file(path, buf, sizeof(buf)-1, 0);
        if (n < 0) {
            std::cerr << "cache_read_file (in-cache) failed\n";
        } else {
            std::cout << "Read (in-cache): \"" << buf << "\"\n";
        }
    }

    sleep(timeout + 1);
    cache_apply_eviction();
    std::cout << "cache_apply_eviction done\n";

    {
        char buf2[64] = {0};
        ssize_t n2 = cache_read_file(path, buf2, sizeof(buf2)-1, 0);
        if (n2 < 0) {
            std::cerr << "cache_read_file (read-through) failed\n";
        } else {
            std::cout << "Read (read-through): \"" << buf2 << "\"\n";
        }
    }

    cache_cleanup();
    std::cout << "cache_cleanup OK\n";
    return 0;
}
