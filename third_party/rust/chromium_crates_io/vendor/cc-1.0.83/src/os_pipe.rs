//! Adapted from:
//!  - https://doc.rust-lang.org/src/std/sys/unix/pipe.rs.html
//!  - https://doc.rust-lang.org/src/std/sys/unix/fd.rs.html#385
//!  - https://github.com/rust-lang/rust/blob/master/library/std/src/sys/mod.rs#L57
//!  - https://github.com/oconnor663/os_pipe.rs
use std::fs::File;

/// Open a new pipe and return a pair of [`File`] objects for the reader and writer.
///
/// This corresponds to the `pipe2` library call on Posix and the
/// `CreatePipe` library call on Windows (though these implementation
/// details might change). These pipes are non-inheritable, so new child
/// processes won't receive a copy of them unless they're explicitly
/// passed as stdin/stdout/stderr.
pub fn pipe() -> std::io::Result<(File, File)> {
    sys::pipe()
}

#[cfg(unix)]
#[path = "os_pipe/unix.rs"]
mod sys;

#[cfg(windows)]
#[path = "os_pipe/windows.rs"]
mod sys;

#[cfg(all(not(unix), not(windows)))]
compile_error!("Only unix and windows support os_pipe!");
