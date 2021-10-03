
#include <cassert>
#include <cstdint>
#include <map>

extern "C" {
#include <xpmem.h>
}

extern int CmiMyNode(void);

xpmem_segid_t make_segment(void) {
  return xpmem_make(0, XPMEM_MAXADDR_SIZE, XPMEM_PERMIT_MODE, (void *)0666);
}

using xpmem_segment_map_t = std::map<int, xpmem_segid_t>;

std::map<int, xpmem_segid_t> segments_;
std::map<xpmem_segid_t, xpmem_apid_t> instances_;

void put_segment(const int &pe, const xpmem_segid_t &segid) {
  auto ins = segments_.emplace(pe, segid);
  assert(ins.second);
}

xpmem_apid_t get_instance(const int &pe) {
  auto outer = segments_.find(pe);
  if (outer == std::end(segments_)) {
    return -1;
  } else {
    auto &segid = outer->second;
    auto inner = instances_.find(segid);
    if (inner == std::end(instances_)) {
      auto ins = instances_.emplace(
          segid, xpmem_get(segid, XPMEM_RDWR, XPMEM_PERMIT_MODE, NULL));
      inner = ins.first;
    }
    return inner->second;
  }
}

// "borrowed" from VADER
// (https://github.com/open-mpi/ompi/tree/386ba164557bb8115131921041757be94a989646/opal/mca/smsc/xpmem)
#define OPAL_DOWN_ALIGN(x, a, t) ((x) & ~(((t)(a)-1)))
#define OPAL_DOWN_ALIGN_PTR(x, a, t)                                           \
  ((t)OPAL_DOWN_ALIGN((uintptr_t)x, a, uintptr_t))
#define OPAL_ALIGN(x, a, t) (((x) + ((t)(a)-1)) & ~(((t)(a)-1)))
#define OPAL_ALIGN_PTR(x, a, t) ((t)OPAL_ALIGN((uintptr_t)x, a, uintptr_t))
#define OPAL_ALIGN_PAD_AMOUNT(x, s)                                            \
  ((~((uintptr_t)(x)) + 1) & ((uintptr_t)(s)-1))

void *translate_address(const int &pe, void *remote_ptr,
                        const std::size_t &size) {
  if (pe == CmiMyNode()) {
    return remote_ptr;
  } else {
    auto apid = get_instance(pe);
    assert(apid >= 0);

    uintptr_t attach_align = 1 << 23;
    auto base = OPAL_DOWN_ALIGN_PTR(remote_ptr, attach_align, uintptr_t);
    auto bound =
        OPAL_ALIGN_PTR(remote_ptr + size - 1, attach_align, uintptr_t) + 1;

    using offset_type = decltype(xpmem_addr::offset);
    xpmem_addr addr{.apid = apid, .offset = (offset_type)base};
    auto *ctx = xpmem_attach(addr, bound - base, NULL);
    assert(ctx != (void *)-1);

    return (void *)((uintptr_t)ctx + (ptrdiff_t)((uintptr_t)remote_ptr - (uintptr_t)base));
  }
}
