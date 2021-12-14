#include "core.hh"

#include "callback.hh"
#include "collective.hh"
#include "ep.hh"

namespace cmk {
CpvDeclare(int, deliver_handler_);

entry_table_t entry_table_;
chare_table_t chare_table_;
message_table_t message_table_;
callback_table_t callback_table_;
combiner_table_t combiner_table_;
collective_kinds_t collective_kinds_;
collective_table_t collective_table_;
collective_buffer_t collective_buffer_;
std::uint32_t local_collective_count_ = 0;

void start_fn_(int, char**) {
  register_deliver_();
  CsdScheduleForever();
}

// circulate an exit message between pes
// TODO ( don't req a flag! use src pe or something )
void exit(message* msg) {
  auto xting = msg->is_exiting();
  if (xting) {
    message::free(msg);
  } else {
    xting = true;
    CmiSyncBroadcastAndFree(msg->total_size_, (char*)msg);
  }
  CsdExitScheduler();
}

inline void deliver_to_endpoint_(message* msg) {
  auto& col = msg->dst_.endpoint_.id_;
  if (auto* kind = msg->collective_kind()) {
    auto& rec = collective_kinds_[*kind - 1];
    auto* obj = rec(col);
    auto ins = collective_table_.emplace(col, obj);
    CmiAssertMsg(ins.second, "insertion did not occur!");
    auto find = collective_buffer_.find(col);
    if (find == std::end(collective_buffer_)) {
      return;
    } else {
      auto& buffer = find->second;
      while (!buffer.empty()) {
        obj->deliver(buffer.front().release());
        buffer.pop_front();
      }
    }
  } else {
    auto search = collective_table_.find(col);
    if (search == std::end(collective_table_)) {
      collective_buffer_[col].emplace_back(msg);
    } else {
      search->second->deliver(msg);
    }
  }
}

inline void deliver_to_callback_(message* msg) { (callback_for(msg))(msg); }

void deliver(void* raw) {
  auto* msg = static_cast<message*>(raw);
  switch (msg->dst_kind_) {
    case kEndpoint:
      deliver_to_endpoint_(msg);
      break;
    case kCallback:
      deliver_to_callback_(msg);
      break;
    default:
      CmiAbort("invalid message destination");
  }
}
}  // namespace cmk