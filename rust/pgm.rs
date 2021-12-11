use std::env;
use std::os::raw::{c_char, c_void};

mod cmi {
    use std::os::raw::{c_char, c_void};
    use std::ffi::{CString};

    type StartFn = extern fn(i32, *const *const c_char);
    type HandlerFn = extern fn(*mut c_void);

    extern "C" {
        fn ConverseInit(argc: i32,
                        argv: *const *const c_char,
                        start: StartFn,
                        usched: i32,
                        initret: i32);

        #[link_name = "CmiRegisterHandler"]
        pub fn register_handler(handler: HandlerFn) -> i32;

        static _Cmi_mype: i32;

        fn CmiAlloc(size: i32) -> *mut c_void;

        fn CmiPrintf(format: *const c_char, ...);
    }

    pub unsafe fn initialize<I>(args: I, start: StartFn, usched: i32, initret: i32)
    where I: Iterator<Item = String> {
        let argv: Vec<_> =
            args.map(|x| CString::new(x))
                .map(|x| {
                    let c_str = x.as_ref().unwrap().as_ptr() as *const c_char;
                    std::mem::forget(x);
                    c_str
                })
                .collect();
        let argc: i32 = argv.len() as i32;
        ConverseInit(argc, argv.as_ptr(), start, 0, 0);
        std::mem::forget(argv);
    }

    pub fn set_handler<M>(msg: *mut M) {

    }

    pub unsafe fn allocate_message<M>() -> *mut M {
        let sz = std::mem::size_of::<M>();
        return CmiAlloc(sz as i32) as *mut M;
    }

    pub fn this_pe() -> i32 {
        unsafe {
            return _Cmi_mype;
        }
    }

    pub fn println(s: String) {
        unsafe {
            let c_str = CString::new(s).unwrap();
            CmiPrintf(b"%s\n\0".as_ptr() as *const c_char, c_str.as_ptr() as *const c_char);
        }
    }
}

struct TestMessage {
    core: [u8; 64]
}

extern "C" fn exit(msg: *mut c_void) {

}

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