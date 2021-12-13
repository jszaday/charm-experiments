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
    using decayed_t = typename std::decay<argument_t>::type;
    auto id = constructor<T, argument_t>();
    auto* self = static_cast<T*>(::operator new(sizeof(T)));
    proxy<T> p(self);
    cmk::invoke(self, id, std::forward<decayed_t>(msg));
    return std::move(p);
  }

  template <typename Message, member_fn_t<T, Message> Fn>
  void invoke(message_ptr<Message>&& msg) {
    auto id = entry<member_fn_t<T, Message>, Fn>();
    cmk::invoke(this->self_, id, std::move(msg));
  }
};
}  // namespace cmk

#endif
