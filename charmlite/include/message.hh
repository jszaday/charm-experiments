#ifndef __CMK_MSG_HH__
#define __CMK_MSG_HH__

#include <bitset>

#include "common.hh"

namespace cmk {
using message_deleter_t = void (*)(void *);

struct message_record_ {
  message_deleter_t deleter_;

  message_record_(const message_deleter_t &deleter) : deleter_(deleter) {}
};

using message_table_t = std::vector<message_record_>;
using message_kind_t = typename message_table_t::size_type;
extern message_table_t message_table_;

constexpr std::size_t header_size = CmiMsgHeaderSizeBytes;

template <typename T>
struct message_helper_ {
  static message_kind_t kind_;
};

struct destination_ {
  struct s_endpoint_ {
    collective_index_t id_;
    chare_index_t idx_;
    entry_id_t ep_;
  } endpoint_;

  callback_id_t callback_;
};

enum destination_kind_ : std::uint8_t { kCallback, kEndpoint };

struct message {
  char core_[header_size];
  std::bitset<8> flags_;
  // TODO ( think of better names? )
  message_kind_t kind_;
  destination_kind_ dst_kind_;
  destination_ dst_;
  std::size_t total_size_;

 private:
  static constexpr auto has_combiner = 0;
  static constexpr auto has_collective_kind = has_combiner + 1;
  static constexpr auto is_exiting_ = has_collective_kind + 1;

  void *offset_(void) { return &(this->dst_.endpoint_.idx_); }

 public:
  message(void) : kind_(0), total_size_(header_size) {
    CmiSetHandler(this, CpvAccess(deliver_handler_));
  }

  message(message_kind_t kind, std::size_t total_size)
      : kind_(kind), total_size_(total_size) {
    // FIXME ( DRY failure )
    CmiSetHandler(this, CpvAccess(deliver_handler_));
  }

  collective_kind_t *collective_kind(void) {
    if (this->flags_[has_collective_kind]) {
      return reinterpret_cast<collective_kind_t *>(this->offset_());
    } else {
      return nullptr;
    }
  }

  void set_collective_kind(const collective_kind_t &kind) {
    this->flags_[has_collective_kind] = true;
    *(this->collective_kind()) = kind;
  }

  combiner_id_t *combiner(void) {
    if (this->flags_[has_combiner]) {
      return reinterpret_cast<combiner_id_t *>(this->offset_());
    } else {
      return nullptr;
    }
  }

  void set_combiner(const combiner_id_t &id) {
    this->flags_[has_combiner] = true;
    *(this->combiner()) = id;
  }

  static void free(void *msg) {
    if (msg == nullptr) {
      return;
    } else {
      auto &kind = static_cast<message *>(msg)->kind_;
      if (kind == 0) {
        message::operator delete (msg);
      } else {
        message_table_[kind - 1].deleter_(msg);
      }
    }
  }

  void *operator new(std::size_t sz) { return CmiAlloc(sz); }

  void operator delete(void *blk) { CmiFree(blk); }

  std::bitset<8>::reference is_exiting(void) {
    return this->flags_[is_exiting_];
  }
};

template <typename T>
struct plain_message : public message {
  plain_message(void) : message(message_helper_<T>::kind_, sizeof(T)) {}
};
}  // namespace cmk

#endif
