#include "time_policy.h"

TimePolicy::TimePolicy(long ttlSeconds)
    : ttlSeconds_(ttlSeconds) {}

void TimePolicy::touch(size_t blockId) {
    timestamps_[blockId] = Clock::now();
}

void TimePolicy::remove(size_t blockId) {
    timestamps_.erase(blockId);
}

size_t TimePolicy::evict() {
    auto now = Clock::now();
    size_t victim = SIZE_MAX;
    for (auto& [blockId, ts] : timestamps_) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - ts).count();
        if (age >= ttlSeconds_) {
            victim = blockId;
            break;
        }
    }
    if (victim != SIZE_MAX) {
        timestamps_.erase(victim);
    }
    return victim;
}
