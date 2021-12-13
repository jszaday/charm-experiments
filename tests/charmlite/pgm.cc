#include <cassert>
#include <iostream>

#include "cmk.decl.hh"

struct test_message : public cmk::message {
  int val;

  test_message(int _) : val(_) {}
};

struct foo {
  int val;

  foo(test_message* msg) : val(msg->val) { delete msg; }

  void bar(test_message* msg) {
    std::cout << this->val << "+" << msg->val << "=" << (val + msg->val)
              << std::endl;
    delete msg;
  }
};

int main(void) {
  assert(!cmk::entry_table_.empty());
  auto* m1 = new test_message(20);
  auto p = cmk::proxy<foo>::construct<test_message>(m1);
  auto* m2 = new test_message(22);
  p.send<test_message, &foo::bar>(m2);
  return 0;
}

#include "cmk.def.hh"
