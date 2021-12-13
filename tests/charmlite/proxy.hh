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
    msg->ep_ = constructor<T, Message*>();
    msg->id_ = this->id_;
    msg->idx_ = this->idx_;
    deliver(msg);
  }

  void insert(void) {
    auto* msg = new message;
    msg->ep_ = constructor<T, void>();
    msg->id_ = this->id_;
    msg->idx_ = this->idx_;
    deliver(msg);
  }

  template <typename Message, member_fn_t<T, Message> Fn>
  void send(Message* msg) {
    msg->ep_ = entry<member_fn_t<T, Message>, Fn>();
    msg->id_ = this->id_;
    msg->idx_ = this->idx_;
    deliver(msg);
  }
};

template <typename T, typename Mapper = default_mapper>
class collective_proxy {
  collective_index_t id_;

 public:
  using index_type = index_for_t<T>;

  collective_proxy(const collective_index_t& id) : id_(id) {}

  static const collective_kind_t& kind(void) {
    return collective_helper_<collective<T, Mapper>>::kind_;
  }

  element_proxy<T> operator[](const index_type& idx) {
    auto& view = index_view<index_type>::reinterpret(idx);
    return element_proxy<T>(this->id_, view);
  }

  static collective_proxy<T, Mapper> construct(void) {
    collective_index_t id{(std::uint32_t)CmiMyPe(), local_collective_count_++};
    auto* msg = new message();
    msg->id_ = id;
    msg->set_collective_kind(kind());
    // CmiFreeBroadcastAllFn(sizeof(message), (char*)msg);
    deliver(msg);
    return collective_proxy<T, Mapper>(id);
  }
};

}  // namespace cmk

#endif
