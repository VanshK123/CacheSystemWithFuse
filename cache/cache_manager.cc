#include "block_store.h"
#include "metadata_store.h"
#include "lru_policy.h"
#include "thread_pool.h"
#include "backend/backend.h"
#include "fs_layout.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>

namespace fs = std::filesystem;

#ifndef PREFETCH_WINDOW
#define PREFETCH_WINDOW 4
#endif
static constexpr std::size_t kBlockSize = 64 * 1024;
static constexpr std::size_t kCacheBlocksCapacity = 200'000;

static std::string hash_hex(const std::string& s) {
    std::size_t h = std::hash<std::string>{}(s);
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << h;
    return oss.str();
}

struct CacheEntry {
    std::string path;
    std::string hash_hex;
    std::size_t last_block = std::numeric_limits<std::size_t>::max();
    bool evicted = false;
};

class CacheManager {
public:
    explicit CacheManager(const std::string& root) 
        : store_(root, kBlockSize), meta_("cache_meta.db", root), 
          lru_(kCacheBlocksCapacity), prefetch_pool_(4), root_(root),
          cache_hits_(0), cache_misses_(0) {
        store_.init();
        meta_.init();
    }

    ssize_t read(const std::string& path, char* buf, std::size_t len, off_t off);
    ssize_t write(const std::string& path, const char* buf, std::size_t len, off_t off);

    void flush_all();
    void evict_until_gb(double free_gb);
    void reset_stats() {
        cache_hits_ = 0;
        cache_misses_ = 0;
    }


    bool has_valid_entry(const std::string& path) {
        std::lock_guard<std::mutex> g(mu_);
        auto it = entries_.find(path);
        return it != entries_.end() && !it->second.evicted;
    }

    CacheEntry* get_entry(const std::string& path) {
        std::lock_guard<std::mutex> g(mu_);
        auto it = entries_.find(path);
        return (it != entries_.end() && !it->second.evicted) ? &it->second : nullptr;
    }

    std::size_t get_cache_hits() const { return cache_hits_.load(); }
    std::size_t get_cache_misses() const { return cache_misses_.load(); }

private:
    CacheEntry& entry(const std::string& path);
    void schedule_prefetch(CacheEntry ce, std::size_t first_blk);

    std::mutex mu_;
    BlockStore store_;
    MetadataStore meta_;
    LruPolicy lru_;
    ThreadPool prefetch_pool_;
    std::unordered_map<std::string, CacheEntry> entries_;
    std::string root_;

    std::atomic<std::size_t> cache_hits_;
    std::atomic<std::size_t> cache_misses_;
};

ssize_t CacheManager::read(const std::string& path, char* buf, std::size_t len, off_t off) {
    std::lock_guard<std::mutex> g(mu_);
    CacheEntry& ce = entry(path);
    if (ce.evicted) return -ENOENT;

    ssize_t done = 0;
    while (done < static_cast<ssize_t>(len)) {
        std::size_t blk = (off + done) / kBlockSize;
        off_t blk_off = blk * kBlockSize;
        std::size_t in = (off + done) - blk_off;
        std::size_t want = std::min<std::size_t>(kBlockSize - in, len - done);

        char block[kBlockSize];
        bool cached = store_.read(ce.hash_hex, block, kBlockSize, blk_off) == static_cast<ssize_t>(kBlockSize);
        if (cached) {
            cache_hits_++;
        } else {
            cache_misses_++;
            ssize_t got = cache_fs::backend_read_range(path, block, kBlockSize, blk_off);
            if (got <= 0) {
                fs::path src = fs::path(root_) / fs::path(path[0] == '/' ? path.substr(1) : path);
                int fd = ::open(src.c_str(), O_RDONLY);
                if (fd >= 0) {
                    got = ::pread(fd, block, kBlockSize, blk_off);
                    ::close(fd);
                }
            }
            if (got <= 0) return (done ? done : -1);
            store_.write(ce.hash_hex, block, got, blk_off, false);
        }
        std::memcpy(buf + done, block + in, want);
        done += want;

        lru_.touch(reinterpret_cast<std::uintptr_t>(&ce) << 32 | blk, kBlockSize, 1.0);

        bool seq = (ce.last_block != std::numeric_limits<std::size_t>::max()) && (blk == ce.last_block + 1);
        ce.last_block = blk;
        if (seq) schedule_prefetch(ce, blk + 1);
    }
    return done;
}

ssize_t CacheManager::write(const std::string& path, const char* buf, std::size_t len, off_t off)
{
    std::lock_guard<std::mutex> g(mu_);
    CacheEntry& ce = entry(path);
    if (ce.evicted) return -ENOENT;

    int dst_fd = -1;
    auto ensure_dst = [&] {
    if (dst_fd != -1) return;
    fs::path dst = fs::path(root_) /
    fs::path(path[0] == '/' ? path.substr(1) : path);
    fs::create_directories(dst.parent_path());
    dst_fd = ::open(dst.c_str(), O_RDWR | O_CREAT, 0644);
};

std::size_t done = 0;
while (done < len) {
std::size_t blk  = (off + done) / kBlockSize;
off_t       boff = blk * kBlockSize;
std::size_t in   = (off + done) - boff;
std::size_t chunk= std::min<std::size_t>(kBlockSize - in, len - done);

char block[kBlockSize]{};
store_.read(ce.hash_hex, block, kBlockSize, boff);
std::memcpy(block + in, buf + done, chunk);
store_.write(ce.hash_hex, block, kBlockSize, boff, true);

meta_.markDirtyBlock(ce.hash_hex, boff / fs_layout::kMaxPartSize, blk);
lru_.touch(reinterpret_cast<std::uintptr_t>(&ce)<<32 | blk,
kBlockSize, 1.0);

ensure_dst();
::pwrite(dst_fd, buf + done, chunk, off + done);

done += chunk;
}
if (dst_fd != -1) ::close(dst_fd);
return done;
}

void CacheManager::flush_all() {
    std::lock_guard<std::mutex> g(mu_);
    for (auto& [_, ce] : entries_) meta_.flushBitmaps(ce.hash_hex);
}

void CacheManager::evict_until_gb(double free_gb) {
    auto used_gb = [&] {
        std::uintmax_t bytes = 0;
        for (auto& f : fs::recursive_directory_iterator(root_))
            if (f.is_regular_file()) bytes += f.file_size();
        return double(bytes)/(1024.0*1024.0*1024.0);
    };
    while (used_gb() > free_gb) {
        std::uintptr_t key = lru_.evict();
        if (key == std::numeric_limits<std::uintptr_t>::max()) break;
        auto* ce = reinterpret_cast<CacheEntry*>(key >> 32);
        if (!ce || ce->evicted) continue;
        store_.delete_object(ce->hash_hex);
        meta_.flushBitmaps(ce->hash_hex);
        ce->evicted = true;
    }
}

CacheEntry& CacheManager::entry(const std::string& path) {
    auto it = entries_.find(path);
    if (it != entries_.end()) return it->second;
    CacheEntry ce{path, hash_hex(path)};
    return entries_.emplace(path, std::move(ce)).first->second;
}

void CacheManager::schedule_prefetch(CacheEntry ce, std::size_t first_blk) {
    prefetch_pool_.enqueue([this, ce, first_blk]() {
        for (std::size_t i = 0; i < PREFETCH_WINDOW; ++i) {
            std::size_t blk = first_blk + i;
            off_t off       = blk * kBlockSize;
            char  buf[kBlockSize];
            if (store_.read(ce.hash_hex, buf, kBlockSize, off) ==
                static_cast<ssize_t>(kBlockSize))
                continue;
            ssize_t got = cache_fs::backend_read_range(
                            ce.path, buf, kBlockSize, off);
            if (got > 0) {
                store_.write(ce.hash_hex, buf, got, off, false);
                lru_.touch(reinterpret_cast<std::uintptr_t>(
                            const_cast<CacheEntry*>(&ce))<<32 | blk,
                        kBlockSize, 0.25);
            }
        }
    });
}

static std::unique_ptr<CacheManager> g_cache;

extern "C" {

std::size_t cache_get_hits() {
    return g_cache ? g_cache->get_cache_hits() : 0;
}

std::size_t cache_get_misses() {
    return g_cache ? g_cache->get_cache_misses() : 0;
}

void cache_reset_stats() {
    if (g_cache) g_cache->reset_stats();
}

int cache_init(const char* root, int) {
    try { g_cache = std::make_unique<CacheManager>(root); return 0; }
    catch (...) { return -1; }
}

int cache_store_file(const char* p, const char* d, size_t len, off_t off) {
    if (!g_cache) return -ENODEV;
    ssize_t rc = g_cache->write(p, d, len, off);
    return (rc < 0) ? static_cast<int>(rc) : 0;
}

ssize_t cache_read_file(const char* p, char* b, size_t len, off_t off) {
    return g_cache ? g_cache->read(p, b, len, off) : -ENODEV;
}

int cache_evict_gb(double gb) {
    if (!g_cache) return -ENODEV;
    g_cache->evict_until_gb(gb);
    return 0;
}

void cache_flush_all() {
    if (g_cache) g_cache->flush_all();
}

void cache_cleanup(void) {
    if (g_cache) {
        g_cache->flush_all();
        g_cache.release();
    }
}

bool cache_has_valid_entry(const char* path) {
    return g_cache && g_cache->has_valid_entry(path);
}

void* cache_get_entry(const char* path) {
    return g_cache ? static_cast<void*>(g_cache->get_entry(path)) : nullptr;
}

int cache_apply_eviction(void) {
    return cache_evict_gb(1.0);
}

} // extern "C"
