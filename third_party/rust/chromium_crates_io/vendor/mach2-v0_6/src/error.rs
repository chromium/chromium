//! This module corresponds to `mach/error.h`.

use crate::kern_return::kern_return_t;
use core::ffi::c_int;

pub type mach_error_t = kern_return_t;
pub type mach_error_fn_t = Option<unsafe extern "C" fn() -> mach_error_t>;

pub const err_none: mach_error_t = 0;
pub const ERR_SUCCESS: mach_error_t = err_none;
pub const ERR_ROUTINE_NIL: mach_error_fn_t = None;

#[inline]
pub const fn err_system(x: c_int) -> mach_error_t {
    (((x as u32) & 0x3f) << 26) as mach_error_t
}

#[inline]
pub const fn err_sub(x: c_int) -> mach_error_t {
    (((x as u32) & 0xfff) << 14) as mach_error_t
}

#[inline]
pub const fn err_get_system(err: mach_error_t) -> c_int {
    (((err as u32) >> 26) & 0x3f) as c_int
}

#[inline]
pub const fn err_get_sub(err: mach_error_t) -> c_int {
    (((err as u32) >> 14) & 0xfff) as c_int
}

#[inline]
pub const fn err_get_code(err: mach_error_t) -> c_int {
    ((err as u32) & 0x3fff) as c_int
}

pub const system_emask: mach_error_t = err_system(0x3f);
pub const sub_emask: mach_error_t = err_sub(0xfff);
pub const code_emask: mach_error_t = 0x3fff;

pub const err_kern: mach_error_t = err_system(0x0);
pub const err_us: mach_error_t = err_system(0x1);
pub const err_server: mach_error_t = err_system(0x2);
pub const err_ipc: mach_error_t = err_system(0x3);
pub const err_mach_ipc: mach_error_t = err_system(0x4);
pub const err_dipc: mach_error_t = err_system(0x7);
pub const err_vm: mach_error_t = err_system(0x8);
pub const err_local: mach_error_t = err_system(0x3e);
pub const err_ipc_compat: mach_error_t = err_system(0x3f);

pub const err_max_system: c_int = 0x3f;

#[inline]
pub const fn unix_err(errno: mach_error_t) -> mach_error_t {
    err_kern | err_sub(3) | errno
}
