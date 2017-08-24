
#pragma once

#include "base/thread_pool.hpp"

namespace principia {
namespace base {
namespace internal_thread_pool {

template<typename T>
void ExecuteAndSetValue(std::function<T()> const& function,
                        std::promise<T>& promise) {
  promise.set_value(function());
}

template<>
void ExecuteAndSetValue<void>(std::function<void()> const& function,
                              std::promise<void>& promise) {
  function();
  promise.set_value();
}

}  // namespace internal_thread_pool

template<typename T>
ThreadPool<T>::ThreadPool(std::int64_t const pool_size) {
  for (std::int64_t i = 0; i < pool_size; ++i) {
    threads_.emplace_back(std::bind(&ThreadPool::DequeueCallAndExecute, this));
  }
}

template<typename T>
ThreadPool<T>::~ThreadPool() {
  {
    std::lock_guard<std::mutex> l(lock_);
    shutdown_ = true;
  }
  for (auto& thread : threads_) {
    thread.join();
  }
}

template<typename T>
std::future<T> ThreadPool<T>::Add(std::function<T()> function) {
  std::future<T> result;
  {
    std::lock_guard<std::mutex> l(lock_);
    calls_.push_back({std::move(function), std::promise<T>()});
    result = calls_.back().promise.get_future();
  }
  has_calls_.notify_one();
  return result;
}

template<typename T>
void ThreadPool<T>::DequeueCallAndExecute() {
  for (;;) {
    Call this_call;
    {
      std::unique_lock<std::mutex> l(lock_);
      has_calls_.wait(l, [this] { return shutdown_ || !calls_.empty(); });
      if (shutdown_) {
        break;
      }
      this_call = std::move(calls_.front());
      calls_.pop_front();
    }
    internal_thread_pool::ExecuteAndSetValue(this_call.function,
                                             this_call.promise);
  }
}

}  // namespace base
}  // namespace principia
