#ifndef __CMK_CALLBACK_HH__
#define __CMK_CALLBACK_HH__

#include "message.hh"

namespace cmk {

constexpr int all = -1;

template <combiner_t Fn>
struct combiner_helper_ {
  static combiner_id_t id_;
};

template <callback_t Fn>
struct callback_helper_ {
  static callback_id_t id_;
};

inline combiner_t combiner_for(combiner_id_t id) {
  return id ? combiner_table_[id - 1] : nullptr;
}

inline callback_t callback_for(callback_id_t id) {
  return id ? callback_table_[id - 1] : nullptr;
}

inline combiner_t combiner_for(message* msg) {
  auto* id = msg->combiner();
  return id ? combiner_for(*id) : nullptr;
}

inline callback_t callback_for(message* msg) {
  return (msg->dst_kind_ == kCallback) ? callback_for(msg->dst_.callback_)
                                       : nullptr;
}

struct callback {
  destination_kind_ kind_;
  destination_ dst_;

  void brand(message* msg) const {
    msg->dst_ = dst_;
    msg->dst_kind_ = this->kind_;
  }

  void send(int pe, message* msg) {
    this->brand(msg);
    auto bcast = msg->is_broadcast();
    bcast = (pe == all);
    if (bcast) {
      CmiSyncBroadcastAllAndFree(msg->total_size_, (char*)msg);
    } else {
      CmiSyncSendAndFree(pe, msg->total_size_, (char*)msg);
    }
  }

  template <callback_t Callback>
  static callback construct(void) {
    callback cb{.kind_ = kCallback};
    cb.dst_.callback_ = callback_helper_<Callback>::id_;
    return std::move(cb);
  }
};
}  // namespace cmk

#endif
