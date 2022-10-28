#include <array>
#include <coroutine>
#include <io_scheduler.hpp>
#include <iostream>
#include <memory>
#include <socket/acceptor.hpp>
#include <socket/tcp_conn.hpp>
#include <string>
#include <sync_wait.hpp>
#include <task.hpp>

auto make_tcpconn(TinyCoroutine::io_scheduler *io_scheduler, int clientfd)
    -> TinyCoroutine::task<void> {
  co_await io_scheduler->schedule();
  TinyTcpServer::TcpConnection conn(io_scheduler, clientfd);
  while (true) {
    auto event = co_await conn.wait_event(TinyCoroutine::poll_op::READ);
    if (event == TinyCoroutine::poll_status::TIMEOUT ||
        event == TinyCoroutine::poll_status::CLOSED ||
        event == TinyCoroutine::poll_status::ERROR) {
      co_return;
    } else if (event == TinyCoroutine::poll_status::READ) {
      constexpr uint64_t buffer_len = 1024;
      std::array<char, buffer_len> buffer;
      ssize_t nreads = ::read(conn.fd(), buffer.data(), buffer.size());
      if (nreads <= 0) {
        co_return;
      } else {
        ssize_t nwrites =
            ::write(conn.fd(), buffer.data(), static_cast<uint64_t>(nreads));
        if (nwrites < nreads) {
          // ...
        }
      }
    } else if (event == TinyCoroutine::poll_status::WRITE) {
      // ...
    }
  }
  co_return;
}

auto make_server_task(TinyCoroutine::io_scheduler *io_scheduler,
                      std::string host, uint16_t port)
    -> TinyCoroutine::task<void> {
  TinyTcpServer::Acceptor acceptor(io_scheduler, host, port);
  while (true) {
    auto event = co_await acceptor.accept();
    if (event == TinyCoroutine::poll_status::READ) {
      struct sockaddr_storage client_addr {};
      socklen_t client_addr_len = sizeof(client_addr);
      int client_fd =
          ::accept4(acceptor.fd(), reinterpret_cast<sockaddr *>(&client_addr),
                    &client_addr_len, SOCK_CLOEXEC | SOCK_NONBLOCK);

      /* 为client创建tcp_connection对象 */
      make_tcpconn(io_scheduler, client_fd).detach();
    } else {
      // ignore...
    }
  }
  co_return;
}

int main() {
  TinyCoroutine::io_scheduler io_scheduler;
  std::string host("0.0.0.0");
  uint16_t port = 9999;
  TinyCoroutine::sync_wait(
      make_server_task(&io_scheduler, std::move(host), port));
  return 0;
}