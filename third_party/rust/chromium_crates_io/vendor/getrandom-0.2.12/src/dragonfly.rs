//! Implementation for DragonFly BSD
use crate::{
    use_file,
    util_libc::{sys_fill_exact, Weak},
    Error,
};
use core::mem::MaybeUninit;

pub fn getrandom_inner(dest: &mut [MaybeUninit<u8>]) -> Result<(), Error> {
    static GETRANDOM: Weak = unsafe { Weak::new("getrandom\0") };
    type GetRandomFn = unsafe extern "C" fn(*mut u8, libc::size_t, libc::c_uint) -> libc::ssize_t;

    // getrandom(2) was introduced in DragonflyBSD 5.7
    if let Some(fptr) = GETRANDOM.ptr() {
        let func: GetRandomFn = unsafe { core::mem::transmute(fptr) };
        return sys_fill_exact(dest, |buf| unsafe {
            func(buf.as_mut_ptr() as *mut u8, buf.len(), 0)
        });
    } else {
        use_file::getrandom_inner(dest)
    }
}
