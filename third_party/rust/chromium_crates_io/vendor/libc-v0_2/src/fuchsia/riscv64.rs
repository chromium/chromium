use crate::prelude::*;

// From psABI Calling Convention for RV64
#[deprecated(
    since = "0.2.189",
    note = "This type does not exist upstream and will eventually be removed."
)]
pub type __u64 = c_ulonglong;
pub type wchar_t = i32;

#[deprecated(
    since = "0.2.189",
    note = "This type does not exist upstream and will eventually be removed."
)]
pub type stat64 = stat;

s! {
    pub struct stat {
        pub st_dev: crate::dev_t,
        pub st_ino: crate::ino_t,
        pub st_nlink: crate::nlink_t,
        pub st_mode: crate::mode_t,
        pub st_uid: crate::uid_t,
        pub st_gid: crate::gid_t,
        __pad0: Padding<c_uint>,
        pub st_rdev: crate::dev_t,
        pub st_size: crate::off_t,
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

    // Not actually used, IPC calls just return ENOSYS
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

    pub struct mcontext_t {
        pub regs: [c_ulong; 32],
    }

    pub struct ucontext_t {
        pub uc_flags: c_ulong,
        pub uc_link: *mut crate::ucontext_t,
        pub uc_stack: crate::stack_t,
        pub uc_sigmask: crate::sigset_t,
        pub uc_mcontext: crate::mcontext_t,
    }
}
