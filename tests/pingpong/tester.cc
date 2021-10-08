#include "tester.hh"

void start_plain(void *msg) {
  if (msg) CmiFree(msg);

  start_plain_(get_state_());
}

void start_xpmem(void *msg) {
  auto *state = get_state_();

  auto mine = CmiMyPe();
  auto peer = (CmiMyPe() + 1) % CmiNumPes();
  auto my_pool = pool_for(mine);
  auto my_queue = queue_for(mine);

  if (msg) CmiFree(msg);

  reset_state_(state);

  if (mine == 0) {
    send_xpmem(state, peer);
  }

  // pseudo-scheduler loop
  bool status;
  ipc::block *block;
  do {
    while ((block = my_queue->dequeue()) == nullptr)
      ;
    status = recv_xpmem(state, my_pool, block);
  } while (status);
}

inline bool recv_common(state_ *state, const int &mine, const char *which) {
  auto done = (++state->it == state->nIters);
  if (done && (mine == 0)) {
    auto endTime = CmiWallTimer();
    test_finish(state, which, endTime - state->startTime);
  }
  return done;
}

void recv_plain(void *msg) {
  auto mine = CmiMyNode();
  auto peer = (mine + 1) % CmiNumNodes();
  auto *state = get_state_();

  if ((mine == 0) && recv_common(state, mine, "plain")) {
    CmiFree(msg);
  } else {
    CmiSyncSendAndFree(peer, state->size, (char *)msg);
  }
}

bool recv_xpmem(state_ *state, ipc::mempool *my_pool, ipc::block *my_block) {
  auto mine = CmiMyNode();
  auto peer = (mine + 1) % CmiNumNodes();

  // put the block back so we can re-use it
  while (!my_pool->lput(my_block))
    ;

  bool done;
  if ((done = recv_common(state, mine, "xpmem")) && (mine == 0)) {
    return false;
  } else {
    send_xpmem(state, peer);

    return !done;
  }
}

void send_xpmem(state_ *state, const int &peer) {
  auto their_pool = pool_for(peer);
  auto their_queue = queue_for(peer);

  // get a remote allocation from the other pe
  ipc::block *their_block;
  while (!(their_block = their_pool->ralloc(state->size)))
    ;

  // and put it into their ipc queue
  auto *xlatd =
      (ipc::block *)translate_address(peer, their_block, sizeof(ipc::block));
  while (!their_queue->enqueue(their_block, xlatd))
    ;
}

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
    auto *msg = (empty_msg_ *)CmiAlloc(sizeof(empty_msg_));
    CmiInitMsgHeader(msg->core, sizeof(empty_msg_));
    CmiSetHandler(msg, CpvAccess(start_plain_handler));
    CmiReduce((char *)msg, sizeof(empty_msg_), null_merge_fn);
  }
}

void handle_exit(void *msg) {
  CmiFree(msg);
  CsdExitScheduler();
}

void test_start(int argc, char **argv) {
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
  CpvRegister(exit_handler, handle_exit);
  CpvRegister(initiate_handler, handle_initiate);
  CpvRegister(recv_plain_handler, recv_plain);
  CpvRegister(start_plain_handler, start_plain);
  CpvRegister(start_xpmem_handler, start_xpmem);
  // Wait for all PEs of the node to complete topology init
  CmiInitCPUAffinity(argv);
  CmiInitCPUTopology(argv);
  CmiNodeAllBarrier();
  // read the command line arguments
  argc = CmiGetArgc(argv);
  auto mine = CmiMyPe();
  auto *state = new state_();
  state->warmup = true;
  state->nIters = (argc >= 2) ? atoi(argv[1]) : 128;
  state->size = (argc >= 3) ? atoi(argv[2]) : 4096;
  if (mine == 0) {
    CmiPrintf("main> pingpong with %d iterations and %luB payload\n",
              state->nIters, state->size);
  }
  // Setup the XPMEM buffers
  populate_buffers_(mine, state);
  // Initialize the state
  CpvInitialize(state_ptr_, state_);
  CpvAccess(state_).reset(state);
  // Enforce the test's topological assumptions
  assert(!CMK_SMP && CmiNumPhysicalNodes() == 1);
  if (CmiNumPes() == 1) {
    start_plain(nullptr);
  } else {
    // Broadcast initialization information to all PEs
    CmiSetHandler(msg, CpvAccess(initiate_handler));
    CmiSyncBroadcastAndFree(sizeof(init_msg_), (char *)msg);
  }
}

inline void test_finish(state_ *state, const char *which, double totalTime) {
  std::string str(which);

  auto mine = CmiMyPe();
  auto peer = (CmiMyPe() + 1) % CmiNumPes();
  auto single = mine == peer;

  if (!state->warmup) {
    auto tIter = single ? (state->nIters / 2) : state->nIters;
    CmiPrintf("main> roundtrip time for %s was %g us\n", str.c_str(),
              (1e6 * totalTime) / (double)tIter);
  }

  if (str == "plain") {
    send_signal(-1, CpvAccess(start_xpmem_handler));
  } else if (state->warmup) {
    state->warmup = false;

    start_plain_(state);
  } else {
    send_signal(-1, CpvAccess(exit_handler));
  }
}

void send_signal(const int &pe, const int &which) {
  auto *msg = (empty_msg_ *)CmiAlloc(sizeof(empty_msg_));
  CmiInitMsgHeader(msg->core, sizeof(empty_msg_));
  CmiSetHandler(msg, which);
  if (pe >= 0) {
    CmiSyncSendAndFree(pe, sizeof(empty_msg_), (char *)msg);
  } else {
    CmiSyncBroadcastAllAndFree(sizeof(empty_msg_), (char *)msg);
  }
}

inline void start_plain_(state_ *state) {
  auto next = (CmiMyPe() + 1) % CmiNumPes();
  auto msg = (char *)CmiAlloc(state->size);
  reset_state_(state);
  CmiSetHandler(msg, CpvAccess(recv_plain_handler));
  CmiSyncSend(next, state->size, msg);
}

inline void populate_buffers_(const int &mine, const state_ *state) {
  auto my_pool = pool_for(mine);
  auto nBuffers = state->nIters / kBufferFactor;
  for (auto i = 0; i < nBuffers; i += 1) {
    auto *ptr = malloc(state->size);
    while (!my_pool->lput(ptr, state->size))
      ;
  }
}

int main(int argc, char **argv) { ConverseInit(argc, argv, test_start, 0, 0); }
