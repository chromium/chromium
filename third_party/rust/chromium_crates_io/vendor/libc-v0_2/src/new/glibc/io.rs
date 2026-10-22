/// Source directory: `io/sys/`
///
/// <https://github.com/sailfishos-mirror/glibc/tree/master/io/sys>
pub(crate) mod sys {
    #[cfg(target_os = "linux")]
    pub(crate) mod statvfs;

    pub use super::socket::sys::*;
}
