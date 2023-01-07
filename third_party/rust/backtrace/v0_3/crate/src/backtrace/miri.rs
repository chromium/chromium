use alloc::boxed::Box;
use core::ffi::c_void;

extern "Rust" {
    fn miri_get_backtrace(flags: u64) -> Box<[*mut ()]>;
    fn miri_resolve_frame(ptr: *mut (), flags: u64) -> MiriFrame;
}

#[derive(Clone, Debug)]
#[repr(C)]
pub struct MiriFrame {
    pub name: Box<[u8]>,
    pub filename: Box<[u8]>,
    pub lineno: u32,
    pub colno: u32,
    pub fn_ptr: *mut c_void,
}

#[derive(Debug, Clone)]
pub struct Frame {
    pub addr: *mut c_void,
    pub inner: MiriFrame,
}

// SAFETY: Miri guarantees that the returned pointer
// can be used from any thread.
unsafe impl Send for Frame {}
unsafe impl Sync for Frame {}

impl Frame {
    pub fn ip(&self) -> *mut c_void {
        self.addr
    }

    pub fn sp(&self) -> *mut c_void {
        core::ptr::null_mut()
    }

    pub fn symbol_address(&self) -> *mut c_void {
        self.inner.fn_ptr
    }

    pub fn module_base_address(&self) -> Option<*mut c_void> {
        None
    }
}

pub fn trace<F: FnMut(&super::Frame) -> bool>(cb: F) {
    // SAFETY: Miri guarantees that the backtrace API functions
    // can be called from any thread.
    unsafe { trace_unsynchronized(cb) };
}

pub fn resolve_addr(ptr: *mut c_void) -> Frame {
    // SAFETY: Miri will stop execution with an error if this pointer
    // is invalid.
    let frame: MiriFrame = unsafe { miri_resolve_frame(ptr as *mut (), 0) };
    Frame {
        addr: ptr,
        inner: frame,
    }
}

pub unsafe fn trace_unsynchronized<F: FnMut(&super::Frame) -> bool>(mut cb: F) {
    let frames = miri_get_backtrace(0);
    for ptr in frames.iter() {
        let frame = resolve_addr(*ptr as *mut c_void);
        cb(&super::Frame { inner: frame });
    }
}
