#include "lru_policy.h"

LruPolicy::LruPolicy(size_t capacity)
    : capacity_(capacity) {}

void LruPolicy::touch(size_t blockId) {
    auto it = map_.find(blockId);
    if (it != map_.end()) {
        order_.erase(it->second);
    } else if (order_.size() >= capacity_) {
        size_t lru = order_.back();
        order_.pop_back();
        map_.erase(lru);
    }
    order_.push_front(blockId);
    map_[blockId] = order_.begin();
}

void LruPolicy::remove(size_t blockId) {
    auto it = map_.find(blockId);
    if (it != map_.end()) {
        order_.erase(it->second);
        map_.erase(it);
    }
}

size_t LruPolicy::evict() {
    if (order_.empty()) return SIZE_MAX;
    size_t victim = order_.back();
    order_.pop_back();
    map_.erase(victim);
    return victim;
}
