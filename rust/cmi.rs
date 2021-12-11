use std::ffi::CString;
use std::os::raw::{c_char, c_void};

type StartFn = extern "C" fn(i32, *const *const c_char);
type HandlerFn = extern "C" fn(*mut c_void);

extern "C" {
    fn ConverseInit(
        argc: i32,
        argv: *const *const c_char,
        start: StartFn,
        usched: i32,
        initret: i32,
    );

    #[link_name = "CmiRegisterHandler"]
    pub fn register_handler(handler: HandlerFn) -> i32;

    static _Cmi_mype: i32;

    fn CmiAlloc(size: i32) -> *mut c_void;

    fn CmiSetHandler_(msg: *mut c_void, handler: i32);

    fn CmiSyncBroadcastAllAndFree_(size: i32, msg: *mut c_void);

    fn CmiPrintf(format: *const c_char, ...);

    fn CmiFree(msg: *mut c_void);

    #[link_name = "CsdExitScheduler_"]
    pub fn exit_scheduler();
}

// TODO ( use rust-bindgen instead )
pub const HEADER_SIZE: usize = 48;

pub unsafe fn initialize<I>(args: I, start: StartFn, usched: i32, initret: i32)
where
    I: Iterator<Item = String>,
{
    let argv: Vec<_> = args
        .map(|x| CString::new(x))
        .map(|x| {
            let c_str = x.as_ref().unwrap().as_ptr() as *const c_char;
            std::mem::forget(x);
            c_str
        })
        .collect();
    let argc: i32 = argv.len() as i32;
    ConverseInit(argc, argv.as_ptr(), start, usched, initret);
    std::mem::forget(argv);
}

pub fn set_handler<M>(msg: *mut M, handler: i32) {
    unsafe {
        CmiSetHandler_(msg as *mut c_void, handler);
    }
}

pub unsafe fn allocate_message<M>() -> *mut M {
    let sz = std::mem::size_of::<M>();
    return CmiAlloc(sz as i32) as *mut M;
}

pub fn free_message<M>(msg: *mut M) {
    unsafe {
        CmiFree(msg as *mut c_void);
    }
}

pub mod sync {
    pub fn broadcast_all_and_free<M>(msg: *mut M) {
        use cmi::CmiSyncBroadcastAllAndFree_;
        use c_void;

        unsafe {
            let sz = std::mem::size_of::<M>();
            CmiSyncBroadcastAllAndFree_(sz as i32, msg as *mut c_void);
        }
    }
}

pub fn this_pe() -> i32 {
    unsafe {
        return _Cmi_mype;
    }
}

pub fn println(s: String) {
    unsafe {
        let c_str = CString::new(s).unwrap();
        CmiPrintf(
            b"%s\n\0".as_ptr() as *const c_char,
            c_str.as_ptr() as *const c_char,
        );
    }
}
