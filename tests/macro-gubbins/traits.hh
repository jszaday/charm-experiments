#ifndef __CMK_TRAITS_HH__
#define __CMK_TRAITS_HH__

#include "message.hh"

namespace cmk {
template <typename T, typename Message>
using member_fn_t = void (T::*)(message_ptr<Message> &&);

template <typename T>
constexpr bool is_message_(void) {
  return std::is_base_of<message, T>::value;
}

template <>
constexpr bool is_message_<void>(void) {
  return true;
}

template <typename T>
struct message_of;

template <typename T, typename Message>
struct message_of<member_fn_t<T, Message>> {
  using type = Message;
};

template <typename T>
using message_of_t = typename message_of<T>::type;

template <typename T>
struct viable_argument {
  using type = message_ptr<T> &&;
};

template <>
struct viable_argument<void> {
  using type = void;
};

template <typename T>
using viable_argument_t = typename viable_argument<T>::type;

}  // namespace cmk

#endif
