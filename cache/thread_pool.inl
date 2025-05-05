#pragma once
#include <utility>

template <class F>
std::future<void> ThreadPool::enqueue(F&& f) {
    auto task_ptr = std::make_shared<std::packaged_task<void()>>(std::forward<F>(f));
    {
        std::lock_guard<std::mutex> lock(mu_);
        tasks_.emplace([task_ptr]() { (*task_ptr)(); });
    }
    cv_.notify_one();
    return task_ptr->get_future();
}
