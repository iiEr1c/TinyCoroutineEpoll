#include "poll.hpp"
#include <coroutine>
#include <map>
#include <optional>
#include <sys/epoll.h>

namespace TinyCoroutine {
struct poll_info {
public:
  int m_fd{-1};
  std::coroutine_handle<> m_awaiting_coroutine;
  /* 根据epoll返回转换的类型, 初始值为error */
  poll_status m_poll_status{poll_status::ERROR};
  std::optional<std::multimap<uint64_t, poll_info *>::iterator>
      m_timer_iterator{std::nullopt};
  /* todo: timeout */
  bool is_timeout{false}; /* 只有超时会触发 */

public:
  poll_info() = default;
  ~poll_info() = default;
  poll_info(const poll_info &) = delete;
  poll_info(poll_info &&) = delete;
  auto operator=(const poll_info &) -> poll_info & = delete;
  auto operator=(poll_info &&) -> poll_info & = delete;

  struct poll_awaiter {
    poll_info &m_pi;
    explicit poll_awaiter(poll_info &pi) noexcept : m_pi(pi) {}
    auto await_ready() const noexcept -> bool { return false; }
    auto await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
        -> void {
      m_pi.m_awaiting_coroutine = awaiting_coroutine;
      /* 暂时使用单线程, 不考虑线程安全问题 */
    }
    auto await_resume() noexcept -> poll_status { return m_pi.m_poll_status; }
  };

  auto operator co_await() noexcept -> poll_awaiter {
    return poll_awaiter{*this};
  }
};
}; // namespace TinyCoroutine