#include <cassert>
#include <iostream>

// merely follows familiar naming convention
// (not generated by charmxi!)
#include "cmk.decl.hh"

// a message with only POD members (constant-size)
struct test_message : public cmk::plain_message<test_message> {
  int val;

  test_message(int _) : val(_) {}
};

// a chare that uses an int for its index
struct foo : public cmk::chare<int> {
  int val;

  foo(test_message* msg) : val(msg->val) { delete msg; }

  void bar(test_message* msg) {
    CmiPrintf("ch%d@pe%d> %d+%d=%d\n", this->index(), CmiMyPe(), this->val,
              msg->val, this->val + msg->val);
    delete msg;
    cmk::exit();  // maybe add qd?
  }
};

int main(int argc, char** argv) {
  cmk::initialize(argc, argv);
  if (CmiMyNode() == 0) {
    // create a collective
    auto arr = cmk::collective_proxy<foo>::construct();
    // for each pe...
    for (auto i = 0; i < CmiNumPes(); i++) {
      auto elt = arr[i];
      // insert an element
      elt.insert(new test_message(i));
      // and send it a message
      elt.send<test_message, &foo::bar>(new test_message(i + 1));
    }
    // currently a no-op, will unblock reductions
    arr.done_inserting();
  }
  cmk::finalize();
  return 0;
}

// registers user-defined types with the rts
#include "cmk.def.hh"
