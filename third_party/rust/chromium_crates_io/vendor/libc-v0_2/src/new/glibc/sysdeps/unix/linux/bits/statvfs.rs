//! Header: `sysdeps/unix/sysv/linux/bits/statvfs.h`

use crate::prelude::*;

s2! {
    pub struct statvfs {
        pub f_bsize: c_ulong,
        pub f_frsize: c_ulong,
        pub f_blocks: crate::fsblkcnt_t,
        pub f_bfree: crate::fsblkcnt_t,
        pub f_bavail: crate::fsblkcnt_t,
        pub f_files: crate::fsfilcnt_t,
        pub f_ffree: crate::fsfilcnt_t,
        pub f_favail: crate::fsfilcnt_t,
        pub f_fsid: c_ulong,
        // Mirrors `_STATVFSBUF_F_UNUSED` in the header. x32 is excluded because
        // its `__SYSCALL_WORDSIZE` is 64, aarch64 because glibc always sets
        // `__WORDSIZE` to 64.
        #[cfg(all(
            target_pointer_width = "32",
            not(any(target_arch = "x86_64", target_arch = "aarch64"))
        ))]
        __f_unused: Padding<c_int>,
        pub f_flag: c_ulong,
        pub f_namemax: c_ulong,
        pub f_type: c_uint,
        __f_spare: Padding<[c_int; 5]>,
    }

    pub struct statvfs64 {
        pub f_bsize: c_ulong,
        pub f_frsize: c_ulong,
        pub f_blocks: u64,
        pub f_bfree: u64,
        pub f_bavail: u64,
        pub f_files: u64,
        pub f_ffree: u64,
        pub f_favail: u64,
        pub f_fsid: c_ulong,
        // Mirrors `_STATVFSBUF_F_UNUSED` in the header. x32 is excluded because
        // its `__SYSCALL_WORDSIZE` is 64, aarch64 because glibc always sets
        // `__WORDSIZE` to 64.
        #[cfg(all(
            target_pointer_width = "32",
            not(any(target_arch = "x86_64", target_arch = "aarch64"))
        ))]
        __f_unused: Padding<c_int>,
        pub f_flag: c_ulong,
        pub f_namemax: c_ulong,
        pub f_type: c_uint,
        __f_spare: Padding<[c_int; 5]>,
    }
}
