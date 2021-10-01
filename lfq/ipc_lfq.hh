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
  using value_type = T*;

  const int fd;
  std::atomic<std::size_t> head;
  std::atomic<std::size_t> count;
  std::size_t tail;
  std::array<std::atomic<value_type>, N> data;

 public:
  ipc_lfq(const int& _) : fd(_), head(0), count(0), tail(0) {
    for (auto& datum : data) {
      datum = nullptr;
    }
  }

  bool enqueue(T* value) {
    auto size = this->count.fetch_add(1ul, std::memory_order_acquire);
    if (size >= N) {
      this->count.fetch_sub(1ul, std::memory_order_release);
      return false;
    }
    auto head = this->head.fetch_add(1ul, std::memory_order_acquire) % N;
    auto* ret = this->data[head].exchange(value, std::memory_order_release);
    assert(ret == nullptr);
    return true;
  }

  value_type dequeue(void) {
    auto ret =
        this->data[this->tail].exchange(nullptr, std::memory_order_acquire);
    if (ret == nullptr) {
      return nullptr;
    }
    if (++(this->tail) >= N) this->tail = 0;
    auto last = this->count.fetch_sub(1ul, std::memory_order_release);
    assert(last > 0);
    return ret;
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
