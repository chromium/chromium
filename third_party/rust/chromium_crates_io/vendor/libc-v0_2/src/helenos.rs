use crate::prelude::*;
use crate::{
    errno_t,
    timespec,
};

pub type intmax_t = i64;
pub type uintmax_t = u64;
pub type intptr_t = isize;
pub type uintptr_t = usize;
pub type size_t = usize;
pub type ssize_t = isize;

// uspace/lib/posix/include/posix/pthread.h
pub type pthread_key_t = c_int;

// uspace/lib/posix/include/posix/sys/types.h
pub type clockid_t = c_int;
pub type pid_t = c_int;

s! {
    // common/include/adt/list.h
    pub struct link_t {
        pub next: *mut link_t,
        pub prev: *mut link_t,
    }

    pub struct list_t {
        pub head: link_t,
    }
}

// uspace/lib/posix/include/posix/time.h
pub const CLOCK_REALTIME: clockid_t = 0;
pub const CLOCK_MONOTONIC: clockid_t = 1;

// 'static inline' functions from libc
// common/include/adt/list.h
f! {
    pub safe fn list_initialize(list: &mut list_t) -> () {
        list.head.next = &mut list.head;
        list.head.prev = &mut list.head;
    }
}

extern "C" {
    // common/include/str_error.h
    pub fn str_error(err: errno_t) -> *const c_char;
    pub fn str_error_name(err: errno_t) -> *const c_char;

    // uspace/lib/posix/include/posix/pthread.h
    pub fn pthread_key_create(
        key: *mut pthread_key_t,
        destructor: Option<unsafe extern "C" fn(*mut c_void)>,
    ) -> c_int;
    pub fn pthread_getspecific(key: pthread_key_t) -> *mut c_void;
    pub fn pthread_setspecific(key: pthread_key_t, value: *const c_void) -> c_int;
    pub fn pthread_key_delete(key: pthread_key_t) -> c_int;

    // uspace/lib/posix/include/posix/time.h
    pub fn clock_getres(clock_id: clockid_t, res: *mut timespec) -> c_int;
    pub fn clock_gettime(clock_id: clockid_t, tp: *mut timespec) -> c_int;
    pub fn clock_settime(clock_id: clockid_t, tp: *const timespec) -> c_int;
    pub fn clock_nanosleep(
        clock_id: clockid_t,
        flags: c_int,
        rqtp: *const timespec,
        rmtp: *mut timespec,
    ) -> c_int;

    // uspace/lib/posix/include/posix/unistd.h
    pub fn getpid() -> pid_t;
    pub fn getcwd(buf: *mut c_char, size: size_t) -> *mut c_char;
    pub fn chdir(buf: *const c_char) -> c_int;
    pub static environ: *const *const c_char;
    pub fn isatty(fd: c_int) -> c_int;
}
