//! Implementation for GNU/Hurd
use crate::util_libc::sys_fill_exact;
use crate::Error;
use core::mem::MaybeUninit;

pub fn getrandom_inner(dest: &mut [MaybeUninit<u8>]) -> Result<(), Error> {
    sys_fill_exact(dest, |buf| unsafe {
        libc::getrandom(buf.as_mut_ptr() as *mut libc::c_void, buf.len(), 0)
    })
}
