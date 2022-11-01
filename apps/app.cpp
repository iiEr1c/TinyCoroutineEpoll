#include <socket/tcp_server.hpp>

int main() {
  TinyTcpServer::TcpServer tcpserver(std::string("0.0.0.0"), 9999);
  tcpserver.start();
  return 0;
}