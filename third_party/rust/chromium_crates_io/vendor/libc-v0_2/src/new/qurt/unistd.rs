//! Header: `unistd.h`

use super::*;
pub use crate::new::common::posix::unistd::{
    STDERR_FILENO,
    STDIN_FILENO,
    STDOUT_FILENO,
};
use crate::prelude::*;

// Access mode constants
pub const F_OK: c_int = 0;
pub const X_OK: c_int = 1;
pub const W_OK: c_int = 2;
pub const R_OK: c_int = 4;

// Whence constants for lseek
pub const SEEK_SET: c_int = 0;
pub const SEEK_CUR: c_int = 1;
pub const SEEK_END: c_int = 2;

extern "C" {
    // File operations
    pub fn access(pathname: *const c_char, mode: c_int) -> c_int;
    pub fn close(fd: c_int) -> c_int;
    pub fn dup(oldfd: c_int) -> c_int;
    pub fn dup2(oldfd: c_int, newfd: c_int) -> c_int;
    pub fn lseek(fd: c_int, offset: off_t, whence: c_int) -> off_t;
    pub fn read(fd: c_int, buf: *mut c_void, count: size_t) -> ssize_t;
    pub fn write(fd: c_int, buf: *const c_void, count: size_t) -> ssize_t;

    // Process operations
    pub fn getpid() -> pid_t;
    pub fn getppid() -> pid_t;
    pub fn getuid() -> uid_t;
    pub fn geteuid() -> uid_t;
    pub fn getgid() -> gid_t;
    pub fn getegid() -> gid_t;
    pub fn getpgid(pid: pid_t) -> pid_t;
    pub fn getpgrp() -> pid_t;
    pub fn setpgrp() -> pid_t;
    pub fn seteuid(uid: uid_t) -> c_int;
    pub fn setuid(uid: uid_t) -> c_int;
    pub fn setpgid(pid: pid_t, pgid: pid_t) -> c_int;
    pub fn setsid() -> pid_t;

    // File synchronization
    pub fn fsync(fd: c_int) -> c_int;

    // Sleep functions
    pub fn sleep(seconds: c_uint) -> c_uint;
    pub fn usleep(usec: useconds_t) -> c_int;

    // System configuration
    pub fn sysconf(name: c_int) -> c_long;
    pub fn pathconf(path: *const c_char, name: c_int) -> c_long;
    pub fn fpathconf(fd: c_int, name: c_int) -> c_long;
}
