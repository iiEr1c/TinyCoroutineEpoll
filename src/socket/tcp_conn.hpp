#pragma once
#include "io_scheduler.hpp"
#include "poll.hpp"
#include <array>
#include <unistd.h>

namespace TinyTcpServer {
class TcpConnection {
  TinyCoroutine::io_scheduler *m_io_schedule;
  int m_client_fd;
  /* 这里为了简单, 先使用1024字节的buffer */
  std::array<char, 1024> m_read_buf;
  std::array<char, 1024> m_write_buf;

public:
  TcpConnection(TinyCoroutine::io_scheduler *io_scheduler, int client_fd)
      : m_io_schedule{io_scheduler}, m_client_fd{client_fd} {}
  ~TcpConnection() {
    if (m_client_fd > 0) [[likely]] {
      ::close(m_client_fd);
    }
  }

  /**
   * @brief 因为超时, 一个 poll_info 可能会因为两个时间返回两次
   * 如何避免这种情况呢?
   *
   * @param op
   * @return auto
   */
  auto wait_event(TinyCoroutine::poll_op op)
      -> TinyCoroutine::task<TinyCoroutine::poll_status> {
    // return m_io_schedule->poll(m_client_fd, op);
    return m_io_schedule->poll(m_client_fd, op, 10'000);
  }

  inline int fd() const { return m_client_fd; }
  inline void resetfd() { m_client_fd = -1; }
  inline auto &readbuf() const { return m_read_buf; }
  inline auto &writebuf() const { return m_write_buf; }

  ssize_t recv() {
    /* 因为设置的EPOLLONESHOT, 应该尽可能地读完 */
    ssize_t n = ::read(m_client_fd, m_read_buf.data(), 1024);
    return n;
  }

  ssize_t send(void *buf, size_t length) {
    ssize_t n = ::write(m_client_fd, buf, length);
    return n;
  }
};
}; // namespace TinyTcpServer