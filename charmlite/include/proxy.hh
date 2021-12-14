#ifndef __CMK_PROXY_HH__
#define __CMK_PROXY_HH__

#include "ep.hh"

namespace cmk {

template <typename T, typename Enable = void>
struct message_extractor;

template <>
struct message_extractor<void> {
  static message* get(void) { return new message; }
};

template <typename Message>
struct message_extractor<
    Message*,
    typename std::enable_if<std::is_base_of<message, Message>::value>::type> {
  static message* get(Message* msg) { return msg; }
};

template <typename... Args>
struct pack_helper;

template <typename T>
struct pack_helper<T> {
  using type = T;
};

template <>
struct pack_helper<> {
  using type = void;
};

template <typename... Args>
using pack_helper_t = typename pack_helper<Args...>::type;

template <typename T>
class element_proxy {
  collective_index_t id_;
  chare_index_t idx_;

 public:
  element_proxy(element_proxy<T>&&) = default;
  element_proxy(const element_proxy<T>&) = default;
  element_proxy(const collective_index_t& id, const chare_index_t& idx)
      : id_(id), idx_(idx) {}

  template <typename... Args>
  void insert(Args... args) {
    using arg_type = pack_helper_t<Args...>;
    auto* msg = message_extractor<arg_type>::get(args...);
    msg->dst_kind_ = kEndpoint;
    msg->is_broadcast() = false;
    msg->dst_.endpoint_.ep_ = constructor<T, arg_type>();
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

struct group_mapper : public default_mapper {};

template <typename T, typename Mapper = default_mapper>
class collective_proxy_base_ {
 protected:
  collective_index_t id_;

 public:
  using index_type = index_for_t<T>;

  collective_proxy_base_(const collective_index_t& id) : id_(id) {}

  static const collective_kind_t& kind(void) {
    return collective_helper_<collective<T, Mapper>>::kind_;
  }

  element_proxy<T> operator[](const index_type& idx) {
    auto& view = index_view<index_type>::decode(idx);
    return element_proxy<T>(this->id_, view);
  }
};

template <typename T, typename Mapper = default_mapper>
class collective_proxy : public collective_proxy_base_<T, Mapper> {
  using base_type = collective_proxy_base_<T, Mapper>;

 public:
  using index_type = typename base_type::index_type;

  collective_proxy(const collective_index_t& id) : base_type(id) {}

  static collective_proxy<T, Mapper> construct(void) {
    collective_index_t id{(std::uint32_t)CmiMyPe(), local_collective_count_++};
    auto* msg = new message();
    msg->dst_kind_ = kEndpoint;
    msg->is_broadcast() = true;
    msg->dst_.endpoint_.id_ = id;
    msg->set_collective_kind(base_type::kind());
    CmiSyncBroadcastAllAndFree(msg->total_size_, (char*)msg);
    return collective_proxy<T, Mapper>(id);
  }

  void done_inserting(void) {}
};

template <typename T>
class group_proxy : public collective_proxy_base_<T, group_mapper> {
  using base_type = collective_proxy_base_<T, group_mapper>;

 public:
  using index_type = typename base_type::index_type;

  static_assert(std::is_same<index_type, int>::value,
                "groups must use integer indices");

  group_proxy(const collective_index_t& id) : base_type(id) {}

  T* local_branch(void) {
    auto* loc = lookup(this->id_);
    return loc ? loc->template lookup<T>(CmiMyPe()) : nullptr;
  }

  // TODO ( this will ONLY work for group proxies )
  template <typename Message, member_fn_t<T, Message> Fn>
  void broadcast(Message* msg) {
    msg->dst_kind_ = kEndpoint;
    msg->is_broadcast() = true;
    msg->dst_.endpoint_.ep_ = entry<member_fn_t<T, Message>, Fn>();
    msg->dst_.endpoint_.id_ = this->id_;
    broadcast_helper_(msg);
  }

  template <typename... Args>
  static group_proxy<T> construct(Args... args) {
    collective_index_t id{(std::uint32_t)CmiMyPe(), local_collective_count_++};
    {
      auto* msg = new message();
      msg->dst_kind_ = kEndpoint;
      msg->is_broadcast() = true;
      msg->dst_.endpoint_.id_ = id;
      msg->set_collective_kind(base_type::kind());
      CmiSyncBroadcastAllAndFree(msg->total_size_, (char*)msg);
    }
    {
      using arg_type = pack_helper_t<Args...>;
      auto* msg = message_extractor<arg_type>::get(args...);
      msg->dst_kind_ = kEndpoint;
      msg->is_broadcast() = true;
      msg->dst_.endpoint_.ep_ = constructor<T, arg_type>();
      msg->dst_.endpoint_.id_ = id;
      broadcast_helper_(msg);
    }
    return group_proxy<T>(id);
  }

 private:
  // TODO ( use spanning tree )
  static void broadcast_helper_(message* msg) {
    auto n = CmiNumPes();
    auto& ep = msg->dst_.endpoint_;
    for (auto i = 1; i < n; i++) {
      ep.idx_ = index_view<index_type>::encode(i);
      CmiSyncSend(i, msg->total_size_, (char*)msg);
    }
    ep.idx_ = index_view<index_type>::encode(0);
    CmiSyncSendAndFree(0, msg->total_size_, (char*)msg);
  }
};

template <typename T, typename Index>
struct chare : public chare_base_ {
  const Index& index(void) const {
    return index_view<Index>::decode(this->index_);
  }

  const collective_index_t& collective(void) const { return this->parent_; }

  const cmk::element_proxy<T> element_proxy(void) {
    return cmk::element_proxy<T>(this->parent_, this->index_);
  }
};

}  // namespace cmk

#endif
