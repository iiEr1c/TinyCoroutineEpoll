#pragma once
#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace TinyCoroutine {
class ThreadPool {
public:
  explicit ThreadPool(std::size_t threadCount) {
    for (std::size_t i = 0; i < threadCount; ++i) {
      m_threads.emplace_back(std::thread([this] { this->loop(); }));
    }
  }

  ~ThreadPool() {
    std::cout << "~ThreadPool()\n";
    stop();
  }

  auto schedule() {
    struct awaiter {
      ThreadPool *m_threadpool;

      constexpr bool await_ready() const noexcept { return false; }
      constexpr void await_resume() const noexcept {}
      void await_suspend(std::coroutine_handle<> coro) const noexcept {
        m_threadpool->enqueue_task(coro);
      }
    };

    return awaiter{this};
  }

private:
  void loop() {
    while (!m_stop) {
      std::unique_lock<std::mutex> lock(m_mutex);
      /* 这里要判断m_stop是否停止, 否则陷入无限等待 */
      while (!m_stop && m_coroutines.size() == 0) {
        m_cond.wait_for(lock, std::chrono::microseconds(100));
      }
      if (m_stop) [[unlikely]] {
        break; /* 处理停止的情况 */
      }
      auto coro = m_coroutines.front();
      m_coroutines.pop();
      coro.resume(); /* 执行协程队列中的协程 */
    }
  }

  void stop() {
    m_stop = true;
    while (m_threads.size() > 0) {
      std::thread &thr = m_threads.back();
      if (thr.joinable()) {
        thr.join(); /* 回收所有的线程 */
      }
      m_threads.pop_back();
    }
  }

  void enqueue_task(std::coroutine_handle<> coro) noexcept {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_coroutines.emplace(std::move(coro));
    m_cond.notify_one();
  }

private:
  std::vector<std::thread> m_threads;
  std::mutex m_mutex;
  std::condition_variable m_cond;
  std::queue<std::coroutine_handle<>> m_coroutines;
  std::atomic<bool> m_stop = false;
};

}; // namespace TinyCoroutine