//! Header: `bits/signum-generic.h`

use crate::prelude::*;

pub const SIGINT: c_int = 2;
pub const SIGILL: c_int = 4;
pub const SIGABRT: c_int = 6;
pub const SIGFPE: c_int = 8;
pub const SIGSEGV: c_int = 11;
pub const SIGTERM: c_int = 15;

pub const SIGHUP: c_int = 1;
pub const SIGQUIT: c_int = 3;
pub const SIGTRAP: c_int = 5;
pub const SIGKILL: c_int = 9;
pub const SIGPIPE: c_int = 13;
pub const SIGALRM: c_int = 14;

pub use crate::new::glibc::bits::signum_arch::*;
