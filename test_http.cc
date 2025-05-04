// test_http.cc

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fstream>

#include "cache/cache_manager.h"
#include "backend/backend.h"

int main() {
    const int timeout = 2;               // eviction timeout
    const std::string fname   = "hello.txt";
    const std::string content = "Hello, HTTP!";

    // 1) Prepare cache_dir and seed file
    system("rm -rf cache_dir && mkdir -p cache_dir");
    {
        std::ofstream ofs("cache_dir/" + fname, std::ios::binary);
        ofs << content;
    }

    // 2) Fork + launch Python HTTP server in cache_dir on port 8000
    pid_t pid = fork();
    if (pid == 0) {
        // child: serve
        chdir("cache_dir");
        execlp("python3", "python3", "-m", "http.server", "8000", nullptr);
        _exit(1);
    }
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    // give server a moment
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 3) Init cache
    if (cache_init("./cache_dir", timeout) != 0) {
        std::cerr << "cache_init failed\n";
        kill(pid, SIGTERM); waitpid(pid, nullptr, 0);
        return 1;
    }
    std::cout << "cache_init OK\n";

    // 4) Init HTTP backend against localhost
    auto backend = cache_fs::create_backend("http://localhost:8000");
    if (!backend) {
        std::cerr << "create_backend failed\n";
        kill(pid, SIGTERM); waitpid(pid, nullptr, 0);
        return 1;
    }
    std::cout << "HTTP backend initialized\n";

    // 5) First GET (miss)
    {
        std::vector<char> buf(1024);
        ssize_t n1 = backend->download("/" + fname, buf.data(), buf.size(), 0);
        if (n1 < 0) {
            std::cerr << "download failed\n";
            cache_cleanup();
            kill(pid, SIGTERM); waitpid(pid, nullptr, 0);
            return 1;
        }
        std::cout << "Downloaded (miss): \"" 
                  << std::string(buf.data(), n1) << "\"\n";
    }

    // 6) Evict after timeout
    std::this_thread::sleep_for(std::chrono::seconds(timeout + 1));
    cache_apply_eviction();
    std::cout << "cache_apply_eviction done\n";

    // 7) Second GET (hit)
    {
        std::vector<char> buf2(1024);
        ssize_t n2 = backend->download("/" + fname, buf2.data(), buf2.size(), 0);
        if (n2 < 0) {
            std::cerr << "second download failed\n";
            cache_cleanup();
            kill(pid, SIGTERM); waitpid(pid, nullptr, 0);
            return 1;
        }
        std::cout << "Downloaded (hit): \"" 
                  << std::string(buf2.data(), n2) << "\"\n";
    }

    // 8) Cleanup
    cache_cleanup();
    std::cout << "cache_cleanup OK\n";

    // 9) Tear down HTTP server
    kill(pid, SIGTERM);
    waitpid(pid, nullptr, 0);

    return 0;
}
