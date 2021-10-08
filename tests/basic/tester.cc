#include "tester.hh"

// exchange ipc mempools and queues with peers.
// contribute to a reduction when all info received.
void handle_initiate(void *msg) {
  auto *typed = (init_msg_ *)msg;
  auto &srcPe = typed->src;

  put_segment(srcPe, typed->segid);
  auto ins_pool =
      pools_.emplace(srcPe, (ipc::mempool *)translate_address(
                                srcPe, typed->pool, sizeof(ipc::mempool)));
  assert(ins_pool.second);

  auto ins_queue = queues_.emplace(
      srcPe,
      (ipc::queue *)translate_address(srcPe, typed->queue, sizeof(ipc::queue)));
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

/* goes through the following process:
 * 1) populate local (receive) buffers.
 * 2) send messages to peers.
 * 3) wait for messages to arrive, then handle.
 * 4) exit once all messages handled.
 */
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
    while (!my_pool->lput(malloc(maxSize), maxSize))
      ;
    while (!my_pool->lput(malloc(maxSize / 2), maxSize / 2))
      ;
  }

  auto theirs = (mine + 1) % CmiNumPes();
  auto their_pool = pool_for(theirs);
  auto their_queue = queue_for(theirs);
  assert(theirs == their_pool->pe);
  assert(theirs == their_queue->pe);

  srand(time(0));

  CmiPrintf("%d> awaiting remote buffers...\n", mine);
  for (auto i = 0; i < numBlocks * 2;) {
    constexpr auto intSz = sizeof(int);
    auto sz = rand() % maxSize;
    sz = sz + intSz - (sz % intSz);
    if (sz > maxSize) sz = maxSize;
    assert((sz % intSz) == 0);

    auto *block = their_pool->ralloc(sz);
    CmiPrintf("%d> requesting block of size %d bytes... %s\n", mine, sz,
              block ? "pass" : "fail");

    if (block != nullptr) {
      // fill the block with test data
      auto *xlatd =
          (ipc::block *)translate_address(theirs, block, sizeof(ipc::block));
      xlatd->size = sz;
      auto *ptr = (char *)translate_address(theirs, xlatd->ptr, sz);
      auto &last = *((int *)ptr);
      last = ++i == (numBlocks * 2);
      auto *data = (int *)(ptr + sizeof(int));
      std::fill(data, data + (sz / intSz), (int)sz);
      // and send it back to the host pe for verification/free
      while (!their_queue->enqueue(block, xlatd))
        ;
    }
  }

  // pseudo-scheduler loop for short messages
  bool status;
  ipc::block *block = nullptr;
  auto my_queue = queue_for(mine);
  do {
    while ((block = my_queue->dequeue()) == nullptr)
      ;
    status = handle_block(block);
  } while (status);

  CsdExitScheduler();
}

bool handle_block(ipc::block *block) {
  auto mine = CmiMyNode();
  auto size = block->size;
  auto last = *((int *)block->ptr);
  auto *data = (int *)((char *)block->ptr + sizeof(int));
  CmiPrintf("%d> received %s block!\n", mine, last ? "last" : "a");
  // verify the block
  assert(std::all_of(data, data + (size / sizeof(int)),
                     [&](const int &value) { return value == (int)size; }));
  // clean everything up
  free(block->ptr);
  delete block;
  // return status
  return !last;
}

void test_init(int argc, char **argv) {
  // Allocate initialization message
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
  CmiInitCPUAffinity(argv);
  CmiInitCPUTopology(argv);
  CmiNodeAllBarrier();
  // Enforce the test's topological assumptions
  assert(!CMK_SMP && CmiNumPhysicalNodes() == 1);
  if (CmiNumPes() == 1) {
    // Just start runnning (we are peerless)
    CmiSetHandler(msg, CpvAccess(run_handler));
    CmiSyncSendAndFree(CmiMyPe(), sizeof(init_msg_), (char *)msg);
  } else {
    // Broadcast initialization information to all PEs
    CmiSetHandler(msg, CpvAccess(initiate_handler));
    CmiSyncBroadcastAndFree(sizeof(init_msg_), (char *)msg);
  }
}

int main(int argc, char **argv) { ConverseInit(argc, argv, test_init, 0, 0); }
