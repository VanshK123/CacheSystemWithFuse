#ifndef BLOCK_STORE_H
#define BLOCK_STORE_H

#include <cstddef>
#include <string>

class BlockStore {
public:
    BlockStore(const std::string& storage_dir, size_t block_size);
    ~BlockStore();

    bool init();

    ssize_t read_block(size_t block_id, char* buffer);

    bool write_block(size_t block_id, const char* buffer);

    bool delete_block(size_t block_id);

    bool has_block(size_t block_id);

    void cleanup();

private:
    std::string storage_dir_;
    size_t      block_size_;
};

#endif