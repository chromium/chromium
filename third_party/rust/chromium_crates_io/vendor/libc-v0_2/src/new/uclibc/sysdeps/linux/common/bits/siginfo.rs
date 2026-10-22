//! Header source: `libc/sysdeps/linux/common/bits/siginfo.h`
//!
//! Note this header file unifies all header files in upstream uClibc-ng for
//! both the generic definition and the architecture-specific definitions.
//! Currently, this Rust module has the definitions for the following set of
//! header files:
//!
//! - `libc/sysdeps/linux/common/bits/siginfo.h`
//! - `libc/sysdeps/linux/mips/bits/siginfo.h`
//!
//! * Headers: <https://gogs.waldemar-brodkorb.de/oss/uclibc-ng/src/master/libc/sysdeps/linux/common/bits/siginfo.h> (official)
//! * Headers: <https://github.com/wbx-github/uclibc-ng/blob/master/libc/sysdeps/linux/common/bits/siginfo.h> (mirror)

use crate::prelude::*;

const __SI_MAX_SIZE: usize = 128;
const __SI_PAD_SIZE: usize = if cfg!(target_pointer_width = "64") {
    __SI_MAX_SIZE / size_of::<c_int>() - 4
} else {
    __SI_MAX_SIZE / size_of::<c_int>() - 3
};

s_no_extra_traits! {
    pub struct siginfo_t {
        pub si_signo: c_int,

        #[cfg(any(
            target_arch = "mips",
            target_arch = "mips64",
            target_arch = "mips32r6",
            target_arch = "mips64r6",
        ))]
        pub si_code: c_int,

        pub si_errno: c_int,

        #[cfg(not(any(
            target_arch = "mips",
            target_arch = "mips64",
            target_arch = "mips32r6",
            target_arch = "mips64r6",
        )))]
        pub si_code: c_int,

        #[cfg(any(
            target_arch = "mips",
            target_arch = "mips64",
            target_arch = "mips32r6",
            target_arch = "mips64r6",
        ))]
        __pad0: Padding<[c_int; __SI_MAX_SIZE / size_of::<c_int>() - __SI_PAD_SIZE - 3]>,

        _sifields: __c_anonymous_siginfo_t__si_fields,
    }

    union __c_anonymous_siginfo_t__si_fields {
        _pad: Padding<[c_int; __SI_PAD_SIZE]>,
        _kill: __c_anonymous__si_fields__kill,
        _timer: __c_anonymous__si_fields__timer,
        _rt: __c_anonymous__si_fields__rt,
        _sigchld: __c_anonymous__si_fields__sigchld,
        _sigfault: __c_anonymous__si_fields__sigfault,
        _sigpoll: __c_anonymous__si_fields__sigpoll,
        _sigsys: __c_anonymous__si_fields__sigsys,
    }

    struct __c_anonymous__si_fields__kill {
        si_pid: crate::pid_t,
        si_uid: crate::uid_t,
    }

    struct __c_anonymous__si_fields__timer {
        si_tid: c_int,
        si_overrun: c_int,
        si_sigval: crate::sigval,
    }

    struct __c_anonymous__si_fields__rt {
        si_pid: crate::pid_t,
        si_uid: crate::uid_t,
        si_sigval: crate::sigval,
    }

    struct __c_anonymous__si_fields__sigchld {
        si_pid: crate::pid_t,
        si_uid: crate::uid_t,
        si_status: c_int,
        si_utime: crate::clock_t,
        si_stime: crate::clock_t,
    }

    struct __c_anonymous__si_fields__sigfault {
        si_addr: *mut c_void,
        #[cfg(any(
            target_arch = "mips",
            target_arch = "mips64",
            target_arch = "mips32r6",
            target_arch = "mips64r6",
        ))]
        si_addr_lsb: c_short,
    }

    struct __c_anonymous__si_fields__sigpoll {
        si_band: c_long,
        si_fd: c_int,
    }

    struct __c_anonymous__si_fields__sigsys {
        _call_addr: *mut c_void,
        _syscall: c_int,
        _arch: c_uint,
    }
}

impl siginfo_t {
    #[inline]
    pub const unsafe fn si_pid(&self) -> crate::pid_t {
        unsafe { self._sifields._kill.si_pid }
    }

    #[inline]
    pub const unsafe fn si_uid(&self) -> crate::uid_t {
        unsafe { self._sifields._kill.si_uid }
    }

    #[inline]
    pub const unsafe fn si_timerid(&self) -> c_int {
        unsafe { self._sifields._timer.si_tid }
    }

    #[inline]
    pub const unsafe fn si_overrun(&self) -> c_int {
        unsafe { self._sifields._timer.si_overrun }
    }

    #[inline]
    pub const unsafe fn si_status(&self) -> c_int {
        unsafe { self._sifields._sigchld.si_status }
    }

    #[inline]
    pub const unsafe fn si_utime(&self) -> crate::clock_t {
        unsafe { self._sifields._sigchld.si_utime }
    }

    #[inline]
    pub const unsafe fn si_stime(&self) -> crate::clock_t {
        unsafe { self._sifields._sigchld.si_stime }
    }

    #[inline]
    pub const unsafe fn si_value(&self) -> crate::sigval {
        unsafe { self._sifields._rt.si_sigval }
    }

    #[inline]
    pub unsafe fn si_int(&self) -> c_int {
        // DIFF(main): sigval is a union on `main`, struct on `libc-0.2`. The cast means
        // this can't be const at the MSRV.
        unsafe { self._sifields._rt.si_sigval.sival_ptr as usize as c_int }
    }

    #[inline]
    pub const unsafe fn si_ptr(&self) -> *mut c_void {
        unsafe { self._sifields._rt.si_sigval.sival_ptr }
    }

    #[inline]
    pub const unsafe fn si_addr(&self) -> *mut c_void {
        unsafe { self._sifields._sigfault.si_addr }
    }

    #[cfg(any(
        target_arch = "mips",
        target_arch = "mips64",
        target_arch = "mips32r6",
        target_arch = "mips64r6",
    ))]
    #[inline]
    pub const unsafe fn si_addr_lsb(&self) -> c_short {
        unsafe { self._sifields._sigfault.si_addr_lsb }
    }

    #[inline]
    pub const unsafe fn si_band(&self) -> c_long {
        unsafe { self._sifields._sigpoll.si_band }
    }

    #[inline]
    pub const unsafe fn si_fd(&self) -> c_int {
        unsafe { self._sifields._sigpoll.si_fd }
    }

    #[inline]
    pub const unsafe fn si_call_addr(&self) -> *mut c_void {
        unsafe { self._sifields._sigsys._call_addr }
    }

    #[inline]
    pub const unsafe fn si_syscall(&self) -> c_int {
        unsafe { self._sifields._sigsys._syscall }
    }

    #[inline]
    pub const unsafe fn si_arch(&self) -> c_uint {
        unsafe { self._sifields._sigsys._arch }
    }
}

cfg_if! {
    if #[cfg(feature = "extra_traits")] {
        impl PartialEq for siginfo_t {
            fn eq(&self, other: &Self) -> bool {
                (self.si_signo, self.si_code, self.si_errno)
                    == (other.si_signo, other.si_code, other.si_errno)
            }
        }

        impl Eq for siginfo_t {}

        impl core::hash::Hash for siginfo_t {
            fn hash<H: core::hash::Hasher>(&self, state: &mut H) {
                self.si_signo.hash(state);
                self.si_code.hash(state);
                self.si_errno.hash(state);
            }
        }
    }
}
