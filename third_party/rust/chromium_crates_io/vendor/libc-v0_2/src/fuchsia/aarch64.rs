use crate::off_t;
use crate::prelude::*;

#[deprecated(
    since = "0.2.189",
    note = "This type does not exist upstream and will eventually be removed."
)]
pub type __u64 = c_ulonglong;
pub type wchar_t = u32;

s! {
    pub struct stat {
        pub st_dev: crate::dev_t,
        pub st_ino: crate::ino_t,
        pub st_mode: crate::mode_t,
        pub st_nlink: crate::nlink_t,
        pub st_uid: crate::uid_t,
        pub st_gid: crate::gid_t,
        pub st_rdev: crate::dev_t,
        __pad: Padding<c_ulong>,
        pub st_size: off_t,
        pub st_blksize: crate::blksize_t,
        __pad2: Padding<c_int>,
        pub st_blocks: crate::blkcnt_t,
        pub st_atime: crate::time_t,
        pub st_atime_nsec: c_long,
        pub st_mtime: crate::time_t,
        pub st_mtime_nsec: c_long,
        pub st_ctime: crate::time_t,
        pub st_ctime_nsec: c_long,
        __unused: Padding<[c_uint; 2]>,
    }

    #[deprecated(
        since = "0.2.189",
        note = "This type does not exist upstream and will eventually be \
                removed."
    )]
    pub struct stat64 {
        pub st_dev: crate::dev_t,
        pub st_ino: crate::ino_t,
        pub st_mode: crate::mode_t,
        pub st_nlink: crate::nlink_t,
        pub st_uid: crate::uid_t,
        pub st_gid: crate::gid_t,
        pub st_rdev: crate::dev_t,
        __pad0: Padding<c_ulong>,
        pub st_size: off_t,
        pub st_blksize: crate::blksize_t,
        __pad1: Padding<c_int>,
        pub st_blocks: crate::blkcnt_t,
        pub st_atime: crate::time_t,
        pub st_atime_nsec: c_long,
        pub st_mtime: crate::time_t,
        pub st_mtime_nsec: c_long,
        pub st_ctime: crate::time_t,
        pub st_ctime_nsec: c_long,
        __unused: Padding<[c_uint; 2]>,
    }

    #[deprecated(
        since = "0.2.189",
        note = "This type does not exist upstream and will eventually be \
                removed."
    )]
    pub struct ipc_perm {
        pub __ipc_perm_key: crate::key_t,
        pub uid: crate::uid_t,
        pub gid: crate::gid_t,
        pub cuid: crate::uid_t,
        pub cgid: crate::gid_t,
        pub mode: crate::mode_t,
        pub __seq: c_ushort,
        __unused1: Padding<c_ulong>,
        __unused2: Padding<c_ulong>,
    }

    // FIXME(f128): __reserved is meant to be an array of `long double`s. That
    // requires native support for `f128`. This is currently missing.
    pub struct mcontext_t {
        pub fault_address: c_ulong,
        pub regs: [c_ulong; 31],
        pub sp: c_ulong,
        pub pc: c_ulong,
        pub pstate: c_ulong,
        __reserved: Padding<[i128; 256]>,
    }

    pub struct ucontext_t {
        pub uc_flags: c_ulong,
        pub uc_link: *mut crate::ucontext_t,
        pub uc_stack: crate::stack_t,
        pub uc_sigmask: crate::sigset_t,
        pub uc_mcontext: crate::mcontext_t,
    }
}

// From https://cs.opensource.google/fuchsia/fuchsia/+/main:zircon/third_party/ulib/musl/include/bits/signal.h;l=20-21;drc=0827b18ab9540c46f8037f407d17ea15a79e9ba7
pub const MINSIGSTKSZ: size_t = 6144;
pub const SIGSTKSZ: size_t = 12288;
