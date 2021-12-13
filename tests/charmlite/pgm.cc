#include <cassert>
#include <iostream>

#include "cmk.decl.hh"

struct test_message : public cmk::message {
  int val;

  test_message(int _) : val(_) {}
};

struct foo {
  int val;

  foo(cmk::message_ptr<test_message>&& msg) : val(msg->val) {}

  void bar(cmk::message_ptr<test_message>&& msg) {
    std::cout << this->val << "+" << msg->val << "=" << (val + msg->val)
              << std::endl;
  }
};

int main(void) {
  assert(!cmk::entry_table_.empty());
  using message_t = cmk::message_ptr<test_message>;
  auto* m1 = new test_message(20);
  auto p = cmk::proxy<foo>::construct<test_message>(message_t(m1));
  auto* m2 = new test_message(22);
  p.invoke<test_message, (&foo::bar)>(message_t(m2));
  return 0;
}

#include "cmk.def.hh"
