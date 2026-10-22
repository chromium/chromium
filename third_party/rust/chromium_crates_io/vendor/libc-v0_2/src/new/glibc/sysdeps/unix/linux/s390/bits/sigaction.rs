//! Header: `sysdeps/unix/sysv/linux/s390/bits/sigaction.h`

use crate::prelude::*;

s! {
    // FIXME(1.0): This should not implement `PartialEq`
    #[allow(unpredictable_function_pointer_comparisons)]
    pub struct sigaction {
        pub sa_sigaction: crate::sighandler_t,
        __glibc_reserved0: Padding<c_int>,
        pub sa_flags: c_int,
        pub sa_restorer: Option<extern "C" fn()>,
        pub sa_mask: crate::sigset_t,
    }
}

pub const SA_NOCLDSTOP: c_int = 0x00000001;
pub const SA_NOCLDWAIT: c_int = 2;
pub const SA_SIGINFO: c_int = 4;
pub const SA_ONSTACK: c_int = 0x08000000;
pub const SA_RESTART: c_int = 0x10000000;
pub const SA_NODEFER: c_int = 0x40000000;
pub const SA_RESETHAND: c_int = u32_cast_int(0x80000000);

pub const SIG_BLOCK: c_int = 0;
pub const SIG_UNBLOCK: c_int = 1;
pub const SIG_SETMASK: c_int = 2;
