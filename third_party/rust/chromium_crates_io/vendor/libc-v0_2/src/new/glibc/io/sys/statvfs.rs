//! Header: `io/sys/statvfs.h`

pub use crate::new::glibc::bits::statvfs::*;
use crate::prelude::*;

extern "C" {
    #[cfg_attr(gnu_file_offset_bits64, link_name = "statvfs64")]
    pub fn statvfs(path: *const c_char, buf: *mut statvfs) -> c_int;
    #[cfg_attr(gnu_file_offset_bits64, link_name = "fstatvfs64")]
    pub fn fstatvfs(fd: c_int, buf: *mut statvfs) -> c_int;
    // FIXME(1.0,deprecate): lfs binding to be removed
    pub fn statvfs64(path: *const c_char, buf: *mut statvfs64) -> c_int;
    // FIXME(1.0,deprecate): lfs binding to be removed
    pub fn fstatvfs64(fd: c_int, buf: *mut statvfs64) -> c_int;
}
