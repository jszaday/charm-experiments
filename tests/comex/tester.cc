#include "component.hh"
#include "tester.decl.h"

constexpr auto kNumElts = 4;
constexpr auto kNumIters = 8;

auto intData = 87654321;
auto doubleData = 0.12345678;

template <typename... Ts>
struct test_component : public component<std::tuple<Ts...>, std::tuple<>> {
  using parent_t = component<std::tuple<Ts...>, std::tuple<>>;
  using in_set = typename parent_t::in_set;
  using out_set = typename parent_t::out_set;

  static_assert(sizeof...(Ts) == 2, "expected exactly two values");

  test_component(std::size_t id_)
      : parent_t(id_) {
    this->active = true;
    this->persistent = true;
  }

  virtual out_set action(in_set& set) override {
    std::stringstream ss;
    ss << "{" << **(std::get<0>(set)) << "," << **(std::get<1>(set)) << "}";
    CkPrintf("com%d> recvd value set %s!\n", this->id, ss.str().c_str());
    return {};
  }
};

class test_main : public CBase_test_main {
 public:
  // checks whether the "accepts" function is working as expected
  void check_accepts(void) {
    using com_t = test_component<int, double>;
    auto* com = (component_base_*)(new com_t(0x1));
    CkEnforce(com->accepts(0, typeid(int)));
    CkEnforce(com->accepts(1, typeid(double)));
    CkEnforce(!com->accepts(1, typeid(int)));
    CkEnforce(!com->accepts(2, typeid(int)));
    delete com;
  }

  test_main(CkArgMsg* msg) {
    this->check_accepts();

    CProxy_exchanger::ckNew(kNumElts);

    CkExitAfterQuiescence();
  }
};

class exchanger : public CBase_exchanger {
  // this maps a component's id to its implementation
  std::map<std::size_t, std::unique_ptr<component_base_>> components_;

  static_assert((2 * sizeof(std::uint8_t)) <= sizeof(CMK_REFNUM_TYPE),
                "refnum too small?");

 public:
  exchanger(void) {
    auto& mine = this->thisIndex;
    CkEnforce(mine <= std::numeric_limits<std::uint8_t>::max());

    // depending on whether we're even or odd -- xchg the port types
    if ((mine % 2) == 0) {
      this->components_.emplace(mine, new test_component<int, double>(mine));
    } else {
      this->components_.emplace(mine, new test_component<double, int>(mine));
    }

    this->do_sends();
  }

  void do_sends(void) {
    auto& mine = this->thisIndex;
    auto even = (mine + (2 - (mine % 2))) % kNumElts;
    auto odd = (mine + (mine % 2) + 1) % kNumElts;

    for (auto it = 0; it < kNumIters; it += 1) {
      CkDataMsg* msg;
      auto dstPort = it % 4 / 2;
      auto dstCom = (it % 2 == 0) ? even : odd;
      // create a message of the appropriate type for the dst port
      if (((even == dstCom) && !dstPort) || ((odd == dstCom) && dstPort)) {
        msg = CkDataMsg::buildNew(sizeof(int), &intData);
      } else {
        msg = CkDataMsg::buildNew(sizeof(double), &doubleData);
      }
      // encode the destination in a legible way
      auto refnum =
          (CMK_REFNUM_TYPE)(((std::uint8_t)dstCom) << 8) | (0xFF & dstPort);
      CkSetRefNum(msg, refnum);
      // and send the message along
      thisProxy[dstCom].accept(msg);
    }
  }

  void accept(CkDataMsg* msg) {
    auto refnum = CkGetRefNum(msg);
    // determine the destination com/port pair
    auto* port = (std::uint8_t*)&refnum;
    auto* com = port + 1;
    // and route the message accordingly
    this->components_[*com]->accept(*port, msg);
  }
};

#include "tester.def.h"
