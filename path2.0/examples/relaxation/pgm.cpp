#include "pgm.decl.h"
#include <hypercomm/workgroup.hpp>

class relaxation_task : public hypercomm::task<relaxation_task> {
  bool converged;
  bool hasLeft, hasRight;
  int nRecvd, nNeighbors, nWorkers;
  int it, maxIters;

public:
  relaxation_task(hypercomm::task_payload &&payload) : converged(false), nRecvd(0) {
    PUP::fromMem p(payload.data);
    auto args = std::forward_as_tuple(this->nWorkers, this->maxIters);
    p | args;

    auto idx = this->index();
    this->hasLeft = idx > 0;
    this->hasRight = (idx + 1) < this->nWorkers;
    this->nNeighbors = (int)this->hasLeft + this->hasRight;

    CkPrintf("task %d of %d created, has %d neighbors.\n", idx, this->nWorkers, this->nNeighbors);

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
      this->wrap_up();
    }
  }

  void send_ghosts(void) {
    if (this->hasLeft) {

    }

    if (this->hasRight) {

    }
  }

  void receive_ghost(hypercomm::task_payload &&payload) {
    // copy ghost

    if ((++this->nRecvd) >= this->nNeighbors) {
      this->check_and_compute();
    } else {
      this->suspend<&relaxation_task::receive_ghost>();
    }
  }

  void check_and_compute(void) {
    CkPrintf("%d> iteration %d\n", this->index(), this->it);
    
    this->converge_loop();
  }

  void wrap_up(void) {
    CkExit();
  }
};

class pgm : public CBase_pgm {
  hypercomm::workgroup_proxy group;

public:
  pgm(CkArgMsg *msg) {
    auto factor = (msg->argc >= 2) ? atoi(msg->argv[1]) : 4;
    auto n = factor * CkNumPes();
    auto maxIters = 100;
    group = hypercomm::workgroup_proxy::ckNew(n);
    hypercomm::launch<relaxation_task>(group, n, maxIters);
  }
};

#include "pgm.def.h"
#include <hypercomm/workgroup.def.h>
