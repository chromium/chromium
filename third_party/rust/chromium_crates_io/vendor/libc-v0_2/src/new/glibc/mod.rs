//! GNU libc.
//!
//! * Headers: <https://sourceware.org/git/?p=glibc.git> (official)
//! * Headers: <https://github.com/sailfishos-mirror/glibc> (mirror)
//!
//! This module structure is modeled after glibc's source tree. Its build system selects headers
//! from different locations based on the platform, which we mimic here with reexports.

/// Source directory: `bits/`
///
/// This directory contains default
///
/// <https://github.com/sailfishos-mirror/glibc/tree/master/bits>
mod bits {
    #[cfg(target_os = "linux")]
    pub(crate) mod signum_generic;
    #[cfg(target_os = "linux")]
    pub(crate) use super::sysdeps::unix::linux::bits::{
        sigaction,
        signum_arch,
        socket,
        statvfs,
        types,
    };
}

mod io {
    pub(crate) mod sys;
}

/// Source directory: `posix/`
///
/// <https://github.com/sailfishos-mirror/glibc/tree/master/posix>
mod posix {
    pub(crate) mod unistd;
}

#[cfg(target_os = "linux")]
pub(crate) mod signal;

/// Source directory: `socket/`
///
/// <https://github.com/sailfishos-mirror/glibc/tree/master/socket>
#[cfg(target_os = "linux")]
mod socket;

/// Source directory: `sysdeps/`
///
/// <https://github.com/sailfishos-mirror/glibc/tree/master/sysdeps>
mod sysdeps {
    // FIXME(pthread): eventually all platforms should use this module
    #[cfg(target_os = "linux")]
    pub(crate) mod nptl;
    pub(crate) mod unix;

    // You'll notice some directories that provide a `bits` module. These are included via
    // `path = "..."` wherever the generic implementation lives.
}

pub(crate) mod sys;

pub(crate) use posix::*;
// FIXME(pthread): eventually all platforms should use this module
#[cfg(target_os = "linux")]
#[allow(unused)]
pub(crate) use sysdeps::nptl::pthread;
#[cfg(target_os = "linux")]
pub(crate) use sysdeps::unix::linux::net;
