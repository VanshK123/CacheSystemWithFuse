#ifndef CACHE_BLOCK_STORE_H
#define CACHE_BLOCK_STORE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>

class BlockStore {
public:

BlockStore(const std::string& cache_root, std::size_t block_size);

bool init();

ssize_t read(const std::string& hash_hex, char* buf, std::size_t len,  off_t off);

ssize_t write(const std::string& hash_hex, const char* buf, std::size_t len, off_t off, bool mark_dirty);

bool delete_object(const std::string& hash_hex);

void cleanup();

private:
std::string root_; 
std::size_t block_size_;

BlockStore(const BlockStore&)            = delete;
BlockStore& operator=(const BlockStore&) = delete;
};

#endif
