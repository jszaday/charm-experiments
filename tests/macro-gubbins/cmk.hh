#ifndef __CMK_HH__
#define __CMK_HH__

#include "ep.hh"

#define CMK_INDEX_OF(Class, Method) Cmk_##Class##_##Method##_Index##_

#define CMK_REGISTER(Class, Method)                                      \
  cmk::entry_id_t CMK_INDEX_OF(Class, Method) =                          \
      cmk::register_<Class, cmk::message_of_t<decltype(&Class::Method)>, \
                     &Class::Method>();

#define CMK_CONSTRUCTOR(Class, Argument)       \
  cmk::entry_id_t CMK_INDEX_OF(Class, Class) = \
      cmk::register_constructor_<Class, Argument>();

#endif
