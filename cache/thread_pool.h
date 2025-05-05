#ifndef CACHE_THREAD_POOL_H
#define CACHE_THREAD_POOL_H

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(std::size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    template <class F>
    std::future<void> enqueue(F&& task);

private:
    void worker_loop();

    std::vector<std::thread>               workers_;
    std::queue<std::function<void()>>      tasks_;
    std::mutex                             mu_;
    std::condition_variable                cv_;
    bool                                   stop_ = false;
};

#include "thread_pool.inl"

#endif
