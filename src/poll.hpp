#pragma once
#include <sys/epoll.h>

namespace TinyCoroutine {
enum class poll_status { CLOSED, READ, WRITE, TIMEOUT, ERROR };

enum class poll_op : uint64_t {
  READ = EPOLLIN,
  WRITE = EPOLLOUT,
  READ_WRITE = EPOLLIN | EPOLLOUT
};
}; // namespace TinyCoroutine