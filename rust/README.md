# Rust+Converse Interop

A cursory investigation into Rust's FFI. In reality, tools like `bindgen` could be used to make a usable interface. There are many areas where macros would help as well. This demo could be considerably extended, in that regard.

This demo has only been tested on non-SMP Charm++ builds. I am unsure how it would interact with SMP-builds. This demo may require debugging to build correctly -- you'd benefit by consulting `CMK_SYSLIBS` and working backwards.