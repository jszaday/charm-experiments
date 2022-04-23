#ifndef __IPC_COMMON_HH__
#define __IPC_COMMON_HH__

#include <converse.h>

#include <atomic>
#include <cstdint>
#include <limits>

#include "xpmem.hh"

namespace ipc {
struct block {
  block *orig;
  block *next;
  void *ptr;
  std::size_t size;
  std::atomic<bool> used;
  void *xlatd = nullptr;

  block(block *next_, void *ptr_, const std::size_t &size_)
      : orig(this), next(next_), ptr(ptr_), size(size_), used(false) {}
};

static constexpr auto kTail = std::numeric_limits<std::uintptr_t>::max();
}  // namespace ipc

#endif
