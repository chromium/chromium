//! Header source: `include/signal.h`
//!
//! * Headers: <https://git.musl-libc.org/cgit/musl/tree/include/signal.h> (official)
//! * Headers: <https://github.com/kraj/musl/blob/master/include/signal.h> (mirror)

use crate::prelude::*;

s_no_extra_traits! {
    // `mips*` targets swap the `s_errno` and `s_code` fields otherwise this
    // struct is target-agnostic (see
    // <https://www.openwall.com/lists/musl/2016/01/27/1/2>)
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

        __si_fields: __c_anonymous_siginfo_t___si_fields,
    }

    union __c_anonymous_siginfo_t___si_fields {
        __pad: [c_char; 128 - 2 * size_of::<c_int>() - size_of::<c_long>()],
        __si_common: __c_anonymous___si_fields___si_common,
        __sigfault: __c_anonymous___si_fields___sigfault,
        __sigpoll: __c_anonymous___si_fields___sigpoll,
        __sigsys: __c_anonymous___si_fields___sigsys,
    }

    struct __c_anonymous___si_fields___si_common {
        __first: __c_anonymous___si_common___first,
        __second: __c_anonymous___si_common___second,
    }

    struct __c_anonymous___si_fields___sigfault {
        si_addr: *mut c_void,
        si_addr_lsb: c_short,
        __first: __c_anonymous___sigfault___first,
    }

    struct __c_anonymous___si_fields___sigpoll {
        si_band: c_long,
        si_fd: c_int,
    }

    struct __c_anonymous___si_fields___sigsys {
        si_call_addr: *mut c_void,
        si_syscall: c_int,
        si_arch: c_uint,
    }

    union __c_anonymous___si_common___first {
        __piduid: __c_anonymous___first___piduid,
        __timer: __c_anonymous___first___timer,
    }

    union __c_anonymous___si_common___second {
        si_value: crate::sigval,
        __sigchld: __c_anonymous___second___sigchld,
    }

    union __c_anonymous___sigfault___first {
        __addr_band: __c_anonymous___first___addr_band,
        si_pkey: c_uint,
    }

    struct __c_anonymous___first___piduid {
        si_pid: crate::pid_t,
        si_uid: crate::uid_t,
    }

    struct __c_anonymous___first___timer {
        si_timerid: c_int,
        si_overrun: c_int,
    }

    struct __c_anonymous___second___sigchld {
        si_status: c_int,
        si_utime: crate::clock_t,
        si_stime: crate::clock_t,
    }

    struct __c_anonymous___first___addr_band {
        si_lower: *mut c_void,
        si_upper: *mut c_void,
    }
}

impl siginfo_t {
    #[inline]
    pub const unsafe fn si_pid(&self) -> crate::pid_t {
        unsafe { self.__si_fields.__si_common.__first.__piduid.si_pid }
    }

    #[inline]
    pub const unsafe fn si_uid(&self) -> crate::uid_t {
        unsafe { self.__si_fields.__si_common.__first.__piduid.si_uid }
    }

    #[inline]
    pub const unsafe fn si_status(&self) -> c_int {
        unsafe { self.__si_fields.__si_common.__second.__sigchld.si_status }
    }

    #[inline]
    pub const unsafe fn si_utime(&self) -> crate::clock_t {
        unsafe { self.__si_fields.__si_common.__second.__sigchld.si_utime }
    }

    #[inline]
    pub const unsafe fn si_stime(&self) -> crate::clock_t {
        unsafe { self.__si_fields.__si_common.__second.__sigchld.si_stime }
    }

    #[inline]
    pub const unsafe fn si_value(&self) -> crate::sigval {
        unsafe { self.__si_fields.__si_common.__second.si_value }
    }

    #[inline]
    pub const unsafe fn si_addr(&self) -> *mut c_void {
        unsafe { self.__si_fields.__sigfault.si_addr }
    }

    #[inline]
    pub const unsafe fn si_addr_lsb(&self) -> c_short {
        unsafe { self.__si_fields.__sigfault.si_addr_lsb }
    }

    #[inline]
    pub const unsafe fn si_lower(&self) -> *mut c_void {
        unsafe { self.__si_fields.__sigfault.__first.__addr_band.si_lower }
    }

    #[inline]
    pub const unsafe fn si_upper(&self) -> *mut c_void {
        unsafe { self.__si_fields.__sigfault.__first.__addr_band.si_upper }
    }

    #[inline]
    pub const unsafe fn si_pkey(&self) -> c_uint {
        unsafe { self.__si_fields.__sigfault.__first.si_pkey }
    }

    #[inline]
    pub const unsafe fn si_band(&self) -> c_long {
        unsafe { self.__si_fields.__sigpoll.si_band }
    }

    #[inline]
    pub const unsafe fn si_fd(&self) -> c_int {
        unsafe { self.__si_fields.__sigpoll.si_fd }
    }

    #[inline]
    pub const unsafe fn si_timerid(&self) -> c_int {
        unsafe { self.__si_fields.__si_common.__first.__timer.si_timerid }
    }

    #[inline]
    pub const unsafe fn si_overrun(&self) -> c_int {
        unsafe { self.__si_fields.__si_common.__first.__timer.si_overrun }
    }

    #[inline]
    pub const unsafe fn si_ptr(&self) -> *mut c_void {
        unsafe { self.__si_fields.__si_common.__second.si_value.sival_ptr }
    }

    #[inline]
    pub unsafe fn si_int(&self) -> c_int {
        // DIFF(main): sigval is a union on `main`, struct on `libc-0.2`. The cast means
        // this can't be const at the MSRV.
        unsafe { self.__si_fields.__si_common.__second.si_value.sival_ptr as usize as c_int }
    }

    #[inline]
    pub const unsafe fn si_call_addr(&self) -> *mut c_void {
        unsafe { self.__si_fields.__sigsys.si_call_addr }
    }

    #[inline]
    pub const unsafe fn si_syscall(&self) -> c_int {
        unsafe { self.__si_fields.__sigsys.si_syscall }
    }

    #[inline]
    pub const unsafe fn si_arch(&self) -> c_uint {
        unsafe { self.__si_fields.__sigsys.si_arch }
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
