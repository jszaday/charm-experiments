#ifndef __IPC_LFQ_HH__
#define __IPC_LFQ_HH__

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>

template <typename T, std::size_t N>
class ipc_lfq {
  static_assert(std::is_trivially_copyable<T>::value,
                "value must be trivially copyable");

  struct node {
    std::atomic<bool> lock;
    bool has_value;
    T value[1];

    node(void) : lock(false), has_value(false) {}

    inline bool acquire(void) {
      return !this->lock.exchange(true, std::memory_order_acquire);
    }

    inline bool release(void) {
      return this->lock.exchange(false, std::memory_order_release);
    }
  };

  const int fd;
  std::atomic<std::size_t> head;
  std::atomic<std::size_t> count;
  std::size_t tail;
  std::array<node, N> data;

 public:
  ipc_lfq(const int& _) : fd(_), head(0), count(0), tail(0) {}

  bool enqueue(const T& value) {
    auto size = this->count.fetch_add(1ul, std::memory_order_acquire);
    if (size >= N) {
      this->count.fetch_sub(1ul, std::memory_order_release);
      return false;
    }
    auto head = this->head.fetch_add(1ul, std::memory_order_acquire) % N;
    auto& node = this->data[head];
    while (!node.acquire())
      ;
    auto set_value = true;
    std::swap(node.has_value, set_value);
    memcpy(node.value, &value, sizeof(T));
    assert(!set_value && node.release());
    return true;
  }

  bool dequeue(T* result) {
    auto* node = &(this->data[this->tail]);
    while (!node->acquire())
      ;
    auto ret = node->has_value;
    if (ret) {
      memcpy(result, node->value, sizeof(T));
      node->has_value = false;
    }
    assert(node->release());
    if (!ret) return false;
    if (++(this->tail) >= N) this->tail = 0;
    auto last = this->count.fetch_sub(1ul, std::memory_order_release);
    assert(last > 0);
    return true;
  }

  static ipc_lfq<T, N>* open(const char* name) {
    ipc_lfq<T, N>* res = nullptr;
    auto fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0666);
    auto size = sizeof(ipc_lfq<T, N>);
    auto init = fd >= 0;

    if (init) {
      auto status = ftruncate(fd, size);
      assert(status >= 0);
    } else {
      fd = shm_open(name, O_RDWR, 0666);
      assert(fd >= 0);
    }

    res = (ipc_lfq<T, N>*)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                               0);
    assert(res != MAP_FAILED);

    if (init) {
      new (res) ipc_lfq<T, N>(fd);
    }

    return res;
  }

  static bool close(const char* name, ipc_lfq<T, N>* self) {
    auto fd = self->fd;
    auto status = munmap(self, sizeof(ipc_lfq<T, N>)) >= 0;
    status = status && (::close(fd) >= 0);
    shm_unlink(name);
    return status;
  }
};

#endif
