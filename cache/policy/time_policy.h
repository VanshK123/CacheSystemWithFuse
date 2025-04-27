#ifndef TIME_POLICY_H
#define TIME_POLICY_H

#include <cstddef>
#include <chrono>
#include <unordered_map>

class TimePolicy {
public:
    using Clock = std::chrono::steady_clock;

    TimePolicy(long ttlSeconds);

    void touch(size_t blockId);

    void remove(size_t blockId);

    size_t evict();

private:
    long ttlSeconds_;
    std::unordered_map<size_t, Clock::time_point> timestamps_;
};

#endif