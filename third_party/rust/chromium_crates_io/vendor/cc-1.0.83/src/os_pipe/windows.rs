use crate::windows_sys::{CreatePipe, INVALID_HANDLE_VALUE};
use std::{fs::File, io, os::windows::prelude::*, ptr};

/// NOTE: These pipes do not support IOCP.
///
/// If IOCP is needed, then you might want to emulate
/// anonymous pipes with CreateNamedPipe, as Rust's stdlib does.
pub(super) fn pipe() -> io::Result<(File, File)> {
    let mut read_pipe = INVALID_HANDLE_VALUE;
    let mut write_pipe = INVALID_HANDLE_VALUE;

    let ret = unsafe { CreatePipe(&mut read_pipe, &mut write_pipe, ptr::null_mut(), 0) };

    if ret == 0 {
        Err(io::Error::last_os_error())
    } else {
        unsafe {
            Ok((
                File::from_raw_handle(read_pipe as RawHandle),
                File::from_raw_handle(write_pipe as RawHandle),
            ))
        }
    }
}
