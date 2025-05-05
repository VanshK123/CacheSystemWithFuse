#include "block_store.h"
#include "fs_layout.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
using namespace fs_layout;


BlockStore::BlockStore(const std::string& cache_root, std::size_t block_sz) : root_(cache_root), block_size_(block_sz) {}

bool BlockStore::init() {
    std::error_code ec;
    if (!fs::exists(root_)) {
        if (!fs::create_directories(root_, ec)) {
        std::cerr << "[block_store] failed to create cache root '" << root_ << "': " << ec.message() << '\n';
        return false;
        }
    }
    return true;
}

static void ensure_shard_dirs(const std::string& root, const std::string& hash_hex) {
    std::string lvl1 = root + "/" + hash_hex.substr(0, 2);
    std::string lvl2 = lvl1 + "/" + hash_hex.substr(2, 2);

    ::mkdir(lvl1.c_str(), 0755);
    ::mkdir(lvl2.c_str(), 0755);
}

static int open_file(const std::string& path, int flags, mode_t mode = 0644) {
    int fd = ::open(path.c_str(), flags, mode);
    return (fd < 0) ? -errno : fd;
}


ssize_t BlockStore::read(const std::string& hash_hex, char* buf, std::size_t len, off_t off) {
    std::size_t part_idx = off / kMaxPartSize;
    off_t part_off = off % kMaxPartSize;

    std::string path = data_part_path(root_, hash_hex, part_idx);

    int fd = open_file(path, O_RDONLY);
    if (fd < 0) return fd;

    ssize_t n = ::pread(fd, buf, len, part_off);
    if (n < 0) n = -errno;
    ::close(fd);
    return n;
}

ssize_t BlockStore::write(const std::string& hash_hex, const char* buf, std::size_t len, off_t off, bool) {
    ensure_shard_dirs(root_, hash_hex);

    std::size_t part_idx = off / kMaxPartSize;
    off_t       part_off = off % kMaxPartSize;

    std::string path = data_part_path(root_, hash_hex, part_idx);

    int fd = open_file(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return fd;

    ssize_t n = ::pwrite(fd, buf, len, part_off);
    if (n < 0) n = -errno;
    ::close(fd);
    return n;
}

bool BlockStore::delete_object(const std::string& hash_hex) {
    bool ok = true;
    std::string dir = root_ + "/" + shard_dir(hash_hex);

    if (!fs::exists(dir)) return true;
    for (auto const& entry : fs::directory_iterator(dir)) {
        if (entry.path().filename().string().rfind(hash_hex, 0) == 0) {
            std::error_code ec;
            fs::remove(entry, ec);
            if (ec) {
                std::cerr << "[block_store] failed to remove " << entry.path() << ": " << ec.message() << '\n';
                ok = false;
            }
        }
    }
    std::error_code ec;
    fs::remove(dir, ec);
    fs::remove(dir.substr(0, dir.find_last_of('/')), ec);
    return ok;
}

