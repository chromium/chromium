//! Header: `sysdeps/unix/sysv/linux/sparc/bits/sigaction.h`

use crate::prelude::*;

s! {
    // FIXME(1.0): This should not implement `PartialEq`
    #[allow(unpredictable_function_pointer_comparisons)]
    pub struct sigaction {
        pub sa_sigaction: crate::sighandler_t,
        pub sa_mask: crate::sigset_t,
        #[cfg(target_pointer_width = "64")]
        __reserved0: Padding<c_int>,
        pub sa_flags: c_int,
        pub sa_restorer: Option<extern "C" fn()>,
    }
}

pub const SA_NOCLDSTOP: c_int = 0x00000008;
pub const SA_NOCLDWAIT: c_int = 0x100;
pub const SA_SIGINFO: c_int = 0x200;
pub const SA_ONSTACK: c_int = 1;
pub const SA_RESTART: c_int = 0x2;
pub const SA_NODEFER: c_int = 0x20;
pub const SA_RESETHAND: c_int = 0x4;

pub const SIG_BLOCK: c_int = 1;
pub const SIG_UNBLOCK: c_int = 2;
pub const SIG_SETMASK: c_int = 4;
