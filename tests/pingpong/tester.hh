#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <algorithm>
#include <ctime>
#include <memory>

#include "pool.hh"
#include "queue.hh"

#define CpvRegister(v, fn) \
  CpvInitialize(v, int);   \
  CpvAccess(v) = CmiRegisterHandler(fn)

CpvDeclare(int, exit_handler);
CpvDeclare(int, initiate_handler);
CpvDeclare(int, recv_plain_handler);
CpvDeclare(int, start_plain_handler);
CpvDeclare(int, start_xpmem_handler);

struct state_ {
  double startTime;
  std::size_t size;
  bool warmup;
  int nIters;
  int it;
};

using state_ptr_ = std::unique_ptr<state_>;
CpvDeclare(state_ptr_, state_);

inline state_ *get_state_(void) { return CpvAccess(state_).get(); }

inline void reset_state_(state_ *state) {
  state->it = 0;
  state->startTime = CmiWallTimer();
}

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

constexpr auto kBufferFactor = 4;

void start_xpmem(void *);
void start_plain(void *);
void start_plain_(state_ *state);

void send_signal(const int &pe, const int &which);

void send_xpmem(state_ *state, const int &pe);
bool recv_xpmem(state_ *, ipc::mempool *, ipc::block *);

void populate_buffers_(const int &mine, const state_ *state);

void test_finish(state_ *state, const char *which, double totalTime);

#endif
