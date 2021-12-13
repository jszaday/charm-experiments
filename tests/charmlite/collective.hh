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
    auto pe = mapper_.pe_for(idx);
    // TODO ( temporary constraint, elements only created on home pe )
    if (ep->is_constructor_ && (pe == CmiMyPe())) {
      auto* ch = static_cast<T*>((record_for<T>()).allocate());
      // set properties of the newly created chare
      property_setter_<T>()(ch, idx);
      auto ins = chares_.emplace(idx, ch);
      (ep->fn_)(ch, msg);
      flush_buffers(idx);
    } else {
      auto find = chares_.find(idx);
      if (find == std::end(this->chares_)) {
        if (pe == CmiMyPe()) {
          return false;
        } else {
          CmiSyncSendAndFree(pe, msg->total_size_, (char*)msg);
        }
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
}  // namespace cmk

#endif
