#include <cstdint>
#include <memory>
#include <vector>

namespace cmk {
using message_deleter_t = void (*)(void *);
struct message_record_ {
  message_deleter_t deleter;
};

using message_table_t = std::vector<message_record_>;
using message_id_t = typename message_table_t::size_type;
message_table_t message_table_;

constexpr std::size_t header_size = 48;

struct message {
  char core_[header_size];
  message_id_t kind_;

  static void free(void *msg) {
    if (msg == nullptr) {
      return;
    } else {
      auto &kind = static_cast<message *>(msg)->kind_;
      if (kind == 0) {
        // ABORT;
      } else {
        message_table_[kind - 1].deleter(msg);
      }
    }
  }
};

template <typename T>
struct message_deleter {
  void operator()(T *t) { delete t; }
};

template <>
struct message_deleter<void> {
  void operator()(void *t) { message::free(t); }
};

template <typename T>
using message_ptr = std::unique_ptr<T, message_deleter<T>>;

template <typename T, typename Message>
using member_fn_t = void (T::*)(message_ptr<Message> &&);

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

#define CMK_INDEX_OF(Class, Method) Cmk_##Class##_##Method##_Index##_

#define CMK_REGISTER(Class, Method)                                      \
  cmk::entry_id_t CMK_INDEX_OF(Class, Method) =                          \
      cmk::register_<Class, cmk::message_of_t<decltype(&Class::Method)>, \
                     &Class::Method>();

#define CMK_CONSTRUCTOR(Class, Argument)       \
  cmk::entry_id_t CMK_INDEX_OF(Class, Class) = \
      cmk::register_constructor_<Class, Argument>();
