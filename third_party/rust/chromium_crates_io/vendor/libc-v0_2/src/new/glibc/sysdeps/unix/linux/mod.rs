//! Source directory: `sysdeps/unix/sysv/linux` (the `sysv` is flattened).
//!
//! <https://github.com/sailfishos-mirror/glibc/tree/master/sysdeps/unix/sysv/linux>

/// Directory: `sysdeps/unix/sysv/linux/bits`
pub(crate) mod bits {
    #[cfg_attr(
        any(
            target_arch = "mips",
            target_arch = "mips32r6",
            target_arch = "mips",
            target_arch = "mips32r6",
        ),
        path = "../mips/bits/sigaction.rs"
    )]
    #[cfg_attr(target_arch = "s390x", path = "../s390/bits/sigaction.rs")]
    #[cfg_attr(
        any(target_arch = "sparc", target_arch = "sparc64"),
        path = "../sparc/bits/sigaction.rs"
    )]
    pub(crate) mod sigaction;

    #[cfg_attr(
        any(
            target_arch = "mips",
            target_arch = "mips32r6",
            target_arch = "mips",
            target_arch = "mips32r6",
        ),
        path = "../mips/bits/signum_arch.rs"
    )]
    #[cfg_attr(
        any(target_arch = "sparc", target_arch = "sparc64"),
        path = "../sparc/bits/signum_arch.rs"
    )]
    pub(crate) mod signum_arch;

    #[cfg_attr(
        any(
            target_arch = "mips",
            target_arch = "mips32r6",
            target_arch = "mips",
            target_arch = "mips32r6",
        ),
        path = "../mips/bits/socket_type.rs"
    )]
    #[cfg_attr(
        any(target_arch = "sparc", target_arch = "sparc64"),
        path = "../sparc/bits/socket_type.rs"
    )]
    pub(crate) mod socket_type;

    pub(crate) mod socket;

    pub(crate) mod statvfs;

    pub(crate) mod types;
}

/// Directory: `net/`
///
/// Source directory: `sysdeps/unix/sysv/linux/net`
pub(crate) mod net {
    pub(crate) mod route;
}
