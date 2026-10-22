//! Header: `sysdeps/unix/sysv/linux/mips/bits/sigaction.h`

use crate::prelude::*;

s! {
    //
    // FIXME(1.0): This should not implement `PartialEq`
    #[allow(unpredictable_function_pointer_comparisons)]
    pub struct sigaction {
        pub sa_flags: c_int,
        pub sa_sigaction: crate::sighandler_t,
        pub sa_mask: crate::sigset_t,
        pub sa_restorer: Option<extern "C" fn()>,
        #[cfg(target_pointer_width = "32")]
        _resv: Padding<[c_int; 1]>,
    }
}

pub const SA_NOCLDSTOP: c_int = 0x00000001;
pub const SA_NOCLDWAIT: c_int = 0x00010000;
pub const SA_SIGINFO: c_int = 0x00000008;
pub const SA_ONSTACK: c_int = 0x08000000;
pub const SA_RESTART: c_int = 0x10000000;
pub const SA_NODEFER: c_int = 0x40000000;
pub const SA_RESETHAND: c_int = u32_cast_int(0x80000000);

pub const SIG_BLOCK: c_int = 0x1;
pub const SIG_UNBLOCK: c_int = 0x2;
pub const SIG_SETMASK: c_int = 3;
