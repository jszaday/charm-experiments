#ifndef __CMK_CHARE_HH__
#define __CMK_CHARE_HH__

#include "common.hh"

namespace cmk {

template <typename T>
struct chare_kind_helper_ {
  static chare_kind_t kind_;
};

struct chare_record_ {
  const char* name_;
  std::size_t size_;

  chare_record_(const char* name, std::size_t size)
      : name_(name), size_(size) {}

  void* allocate(void) const { return ::operator new (this->size_); }

  void deallocate(void* obj) const { ::operator delete (obj); }
};

template <typename T>
const chare_record_& record_for(void) {
  auto id = chare_kind_helper_<T>::kind_;
  return chare_table_[id - 1];
}

template <typename T, typename Enable = void>
struct property_setter_ {
  void operator()(T*, const collective_index_t&, const chare_index_t&) {}
};

template <typename T, typename Index>
struct chare;

template <typename T, typename Mapper>
class collective;

struct chare_base_ {
 private:
  collective_index_t parent_;
  chare_index_t index_;
  bcast_id_t last_bcast_ = 0;

 public:
  template <typename T, typename Index>
  friend class chare;

  template <typename T, typename Mapper>
  friend class collective;

  template <typename T, typename Enable>
  friend class property_setter_;
};

template <typename T, typename Enable = void>
struct index_view {
  static_assert(sizeof(T) <= sizeof(chare_index_t),
                "index must fit in constraints!");

  static const T& decode(const chare_index_t& idx) {
    return reinterpret_cast<const T&>(idx);
  }

  static chare_index_t encode(const T& idx) {
    return static_cast<chare_index_t>(idx);
  }
};

template <typename T>
struct property_setter_<
    T, typename std::enable_if<std::is_base_of<chare_base_, T>::value>::type> {
  void operator()(T* t, const collective_index_t& id,
                  const chare_index_t& idx) {
    t->parent_ = id;
    t->index_ = idx;
  }
};

template <typename T, typename Index>
static Index index_for_impl_(const chare<T, Index>*);

template <typename T>
using index_for_t = decltype(index_for_impl_(std::declval<T*>()));

}  // namespace cmk

#endif
