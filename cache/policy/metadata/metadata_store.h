#ifndef METADATA_STORE_H
#define METADATA_STORE_H

#include <string>
#include <vector>
#include <ctime>
#include <optional>

struct CacheMetadata {
    std::string path;
    std::string local_path;
    size_t      size;
    std::time_t timestamp;
    std::time_t last_accessed;
    bool        dirty;
};

class MetadataStore {
public:
    explicit MetadataStore(const std::string& db_path);

    ~MetadataStore();

    bool init();

    std::optional<CacheMetadata> get(const std::string& path);

    bool put(const CacheMetadata& meta);

    bool updateAccessTime(const std::string& path, std::time_t last_accessed);

    bool markDirty(const std::string& path, bool dirty);

    bool remove(const std::string& path);

    std::vector<CacheMetadata> allEntries();

    void cleanup();

private:
    std::string db_path_;
    void*       db_handle_ = nullptr; // sqlite3* specific 
};

#endif
