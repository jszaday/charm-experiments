#ifndef __CMK_MSG_HH__
#define __CMK_MSG_HH__

#include "chare.hh"

namespace cmk {
using message_deleter_t = void (*)(void *);
struct message_record_ {
  message_deleter_t deleter;
};

using message_table_t = std::vector<message_record_>;
using message_id_t = typename message_table_t::size_type;
extern message_table_t message_table_;

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
}  // namespace cmk

#endif
