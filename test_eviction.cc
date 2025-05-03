#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>

#include "cache/cache_manager.h"

int main() {
    const char* backing_dir = "./cache_dir";
    const char* path        = "/foo/bar.txt";
    const char* data        = "evict-me!";
    const int   timeout     = 1;

    if (cache_init(backing_dir, timeout) != 0) {
        std::cerr << "cache_init failed\n";
        return 1;
    }
    std::cout << "cache_init OK (timeout=" << timeout << "s)\n";

    if (cache_store_file(path, data, strlen(data), 0) != 0) {
        std::cerr << "cache_store_file failed\n";
        return 1;
    }
    std::cout << "cache_store_file OK\n";

    std::cout << "  has_valid_entry? "
              << (cache_has_valid_entry(path) ? "yes" : "no") << "\n";

    sleep(timeout + 1);
    std::cout << "  slept for " << (timeout + 1) << "s\n";

    cache_apply_eviction();
    std::cout << "cache_apply_eviction done\n";

    std::cout << "  has_valid_entry? "
              << (cache_has_valid_entry(path) ? "yes" : "no") << "\n";

    std::string fullpath = std::string(backing_dir) + path;
    std::ifstream ifs(fullpath, std::ios::binary);
    if (!ifs) {
        std::cerr << "ERROR: backing store file not found at " << fullpath << "\n";
    } else {
        std::string contents;
        std::getline(ifs, contents);
        std::cout << "Backing file contents: \"" << contents << "\"\n";
    }

    cache_cleanup();
    std::cout << "cache_cleanup OK\n";
    return 0;
}
