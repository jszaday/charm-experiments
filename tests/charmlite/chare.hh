#ifndef __CMK_CHARE_HH__
#define __CMK_CHARE_HH__

#include "common.hh"

namespace cmk {

template <typename T>
struct chare_id_helper_ {
  static chare_id_t id_;
};

struct chare_record_ {
  const char* name_;
  std::size_t size_;

  chare_record_(const char* name, std::size_t size)
      : name_(name), size_(size) {}

  void* allocate(void) const { return ::operator new(this->size_); }

  void deallocate(void* obj) const { ::operator delete(obj); }
};

template <typename T>
const chare_record_& record_for(void) {
  auto id = chare_id_helper_<T>::id_;
  return chare_table_[id - 1];
}

struct chare_base_ {
  chare_index_t index_;
};

template <typename T, typename Enable = void>
struct index_view {
  static_assert(sizeof(T) <= sizeof(chare_index_t),
                "index must fit in constraints!");

  static const T& reinterpret(const chare_index_t& idx) {
    return reinterpret_cast<const T&>(idx);
  }
};

template <typename Index>
struct chare : public chare_base_ {
  const Index& index(void) {
    return index_view<Index>::reinterpret(this->index_);
  }
};

template <typename Index>
static Index index_for_impl_(const chare<Index>*);

template <typename T>
using index_for_t = decltype(index_for_impl_(std::declval<T*>()));

}  // namespace cmk

#endif
