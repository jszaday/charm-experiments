#ifndef __COMPONENT_HH__
#define __COMPONENT_HH__

#include "values.hh"

class component_base_ {
 public:
  const std::size_t id;
  const std::size_t n_inputs;
  const std::size_t n_outputs;

 protected:
  component_base_(std::size_t id_, std::size_t n_inputs_,
                  std::size_t n_outputs_)
      : id(id_), n_inputs(n_inputs_), n_outputs(n_outputs_) {}

 public:
  virtual ~component_base_() {}

  // we need a way to accept arbitrary messages
  virtual void accept(std::size_t port, CkMessage* msg) = 0;

  // checks whether a given port can accept a value with the type
  // given by idx, this only used in strict/error-checking modes
  virtual bool accepts(std::size_t port, const std::type_index& idx) const = 0;
};

template <typename Inputs, typename Outputs>
class component : public component_base_ {
 public:
  using in_type = typename wrapped_<Inputs>::type;
  using out_type = typename wrapped_<Outputs>::type;

 private:
  template <std::size_t I>
  using in_elt_t = typename std::tuple_element<I, in_type>::type;

  static constexpr std::size_t n_inputs_ = std::tuple_size<in_type>::value;
  static constexpr std::size_t n_outputs_ = std::tuple_size<out_type>::value;
  using acceptor_fn_t = void (*)(component<Inputs, Outputs>*, CkMessage*);

 public:
  using in_type_array_t = std::array<std::type_index, n_inputs_>;
  using acceptor_array_t = std::array<acceptor_fn_t, n_inputs_>;

  static in_type_array_t in_types;
  // effectively a vtable of typed acceptors for each port
  static acceptor_array_t acceptors;

  component(std::size_t id_) : component_base_(id_, n_inputs_, n_outputs_) {}

  // NOTE ( in reality, this would do something with the value )
  template <std::size_t I>
  static void accept(component<Inputs, Outputs>* self, CkMessage* msg) {
    auto val = msg2typed<in_elt_t<I>>(msg);

    std::stringstream ss;
    ss << **val;
    auto str = ss.str();

    CkPrintf("com%lu> got value %s!\n", self->id, str.c_str());
  }

  virtual void accept(std::size_t port, CkMessage* msg) override {
    (*acceptors[port])(this, msg);
  }

  virtual bool accepts(std::size_t port,
                       const std::type_index& idx) const override {
    return (port < n_inputs_) && (idx == in_types[port]);
  }

 private:
  template <std::size_t I>
  static typename std::enable_if<(I == 0)>::type make_acceptors_(
      acceptor_array_t& arr) {
    new (&arr[I]) acceptor_fn_t(accept<I>);
  }

  template <std::size_t I>
  static typename std::enable_if<(I >= 1)>::type make_acceptors_(
      acceptor_array_t& arr) {
    new (&arr[I]) acceptor_fn_t(accept<I>);
    make_acceptors_<I - 1>(arr);
  }

  static acceptor_array_t make_acceptors_(void) {
    using type = acceptor_array_t;
    std::aligned_storage<sizeof(type), alignof(type)> storage;
    auto* arr = reinterpret_cast<type*>(&storage);
    make_acceptors_<n_inputs_ - 1>(*arr);
    return *arr;
  }
};

template <typename Inputs, typename Outputs>
typename component<Inputs, Outputs>::in_type_array_t
    component<Inputs, Outputs>::in_types =
        make_type_list_<component<Inputs, Outputs>::in_type,
                        component<Inputs, Outputs>::n_inputs_>();

template <typename Inputs, typename Outputs>
typename component<Inputs, Outputs>::acceptor_array_t
    component<Inputs, Outputs>::acceptors =
        component<Inputs, Outputs>::make_acceptors_();

#endif
