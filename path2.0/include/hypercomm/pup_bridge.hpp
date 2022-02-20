#ifndef HYPERCOMM_PUP_BRIDGE_HPP
#define HYPERCOMM_PUP_BRIDGE_HPP

#include <hypercomm/common.hpp>
#include <pup.h>
#include <pup_stl.h>

PUPbytes(hypercomm::task_id);

namespace hypercomm {
using puper_t = PUP::er;

inline void pup_message(puper_t &p, void *&msg) { CkPupMessage(p, &msg); }

inline bool pup_is_unpacking(puper_t &p) { return p.isUnpacking(); }
} // namespace hypercomm

namespace PUP {
template <> struct ptr_helper<hypercomm::task_base_, false>;
};

#endif
