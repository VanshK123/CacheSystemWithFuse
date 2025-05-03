#include <iostream>
#include <cstring>
#include "cache/cache_manager.h"

int main() {
    if (cache_init("./cache_dir", 5) != 0) {
        std::cerr << "cache_init failed\n";
        return 1;
    }
    std::cout << "cache_init OK\n";

    const char* path = "/foo/bar.txt";
    const char* data = "hello, cache\n";
    if (cache_store_file(path, data, strlen(data), 0) != 0) {
        std::cerr << "cache_store_file failed\n";
        return 1;
    }
    std::cout << "cache_store_file OK\n";

    if (cache_has_valid_entry(path)) {
        std::cout << "cache_has_valid_entry: yes\n";
    } else {
        std::cout << "cache_has_valid_entry: no\n";
    }

    cache_cleanup();
    std::cout << "cache_cleanup OK\n";
    return 0;
}
