#ifndef __LOCALITY_HH__
#define __LOCALITY_HH__

#include "values.hh"

class locality_ {
  static locality_ *context_;

 public:
  static locality_ *context(void) { return context_; }

  static void set_context(locality_ *ctx) { context_ = ctx; }

  virtual void accept(std::size_t, std::size_t, value_ptr &&) = 0;
};

locality_ *locality_::context_ = nullptr;

#endif
