#include "ep.hh"

#include "collective.hh"
#include "core.hh"

namespace cmk {
CpvDeclare(int, deliver_handler_);

entry_table_t entry_table_;
chare_table_t chare_table_;
message_table_t message_table_;
collective_kinds_t collective_kinds_;
collective_table_t collective_table_;
collective_buffer_t collective_buffer_;
std::uint32_t local_collective_count_ = 0;

void start_fn_(int, char**) {
  register_deliver_();
  CsdScheduleForever();
}

void deliver(void* raw) {
  auto* msg = static_cast<message*>(raw);
  if (auto* kind = msg->kind()) {
    auto& rec = collective_kinds_[*kind - 1];
    auto* cons = rec(msg->id_);
    auto ins = collective_table_.emplace(msg->id_, cons);
    CmiAssertMsg(ins.second, "insertion did not occur!");
    auto find = collective_buffer_.find(msg->id_);
    if (find == std::end(collective_buffer_)) {
      return;
    } else {
      auto& buffer = find->second;
      while (!buffer.empty()) {
        cons->deliver(buffer.front().release());
        buffer.pop_front();
      }
    }
  } else {
    auto search = collective_table_.find(msg->id_);
    if (search == std::end(collective_table_)) {
      collective_buffer_[msg->id_].emplace_back(msg);
    } else {
      search->second->deliver(msg);
    }
  }
}
}  // namespace cmk