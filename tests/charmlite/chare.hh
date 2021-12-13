#ifndef __CMK_CHARE_HH__
#define __CMK_CHARE_HH__

#include <cstdint>
#include <vector>

namespace cmk {

struct chare_record_;

using chare_table_t = std::vector<chare_record_>;
using chare_id_t = typename chare_table_t::size_type;
using chare_index_t = std::size_t;

extern chare_table_t chare_table_;

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
const chare_record_& record_of(void) {
  auto id = chare_id_helper_<T>::id_;
  return chare_table_[id - 1];
}

struct chare_base_ {
  chare_index_t index_;
};

template <typename T, typename Enable = void>
struct index_view {
  static const T& reinterpret(const chare_index_t& idx) {
    return reinterpret_cast<T&>(idx);
  }
};

template <typename Index>
struct chare : public chare_base_ {
  static_assert(sizeof(Index) <= sizeof(chare_index_t),
                "index must fit in constraints!");

  const Index& index(void) {
    return index_view<Index>::reinterpret(this->index_);
  }
};
}  // namespace cmk

#endif
