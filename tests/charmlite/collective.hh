#ifndef __CMK_COLLECTIVE_HH__
#define __CMK_COLLECTIVE_HH__

#include "chare.hh"
#include "ep.hh"
#include "message.hh"

namespace cmk {

struct default_mapper {
  int pe_for(const chare_index_t& idx) const { return (idx % CmiNumPes()); }
};

struct collective_base_ {
  collective_index_t id_;
  collective_base_(const collective_index_t& id) : id_(id) {}
  virtual ~collective_base_() = default;
  virtual void deliver(message* msg) = 0;
};

template <typename T, typename Mapper = default_mapper>
struct collective : public collective_base_ {
  Mapper mapper_;

  std::unordered_map<chare_index_t, message_buffer_t> buffers_;
  std::unordered_map<chare_index_t, std::unique_ptr<T>> chares_;

  collective(const collective_index_t& id) : collective_base_(id) {}

  void flush_buffers(const chare_index_t& idx) {
    auto find = this->buffers_.find(idx);
    if (find == std::end(this->buffers_)) {
      return;
    } else {
      auto& buffer = find->second;
      while (!buffer.empty()) {
        auto& msg = buffer.front();
        if (this->try_deliver(msg.get())) {
          // release since we consumed the message
          msg.release();
          // the pop it from the queue
          buffer.pop_front();
        } else {
          break;
        }
      }
    }
  }

  bool try_deliver(message* msg) {
    auto* ep = record_for(msg->ep_);
    auto& idx = msg->idx_;

    if (ep->is_constructor_) {
      auto* ch = (record_for<T>()).allocate();
      auto ins = chares_.emplace(idx, static_cast<T*>(ch));
      CmiAssertMsg(ins.second, "insertion did not occur!");
      (ep->fn_)(ch, msg);
      flush_buffers(idx);
    } else {
      auto find = chares_.find(idx);
      if (find == std::end(this->chares_)) {
        return false;
      } else {
        (ep->fn_)((find->second).get(), msg);
      }
    }

    return true;
  }

  virtual void deliver(message* msg) override {
    if (!try_deliver(msg)) {
      this->buffers_[msg->idx_].emplace_back(msg);
    }
  }
};

template <typename T>
struct collective_helper_;

template <typename T, typename Mapper>
struct collective_helper_<collective<T, Mapper>> {
  static collective_kind_t kind_;
};

inline void deliver(void* raw) {
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

#endif
