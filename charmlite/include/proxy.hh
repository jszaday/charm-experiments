#ifndef __CMK_PROXY_HH__
#define __CMK_PROXY_HH__

#include "ep.hh"

namespace cmk {

template <typename T>
class element_proxy {
  collective_index_t id_;
  chare_index_t idx_;

 public:
  element_proxy(element_proxy<T>&&) = default;
  element_proxy(const element_proxy<T>&) = default;
  element_proxy(const collective_index_t& id, const chare_index_t& idx)
      : id_(id), idx_(idx) {}

  template <typename Message>
  void insert(Message* msg) {
    msg->dst_kind_ = kEndpoint;
    msg->is_broadcast() = false;
    msg->dst_.endpoint_.ep_ = constructor<T, Message*>();
    msg->dst_.endpoint_.id_ = this->id_;
    msg->dst_.endpoint_.idx_ = this->idx_;
    deliver(msg);
  }

  void insert(void) {
    auto* msg = new message;
    msg->dst_kind_ = kEndpoint;
    msg->is_broadcast() = false;
    msg->dst_.endpoint_.ep_ = constructor<T, void>();
    msg->dst_.endpoint_.id_ = this->id_;
    msg->dst_.endpoint_.idx_ = this->idx_;
    deliver(msg);
  }

  template <typename Message, member_fn_t<T, Message> Fn>
  void send(Message* msg) {
    msg->dst_kind_ = kEndpoint;
    msg->is_broadcast() = false;
    msg->dst_.endpoint_.ep_ = entry<member_fn_t<T, Message>, Fn>();
    msg->dst_.endpoint_.id_ = this->id_;
    msg->dst_.endpoint_.idx_ = this->idx_;
    deliver(msg);
  }
};

template <typename T, typename Mapper = default_mapper>
class collective_proxy {
  collective_index_t id_;

 public:
  using index_type = index_for_t<T>;

  collective_proxy(const collective_index_t& id) : id_(id) {}

  // TODO ( isolate this to group proxies )
  template <typename Index = index_type>
  typename std::enable_if<std::is_same<Index, int>::value, T*>::type
  local_branch(void) {
    auto* loc = lookup(this->id_);
    return loc ? loc->template lookup<T>(CmiMyPe()) : nullptr;
  }

  static const collective_kind_t& kind(void) {
    return collective_helper_<collective<T, Mapper>>::kind_;
  }

  element_proxy<T> operator[](const index_type& idx) {
    auto& view = index_view<index_type>::reinterpret(idx);
    return element_proxy<T>(this->id_, view);
  }

  // TODO ( this will ONLY work for group proxies )
  template <typename Message, member_fn_t<T, Message> Fn>
  void broadcast(Message* msg) {
    msg->dst_kind_ = kEndpoint;
    msg->is_broadcast() = true;
    msg->dst_.endpoint_.ep_ = entry<member_fn_t<T, Message>, Fn>();
    msg->dst_.endpoint_.id_ = this->id_;
    CmiSyncBroadcastAllAndFree(msg->total_size_, (char*)msg);
  }

  static collective_proxy<T, Mapper> construct(void) {
    collective_index_t id{(std::uint32_t)CmiMyPe(), local_collective_count_++};
    auto* msg = new message();
    msg->dst_kind_ = kEndpoint;
    msg->is_broadcast() = true;
    msg->dst_.endpoint_.id_ = id;
    msg->set_collective_kind(kind());
    CmiSyncBroadcastAllAndFree(msg->total_size_, (char*)msg);
    return collective_proxy<T, Mapper>(id);
  }

  void done_inserting(void) {}
};

template <typename T, typename Index>
struct chare : public chare_base_ {
  const Index& index(void) const {
    return index_view<Index>::reinterpret(this->index_);
  }

  const collective_index_t& collective(void) const { return this->parent_; }

  const cmk::element_proxy<T> element_proxy(void) {
    return cmk::element_proxy<T>(this->parent_, this->index_);
  }
};

}  // namespace cmk

#endif
