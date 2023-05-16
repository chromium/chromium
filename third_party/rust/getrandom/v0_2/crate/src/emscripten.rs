//! Implementation for Emscripten
use crate::{util_libc::last_os_error, Error};
use core::mem::MaybeUninit;

// Not yet defined in libc crate.
extern "C" {
    fn getentropy(buffer: *mut libc::c_void, length: usize) -> libc::c_int;
}

pub fn getrandom_inner(dest: &mut [MaybeUninit<u8>]) -> Result<(), Error> {
    // Emscripten 2.0.5 added getentropy, so we can use it unconditionally.
    // Unlike other getentropy implementations, there is no max buffer length.
    let ret = unsafe { getentropy(dest.as_mut_ptr() as *mut libc::c_void, dest.len()) };
    if ret < 0 {
        return Err(last_os_error());
    }
    Ok(())
}
