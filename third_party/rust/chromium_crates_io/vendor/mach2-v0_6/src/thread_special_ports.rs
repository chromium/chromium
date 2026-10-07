//! This module corresponds to `mach/thread_special_ports.h`.
use core::ffi::c_int;

pub const THREAD_KERNEL_PORT: c_int = 1;
pub const THREAD_INSPECT_PORT: c_int = 2;
pub const THREAD_READ_PORT: c_int = 3;
pub const THREAD_MAX_SPECIAL_PORT: c_int = THREAD_READ_PORT;
