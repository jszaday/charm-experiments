#ifndef __CMK_DEF_HH__
#define __CMK_DEF_HH__

#include "ep.hh"

namespace cmk {
template <entry_fn_t Fn>
static entry_id_t register_entry_fn_(void) {
  auto id = entry_table_.size() + 1;
  entry_table_.emplace_back(Fn);
  return id;
}

template <entry_fn_t Fn>
entry_id_t entry_id_helper_<Fn>::id_ = register_entry_fn_<Fn>();
}  // namespace cmk

#endif