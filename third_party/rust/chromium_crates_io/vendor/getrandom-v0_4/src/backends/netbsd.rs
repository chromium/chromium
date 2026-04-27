//! Implementation for NetBSD
//!
//! `getrandom(2)` was introduced in NetBSD 10. To support older versions we
//! implement our own weak linkage to it, and provide a fallback based on the
//! KERN_ARND sysctl.
use crate::Error;
use core::{
    cmp,
    ffi::c_void,
    mem::{self, MaybeUninit},
    ptr::{self, NonNull},
};

pub use crate::util::{inner_u32, inner_u64};

#[path = "../utils/sys_fill_exact.rs"]
mod utils;

unsafe extern "C" fn polyfill_using_kern_arand(
    buf: *mut c_void,
    buflen: libc::size_t,
    flags: libc::c_uint,
) -> libc::ssize_t {
    debug_assert_eq!(flags, 0);

    const MIB_LEN: libc::c_uint = 2;
    static MIB: [libc::c_int; MIB_LEN as usize] = [libc::CTL_KERN, libc::KERN_ARND];

    // NetBSD will only return up to 256 bytes at a time, and
    // older NetBSD kernels will fail on longer buffers.
    let mut len = cmp::min(buflen, 256);
    let ret = unsafe { libc::sysctl(MIB.as_ptr(), MIB_LEN, buf, &mut len, ptr::null(), 0) };

    match ret {
        0 if len <= 256 => libc::ssize_t::try_from(len).expect("len is in the range of 0..=256"),
        -1 => -1,
        // Zero return result will be converted into `Error::UNEXPECTED` by `sys_fill_exact`
        _ => 0,
    }
}

type GetRandomFn = unsafe extern "C" fn(*mut c_void, libc::size_t, libc::c_uint) -> libc::ssize_t;

#[cold]
#[inline(never)]
fn init() -> NonNull<c_void> {
    let ptr = unsafe { libc::dlsym(libc::RTLD_DEFAULT, c"getrandom".as_ptr()) };
    if !cfg!(getrandom_test_netbsd_fallback) {
        if let Some(ptr) = NonNull::new(ptr) {
            return ptr;
        }
    }
    // Verify `polyfill_using_kern_arand` has the right signature.
    const POLYFILL: GetRandomFn = polyfill_using_kern_arand;
    unsafe { NonNull::new_unchecked(POLYFILL as *mut c_void) }
}

#[inline]
pub fn fill_inner(dest: &mut [MaybeUninit<u8>]) -> Result<(), Error> {
    #[path = "../utils/lazy_ptr.rs"]
    mod lazy;

    static GETRANDOM_FN: lazy::LazyPtr<c_void> = lazy::LazyPtr::new();

    let fptr = GETRANDOM_FN.unsync_init(init);
    let fptr = unsafe { mem::transmute::<*mut c_void, GetRandomFn>(fptr.as_ptr()) };
    utils::sys_fill_exact(dest, |buf| unsafe {
        fptr(buf.as_mut_ptr().cast::<c_void>(), buf.len(), 0)
    })
}
