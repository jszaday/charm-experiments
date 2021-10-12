#ifndef __COMPONENT_HH__
#define __COMPONENT_HH__

#include <ck.h>

#include <deque>

#include "values.hh"

class component_base_ {
 public:
  const std::size_t id;
  const std::size_t n_inputs;
  const std::size_t n_outputs;

  using acceptor_fn_t = void (*)(component_base_*, CkMessage*);

 private:
  // effectively a vtable of typed acceptors for each port
  const acceptor_fn_t* acceptors;
  const std::type_index* in_types;

 protected:
  component_base_(std::size_t id_, std::size_t n_inputs_,
                  std::size_t n_outputs_, const acceptor_fn_t* acceptors_,
                  const std::type_index* in_types_)
      : id(id_),
        n_inputs(n_inputs_),
        n_outputs(n_outputs_),
        acceptors(acceptors_),
        in_types(in_types_) {}

 public:
  virtual ~component_base_() {}

  // we need a way to accept arbitrary messages
  inline void accept(std::size_t port, CkMessage* msg) {
    CkAssertMsg(port < this->n_inputs, "port out of range!");
    (this->acceptors[port])(this, msg);
  }

  // checks whether a given port can accept a value with the type
  // given by idx, this only used in strict/error-checking modes
  inline bool accepts(std::size_t port, const std::type_index& idx) const {
    return (port < this->n_inputs) && (idx == this->in_types[port]);
  }
};

template <typename Inputs, typename Outputs>
class component : public component_base_ {
 public:
  using in_type = typename tuplify_<Inputs>::type;
  using out_type = typename tuplify_<Outputs>::type;
  using value_set = typename wrap_<in_type>::type;

 private:
  template <std::size_t I>
  using in_elt_t = typename std::tuple_element<I, in_type>::type;

  static constexpr std::size_t n_inputs_ = std::tuple_size<in_type>::value;
  static constexpr std::size_t n_outputs_ = std::tuple_size<out_type>::value;
  static_assert(n_inputs_ >= 1, "expected at least one input!");

 protected:
  using accepted_type = std::deque<value_set>;
  using accepted_iterator = typename accepted_type::iterator;

  accepted_type accepted_;

 public:
  using in_type_array_t = std::array<std::type_index, n_inputs_>;
  using acceptor_array_t = std::array<acceptor_fn_t, n_inputs_>;

  static in_type_array_t in_types;
  static acceptor_array_t acceptors;

  component(std::size_t id_)
      : component_base_(id_, n_inputs_, n_outputs_, acceptors.data(),
                        in_types.data()) {}

  template <std::size_t I>
  static void accept(component<Inputs, Outputs>* self,
                     typed_value_ptr<in_elt_t<I>>&& val) {
    if (n_inputs_ == 1) {
      // bypass seeking a partial set for single-input coms
      direct_stage<I>(self, std::move(val));
    } else if (val) {
      // look for a set missing this value
      auto search = self->find_gap_<I>();
      const auto gapless = search == std::end(self->accepted_);
      // if we couldn't find one:
      if (gapless) {
        // create a new set
        self->accepted_.emplace_front();
        // then update the search iterator
        search = self->accepted_.begin();
      }
      // update the value in the set
      std::get<I>(*search) = std::move(val);
      QdCreate(1);
      // check whether it's ready, and stage it if so!
      if (!gapless && is_ready(*search)) {
        self->stage_action(search);
      }
    } else {
      self->on_invalidation_<I>();
    }
  }

  template <std::size_t I>
  static void accept(component_base_* base, CkMessage* msg) {
    auto* self = static_cast<component<Inputs, Outputs>*>(base);
    self->accept<I>(self, msg2typed<in_elt_t<I>>(msg));
  }

 protected:
  inline static bool is_ready(const value_set& set) {
    return is_ready_<n_inputs_ - 1>(set);
  }

 private:
  template<std::size_t I>
  void on_invalidation_(void) {
    CkAbort("-- not implemented --");
  }

  void buffer_ready_set_(value_set&& set) {
    // this should be consistent with "find_ready"
    // and the opposite of "find_gap"
    this->accepted_.emplace_front(std::move(set));
    QdCreate(n_inputs_);
  }

  template <std::size_t I>
  inline static typename std::enable_if<(I == 0) && (n_inputs_ == 1)>::type
  direct_stage(component<Inputs, Outputs>* self,
               typed_value_ptr<in_elt_t<I>>&& val) {
    if (val) {
      value_set set(std::move(val));
      if (!self->stage_action(set)) {
        buffer_ready_set_(std::move(set));
      }
    } else {
      self->on_invalidation_<I>();
    }
  }

  template <std::size_t I>
  inline static typename std::enable_if<(I >= 0) && (n_inputs_ > 1)>::type
  direct_stage(component<Inputs, Outputs>* self,
               typed_value_ptr<in_elt_t<I>>&& val) {
    CkAbort("-- unreachable --");
  }

  inline void stage_action(const accepted_iterator& search) {
    if (this->stage_action(*search)) {
      this->accepted_.erase(search);
      QdProcess(n_inputs_);
    }
  }

  // returns true if the set was consumed
  inline bool stage_action(value_set& set) {
    std::stringstream ss;
    ss << "{" << **(std::get<0>(set)) << "," << **(std::get<1>(set)) << "}";
    CkPrintf("com%d> recvd value set %s!\n", this->id, ss.str().c_str());
    return true;
  }

  template <std::size_t I>
  inline accepted_iterator find_gap_(void) {
    if (!this->accepted_.empty()) {
      auto search = this->accepted_.rbegin();
      for (; search != this->accepted_.rend(); search++) {
        if (!std::get<I>(*search)) {
          return --search.base();
        }
      }
    }

    return std::end(this->accepted_);
  }

  template <std::size_t I>
  inline static typename std::enable_if<I == 0, bool>::type is_ready_(
      const value_set& set) {
    return (bool)std::get<I>(set);
  }

  template <std::size_t I>
  inline static typename std::enable_if<I >= 1, bool>::type is_ready_(
      const value_set& set) {
    return (bool)std::get<I>(set) && is_ready_<(I - 1)>(set);
  }

  template <std::size_t I>
  inline static typename std::enable_if<(I == 0)>::type make_acceptors_(
      acceptor_array_t& arr) {
    new (&arr[I]) acceptor_fn_t(accept<I>);
  }

  template <std::size_t I>
  inline static typename std::enable_if<(I >= 1)>::type make_acceptors_(
      acceptor_array_t& arr) {
    new (&arr[I]) acceptor_fn_t(accept<I>);
    make_acceptors_<(I - 1)>(arr);
  }

  inline static acceptor_array_t make_acceptors_(void) {
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
