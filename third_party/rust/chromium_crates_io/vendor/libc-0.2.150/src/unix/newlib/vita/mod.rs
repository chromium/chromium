pub type clock_t = ::c_long;

pub type c_char = i8;
pub type wchar_t = u32;

pub type c_long = i32;
pub type c_ulong = u32;

s! {
    pub struct sockaddr {
        pub sa_family: ::sa_family_t,
        pub sa_data: [::c_char; 14],
    }

    pub struct sockaddr_in6 {
        pub sin6_family: ::sa_family_t,
        pub sin6_port: ::in_port_t,
        pub sin6_flowinfo: u32,
        pub sin6_addr: ::in6_addr,
        pub sin6_scope_id: u32,
    }

    pub struct sockaddr_in {
        pub sin_family: ::sa_family_t,
        pub sin_port: ::in_port_t,
        pub sin_addr: ::in_addr,
        pub sin_zero: [u8; 8],
    }

    pub struct sockaddr_un {
        pub sun_len: ::c_uchar,
        pub sun_family: ::sa_family_t,
        pub sun_path: [::c_char; 104usize],
    }

    pub struct sockaddr_storage {
        pub ss_family: ::sa_family_t,
        pub __ss_padding: [u8; 26],
    }


    pub struct sched_param {
        pub sched_priority: ::c_int,
    }
}

pub const AF_UNIX: ::c_int = 1;
pub const AF_INET6: ::c_int = 23;

pub const FIONBIO: ::c_ulong = 0x8004667e;

pub const POLLIN: ::c_short = 1;
pub const POLLPRI: ::c_short = 2;
pub const POLLOUT: ::c_short = 4;
pub const POLLERR: ::c_short = 8;
pub const POLLHUP: ::c_short = 16;
pub const POLLNVAL: ::c_short = 32;

pub const RTLD_DEFAULT: *mut ::c_void = 0 as *mut ::c_void;

pub const SOL_SOCKET: ::c_int = 0xffff;

pub const MSG_OOB: ::c_int = 0x1;
pub const MSG_PEEK: ::c_int = 0x2;
pub const MSG_DONTROUTE: ::c_int = 0x4;
pub const MSG_WAITALL: ::c_int = 0x8;
pub const MSG_DONTWAIT: ::c_int = 0x10;
pub const MSG_NOSIGNAL: ::c_int = 0x20;
pub const MSG_TRUNC: ::c_int = 0x0100;
pub const MSG_CTRUNC: ::c_int = 0x0200;

pub const UTIME_OMIT: c_long = -1;
pub const AT_FDCWD: ::c_int = -2;

pub const O_DIRECTORY: ::c_int = 0x200000;
pub const O_NOFOLLOW: ::c_int = 0x100000;

pub const AT_EACCESS: ::c_int = 1;
pub const AT_SYMLINK_NOFOLLOW: ::c_int = 2;
pub const AT_SYMLINK_FOLLOW: ::c_int = 4;
pub const AT_REMOVEDIR: ::c_int = 8;

pub const SIGHUP: ::c_int = 1;
pub const SIGINT: ::c_int = 2;
pub const SIGQUIT: ::c_int = 3;
pub const SIGILL: ::c_int = 4;
pub const SIGTRAP: ::c_int = 5;
pub const SIGABRT: ::c_int = 6;
pub const SIGEMT: ::c_int = 7;
pub const SIGFPE: ::c_int = 8;
pub const SIGKILL: ::c_int = 9;
pub const SIGBUS: ::c_int = 10;
pub const SIGSEGV: ::c_int = 11;
pub const SIGSYS: ::c_int = 12;
pub const SIGPIPE: ::c_int = 13;
pub const SIGALRM: ::c_int = 14;
pub const SIGTERM: ::c_int = 15;

pub const EAI_BADFLAGS: ::c_int = -1;
pub const EAI_NONAME: ::c_int = -2;
pub const EAI_AGAIN: ::c_int = -3;
pub const EAI_FAIL: ::c_int = -4;
pub const EAI_NODATA: ::c_int = -5;
pub const EAI_FAMILY: ::c_int = -6;
pub const EAI_SOCKTYPE: ::c_int = -7;
pub const EAI_SERVICE: ::c_int = -8;
pub const EAI_ADDRFAMILY: ::c_int = -9;
pub const EAI_MEMORY: ::c_int = -10;
pub const EAI_SYSTEM: ::c_int = -11;
pub const EAI_OVERFLOW: ::c_int = -12;

pub const _SC_PAGESIZE: ::c_int = 8;
pub const _SC_GETPW_R_SIZE_MAX: ::c_int = 51;
pub const PTHREAD_STACK_MIN: ::size_t = 200;

extern "C" {
    pub fn futimens(fd: ::c_int, times: *const ::timespec) -> ::c_int;
    pub fn writev(fd: ::c_int, iov: *const ::iovec, iovcnt: ::c_int) -> ::ssize_t;
    pub fn readv(fd: ::c_int, iov: *const ::iovec, iovcnt: ::c_int) -> ::ssize_t;

    pub fn pthread_create(
        native: *mut ::pthread_t,
        attr: *const ::pthread_attr_t,
        f: extern "C" fn(_: *mut ::c_void) -> *mut ::c_void,
        value: *mut ::c_void,
    ) -> ::c_int;

    pub fn pthread_attr_getschedparam(
        attr: *const ::pthread_attr_t,
        param: *mut sched_param,
    ) -> ::c_int;

    pub fn pthread_attr_setschedparam(
        attr: *mut ::pthread_attr_t,
        param: *const sched_param,
    ) -> ::c_int;

    pub fn pthread_attr_getprocessorid_np(
        attr: *const ::pthread_attr_t,
        processor_id: *mut ::c_int,
    ) -> ::c_int;

    pub fn pthread_attr_setprocessorid_np(
        attr: *mut ::pthread_attr_t,
        processor_id: ::c_int,
    ) -> ::c_int;

    pub fn pthread_getschedparam(
        native: ::pthread_t,
        policy: *mut ::c_int,
        param: *mut ::sched_param,
    ) -> ::c_int;

    pub fn pthread_setschedparam(
        native: ::pthread_t,
        policy: ::c_int,
        param: *const ::sched_param,
    ) -> ::c_int;

    pub fn pthread_condattr_getclock(
        attr: *const ::pthread_condattr_t,
        clock_id: *mut ::clockid_t,
    ) -> ::c_int;

    pub fn pthread_condattr_setclock(
        attr: *mut ::pthread_condattr_t,
        clock_id: ::clockid_t,
    ) -> ::c_int;

    pub fn pthread_getprocessorid_np() -> ::c_int;

    pub fn getentropy(buf: *mut ::c_void, buflen: ::size_t) -> ::c_int;
}

pub use crate::unix::newlib::generic::{sigset_t, stat};
