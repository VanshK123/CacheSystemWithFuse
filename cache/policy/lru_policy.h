#ifndef CACHE_POLICY_LRU_POLICY_H
#define CACHE_POLICY_LRU_POLICY_H

#include <cstddef>
#include <list>
#include <unordered_map>

class LruPolicy {
public:
    explicit LruPolicy(std::size_t capacity);

    void touch(std::size_t blockId, std::size_t bytes, double hotness);

    void remove(std::size_t blockId);

    std::size_t evict();

private:
    struct Node {
        std::size_t id;
        std::size_t bytes;
        double      hotness;
    };

    static inline double score(const Node& n) {
        return n.bytes * (1.0 - n.hotness);
    }

    std::size_t capacity_;
    std::list<Node> order_;
    std::unordered_map<std::size_t,
        std::list<Node>::iterator> map_;

    LruPolicy(const LruPolicy&)            = delete;
    LruPolicy& operator=(const LruPolicy&) = delete;
};

#endif
