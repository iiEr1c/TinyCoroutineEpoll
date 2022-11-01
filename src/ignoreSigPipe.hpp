#pragma once
#include <signal.h>

struct IgnoreSigPipe {
  IgnoreSigPipe() { ::signal(SIGPIPE, SIG_IGN); }
  IgnoreSigPipe(const IgnoreSigPipe &) = delete;
  IgnoreSigPipe &operator=(const IgnoreSigPipe &) = delete;
};

IgnoreSigPipe ignoreSigPipe;