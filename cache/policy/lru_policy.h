#ifndef LRU_POLICY_H
#define LRU_POLICY_H

#include <cstddef>
#include <list>
#include <unordered_map>

class LruPolicy {
public:
    LruPolicy(size_t capacity);

    void touch(size_t blockId);

    void remove(size_t blockId);

    size_t evict();

private:
    size_t capacity_;  
    std::list<size_t> order_;
    std::unordered_map<size_t, std::list<size_t>::iterator> map_;
};

#endif