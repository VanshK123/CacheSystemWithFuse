#pragma once

#include <cstddef>
#include <string>

namespace fs_layout {

constexpr std::size_t kMaxPartSize = 2ULL * 1024 * 1024 * 1024;

constexpr std::size_t kFilesPerDir = 256;


inline std::string shard_dir(const std::string& hash_hex) {
    return hash_hex.substr(0, 2) + "/" + hash_hex.substr(2, 2);
}

inline std::string data_part_path(const std::string& cache_root, const std::string& hash_hex, std::size_t part_idx) {
    return cache_root + "/" + shard_dir(hash_hex) + "/" + hash_hex + "." + std::to_string(part_idx) + ".blk";
}

inline std::string bitmap_path(const std::string& cache_root, const std::string& hash_hex, std::size_t part_idx) {
    return cache_root + "/" + shard_dir(hash_hex) + "/" + hash_hex + "." + std::to_string(part_idx) + ".dmap";
}

}
