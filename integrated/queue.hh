#ifndef __IPC_QUEUE__
#define __IPC_QUEUE__

#include "common.hh"

namespace ipc {

struct queue {
  const int pe;

  std::atomic<block *> head;

  queue(const int &pe_) : pe(pe_), head((block *)kTail) {}

  bool enqueue(block *ptr, block *loc = nullptr) {
    loc = (this->pe == CmiMyPe()) ? ptr : loc;
    CmiAssertMsg(loc, "expected non-null (local) pointer");
    auto prev = head.exchange(nullptr, std::memory_order_acquire);
    if (prev == nullptr) {
      return false;
    }
    loc->next = prev;
    auto check = head.exchange(ptr, std::memory_order_release);
    CmiAssertMsg(check == nullptr, "bad atomic exchange result");
    return true;
  }

  block *dequeue(void) {
    auto prev = head.exchange(nullptr, std::memory_order_acquire);
    if (prev == nullptr) {
      return nullptr;
    }
    auto nil = prev == (block *)kTail;
    auto next = nil ? prev : prev->next;
    auto check = head.exchange(next, std::memory_order_release);
    CmiAssertMsg(check == nullptr, "bad atomic exchange result");
    return nil ? nullptr : prev;
  }
};
}  // namespace ipc

#endif
