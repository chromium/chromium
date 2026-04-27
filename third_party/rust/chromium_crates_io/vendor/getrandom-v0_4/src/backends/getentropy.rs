//! Implementation using getentropy(2)
//!
//! When porting to a new target, ensure that its implementation follows the
//! POSIX conventions from
//! <https://pubs.opengroup.org/onlinepubs/9799919799/functions/getentropy.html>.
//!
//! Available since:
//!   - macOS 10.12
//!   - OpenBSD 5.6
//!   - Emscripten 2.0.5
//!   - vita newlib since Dec 2021
//!
//! For these targets, we use getentropy(2) because getrandom(2) doesn't exist.
use crate::Error;
use core::{ffi::c_void, mem::MaybeUninit};

pub use crate::util::{inner_u32, inner_u64};

#[path = "../utils/get_errno.rs"]
mod utils;

#[inline]
pub fn fill_inner(dest: &mut [MaybeUninit<u8>]) -> Result<(), Error> {
    // https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/limits.h.html
    // says `GETENTROPY_MAX` is at least 256.
    const GETENTROPY_MAX: usize = 256;

    for chunk in dest.chunks_mut(GETENTROPY_MAX) {
        let ret = unsafe { libc::getentropy(chunk.as_mut_ptr().cast::<c_void>(), chunk.len()) };
        match ret {
            0 => continue,
            -1 => return Err(Error::from_errno(utils::get_errno())),
            _ => return Err(Error::UNEXPECTED),
        }
    }
    Ok(())
}
