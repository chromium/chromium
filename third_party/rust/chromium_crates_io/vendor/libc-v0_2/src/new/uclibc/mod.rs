//! uClibc.
//!
//! * About: <https://uclibc.org/>
//! * Headers: <https://github.com/kraj/uClibc> (mirror)

pub(crate) mod pthread;

#[cfg(target_os = "linux")]
pub(crate) mod socket;

/// Directory source: `libc/sysdeps`
///
/// * Headers: <https://gogs.waldemar-brodkorb.de/oss/uclibc-ng/src/master/libc/sysdeps> (official)
/// * Headers: <https://github.com/wbx-github/uclibc-ng/tree/master/libc/sysdeps> (mirror)
pub(crate) mod sysdeps {
    #[cfg(target_os = "linux")]
    pub(crate) mod linux;
}

pub(crate) mod unistd;
