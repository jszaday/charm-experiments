
#ifndef __UTILITIES_HH__
#define __UTILITIES_HH__

#include <charm++.h>

#include <array>
#include <tuple>
#include <type_traits>
#include <typeindex>

template <typename Tuple, std::size_t N, std::size_t I>
inline typename std::enable_if<I == 0>::type make_type_list_(
    std::array<std::type_index, N>& arr) {
  using type = typename std::tuple_element<I, Tuple>::type;
  new (&arr[I]) std::type_index(typeid(type));
}

template <typename Tuple, std::size_t N, std::size_t I>
inline typename std::enable_if<(I >= 1)>::type make_type_list_(
    std::array<std::type_index, N>& arr) {
  using type = typename std::tuple_element<I, Tuple>::type;
  new (&arr[I]) std::type_index(typeid(type));
  make_type_list_<Tuple, N, I - 1>(arr);
}

template <typename Tuple, std::size_t N>
std::array<std::type_index, N> make_type_list_(void) {
  using type = std::array<std::type_index, N>;
  std::aligned_storage<sizeof(type), alignof(type)> storage;
  auto* arr = reinterpret_cast<type*>(&storage);
  make_type_list_<Tuple, N, N - 1>(*arr);
  return *arr;
}

template <typename T>
struct tuplify_ {
  using type = std::tuple<T>;
};

template <>
struct tuplify_<std::tuple<>> {
  using type = std::tuple<void>;
};

template <typename... Ts>
struct tuplify_<std::tuple<Ts...>> {
  using type = std::tuple<Ts...>;
};

#endif
