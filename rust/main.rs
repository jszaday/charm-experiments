use std::env;
use std::os::raw::{c_char, c_void};
mod cmi;

struct TestMessage {
    _core: [u8; cmi::HEADER_SIZE],
}

// TODO ( wrap this with user-defined type )
// TODO ( exploit RUST's ARC to avoid explicit free )
extern "C" fn exit(msg: *mut c_void) {
    let mine = cmi::this_pe();
    cmi::println(format!("{}> shuttting down!", mine));
    cmi::free_message(msg);
    unsafe {
        cmi::exit_scheduler();
    }
}

// TODO ( wrap this with Rust usable args (Vec<String>) )
extern "C" fn start(_argc: i32, _argv: *const *const c_char) {
    unsafe {
        let mine = cmi::this_pe();
        let exit_handler = cmi::register_handler(exit);

        cmi::println(format!("{}> starting up!", mine));

        if mine == 0 {
            let msg = cmi::allocate_message::<TestMessage>();
            cmi::set_handler(msg, exit_handler);
            cmi::sync::broadcast_all_and_free(msg);
        }
    }
}

fn main() {
    unsafe {
        cmi::initialize(env::args(), start, 0, 0);
    }
}
