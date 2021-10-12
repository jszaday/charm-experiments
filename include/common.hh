#ifndef __IPC_COMMON_HH__
#define __IPC_COMMON_HH__

#include <converse.h>

#include <atomic>
#include <cstdint>
#include <limits>

#include "xpmem.hh"

namespace ipc {
struct block {
  block *next;
  void *ptr;
  std::size_t size;

  block(block *next_, void *ptr_, const std::size_t &size_)
      : next(next_), ptr(ptr_), size(size_) {}
};

static constexpr auto kTail = std::numeric_limits<std::uintptr_t>::max();
}  // namespace ipc

#endif