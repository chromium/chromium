//! This module corresponds to `mach/thread_switch.h`.
use core::ffi::c_int;

pub const SWITCH_OPTION_NONE: c_int = 0;
pub const SWITCH_OPTION_DEPRESS: c_int = 1;
pub const SWITCH_OPTION_WAIT: c_int = 2;
