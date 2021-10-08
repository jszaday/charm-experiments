#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <algorithm>
#include <ctime>

#include "pool.hh"
#include "queue.hh"

CpvDeclare(int, run_handler);
CpvDeclare(int, initiate_handler);

struct empty_msg_ {
  char core[CmiMsgHeaderSizeBytes];
};

struct init_msg_ : public empty_msg_ {
  int src;
  xpmem_segid_t segid;
  void *pool;
  void *queue;
};

struct block_msg_ {
  ipc::block *block;
  std::size_t size;
  bool last;

  block_msg_(void) = default;

  block_msg_(ipc::block *block_, const std::size_t &size_, const bool &last_)
      : block(block_), size(size_), last(last_) {}
};

constexpr auto kNumMsgs = 16;

std::map<int, ipc::mempool *> pools_;
std::map<int, ipc::queue *> queues_;

ipc::mempool *pool_for(const int &pe) {
  auto search = pools_.find(pe);
  if (search == std::end(pools_)) {
    if (pe == CmiMyNode()) {
      auto ins = pools_.emplace(pe, new ipc::mempool(pe));
      assert(ins.second);
      search = ins.first;
    } else {
      return nullptr;
    }
  }
  return search->second;
}

ipc::queue *queue_for(const int &pe) {
  auto search = queues_.find(pe);
  if (search == std::end(queues_)) {
    if (pe == CmiMyNode()) {
      auto ins = queues_.emplace(pe, new ipc::queue(pe));
      assert(ins.second);
      search = ins.first;
    } else {
      return nullptr;
    }
  }
  return search->second;
}

void *null_merge_fn(int *size, void *local, void **remote, int count) {
  return local;
}

bool handle_block(ipc::block *);

#endif
