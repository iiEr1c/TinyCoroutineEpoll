#include "ip_v4_address.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>

namespace TinyTcpServer {
/*
 * htons()——"Host to Network short"
 * htonl()——"Host to Network Long"
 * ntohs()——"Network to Host short"
 * ntohl()——"Network to Host Long"
 */
IPv4Address::IPv4Address(const std::string &_ip, uint16_t _port)
    : addr{.sin_family = AF_INET,
           .sin_port = ::htons(_port),
           .sin_addr = ::inet_addr(_ip.data()),
           .sin_zero = 0} {}

IPv4Address::IPv4Address(sockaddr_in _addr) : addr(_addr) {}

sockaddr *IPv4Address::getSockAddr() {
  return reinterpret_cast<sockaddr *>(std::addressof(addr));
}
}; // namespace TinyTcpServer
