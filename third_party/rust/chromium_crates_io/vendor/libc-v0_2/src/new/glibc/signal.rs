//! Header: `signal/signal.h`

pub use crate::new::glibc::bits::sigaction::*;
pub use crate::new::glibc::bits::signum_generic::*;
pub use crate::new::glibc::bits::types::siginfo_t::*;
use crate::prelude::*;

extern "C" {
    pub fn sigaction(signum: c_int, act: *const sigaction, oldact: *mut sigaction) -> c_int;
}
