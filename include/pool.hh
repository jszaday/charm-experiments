#ifndef __POOL_HH__
#define __POOL_HH__

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>

#include "common.hh"

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

namespace ipc {
struct mempool {
  const int pe;

  std::array<std::atomic<block *>, kNumCutOffPoints> blocks;

  mempool(const int &pe_) : pe(pe_) {
    for (std::size_t i = 0; i < kNumCutOffPoints; i++) {
      this->blocks[i] = (block *)kTail;
    }
  }

  bool lput(void *ptr, const std::size_t &sz) {
    return this->lput(new block(nullptr, ptr, sz));
  }

  bool lput(block* blk) {
    auto pt = which_pow2(blk->size);
    if (pt >= kNumCutOffPoints || blk->size != kCutOffPoints[pt]) {
      return false;
    }
    auto *head = this->blocks[pt].exchange(nullptr, std::memory_order_acquire);
    if (head == nullptr) {
      return false;
    }
    blk->next = head;
    auto *swap = this->blocks[pt].exchange(blk, std::memory_order_release);
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
      auto *xlatd = translate_address(this->pe, head, kCutOffPoints[pt]);
      auto *next = ((block *)xlatd)->next;
      auto *swap = this->blocks[pt].exchange(next, std::memory_order_release);
      assert(swap == nullptr);
      return head;
    }
  }
};
}  // namespace ipc

#endif
