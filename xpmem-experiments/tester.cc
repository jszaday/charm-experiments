struct sm_;

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" {
#include <xpmem.h>
}

#include "tester.decl.h"

// "borrowed" from VADER
// (https://github.com/open-mpi/ompi/tree/386ba164557bb8115131921041757be94a989646/opal/mca/smsc/xpmem)
#define OPAL_DOWN_ALIGN(x, a, t) ((x) & ~(((t)(a)-1)))
#define OPAL_DOWN_ALIGN_PTR(x, a, t) \
  ((t)OPAL_DOWN_ALIGN((uintptr_t)x, a, uintptr_t))
#define OPAL_ALIGN(x, a, t) (((x) + ((t)(a)-1)) & ~(((t)(a)-1)))
#define OPAL_ALIGN_PTR(x, a, t) ((t)OPAL_ALIGN((uintptr_t)x, a, uintptr_t))
#define OPAL_ALIGN_PAD_AMOUNT(x, s) \
  ((~((uintptr_t)(x)) + 1) & ((uintptr_t)(s)-1))

struct sm_ {
  xpmem_segid_t segid;
  void *ptr;
  std::size_t size;
};

PUPbytes(sm_);

struct xp_mem_segment_ {
  xpmem_segid_t segid;
  uintptr_t addr_max;
  int node;
};

CksvDeclare(xp_mem_segment_, segment_);

static constexpr auto kNumMsgs = 32;

void setup_segment(void) {
  CksvInitialize(xp_mem_segment_, segment_);
  auto *segment = &(CksvAccess(segment_));
  auto segid =
      xpmem_make(0, XPMEM_MAXADDR_SIZE, XPMEM_PERMIT_MODE, (void *)0666);
  CkEnforceMsg(segid != -1, "could not make xpmem segment");
  new (segment) xp_mem_segment_{.segid = segid, .node = CkMyNode()};
}

struct common {
  int *data;

  common(void) { this->data = new int; }

  ~common() { delete this->data; }

  void recv_msg(sm_ &&msg) {
    // TODO ( this should be called only once per peer and cached! )
    auto apid = xpmem_get(msg.segid, XPMEM_RDWR, XPMEM_PERMIT_MODE, NULL);
    CkEnforceMsg(apid != -1, "could not get apid!");

    uint64_t attach_align = 1 << 23;
    auto remote_ptr = (uintptr_t)msg.ptr;
    auto base = OPAL_DOWN_ALIGN(remote_ptr, attach_align, uintptr_t);
    auto bound =
        OPAL_ALIGN(remote_ptr + msg.size - 1, attach_align, uintptr_t) + 1;

    xpmem_addr addr{.apid = apid, .offset = base};

    auto *ctx = xpmem_attach(addr, bound - base, NULL);
    CkEnforceMsg(ctx != (void *)-1, "could not retrieve buffer!");

    auto buf = (uintptr_t)ctx + (ptrdiff_t)(remote_ptr - (uintptr_t)base);
    auto &recvd = *((int *)buf);

    CkPrintf("%d> received message of %lu bytes containing: %d\n", CkMyPe(),
             msg.size, recvd);

    if (recvd < (kNumMsgs - 1)) {
      // pass on the (next) message
      *(this->data) = recvd + 1;
      this->send_msg(this->data, sizeof(int));
    } else {
      CkExit();
    }

    xpmem_detach(ctx);
    xpmem_release(apid);
  }

  void send_msg(void *data, const std::size_t &size) {
    auto *segid = &(CksvAccess(segment_).segid);

    sm_ msg{.segid = *segid, .ptr = data, .size = size};

    this->send_action(std::move(msg));
  }

  virtual void send_action(sm_ &&) = 0;
};

struct main : public CBase_main, public common {
  CProxy_neighbor neighborProxy;

  main(CkArgMsg *) {
    CkEnforceMsg((CkNumPes() == 2) && (CkNumNodes() == 2),
                 "wrong number of pes/nodes");
    CkPrintf("%d> XPMEM version = %x\n", CkMyPe(), xpmem_version());

    this->neighborProxy =
        CProxy_neighbor::ckNew(thisProxy, (CkMyPe() + 1) % CkNumPes());

    *(this->data) = 0x0;

    this->send_msg(this->data, sizeof(int));
  }

  ~main() { delete this->data; }

  virtual void send_action(sm_ &&msg) override { neighborProxy.recv_msg(msg); }
};

struct neighbor : public CBase_neighbor, public common {
  CProxy_main mainProxy;

  neighbor(const CProxy_main &_) : mainProxy(_) {}

  virtual void send_action(sm_ &&msg) override { mainProxy.recv_msg(msg); }
};

#include "tester.def.h"
