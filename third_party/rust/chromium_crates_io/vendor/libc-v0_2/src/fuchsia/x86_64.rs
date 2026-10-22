use crate::off_t;
use crate::prelude::*;

pub type wchar_t = i32;

#[deprecated(
    since = "0.2.189",
    note = "This type does not exist upstream and will eventually be removed."
)]
pub type __u64 = c_ulonglong;

pub type gregset_t = [c_ulonglong; 23];
pub type fpregset_t = *mut _fpstate;

s! {
    pub struct _fpstate {
        pub cwd: c_ushort,
        pub swd: c_ushort,
        pub ftw: c_ushort,
        pub fop: c_ushort,
        pub rip: c_ulonglong,
        pub rdp: c_ulonglong,
        pub mxcsr: c_uint,
        pub mxcr_mask: c_uint,
        pub _st: [__c_anonymous__fpstate__st; 8],
        pub _xmm: [__c_anonymous__fpstate__xmm; 16],
        padding: Padding<[c_uint; 24]>,
    }

    pub struct __c_anonymous__fpstate__st {
        pub significand: [c_ushort; 4],
        pub exponent: c_ushort,
        padding: Padding<[c_ushort; 3]>,
    }

    pub struct __c_anonymous__fpstate__xmm {
        pub element: [c_uint; 4],
    }

    pub struct stat {
        pub st_dev: crate::dev_t,
        pub st_ino: crate::ino_t,
        pub st_nlink: crate::nlink_t,
        pub st_mode: crate::mode_t,
        pub st_uid: crate::uid_t,
        pub st_gid: crate::gid_t,
        __pad0: Padding<c_uint>,
        pub st_rdev: crate::dev_t,
        pub st_size: off_t,
        pub st_blksize: crate::blksize_t,
        pub st_blocks: crate::blkcnt_t,
        pub st_atime: crate::time_t,
        pub st_atime_nsec: c_long,
        pub st_mtime: crate::time_t,
        pub st_mtime_nsec: c_long,
        pub st_ctime: crate::time_t,
        pub st_ctime_nsec: c_long,
        __unused: Padding<[c_long; 3]>,
    }

    #[deprecated(
        since = "0.2.189",
        note = "This type does not exist upstream and will eventually be \
                removed."
    )]
    pub struct stat64 {
        pub st_dev: crate::dev_t,
        pub st_ino: crate::ino64_t,
        pub st_nlink: crate::nlink_t,
        pub st_mode: crate::mode_t,
        pub st_uid: crate::uid_t,
        pub st_gid: crate::gid_t,
        __pad0: Padding<c_int>,
        pub st_rdev: crate::dev_t,
        pub st_size: off_t,
        pub st_blksize: crate::blksize_t,
        pub st_blocks: crate::blkcnt64_t,
        pub st_atime: crate::time_t,
        pub st_atime_nsec: c_long,
        pub st_mtime: crate::time_t,
        pub st_mtime_nsec: c_long,
        pub st_ctime: crate::time_t,
        pub st_ctime_nsec: c_long,
        __reserved: Padding<[c_long; 3]>,
    }

    pub struct mcontext_t {
        pub gregs: crate::gregset_t,
        pub fpregs: crate::fpregset_t,
        __reserved1: Padding<[c_ulonglong; 8]>,
    }

    pub struct ucontext_t {
        pub uc_flags: c_ulong,
        pub uc_link: *mut ucontext_t,
        pub uc_stack: crate::stack_t,
        pub uc_mcontext: crate::mcontext_t,
        pub uc_sigmask: crate::sigset_t,
        __fpregs_mem: [c_ulong; 64],
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
        pub __seq: c_int,
        __unused1: Padding<c_long>,
        __unused2: Padding<c_long>,
    }
}

// offsets in user_regs_structs, from sys/reg.h
pub const R15: c_int = 0;
pub const R14: c_int = 1;
pub const R13: c_int = 2;
pub const R12: c_int = 3;
pub const RBP: c_int = 4;
pub const RBX: c_int = 5;
pub const R11: c_int = 6;
pub const R10: c_int = 7;
pub const R9: c_int = 8;
pub const R8: c_int = 9;
pub const RAX: c_int = 10;
pub const RCX: c_int = 11;
pub const RDX: c_int = 12;
pub const RSI: c_int = 13;
pub const RDI: c_int = 14;
pub const ORIG_RAX: c_int = 15;
pub const RIP: c_int = 16;
pub const CS: c_int = 17;
pub const EFLAGS: c_int = 18;
pub const RSP: c_int = 19;
pub const SS: c_int = 20;
pub const FS_BASE: c_int = 21;
pub const GS_BASE: c_int = 22;
pub const DS: c_int = 23;
pub const ES: c_int = 24;
pub const FS: c_int = 25;
pub const GS: c_int = 26;

pub const MAP_32BIT: c_int = 0x0040;

pub const SIGSTKSZ: size_t = 8192;
pub const MINSIGSTKSZ: size_t = 2048;
