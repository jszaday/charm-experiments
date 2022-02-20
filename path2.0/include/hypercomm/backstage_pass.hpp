#ifndef __HYPERCOMM_UTILITIES_BACKSTAGE_PASS_HPP__
#define __HYPERCOMM_UTILITIES_BACKSTAGE_PASS_HPP__

#include <charm++.h>

namespace hypercomm {
namespace detail {
template <typename type, type value, typename tag> class access_bypass {
  friend type get(tag) { return value; }
};

struct backstage_pass {};

std::vector<CkMigratable *> CkArray::*get(backstage_pass);

// Explicitly instantiating the class generates the fn declared above.
template class access_bypass<std::vector<CkMigratable *> CkArray::*,
                             &CkArray::localElemVec, backstage_pass>;

} // namespace detail
} // namespace hypercomm

#endif
