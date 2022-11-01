#pragma once
#include "acceptor.hpp"
#include "fmt/core.h"
#include "ignoreSigPipe.hpp"
#include "io_scheduler.hpp"
#include "sync_wait.hpp"
#include "task.hpp"
#include "tcp_conn.hpp"
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>

namespace TinyTcpServer {

struct TcpServer {
  TcpServer(const std::string &host, uint16_t port) {
    m_io_scheduler = std::make_shared<TinyCoroutine::io_scheduler>();
    m_acceptor = std::make_shared<TinyTcpServer::Acceptor>(m_io_scheduler.get(),
                                                           host, port);
  }

  void start() {
    fmt::print("server start........\n");
    TinyCoroutine::sync_wait(accept());
  }

  auto accept() -> TinyCoroutine::task<void> {
    while (true) {
      auto event = co_await m_acceptor->accept();
      if (event == TinyCoroutine::poll_status::READ) {
        struct sockaddr_storage client_addr {};
        socklen_t client_addr_len = sizeof(client_addr);
        int clientfd = ::accept4(
            m_acceptor->fd(), reinterpret_cast<sockaddr *>(&client_addr),
            &client_addr_len, SOCK_CLOEXEC | SOCK_NONBLOCK);
        handle_conn(m_io_scheduler.get(), clientfd).detach();
      } else {
        // log and ignore...
      }
    }
    co_return;
  }

  auto handle_conn(TinyCoroutine::io_scheduler *io_scheduler, int clientfd)
      -> TinyCoroutine::task<void> {
    co_await io_scheduler->schedule();
    TinyTcpServer::TcpConnection conn(io_scheduler, clientfd);
    while (true) {
      // timeout 30's
      auto event =
          co_await conn.wait_event(TinyCoroutine::poll_op::READ, 30'000);
      if (event == TinyCoroutine::poll_status::TIMEOUT ||
          event == TinyCoroutine::poll_status::CLOSED ||
          event == TinyCoroutine::poll_status::ERROR) {
        co_return;
      } else if (event == TinyCoroutine::poll_status::READ) {
        auto readBytes = conn.recv();
        fmt::print("conn[{}] read [{}] bytes\n", conn.fd(), readBytes);
        // echo server
        auto writeBytes = conn.send(conn.readbuf().data(), readBytes);
        // handle write less
        // if (writeBytes < readBytes) {}
      } else if (event == TinyCoroutine::poll_status::WRITE) {
        //
      }
    }
    co_return;
  }

private:
  std::shared_ptr<TinyCoroutine::io_scheduler> m_io_scheduler;
  std::shared_ptr<TinyTcpServer::Acceptor> m_acceptor;
  // 生命周期如何处理?
  std::map<int, TinyCoroutine::task<void>> m_conns;
};
} // namespace TinyTcpServer