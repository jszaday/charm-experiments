#ifndef __CMK_CORE_HH__
#define __CMK_CORE_HH__

#include "common.hh"

namespace cmk {
void start_fn_(int, char**);

inline void register_deliver_(void) {
  CpvInitialize(int, deliver_handler_);
  CpvAccess(deliver_handler_) = CmiRegisterHandler(deliver);
}

inline void initialize(int argc, char** argv) {
  ConverseInit(argc, argv, start_fn_, 1, 1);
  register_deliver_();
}

inline void finalize(void) {
  CsdScheduleForever();
  ConverseExit();
}

void exit(message* msg);
}  // namespace cmk

#endif
