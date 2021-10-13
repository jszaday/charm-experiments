
#ifndef __OUTBOX_HH__
#define __OUTBOX_HH__

#include <deque>

#include "values.hh"

template <typename T>
struct connector_ {
  bool ready_;

  connector_(void) : ready_(false) {}

  void relay(T&&) {}
};

template <typename T>
struct outbox_;

template <>
struct outbox_<std::tuple<>> {
  inline void unspool(std::tuple<>&) {}

  inline bool empty(void) const { return false; }
};

template <typename... Ts>
struct outbox_<std::tuple<Ts...>> {
 private:
  static constexpr auto n_outputs = sizeof...(Ts);

  using tuple_type = std::tuple<Ts...>;

  template <typename T>
  using deque_type = std::deque<T>;

  using buffer_type = typename wrap_<tuple_type, deque_type>::type;

  using connector_type = typename wrap_<tuple_type, connector_>::type;

  template <std::size_t I>
  using buffer_elt_t = typename std::tuple_element<I, buffer_type>::type;

  buffer_type buffer_;
  connector_type connectors_;

  template <std::size_t I>
  inline void deliver_(tuple_type& tuple) {
    auto& con = std::get<I>(this->connectors_);
    if (con.ready_) {
      con.relay(std::move(std::get<I>(tuple)));
    } else {
      (std::get<I>(this->buffer_)).emplace_back(std::move(std::get<I>(tuple)));
    }
  }

  template <std::size_t I>
  inline typename std::enable_if<(I == 0)>::type unspool_(tuple_type& tuple) {
    this->deliver_<I>(tuple);
  }

  template <std::size_t I>
  inline typename std::enable_if<(I >= 1)>::type unspool_(tuple_type& tuple) {
    this->deliver_<I>(tuple);
    unspool_<(I - 1)>(tuple);
  }

  template <std::size_t I>
  inline typename std::enable_if<(I == 0), bool>::type empty_(void) const {
    return std::get<I>(this->buffer_).empty();
  }

  template <std::size_t I>
  inline typename std::enable_if<(I >= 1), bool>::type empty_(void) const {
    return std::get<I>(this->buffer_).empty() && this->empty_<(I - 1)>();
  }

 public:
  template <std::size_t I>
  inline buffer_elt_t<I>& get(void) {
    return std::get<I>(this->buffer_);
  }

  inline void unspool(tuple_type& tuple) {
    this->unspool_<(n_outputs - 1)>(tuple);
  }

  inline bool empty(void) const { return this->empty_<(n_outputs - 1)>(); }
};

#endif
