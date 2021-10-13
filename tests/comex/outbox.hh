
#ifndef __OUTBOX_HH__
#define __OUTBOX_HH__

#include <deque>

#include "utilities.hh"

template <typename T>
struct outbox_;

template <>
struct outbox_<std::tuple<>> {
  inline void unspool(std::tuple<>&) {}
};

template <typename... Ts>
struct outbox_<std::tuple<Ts...>> {
 private:
  static constexpr auto n_outputs = sizeof...(Ts);

  using tuple_type = std::tuple<Ts...>;

  template <typename T>
  using deque_type = std::deque<T>;

  using buffer_type = typename wrap_<tuple_type, deque_type>::type;

  template <std::size_t I>
  using buffer_elt_t = typename std::tuple_element<I, buffer_type>::type;

  buffer_type buffer_;

  template <std::size_t I>
  inline typename std::enable_if<(I == 0)>::type unspool_(tuple_type& tuple) {
    (std::get<I>(this->buffer_)).emplace_back(std::move(std::get<I>(tuple)));
  }

  template <std::size_t I>
  inline typename std::enable_if<(I >= 1)>::type unspool_(tuple_type& tuple) {
    (std::get<I>(this->buffer_)).emplace_back(std::move(std::get<I>(tuple)));
    unspool_<(I - 1)>(tuple);
  }

 public:
  template <std::size_t I>
  inline buffer_elt_t<I>& get(void) {
    return std::get<I>(this->buffer_);
  }

  inline void unspool(tuple_type& tuple) {
    this->unspool_<n_outputs - 1>(tuple);
  }
};

#endif
