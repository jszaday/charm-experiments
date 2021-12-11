use std::env;
use std::os::raw::{c_char, c_void};
mod cmi;

struct TestMessage {
    core: [u8; 64],
}

extern "C" fn exit(msg: *mut c_void) {}

extern "C" fn start(argc: i32, argv: *const *const c_char) {
    unsafe {
        let mine = cmi::this_pe();
        let exit_handler = cmi::register_handler(exit);

        cmi::println(format!("{}> starting up!", mine));

        if mine == 0 {
            let msg = cmi::allocate_message::<TestMessage>();
        }
    }
}

fn main() {
    unsafe {
        cmi::initialize(env::args(), start, 0, 0);
    }
}
