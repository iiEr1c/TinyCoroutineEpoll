#include <io_scheduler.hpp>
#include <iostream>
#include <socket/acceptor.hpp>
#include <socket/tcp_conn.hpp>
#include <string>
#include <sync_wait.hpp>
#include <task.hpp>

#include <coroutine>

TinyCoroutine::task<void> a() {
  co_await std::suspend_always{};
  std::cout << "here\n";
  co_return;
};

TinyCoroutine::task<void> b() { co_return; };

int main() {
  TinyCoroutine::io_scheduler io_scheduler;

  auto make_server_task = [&]() -> TinyCoroutine::task<void> {
    std::cout << "make_server_task: this_thread id: " << std::this_thread::get_id() << '\n';
    TinyTcpServer::Acceptor acceptor(&io_scheduler, "0.0.0.0", 9999);
    std::cout << "acceptor online\n";
    std::cout << "make_server_task: this_thread id: " << std::this_thread::get_id() << '\n';
    while (true) {
      auto evnet = co_await acceptor.accept();
      std::cout << "make_server_task: this_thread id: " << std::this_thread::get_id() << '\n';
      /* 处理事件 */
      struct sockaddr_storage client_addr;
      socklen_t client_addr_len = sizeof(client_addr);
      int client_fd =
          ::accept4(acceptor.fd(), reinterpret_cast<sockaddr *>(&client_addr),
                    &client_addr_len, SOCK_CLOEXEC | SOCK_NONBLOCK);
      std::cout << "client fd = " << client_fd << '\n';

      /* 为client创建tcp_connection对象 */
      auto make_tcpconn = [&](int clientfd) -> TinyCoroutine::task<void> {
        std::cout << "make_tcpconn: this_thread id: " << std::this_thread::get_id() << '\n';
        co_await io_scheduler.schedule();
        std::cout << "make_tcpconn: this_thread id: " << std::this_thread::get_id() << '\n';
        std::cout << "after io_scheduler.schedule()\n";
        TinyTcpServer::TcpConnection conn(&io_scheduler, clientfd);
        while (true) {
          auto event = co_await conn.wait_event(TinyCoroutine::poll_op::READ);
          if (event == TinyCoroutine::poll_status::TIMEOUT) {
            /* 超时,直接结束 */
            std::cout << "timeout\n";
            break;
          } else if (event == TinyCoroutine::poll_status::CLOSED ||
                     event == TinyCoroutine::poll_status::ERROR) {
            // char buf[64];
            // ::read(conn.fd(), buf, 64);
            // ::close(conn.fd());
            // conn.resetfd();
            break;
          } else if (event == TinyCoroutine::poll_status::READ) {
            char buf[64];
            ssize_t n = ::read(conn.fd(), buf, 64);
            if (n == 0) {
              ::close(conn.fd());
              co_return;
            }
            ssize_t nwrite = ::write(conn.fd(), buf, static_cast<size_t>(n));
          } else if (event == TinyCoroutine::poll_status::WRITE) {
            // ...
          }
        }
        co_return;
      };

      // auto h = make_tcpconn(client_fd);
      //  h.detach();
      // std::cout << "tcp_conn's address = " << h.handle().address() << '\n';
      // h.detach();
      make_tcpconn(client_fd).detach();
      /* 如果我们在io_scheduler中存了coroutine object, 那么协程结束后如何释放呢?
       * io_scheduler.get_coroutines().emplace(client_fd,
       * make_tcpconn(client_fd));
       */
    }
    co_return;
  };
  TinyCoroutine::sync_wait(make_server_task());
  // auto h = make_server_task();
  // h.resume();
  // using namespace std::chrono_literals;
  // std::this_thread::sleep_for(30s);
  // TinyCoroutine::sync_wait(make_server_task());
  return 0;
}