#include <algorithm>
#include <ctime>

#include "../lfq/ipc_lfq.hh"
#include "pool.hh"

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
  ipc_pool::block *block;
  std::size_t size;
  bool last;

  block_msg_(void) = default;

  block_msg_(ipc_pool::block *block_, const std::size_t &size_,
             const bool &last_)
      : block(block_), size(size_), last(last_) {}
};

constexpr auto kNumMsgs = 16;
using ipc_queue = ipc_lfq<block_msg_, kNumMsgs>;

std::map<int, ipc_pool *> pools_;
std::map<int, ipc_queue *> queues_;

ipc_pool *pool_for(const int &pe) {
  auto search = pools_.find(pe);
  if (search == std::end(pools_)) {
    if (pe == CmiMyNode()) {
      auto ins = pools_.emplace(pe, new ipc_pool(pe));
      assert(ins.second);
      search = ins.first;
    } else {
      return nullptr;
    }
  }
  return search->second;
}

ipc_queue *queue_for(const int &pe) {
  auto search = queues_.find(pe);
  if (search == std::end(queues_)) {
    if (pe == CmiMyNode()) {
      auto ins = queues_.emplace(pe, new ipc_queue(pe));
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

void handle_initiate(void *msg) {
  auto *typed = (init_msg_ *)msg;
  auto &srcPe = typed->src;

  put_segment(srcPe, typed->segid);
  auto ins_pool = pools_.emplace(
      srcPe,
      (ipc_pool *)translate_address(srcPe, typed->pool, sizeof(ipc_pool)));
  assert(ins_pool.second);

  auto ins_queue = queues_.emplace(
      srcPe,
      (ipc_queue *)translate_address(srcPe, typed->queue, sizeof(ipc_queue)));
  assert(ins_queue.second);

  CmiFree(msg);

  if (pools_.size() == CmiNumNodes()) {
    CmiPrintf("%d> all pool information received for %d peer(s)...\n",
              CmiMyNode(), CmiNumNodes() - 1);
    auto *msg = (empty_msg_ *)CmiAlloc(sizeof(empty_msg_));
    CmiInitMsgHeader(msg->core, sizeof(empty_msg_));
    CmiSetHandler(msg, CpvAccess(run_handler));
    CmiReduce((char *)msg, sizeof(empty_msg_), null_merge_fn);
  }
}

bool handle_block(void *msg);

void handle_run(void *msg) {
  auto mine = CmiMyNode();

  if (mine == 0) {
    CmiPrintf("%d> broadcasting start up message...\n", mine);
    CmiInitMsgHeader(((empty_msg_ *)msg)->core, sizeof(empty_msg_));
    CmiSetHandler(msg, CpvAccess(run_handler));
    CmiSyncBroadcastAndFree(sizeof(empty_msg_), (char *)msg);
    CthYield();
  } else {
    CmiFree(msg);
  }

  auto numBlocks = kNumMsgs / 2;
  auto maxSize = 128;

  CmiPrintf("%d> populating local buffers...\n", mine);
  auto my_pool = pool_for(mine);
  for (auto i = 0; i < numBlocks; i++) {
    assert(my_pool->lput(malloc(maxSize), maxSize));
    assert(my_pool->lput(malloc(maxSize / 2), maxSize / 2));
  }

  auto theirs = (mine + 1) % CmiNumPes();
  auto their_pool = pool_for(theirs);
  auto their_queue = queue_for(theirs);
  assert(theirs == their_pool->pe);
  assert(theirs == their_queue->fd);

  srand(time(0));

  CmiPrintf("%d> awaiting remote buffers...\n", mine);
  for (auto i = 0; i < numBlocks * 2;) {
    constexpr auto intSz = sizeof(int);
    auto sz = (rand() % maxSize) + 1;
    sz = sz + intSz - (sz % intSz);
    if (sz > maxSize) sz = maxSize;
    assert((sz % intSz) == 0);

    auto *block = their_pool->ralloc(sz);
    CmiPrintf("%d> requesting block of size %d bytes... %s\n", mine, sz,
              block ? "pass" : "fail");

    if (block != nullptr) {
      // fill the block with test data
      auto *xlatd = (ipc_pool::block *)translate_address(
          theirs, block, sizeof(ipc_pool::block));
      auto *data = (int *)translate_address(theirs, xlatd->ptr, sz);
      std::fill(data, data + (sz / intSz), (int)sz);
      // and send it back to the host pe for verification/free
      block_msg_ msg(block, sz, ++i == (numBlocks * 2));
      while (!their_queue->enqueue(msg))
        ;
    }
  }

  // pseudo-scheduler loop for short messages
  bool status;
  auto my_queue = queue_for(mine);
  do {
    block_msg_ msg;
    while (!my_queue->dequeue(&msg))
      ;
    status = handle_block(&msg);
  } while (status);

  CsdExitScheduler();
}

bool handle_block(void *msg) {
  auto mine = CmiMyNode();
  auto *typed = (block_msg_ *)msg;
  CmiPrintf("%d> received %s block!\n", mine, typed->last ? "last" : "a");
  // verify the block
  auto *block = typed->block;
  auto *data = (int *)block->ptr;
  assert(
      std::all_of(data, data + (typed->size / sizeof(int)),
                  [&](const int &value) { return value == (int)typed->size; }));
  // clean everything up
  free(data);
  delete block;
  // return status
  return !typed->last;
}

void test_init(int argc, char **argv) {
  // Allocate initialization mesasage
  auto *msg = (init_msg_ *)CmiAlloc(sizeof(init_msg_));
  CmiInitMsgHeader(msg->core, sizeof(empty_msg_));
  // And populate it (while creating the xpmem segment)
  msg->src = CmiMyNode();
  msg->segid = make_segment();
  msg->pool = pool_for(msg->src);
  msg->queue = queue_for(msg->src);
  put_segment(msg->src, msg->segid);
  assert(msg->segid >= 0);
  // Register handlers
  CpvInitialize(int, run_handler);
  CpvAccess(run_handler) = CmiRegisterHandler((CmiHandler)handle_run);
  CpvInitialize(int, initiate_handler);
  CpvAccess(initiate_handler) = CmiRegisterHandler((CmiHandler)handle_initiate);
  // Wait for all PEs of the node to complete topology init
  CmiNodeAllBarrier();
  // Broadcast initialization information to all PEs
  CmiSetHandler(msg, CpvAccess(initiate_handler));
  CmiSyncBroadcastAndFree(sizeof(init_msg_), (char *)msg);
}

int main(int argc, char **argv) { ConverseInit(argc, argv, test_init, 0, 0); }
