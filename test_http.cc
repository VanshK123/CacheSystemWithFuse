#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <fstream>
#include <cerrno>

#include "cache/cache_manager.h"
#include "backend/backend.h"

static std::string get_abs(const std::string& path) {
    char buf[PATH_MAX];
    if (!realpath(path.c_str(), buf)) {
        perror("realpath");
        exit(1);
    }
    return std::string(buf);
}

int main() {
    const std::string backing_dir = "./cache_dir";
    const int timeout = 2; // seconds
    const std::string fname = "test_file.txt";
    const std::string content = "Hello, HTTP file!";

    system("rm -rf cache_dir");
    if (mkdir(backing_dir.c_str(), 0755) != 0 && errno != EEXIST) {
        perror("mkdir backing_dir");
        return 1;
    }
    {
        std::ofstream ofs(backing_dir + "/" + fname, std::ios::binary);
        ofs << content;
    }

    // 1) init cache
    if (cache_init(backing_dir.c_str(), timeout) != 0) {
        std::cerr << "cache_init failed\n";
        return 1;
    }
    std::cout << "cache_init OK\n";

    auto abs_back = get_abs(backing_dir);
    auto backend = cache_fs::create_backend("file://" + abs_back);
    if (!backend) {
        std::cerr << "create_backend failed\n";
        return 1;
    }
    std::cout << "HTTP backend initialized with file://" << abs_back << "\n";

    std::vector<char> buf(1024);
    ssize_t n1 = backend->download("/" + fname, buf.data(), buf.size(), 0);
    if (n1 < 0) { std::cerr << "download failed\n"; return 1; }
    std::string out1(buf.data(), n1);
    std::cout << "Downloaded (miss): \"" << out1 << "\"\n";

    sleep(timeout + 1);
    cache_apply_eviction();
    std::cout << "cache_apply_eviction done\n";

    std::vector<char> buf2(1024);
    ssize_t n2 = backend->download("/" + fname, buf2.data(), buf2.size(), 0);
    if (n2 < 0) { std::cerr << "2nd download failed\n"; return 1; }
    std::string out2(buf2.data(), n2);
    std::cout << "Downloaded (hit): \"" << out2 << "\"\n";

    cache_cleanup();
    std::cout << "cache_cleanup OK\n";
    return 0;
}
