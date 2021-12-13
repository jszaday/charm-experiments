#ifndef __CMK_DEF_HH__
#define __CMK_DEF_HH__

#include "ep.hh"

namespace cmk {
template <entry_fn_t Fn, bool Constructor>
static entry_id_t register_entry_fn_(void) {
  auto id = entry_table_.size() + 1;
  entry_table_.emplace_back(Fn, Constructor);
  return id;
}

template <entry_fn_t Fn, bool Constructor>
entry_id_t entry_fn_helper_<Fn, Constructor>::id_ =
    register_entry_fn_<Fn, Constructor>();

template <typename T>
static chare_id_t register_chare_(void) {
  auto id = chare_table_.size() + 1;
  chare_table_.emplace_back(typeid(T).name(), sizeof(T));
  return id;
}

template <typename T>
chare_id_t chare_id_helper_<T>::id_ = register_chare_<T>();

template <typename T, typename Mapper>
static collective_base_* construct_collective_(const collective_index_t& id) {
  return new collective<T, Mapper>(id);
}

template <typename T, typename Mapper>
static collective_kind_t register_collective_(void) {
  auto id = collective_kinds_.size() + 1;
  collective_kinds_.emplace_back(&construct_collective_<T, Mapper>);
  return id;
}

template <typename T, typename Mapper>
collective_kind_t collective_helper_<collective<T, Mapper>>::kind_ =
    register_collective_<T, Mapper>();
}  // namespace cmk

#endif
