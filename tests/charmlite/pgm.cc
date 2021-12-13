#include <cassert>
#include <iostream>

#include "cmk.decl.hh"

struct test_message : public cmk::message {
  int val;

  test_message(int _) : val(_) {}
};

struct foo : public cmk::chare<int> {
  int val;

  foo(test_message* msg) : val(msg->val) { delete msg; }

  void bar(test_message* msg) {
    std::cout << this->val << "+" << msg->val << "=" << (val + msg->val)
              << std::endl;
    delete msg;
  }
};

int main(void) {
  auto arr = cmk::collective_proxy<foo>::construct();
  auto elt = arr[0];
  // we can send messages to elements that don't exist yet
  elt.send<test_message, &foo::bar>(new test_message(22));
  // so this will still work...
  elt.insert(new test_message(20));
  return 0;
}

#include "cmk.def.hh"
