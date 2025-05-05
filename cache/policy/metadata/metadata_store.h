#ifndef CACHE_METADATA_STORE_H
#define CACHE_METADATA_STORE_H


#include <cstddef>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct CacheMetadata {
std::string path;
std::string local_path;
std::size_t size = 0;
std::time_t timestamp = 0;
std::time_t last_accessed = 0;
bool        dirty = false;
};


class MetadataStore {
public:

MetadataStore(const std::string& db_path, const std::string& cache_root);
~MetadataStore();

bool init();

std::optional<CacheMetadata> get(const std::string& path);
bool put(const CacheMetadata& meta);
bool updateAccessTime(const std::string& path, std::time_t last_accessed);
bool markDirty(const std::string& path, bool dirty);
bool remove(const std::string& path);
std::vector<CacheMetadata> allEntries();
void cleanup();

void markDirtyBlock(const std::string& hash_hex, std::size_t part_idx, std::size_t block_idx);

bool flushBitmaps(const std::string& hash_hex);

private:
std::string db_path_;
void*       db_handle_ = nullptr;
std::string cache_root_;


using BitVec = std::vector<bool>;
std::unordered_map<std::string, std::unordered_map<std::size_t, BitVec>> bitmap_;


bool loadBitmap(const std::string& hash_hex, std::size_t part_idx);
bool persistBitmap(const std::string& hash_hex, std::size_t part_idx, const BitVec& bits);

MetadataStore(const MetadataStore&) = delete;
MetadataStore& operator=(const MetadataStore&) = delete;
};

#endif
