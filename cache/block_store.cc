#include "block_store.h"
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

BlockStore::BlockStore(const std::string& storage_dir, size_t block_size)
    : storage_dir_(storage_dir), block_size_(block_size) {}

BlockStore::~BlockStore() {}

bool BlockStore::init() {
    std::error_code ec;
    if (!fs::exists(storage_dir_)) {
        if (!fs::create_directories(storage_dir_, ec)) {
            std::cerr << "Failed to create storage directory '"
                      << storage_dir_ << "': " << ec.message() << '\n';
            return false;
        }
    }
    return true;
}

ssize_t BlockStore::read_block(size_t block_id, char* buffer) {
    std::ostringstream oss;
    oss << storage_dir_ << "/" << block_id;
    std::string path = oss.str();
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    ssize_t bytes = pread(fd, buffer, block_size_, 0);
    close(fd);
    return bytes;
}

bool BlockStore::write_block(size_t block_id, const char* buffer) {
    std::ostringstream oss;
    oss << storage_dir_ << "/" << block_id;
    std::string path = oss.str();
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return false;
    }
    ssize_t written = pwrite(fd, buffer, block_size_, 0);
    close(fd);
    return (written == static_cast<ssize_t>(block_size_));
}

bool BlockStore::delete_block(size_t block_id) {
    std::ostringstream oss;
    oss << storage_dir_ << "/" << block_id;
    return (unlink(oss.str().c_str()) == 0);
}

bool BlockStore::has_block(size_t block_id) {
    std::ostringstream oss;
    oss << storage_dir_ << "/" << block_id;
    return fs::exists(oss.str());
}

void BlockStore::cleanup() {
    for (const auto& entry : fs::directory_iterator(storage_dir_)) {
        fs::remove(entry);
    }
}