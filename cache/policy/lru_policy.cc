#include "lru_policy.h"

#include <cfloat>
#include <limits>


LruPolicy::LruPolicy(std::size_t capacity)
: capacity_(capacity) {}


void LruPolicy::touch(std::size_t blockId, std::size_t bytes, double hotness) {
auto it = map_.find(blockId);

if (it != map_.end()) {
    order_.erase(it->second);
    map_.erase(it);
}
if (order_.size() >= capacity_) {
    std::size_t victim = evict();
    (void)victim;
}

order_.push_front({blockId, bytes, hotness});
map_[blockId] = order_.begin();
}


void LruPolicy::remove(std::size_t blockId) {
auto it = map_.find(blockId);
if (it != map_.end()) {
    order_.erase(it->second);
    map_.erase(it);
}
}


std::size_t LruPolicy::evict() {
if (order_.empty()) return std::numeric_limits<std::size_t>::max();

auto victim_it   = order_.end();
double worstScore = -DBL_MAX;

for (auto it = order_.begin(); it != order_.end(); ++it) {
    double s = score(*it);
    if (s > worstScore) {
        worstScore = s;
        victim_it  = it;
    }
}
if (victim_it == order_.end()) return std::numeric_limits<std::size_t>::max();

std::size_t victimId = victim_it->id;
order_.erase(victim_it);
map_.erase(victimId);
return victimId;
}
