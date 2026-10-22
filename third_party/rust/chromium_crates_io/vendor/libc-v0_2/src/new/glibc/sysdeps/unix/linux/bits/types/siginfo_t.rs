//! Source header: `sysdeps/unix/sysv/linux/bits/types/siginfo_t.h`
//!
//! Note this module corresponds with multiple header files upstream. The above
//! header file contains the generic definition, while other
//! architecture-specific definitions live upstream at
//! `sysdeps/unix/sysv/linux/<arch>/bits/siginfo-arch.h`. Currently, the
//! following set of header files are contained in this one Rust module:
//!
//! - `sysdeps/unix/sysv/linux/bits/types/siginfo_t.h`
//! - `sysdeps/unix/sysv/linux/x86/bits/siginfo-arch.h`
//! - `sysdeps/unix/sysv/linux/mips/bits/siginfo-arch.h`
//! - `sysdeps/unix/sysv/linux/sparc/bits/siginfo-arch.h`
//!
//! <https://github.com/sailfishos-mirror/glibc/blob/master/sysdeps/unix/sysv/linux/bits/types/siginfo_t.h>

use crate::prelude::*;

const __SI_MAX_SIZE: usize = 128;
const __SI_PAD_SIZE: usize = if cfg!(target_pointer_width = "64") {
    __SI_MAX_SIZE / size_of::<c_int>() - 4
} else {
    __SI_MAX_SIZE / size_of::<c_int>() - 3
};

cfg_if! {
    if #[cfg(target_arch = "sparc64")] {
        type __SI_BAND_TYPE = c_int;
    } else {
        type __SI_BAND_TYPE = c_long;
    }
}

#[repr(C)]
#[cfg_attr(
    all(target_arch = "x86_64", target_pointer_width = "32"),
    repr(align(4))
)]
#[derive(Clone, Copy, Debug)]
struct __SI_CLOCK_T(crate::clock_t);

s_no_extra_traits! {
    #[cfg_attr(
        all(target_arch = "x86_64", target_pointer_width = "32"),
        repr(align(8))
    )]
    pub struct siginfo_t {
        pub si_signo: c_int,

        #[cfg(any(
            target_arch = "mips",
            target_arch = "mips64",
            target_arch = "mips32r6",
            target_arch = "mips64r6"
        ))]
        pub si_code: c_int,

        pub si_errno: c_int,

        #[cfg(not(any(
            target_arch = "mips",
            target_arch = "mips64",
            target_arch = "mips32r6",
            target_arch = "mips64r6"
        )))]
        pub si_code: c_int,

        #[cfg(target_pointer_width = "64")]
        __pad0: Padding<c_int>,
        _sifields: __c_anonymous_siginfo_t__si_fields,
    }

    union __c_anonymous_siginfo_t__si_fields {
        _pad: [c_int; __SI_PAD_SIZE],
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
        si_utime: __SI_CLOCK_T,
        si_stime: __SI_CLOCK_T,
    }

    struct __c_anonymous__si_fields__sigfault {
        si_addr: *mut c_void,
        #[cfg(any(target_arch = "sparc", target_arch = "sparc64"))]
        _si_trapno: c_int,
        si_addr_lsb: c_short,
        _bounds: __c_anonymous__sigfault__bounds,
    }

    struct __c_anonymous__si_fields__sigpoll {
        si_band: __SI_BAND_TYPE,
        si_fd: c_int,
    }

    struct __c_anonymous__si_fields__sigsys {
        _call_addr: *mut c_void,
        _syscall: c_int,
        _arch: c_uint,
    }

    union __c_anonymous__sigfault__bounds {
        _addr_bnd: __c_anonymous__bounds__addr_bnd,
        _pkey: u32,
    }

    struct __c_anonymous__bounds__addr_bnd {
        _lower: *mut c_void,
        _upper: *mut c_void,
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
        unsafe { self._sifields._sigchld.si_utime.0 }
    }

    #[inline]
    pub const unsafe fn si_stime(&self) -> crate::clock_t {
        unsafe { self._sifields._sigchld.si_stime.0 }
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

    #[cfg(any(target_arch = "sparc", target_arch = "sparc64"))]
    #[inline]
    pub const unsafe fn si_trapno(&self) -> c_int {
        unsafe { self._sifields._sigfault._si_trapno }
    }

    #[inline]
    pub const unsafe fn si_addr_lsb(&self) -> c_short {
        unsafe { self._sifields._sigfault.si_addr_lsb }
    }

    #[inline]
    pub const unsafe fn si_lower(&self) -> *mut c_void {
        unsafe { self._sifields._sigfault._bounds._addr_bnd._lower }
    }

    #[inline]
    pub const unsafe fn si_upper(&self) -> *mut c_void {
        unsafe { self._sifields._sigfault._bounds._addr_bnd._upper }
    }

    #[inline]
    pub const unsafe fn si_pkey(&self) -> u32 {
        unsafe { self._sifields._sigfault._bounds._pkey }
    }

    #[inline]
    pub const unsafe fn si_band(&self) -> __SI_BAND_TYPE {
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
                (self.si_signo, self.si_errno, self.si_code)
                    == (other.si_signo, other.si_errno, other.si_code)
            }
        }

        impl Eq for siginfo_t {}

        impl core::hash::Hash for siginfo_t {
            fn hash<H: core::hash::Hasher>(&self, state: &mut H) {
                self.si_signo.hash(state);
                self.si_errno.hash(state);
                self.si_code.hash(state);
            }
        }
    }
}
