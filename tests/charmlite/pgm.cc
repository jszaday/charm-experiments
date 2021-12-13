#include <cassert>
#include <iostream>

#include "cmk.decl.hh"

struct test_message : public cmk::plain_message<test_message> {
  int val;

  test_message(int _) : val(_) {}
};

struct foo : public cmk::chare<int> {
  int val;

  foo(test_message* msg) : val(msg->val) { delete msg; }

  void bar(test_message* msg) {
    CmiPrintf("ch%d@pe%d> %d+%d=%d\n", this->index(), CmiMyPe(), this->val,
              msg->val, this->val + msg->val);
    delete msg;
    // TODO (temporary constraint -- user-directed exit (qd?))
    CsdExitScheduler();
  }
};

int main(int argc, char** argv) {
  cmk::initialize(argc, argv);
  if (CmiMyPe() == 0) {
    auto arr = cmk::collective_proxy<foo>::construct();
    for (auto i = 0; i < CmiNumPes(); i++) {
      auto elt = arr[i];
      elt.insert(new test_message(i));
      elt.send<test_message, &foo::bar>(new test_message(i + 1));
    }
  }
  cmk::finalize();
  return 0;
}

#include "cmk.def.hh"
