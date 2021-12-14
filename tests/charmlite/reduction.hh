#ifndef __CMK_REDUCTION_HH__
#define __CMK_REDUCTION_HH__

#include "callback.hh"
#include "message.hh"

namespace cmk {
void* converse_combiner_(int* size, void* local, void** remote, int count) {
  auto* sum = static_cast<message*>(local);
  auto comb = combiner_for(sum);
  for (auto i = 0; i < count; i++) {
    auto& lhs = sum;
    auto& rhs = reinterpret_cast<message*&>(remote[i]);
    auto* res = comb(lhs, rhs);
    if (res == lhs) {
      // result is OK
      continue;
    } else if (res == rhs) {
      // result is remote -- so we have
      // to swap lhs so it's freed as remote
      std::swap(lhs, rhs);
    } else {
      // combiner alloc'd a new non-remote
      // message so we have to free
      message::free(lhs);
    }
  }
  *size = (int)sum->total_size_;
  return sum;
}

template <combiner_t Combiner, callback_t Callback>
void reduce(message* msg) {
  msg->dst_kind_ = kCallback;
  msg->dst_.callback_ = callback_helper_<Callback>::id_;
  msg->set_combiner(combiner_helper_<Combiner>::id_);
  CmiReduce(msg, msg->total_size_, converse_combiner_);
}

message* nop(message* msg, message*) { return msg; }
}  // namespace cmk

#endif
