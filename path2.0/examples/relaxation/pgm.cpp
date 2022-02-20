#include "pgm.decl.h"
#include <hypercomm/workgroup.hpp>

class relaxation_task : public hypercomm::task<relaxation_task> {
  bool converged;
  bool hasLeft, hasRight;

  double tolerance;

  int nRecvd, nNeighbors, nWorkers;
  int it, maxIters;

public:
  relaxation_task(PUP::reconstruct) {}

  relaxation_task(hypercomm::task_payload &&payload)
      : converged(false), nRecvd(0), tolerance(-1) {
    auto args =
        std::forward_as_tuple(this->nWorkers, this->maxIters, this->tolerance);
    PUP::fromMem p(payload.data);
    p | args;

    auto idx = this->index();
    this->hasLeft = idx > 0;
    this->hasRight = (idx + 1) < this->nWorkers;
    this->nNeighbors = (int)this->hasLeft + this->hasRight;

    CkPrintf("task %d of %d created, has %d neighbors.\n", idx, this->nWorkers,
             this->nNeighbors);

    this->converge_loop();
  }

  void converge_loop(void) {
    if (!converged && (it++ < maxIters)) {
      this->nRecvd = 0;

      this->send_ghosts();

      if (this->nNeighbors == 0) {
        this->check_and_compute();
      } else {
        this->suspend<&relaxation_task::receive_ghost>();
      }
    } else {
      this->on_converge();
    }
  }

  void send_ghosts(void) {
    auto idx = this->index();

    if (this->hasLeft) {
      // TODO ( pack ghost data )
      this->send(idx - 1);
    }

    if (this->hasRight) {
      // TODO ( pack ghost data )
      this->send(idx + 1);
    }
  }

  void receive_ghost(hypercomm::task_payload &&payload) {
    // TODO ( unpack then copy ghost data )

    if ((++this->nRecvd) >= this->nNeighbors) {
      this->check_and_compute();
    } else {
      this->suspend<&relaxation_task::receive_ghost>();
    }
  }

  void check_and_compute(void) {
    double max_error = this->tolerance * 2;

    CkPrintf("%d> iteration %d\n", this->index(), this->it);

    this->all_reduce(max_error, CkReduction::max_double);
    this->suspend<&relaxation_task::receive_max_error>();
  }

  void receive_max_error(hypercomm::task_payload &&payload) {
    auto &max_error = *((double *)payload.data);
    this->converged = (max_error <= this->tolerance);
    this->converge_loop();
  }

  void on_converge(void) {
    // signal this task can be cleaned up by the RTS
    this->terminate();

    if (this->index() == 0) {
      CkExit();
    }
  }

  void pup(PUP::er &p) {
    p | this->converged;
    p | this->tolerance;
    p | this->hasLeft;
    p | this->hasRight;
    p | this->nRecvd;
    p | this->nNeighbors;
    p | this->nWorkers;
    p | this->it;
    p | this->maxIters;
  }
};

class pgm : public CBase_pgm {
  hypercomm::workgroup_proxy group;

public:
  pgm(CkArgMsg *msg) {
    auto factor = (msg->argc >= 2) ? atoi(msg->argv[1]) : 4;
    auto n = factor * CkNumPes();
    auto maxIters = 100;
    auto tolerance = 0.005;
    group = hypercomm::workgroup_proxy::ckNew(n);
    hypercomm::launch<relaxation_task>(group, n, maxIters, tolerance);
  }
};

#include "pgm.def.h"
#include <hypercomm/workgroup.def.h>
