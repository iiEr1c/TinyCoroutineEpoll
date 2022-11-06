#pragma once
#include "poll.hpp"
#include "poll_info.hpp"
#include "task.hpp"
#include <array>
#include <atomic>
#include <fmt/core.h>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <thread>
#include <unordered_map>
#include <vector>

namespace TinyCoroutine {

class io_scheduler {
public:
  friend struct schedule_operation;
  explicit io_scheduler()
      : m_epoll_fd(::epoll_create1(EPOLL_CLOEXEC)),
        m_timer_fd(
            ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)) {
    /* epoll应该监听timer事件 */
    /* todo: eventfd */
    epoll_event event{.events = EPOLLIN,
                      .data = {.ptr = const_cast<void *>(m_timer_ptr)}};
    ::epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_timer_fd, &event);

    m_thread = std::thread([this]() { this->loop(); });
  }

  ~io_scheduler() {
    shutdown();
    ::close(m_timer_fd);
    ::close(m_epoll_fd);
  }

  /**
   * @brief 暂时不支持超时
   *
   * @param fd
   * @param op
   * @return auto
   */
  auto poll(int fd, poll_op op) -> task<poll_status> {
    poll_info pi{};
    pi.m_fd = fd;

    epoll_event e{.events =
                      static_cast<uint32_t>(op) | EPOLLRDHUP | EPOLLONESHOT,
                  .data = {.ptr = &pi}};
    ::epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &e);
    auto result = co_await pi;
    co_return result;
  }

  /**
   * @brief
   *
   * @param fd
   * @param op
   * @param timeout 毫秒
   * @return task<poll_status>
   */
  auto poll(int fd, poll_op op, int64_t timeout) -> task<poll_status> {
    poll_info pi{};
    pi.m_fd = fd;
    pi.m_timer_iterator =
        add_timer_token(static_cast<uint64_t>(getNow() + timeout), &pi);

    epoll_event e{.events =
                      static_cast<uint32_t>(op) | EPOLLRDHUP | EPOLLONESHOT,
                  .data = {.ptr = &pi}};
    ::epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &e);
    auto result = co_await pi;
    co_return result;
  }

  struct schedule_operation {
    friend class io_scheduler;
    explicit schedule_operation(io_scheduler &io_schedule)
        : m_io_schedule{io_schedule} {}
    auto await_ready() noexcept -> bool { return false; }
    auto await_resume() noexcept -> void {}
    auto await_suspend(std::coroutine_handle<> handle) noexcept -> void {
      /* 添加到io_shceduler的线程任务中 */
      std::scoped_lock lock(m_io_schedule.m_mut);
      m_io_schedule.m_scheduled_tasks.emplace_back(handle);
    }

  private:
    io_scheduler &m_io_schedule;
  };

  auto schedule() -> schedule_operation { return schedule_operation{*this}; }

  auto shutdown() noexcept -> void {
    isStop.test_and_set();
    if (m_thread.joinable()) {
      m_thread.join();
    }
  }

  std::unordered_map<int, task<void>> &get_coroutines() { return m_coroutines; }

private:
  inline auto event_to_poll_status(uint32_t events) -> poll_status {

    if (events & EPOLLRDHUP || events & EPOLLHUP) {
      return poll_status::CLOSED;
    } else if (events & EPOLLIN || events & EPOLLPRI) {
      return poll_status::READ;
    } else if (events & EPOLLOUT) {
      return poll_status::WRITE;
    } else {
      return poll_status::ERROR;
    }
  }

  inline std::string event_to_string(uint32_t events) {
    if (events & EPOLLRDHUP || events & EPOLLHUP) {
      return "EPOLLRDHUP";
    } else if (events & EPOLLIN || events & EPOLLPRI) {
      return "EPOLLIN";
    } else if (events & EPOLLOUT) {
      return "EPOLLOUT";
    } else {
      return "EPOLLERR";
    }
  }

  void loop() {
    while (!isStop.test()) {
      auto recv_count =
          ::epoll_wait(m_epoll_fd, m_events.data(), m_max_events, 1'000);
      if (recv_count <= 0) [[unlikely]] {
        // ...
      } else {
        fmt::print("recv [{}] events\n", recv_count);
        for (size_t i = 0; i < static_cast<std::size_t>(recv_count); ++i) {
          epoll_event &event = m_events[i];
          if (event.data.ptr == m_timer_ptr) {
            fmt::print("handle timeout event\n");
            process_timeout_execute();
          } else {
            /* 暂时未添加超时等功能, 这里直接强转为poll_info* */
            auto *handle_ptr = static_cast<poll_info *>(event.data.ptr);
            fmt::print("{}th event, event type[{}]\n", i + 1,
                       event_to_string(event.events));
            process_event_execute(handle_ptr,
                                  event_to_poll_status(event.events));
          }
        }
      }

      std::vector<std::coroutine_handle<>> tmp;
      {
        std::scoped_lock lk{m_mut};
        tmp.swap(m_scheduled_tasks);
      }
      fmt::print("have [{}] coroutine to resume\n", tmp.size());
      for (const auto &task : tmp) {
        task.resume();
      }
    }
  }

  /* 单位是ms */
  int64_t getNow() {
    timeval val{};
    ::gettimeofday(&val, nullptr);
    int64_t ret = val.tv_sec * 1000 + val.tv_usec / 1000;
    return ret;
  }

  auto process_timeout_execute() -> void {

    // 处理expire的读事件
    uint64_t bytes;
    ssize_t nbytes =
        ::read(m_timer_fd, &bytes, sizeof(bytes)); /* 防止事件一直触发 */

    std::vector<poll_info *> poll_infos{};
    /* 本质是修改poll_info里的状态 */
    /* 加锁并遍历multimap中的到期事件 */
    auto now = getNow();
    while (!m_timed_events.empty()) {
      auto first = m_timed_events.begin();
      auto [tp, pi] = *first;
      if (tp <= static_cast<uint64_t>(now)) {
        m_timed_events.erase(first);
        poll_infos.emplace_back(pi);
      } else {
        break;
      }
    }

    fmt::print("have [{}] events timeout\n", poll_infos.size());
    for (auto pi : poll_infos) {
      /* 超时, 从epoll中删除该fd, 置为已访问过 */
      if (!pi->is_timeout) {
        pi->is_timeout = true;
        /* 从epoll中删除, 确保下一轮loop不会再触发任何的读写事件 */
        if (pi->m_fd != -1) {
          ::epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, pi->m_fd, nullptr);
        }
        pi->m_poll_status = poll_status::TIMEOUT;
        fmt::print("ready to resume timeout coroutine [{}]\n",
                   pi->m_awaiting_coroutine.address());
        pi->m_awaiting_coroutine
            .resume(); /* 恢复pi对应的协程, 注意这里可能会和读写事件竞争,
                          所以增加了 processed 判断 */
      }
    }

    update_timeout(static_cast<uint64_t>(getNow()));
  }

  auto update_timeout(uint64_t timepoint) -> void {
    /* 如果超时队列非空, 那么第一个事件的时间设置为超时 */
    if (!m_timed_events.empty()) {
      auto &[tp, pi] = *m_timed_events.begin();
      /* 如果timepoint大于第一个事件的时间点 */
      int64_t amount =
          static_cast<int64_t>(tp - timepoint); /* 如果amount<0说明溢出 */
      if (amount < 100) [[unlikely]] {
        /* 溢出时将超时设置为相对timerfd_settime的1ms */
        amount = 100; /* 100ms误差 */
      }
      itimerspec ts{.it_interval = {.tv_sec = 0, .tv_nsec = 0},
                    .it_value = {.tv_sec = amount / 1000,
                                 .tv_nsec = (amount % 1'000) * 1'000'000}};
      /* todo: */
      ::timerfd_settime(m_timer_fd, 0, &ts, nullptr);
    }
    /* 如果为空, 忽略 */
  }

  auto add_timer_token(uint64_t tp, poll_info *pi)
      -> std::multimap<uint64_t, poll_info *>::iterator {
    auto pos = m_timed_events.emplace(tp, pi);
    if (pos == m_timed_events.begin()) {
      update_timeout(static_cast<uint64_t>(getNow()));
    }
    return pos;
  }

  auto remove_timer_token(std::multimap<uint64_t, poll_info *>::iterator it)
      -> void {
    bool is_first = (m_timed_events.begin() == it);
    m_timed_events.erase(it);
    if (is_first) {
      update_timeout(static_cast<uint64_t>(getNow()));
    }
  }

  auto process_event_execute(poll_info *pi, poll_status status) -> void {
    /* 在epoll中其实就限制了超时和可读可写不可能在一个loop中出现 */
    if (!pi->is_timeout) [[likely]] {
      fmt::print("process_event_execute(): [{}]\n", pi->m_fd);
      if (pi->m_fd != -1) [[likely]] {
        int ret = ::epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, pi->m_fd, nullptr);
        if (ret != 0) {
          // log
        }
      }

      /* 如果设置了超时, 则需要将超时取消掉 */
      if (pi->m_timer_iterator.has_value()) {
        remove_timer_token(pi->m_timer_iterator.value());
      }

      /* 设置pi的事件状态用于返回给上层 */
      pi->m_poll_status = status;
      /* 直接在此处恢复协程 */
      pi->m_awaiting_coroutine.resume();
    }
  }

private:
  static constexpr const int m_timer_object{0};
  static constexpr const void *m_timer_ptr = &m_timer_object;

  static constexpr std::size_t m_max_events = 16;

private:
  int m_epoll_fd{-1};
  int m_timer_fd{-1};
  std::thread m_thread; /* for run io_scheduler */
  std::array<epoll_event, m_max_events> m_events{};
  std::multimap<uint64_t, poll_info *> m_timed_events{};
  std::vector<std::coroutine_handle<>> m_scheduled_tasks{};
  mutable std::mutex m_mut;
  std::atomic_flag isStop{false};
  /* 添加一个fd--> conn coroutine的map */
  std::unordered_map<int, task<void>> m_coroutines;
};
}; // namespace TinyCoroutine