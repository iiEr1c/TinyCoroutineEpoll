#pragma once
#include "ip_v4_address.hpp"
#include "poll.hpp"
#include <coroutine>
#include <io_scheduler.hpp>
#include <string>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

namespace TinyTcpServer {
class Acceptor {
  TinyCoroutine::io_scheduler *m_io_schedule;
  IPv4Address m_host_addr;
  int m_acceptFd;

public:
  Acceptor(TinyCoroutine::io_scheduler *io_schedule, const std::string &host,
           uint16_t port)
      : m_io_schedule(io_schedule), m_host_addr(host, port) {
    m_acceptFd =
        ::socket(m_host_addr.getFamily(),
                 SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    int optval = 1;
    ::setsockopt(m_acceptFd, SOL_SOCKET, SO_REUSEADDR, &optval,
                 static_cast<socklen_t>(sizeof(optval)));
    ::setsockopt(m_acceptFd, SOL_SOCKET, SO_REUSEPORT, &optval,
                 static_cast<socklen_t>(sizeof(optval)));

    int ret =
        ::bind(m_acceptFd, m_host_addr.getSockaddr_in(), sizeof(sockaddr_in));
    ::listen(m_acceptFd, SOMAXCONN);
  }

  ~Acceptor() { ::close(m_acceptFd); }

  inline int fd() const { return m_acceptFd; }

  auto accept() -> TinyCoroutine::task<TinyCoroutine::poll_status> {
    return m_io_schedule->poll(m_acceptFd, TinyCoroutine::poll_op::READ);
  }

  void setkeepalive(int fd, unsigned int begin, unsigned int cnt,
                    unsigned int intvl) {
    if (fd) {
      int keepalive = 1;
      if (::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive,
                       static_cast<socklen_t>(keepalive)) == -1) {
        // log, enable keepalive error
      }

      // 距离上次发送数据多长时间后开始探测
      if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &begin,
                     static_cast<socklen_t>(begin)) == -1) {
        // log
      }

      //探测没有回应要坚持多少次
      if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt,
                     static_cast<socklen_t>(cnt)) == -1) {
        // log
      }

      //无数据交互下 每隔多长时间探测一次
      if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl,
                     static_cast<socklen_t>(intvl)) == -1) {
        // log
      }
    }
  }
};
}; // namespace TinyTcpServer