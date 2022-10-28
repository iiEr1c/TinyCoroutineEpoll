#pragma once
#include <atomic>
#include <task.hpp>
#include "awaitable.hpp"

namespace TinyCoroutine
{
  struct sync_wait_event
  {

    void set()
    {
      m_flag.test_and_set();
      m_flag.notify_all();
    }

    void wait() { m_flag.wait(false); }

  private:
    std::atomic_flag m_flag;
  };

  template <typename return_type>
  struct sync_wait_task_promise
  {

    using coroutine_type =
        std::coroutine_handle<sync_wait_task_promise<return_type>>;

    sync_wait_task_promise() noexcept : m_return_value{} {}
    ~sync_wait_task_promise() noexcept = default;

    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto get_return_object() noexcept
    {
      return coroutine_type::from_promise(*this);
    }

    auto final_suspend() noexcept
    {
      struct completion_notifier
      {
        auto await_ready() noexcept { return false; }
        auto await_resume() noexcept {}
        auto await_suspend(coroutine_type coro) noexcept
        {
          coro.promise().m_event->set();
        }
      };

      return completion_notifier{};
    }
    auto unhandled_exception() noexcept { std::abort(); }

    auto yield_value(return_type &&value) noexcept
    {
      m_return_value = std::forward<return_type>(value);
      return final_suspend();
    }

    auto result() -> return_type &&
    {
      return static_cast<return_type &&>(m_return_value);
    }

    auto start(sync_wait_event &event)
    {
      m_event = &event;
      coroutine_type::from_promise(*this).resume();
    }

  private:
    sync_wait_event *m_event{nullptr};
    std::remove_const_t<std::remove_reference_t<return_type>> m_return_value;
    // std::remove_reference_t<return_type> m_return_value;
  };

  template <>
  struct sync_wait_task_promise<void>
  {

    using coroutine_type = std::coroutine_handle<sync_wait_task_promise<void>>;

    sync_wait_task_promise() noexcept = default;
    ~sync_wait_task_promise() noexcept = default;

    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto get_return_object() noexcept
    {
      return coroutine_type::from_promise(*this);
    }

    auto final_suspend() noexcept
    {
      struct completion_notifier
      {
        auto await_ready() noexcept { return false; }
        auto await_resume() noexcept {}
        auto await_suspend(coroutine_type coro) noexcept
        {
          coro.promise().m_event->set();
        }
      };

      return completion_notifier{};
    }
    auto unhandled_exception() noexcept -> void { std::abort(); }

    auto return_void() noexcept -> void {}

    auto result() -> void {}

    auto start(sync_wait_event &event)
    {
      m_event = &event;
      coroutine_type::from_promise(*this).resume();
    }

  private:
    sync_wait_event *m_event{nullptr};
  };

  template <typename return_type>
  class sync_wait_task
  {
  public:
    using promise_type = sync_wait_task_promise<return_type>;
    using coroutine_type = std::coroutine_handle<promise_type>;

    sync_wait_task(coroutine_type coro) noexcept : m_coroutine{coro} {}
    ~sync_wait_task()
    {
      if (m_coroutine)
      {
        m_coroutine.destroy();
      }
    }

    auto start(sync_wait_event &event) noexcept
    {
      m_coroutine.promise().start(event);
    }

    auto return_value() -> decltype(auto)
    {
      if constexpr (std::is_void_v<return_type>)
      {
        m_coroutine.promise().result();
        return;
      }
      else
      {
        return m_coroutine.promise().result();
      }
    }

  private:
    coroutine_type m_coroutine{nullptr};
  };

  template <awaitable awaitable_type, typename return_type = typename awaitable_traits<
                                          awaitable_type>::awaiter_return_type>
  static auto make_sync_wait_task(awaitable_type &&a)
      -> sync_wait_task<return_type>;

  template <awaitable awaitable_type, typename return_type>
  static auto make_sync_wait_task(awaitable_type &&a)
      -> sync_wait_task<return_type>
  {
    if constexpr (std::is_void_v<return_type>)
    {
      co_await std::forward<awaitable_type>(a);
      co_return;
    }
    else
    {
      co_yield co_await std::forward<awaitable_type>(a);
    }
  }

  template <awaitable awaitable_type>
  auto sync_wait(awaitable_type &&a) -> decltype(auto)
  {
    sync_wait_event e{};
    auto task = make_sync_wait_task(std::forward<awaitable_type>(a));
    task.start(e);
    e.wait();

    return task.return_value();
  }

}; // namespace TinyCoroutine