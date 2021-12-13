#include <iostream>
#include <cassert>

#include "cmk.hh"

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
  auto* obj = ::operator new(sizeof(foo));
  auto* m1 = new test_message(20);
  auto cons = cmk::constructor<foo, cmk::message_ptr<test_message>&&>();
  cmk::invoke(obj, cons, m1);
  auto* m2 = new test_message(22);
  auto ep = cmk::entry<decltype(&foo::bar), &foo::bar>();
  cmk::invoke(obj, ep, m2);
  return 0;
}

#include "ep.def.hh"
