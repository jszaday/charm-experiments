#ifndef __VALUES_HH__
#define __VALUES_HH__

#include "utilities.hh"

namespace tags {
struct no_init {};
}  // namespace tags

struct value {
  virtual const std::type_index* get_type(void) const { return nullptr; }
};

template <typename T>
struct typed_value;

template <typename T>
struct typed_value : public value {
  std::aligned_storage<sizeof(T), alignof(T)> storage_;

  typed_value(tags::no_init) {}

  template <typename... Args>
  typed_value(Args... args) {
    new (this->get()) T(std::forward<Args>(args)...);
  }

  inline T* get(void) { return reinterpret_cast<T*>(&this->storage_); }

  T* operator->(void) { return this->get(); }

  T& operator*(void) { return *(this->get()); }

  virtual const std::type_index* get_type(void) const override {
    static std::type_index tIdx(typeid(T));
    return &tIdx;
  }
};

template <>
struct typed_value<void> : public value {};

template <typename T>
using typed_value_ptr = std::unique_ptr<typed_value<T>>;

// NOTE ( this would be implemented via ser/des )
template <typename T>
typed_value_ptr<T> msg2typed(CkMessage* msg) {
  auto* dataMsg = (CkDataMsg*)msg;
  auto* value = new typed_value<T>(*((T*)dataMsg->data));
  delete dataMsg;
  return typed_value_ptr<T>(value);
}

template <typename T, typename... Ts>
typed_value_ptr<T> make_typed_value(Ts... ts) {
  return typed_value_ptr<T>(new typed_value<T>(std::forward<Ts>(ts)...));
}

#endif
