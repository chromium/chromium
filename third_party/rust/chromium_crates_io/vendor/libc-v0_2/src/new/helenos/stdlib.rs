use crate::prelude::*;

extern "C" {
    pub fn malloc(size: usize) -> *mut c_void;
    pub fn calloc(nmemb: usize, size: usize) -> *mut c_void;
    pub fn realloc(addr: *mut c_void, size: usize) -> *mut c_void;
    pub fn free(addr: *mut c_void);

    pub fn memalign(align: usize, size: usize) -> *mut c_void;

    pub fn rand() -> c_int;
    pub fn srand(seed: c_uint);

    pub fn exit(code: c_int) -> !;
    pub fn abort() -> !;

    pub fn getenv(env: *const c_char) -> *mut c_char;
}
