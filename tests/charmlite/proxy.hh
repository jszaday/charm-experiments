#ifndef __CMK_PROXY_HH__
#define __CMK_PROXY_HH__

#include "ep.hh"

namespace cmk {
template <typename T>
class proxy {
 private:
  T* self_;

  proxy(T* _) : self_(_) {}

 public:
  template <typename Message>
  static proxy<T> construct(viable_argument_t<Message> msg) {
    using argument_t = viable_argument_t<Message>;
    auto id = constructor<T, argument_t>();
    auto& rec = record_of<T>();
    auto* self = static_cast<T*>(rec.allocate());
    proxy<T> p(self);
    cmk::invoke(self, id, msg);
    return std::move(p);
  }

  template <typename Message, member_fn_t<T, Message> Fn>
  void send(Message* msg) {
    auto id = entry<member_fn_t<T, Message>, Fn>();
    cmk::invoke(this->self_, id, std::move(msg));
  }
};
}  // namespace cmk

#endif
