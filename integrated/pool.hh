#ifndef __POOL_HH__
#define __POOL_HH__

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <limits>

constexpr std::size_t kNumCutOffPoints = 25;
const std::array<std::size_t, kNumCutOffPoints> kCutOffPoints = {
    64,        128,       256,       512,       1024,     2048,     4096,
    8192,      16384,     32768,     65536,     131072,   262144,   524288,
    1048576,   2097152,   4194304,   8388608,   16777216, 33554432, 67108864,
    134217728, 268435456, 536870912, 1073741824};

inline std::size_t which_pow2(const std::size_t &sz) {
  std::size_t i;
  for (i = 0; i <= kNumCutOffPoints; i++) {
    if (sz <= kCutOffPoints[i]) {
      return i;
    }
  }
  return i;
}

struct ipc_pool {
  const int pe;

  struct block {
    block *next;
    void *ptr;

    block(block *next_, void *ptr_) : next(next_), ptr(ptr_) {}
  };

  static constexpr auto kTail = std::numeric_limits<std::uintptr_t>::max();

  std::array<std::atomic<block *>, kNumCutOffPoints> blocks;

  ipc_pool(const int &pe_) : pe(pe_) {
    for (std::size_t i = 0; i < kNumCutOffPoints; i++) {
      this->blocks[i] = (block *)kTail;
    }
  }

  bool lput(void *ptr, const std::size_t &sz) {
    auto pt = which_pow2(sz);
    if (pt >= kNumCutOffPoints || sz != kCutOffPoints[pt]) {
      return false;
    }
    auto *head = this->blocks[pt].exchange(nullptr, std::memory_order_acquire);
    if (head == nullptr) {
      return false;
    }
    auto *next = new block(head, ptr);
    auto *swap = this->blocks[pt].exchange(next, std::memory_order_release);
    assert(swap == nullptr);
    return true;
  }

  block *ralloc(const std::size_t &sz) {
    auto pt = which_pow2(sz);
    if (pt >= kNumCutOffPoints) {
      return nullptr;
    }
    auto *head = this->blocks[pt].exchange(nullptr, std::memory_order_acquire);
    if (head == nullptr) {
      return nullptr;
    } else if (head == (block *)kTail) {
      auto *swap = this->blocks[pt].exchange(head, std::memory_order_release);
      assert(swap == nullptr);
      return nullptr;
    } else {
      auto *next = this->translate_address(head)->next;
      auto *swap = this->blocks[pt].exchange(next, std::memory_order_release);
      assert(swap == nullptr);
      return head;
    }
  }

  template <typename T>
  T *translate_address(T *addr) {
    return addr;
  }
};

#endif
