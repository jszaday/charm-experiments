#ifndef __CMK_MSG_HH__
#define __CMK_MSG_HH__

#include <bitset>

#include "common.hh"

namespace cmk {
using message_deleter_t = void (*)(void *);

struct message_record_ {
  message_deleter_t deleter;
};

using message_table_t = std::vector<message_record_>;
using message_id_t = typename message_table_t::size_type;
extern message_table_t message_table_;

constexpr std::size_t header_size = CmiMsgHeaderSizeBytes;

struct message {
  char core_[header_size];
  std::bitset<8> flags_;
  message_id_t kind_;
  collective_index_t id_;
  chare_index_t idx_;
  entry_id_t ep_;

  static constexpr auto has_collective_kind = 0;

  collective_kind_t *kind(void) {
    if (this->flags_[has_collective_kind]) {
      return reinterpret_cast<collective_kind_t *>(&(this->idx_));
    } else {
      return nullptr;
    }
  }

  void set_collective_kind(const collective_kind_t &kind) {
    this->flags_[has_collective_kind] = true;
    *(this->kind()) = kind;
  }

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
}  // namespace cmk

#endif
