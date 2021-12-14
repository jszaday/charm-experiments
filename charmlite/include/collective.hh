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
  virtual void* lookup(const chare_index_t&) = 0;
  virtual void deliver(message* msg) = 0;

  template <typename T>
  inline T* lookup(const chare_index_t& idx) {
    return static_cast<T*>(this->lookup(idx));
  }
};

template <typename T, typename Mapper = default_mapper>
struct collective : public collective_base_ {
  Mapper mapper_;

  std::unordered_map<chare_index_t, message_buffer_t> buffers_;
  std::unordered_map<chare_index_t, std::unique_ptr<T>> chares_;

  collective(const collective_index_t& id) : collective_base_(id) {}

  virtual void* lookup(const chare_index_t& idx) override {
    auto find = this->chares_.find(idx);
    if (find == std::end(this->chares_)) {
      return nullptr;
    } else {
      return (find->second).get();
    }
  }

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
          // if delivery failed, stop attempting
          // to deliver messages
          break;
        }
      }
    }
  }

  bool try_deliver(message* msg) {
    auto& ep = msg->dst_.endpoint_;
    auto* rec = record_for(ep.ep_);
    auto& idx = ep.idx_;
    auto pe = mapper_.pe_for(idx);
    // TODO ( temporary constraint, elements only created on home pe )
    if (rec->is_constructor_ && (pe == CmiMyPe())) {
      auto* ch = static_cast<T*>((record_for<T>()).allocate());
      // set properties of the newly created chare
      property_setter_<T>()(ch, this->id_, idx);
      // place the chare within our element list
      auto ins = chares_.emplace(idx, ch);
      // call constructor on chare
      (rec->fn_)(ch, msg);
      // flush any messages we have for it
      flush_buffers(idx);
    } else {
      auto find = chares_.find(idx);
      // if the element isn't found locally
      if (find == std::end(this->chares_)) {
        // and it's our chare...
        if (pe == CmiMyPe()) {
          // it hasn't been created yet, so buffer
          return false;
        } else {
          // otherwise route to the home pe
          // XXX ( update bcast? prolly not. )
          CmiSyncSendAndFree(pe, msg->total_size_, (char*)msg);
        }
      } else {
        // otherwise, invoke the EP on the chare
        (rec->fn_)((find->second).get(), msg);
      }
    }

    return true;
  }

  virtual void deliver(message* msg) override {
    // if the delivery attempt fails --
    if (!try_deliver(msg)) {
      // buffer the message
      this->buffers_[msg->dst_.endpoint_.idx_].emplace_back(msg);
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
