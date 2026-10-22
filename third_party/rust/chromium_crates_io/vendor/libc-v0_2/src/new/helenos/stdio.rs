//! HelenOS libc standard I/O APIs
//!
//! * Header file: <https://github.com/HelenOS/helenos/tree/master/uspace/lib/c/include/stdio.h>

use crate::prelude::*;

pub const SEEK_SET: c_int = 0;
pub const SEEK_CUR: c_int = 1;
pub const SEEK_END: c_int = 2;

extern_ty! {
    pub type FILE;
}

extern "C" {
    pub static stdin: *mut FILE;
    pub static stdout: *mut FILE;
    pub static stderr: *mut FILE;

    pub fn fputc(c: c_int, stream: *mut FILE) -> c_int;
    pub fn fputs(s: *const c_char, stream: *mut FILE) -> c_int;

    pub fn fopen(filename: *const c_char, mode: *const c_char) -> *mut FILE;
    pub fn fclose(stream: *mut FILE) -> c_int;
    pub fn fflush(stream: *mut FILE) -> c_int;

    pub fn fread(buf: *mut c_void, size: size_t, nmemb: size_t, stream: *mut FILE) -> size_t;
    pub fn fwrite(buf: *const c_void, size: size_t, nmemb: size_t, stream: *mut FILE) -> size_t;

    pub fn fseek(stream: *mut FILE, offset: c_long, whence: c_int) -> c_int;
    pub fn ftell(stream: *mut FILE) -> c_long;

    pub fn fileno(stream: *mut FILE) -> c_int;
}
