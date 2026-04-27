//! Implementation for Linux / Android with `/dev/urandom` fallback
use super::use_file;
use crate::Error;
use core::{
    ffi::c_void,
    mem::{MaybeUninit, transmute},
    ptr::{self, NonNull},
};
use use_file::utils;

pub use crate::util::{inner_u32, inner_u64};

type GetRandomFn = unsafe extern "C" fn(*mut c_void, libc::size_t, libc::c_uint) -> libc::ssize_t;

/// Sentinel value which indicates that `libc::getrandom` either not available,
/// or not supported by kernel.
const NOT_AVAILABLE: NonNull<c_void> = unsafe { NonNull::new_unchecked(usize::MAX as *mut c_void) };

#[cold]
#[inline(never)]
fn init() -> NonNull<c_void> {
    // Use static linking to `libc::getrandom` on MUSL targets and `dlsym` everywhere else
    #[cfg(not(target_env = "musl"))]
    let raw_ptr = unsafe { libc::dlsym(libc::RTLD_DEFAULT, c"getrandom".as_ptr()) };
    #[cfg(target_env = "musl")]
    let raw_ptr = {
        let fptr: GetRandomFn = libc::getrandom;
        unsafe { transmute::<GetRandomFn, *mut c_void>(fptr) }
    };

    let res_ptr = match NonNull::new(raw_ptr) {
        Some(fptr) => {
            let getrandom_fn = unsafe { transmute::<*mut c_void, GetRandomFn>(fptr.as_ptr()) };
            // Check that `getrandom` syscall is supported by kernel
            let res = unsafe { getrandom_fn(ptr::dangling_mut(), 0, 0) };
            if cfg!(getrandom_test_linux_fallback) {
                NOT_AVAILABLE
            } else if res.is_negative() {
                match utils::get_errno() {
                    libc::ENOSYS => NOT_AVAILABLE, // No kernel support
                    // The fallback on EPERM is intentionally not done on Android since this workaround
                    // seems to be needed only for specific Linux-based products that aren't based
                    // on Android. See https://github.com/rust-random/getrandom/issues/229.
                    #[cfg(target_os = "linux")]
                    libc::EPERM => NOT_AVAILABLE, // Blocked by seccomp
                    _ => fptr,
                }
            } else {
                fptr
            }
        }
        None => NOT_AVAILABLE,
    };

    #[cfg(getrandom_test_linux_without_fallback)]
    if res_ptr == NOT_AVAILABLE {
        panic!("Fallback is triggered with enabled `getrandom_test_linux_without_fallback`")
    }

    res_ptr
}

// Prevent inlining of the fallback implementation
#[inline(never)]
fn use_file_fallback(dest: &mut [MaybeUninit<u8>]) -> Result<(), Error> {
    use_file::fill_inner(dest)
}

#[inline]
pub fn fill_inner(dest: &mut [MaybeUninit<u8>]) -> Result<(), Error> {
    #[path = "../utils/lazy_ptr.rs"]
    mod lazy;

    static GETRANDOM_FN: lazy::LazyPtr<c_void> = lazy::LazyPtr::new();
    let fptr = GETRANDOM_FN.unsync_init(init);

    if fptr == NOT_AVAILABLE {
        use_file_fallback(dest)
    } else {
        // note: `transmute` is currently the only way to convert a pointer into a function reference
        let getrandom_fn = unsafe { transmute::<*mut c_void, GetRandomFn>(fptr.as_ptr()) };
        utils::sys_fill_exact(dest, |buf| unsafe {
            getrandom_fn(buf.as_mut_ptr().cast(), buf.len(), 0)
        })
    }
}
