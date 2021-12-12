#include <iostream>

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

CMK_CONSTRUCTOR(foo, cmk::message_ptr<test_message>&&);
CMK_REGISTER(foo, bar);

int main(void) {
  auto* obj = ::operator new(sizeof(foo));
  auto* m1 = new test_message(20);
  cmk::invoke(obj, CMK_INDEX_OF(foo, foo), m1);
  auto* m2 = new test_message(22);
  cmk::invoke(obj, CMK_INDEX_OF(foo, bar), m2);
  return 0;
}