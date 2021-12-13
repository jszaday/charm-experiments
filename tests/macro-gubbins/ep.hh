#ifndef __CMK_EP_HH__
#define __CMK_EP_HH__

#include "traits.hh"

namespace cmk {
using entry_fn_t = void (*)(void *, void *);
using entry_table_t = std::vector<entry_fn_t>;
using entry_id_t = typename entry_table_t::size_type;

entry_id_t nil_entry_ = 0;
std::vector<entry_fn_t> entry_table_;

void invoke(void *self, entry_id_t id, void *msg) {
  if (id == 0) {
    // ABORT;
  } else {
    (entry_table_[id - 1])(self, msg);
  }
}

template <typename T, typename Message, member_fn_t<T, Message> Fn>
typename std::enable_if<is_message_<Message>()>::type call_(void *self,
                                                            void *msg) {
  message_ptr<Message> owned(static_cast<Message *>(msg));
  (static_cast<T *>(self)->*Fn)(std::move(owned));
}

template <typename T, typename Message, member_fn_t<T, Message> Fn>
entry_id_t register_(void) {
  auto id = entry_table_.size() + 1;
  entry_table_.emplace_back(&(call_<T, Message, Fn>));
  return id;
}

template <typename A, typename B>
struct constructor_caller_;

template <typename A>
struct constructor_caller_<A, void> {
  void operator()(void *self, void *msg) {
    new (static_cast<A *>(self)) A;
    message::free(msg);
  }
};

template <typename A, typename Message>
struct constructor_caller_<A, cmk::message_ptr<Message> &&> {
  typename std::enable_if<is_message_<Message>()>::type operator()(void *self,
                                                                   void *msg) {
    message_ptr<Message> owned(static_cast<Message *>(msg));
    new (static_cast<A *>(self)) A(std::move(owned));
  }
};

template <typename A, typename B>
void call_constructor_(void *self, void *msg) {
  constructor_caller_<A, B>()(self, msg);
}

template <typename A, typename B>
entry_id_t register_constructor_(void) {
  auto id = entry_table_.size() + 1;
  entry_table_.emplace_back(&(call_constructor_<A, B>));
  return id;
}
}  // namespace cmk

#endif