#![allow(unused_unsafe)]

//! Contains implementations for rust core that have not been stabilized
//!
//! Functions in this are expected to be properly peer reviewed by the community
//!
//! Any modifications done are purely to make the code compatible with bincode

use core::mem::{self, MaybeUninit};

/// Pulls `N` items from `iter` and returns them as an array. If the iterator
/// yields fewer than `N` items, `None` is returned and all already yielded
/// items are dropped.
///
/// Since the iterator is passed as a mutable reference and this function calls
/// `next` at most `N` times, the iterator can still be used afterwards to
/// retrieve the remaining items.
///
/// If `iter.next()` panicks, all items already yielded by the iterator are
/// dropped.
#[allow(clippy::while_let_on_iterator)]
pub fn collect_into_array<E, I, T, const N: usize>(iter: &mut I) -> Option<Result<[T; N], E>>
where
    I: Iterator<Item = Result<T, E>>,
{
    if N == 0 {
        // SAFETY: An empty array is always inhabited and has no validity invariants.
        return unsafe { Some(Ok(mem::zeroed())) };
    }

    struct Guard<'a, T, const N: usize> {
        array_mut: &'a mut [MaybeUninit<T>; N],
        initialized: usize,
    }

    impl<T, const N: usize> Drop for Guard<'_, T, N> {
        fn drop(&mut self) {
            debug_assert!(self.initialized <= N);

            // SAFETY: this slice will contain only initialized objects.
            unsafe {
                core::ptr::drop_in_place(slice_assume_init_mut(
                    self.array_mut.get_unchecked_mut(..self.initialized),
                ));
            }
        }
    }

    let mut array = uninit_array::<T, N>();
    let mut guard = Guard {
        array_mut: &mut array,
        initialized: 0,
    };

    while let Some(item_rslt) = iter.next() {
        let item = match item_rslt {
            Err(err) => {
                return Some(Err(err));
            }
            Ok(elem) => elem,
        };

        // SAFETY: `guard.initialized` starts at 0, is increased by one in the
        // loop and the loop is aborted once it reaches N (which is
        // `array.len()`).
        unsafe {
            guard
                .array_mut
                .get_unchecked_mut(guard.initialized)
                .write(item);
        }
        guard.initialized += 1;

        // Check if the whole array was initialized.
        if guard.initialized == N {
            mem::forget(guard);

            // SAFETY: the condition above asserts that all elements are
            // initialized.
            let out = unsafe { array_assume_init(array) };
            return Some(Ok(out));
        }
    }

    // This is only reached if the iterator is exhausted before
    // `guard.initialized` reaches `N`. Also note that `guard` is dropped here,
    // dropping all already initialized elements.
    None
}

/// Assuming all the elements are initialized, get a mutable slice to them.
///
/// # Safety
///
/// It is up to the caller to guarantee that the `MaybeUninit<T>` elements
/// really are in an initialized state.
/// Calling this when the content is not yet fully initialized causes undefined behavior.
///
/// See [`assume_init_mut`] for more details and examples.
///
/// [`assume_init_mut`]: MaybeUninit::assume_init_mut
// #[unstable(feature = "maybe_uninit_slice", issue = "63569")]
// #[rustc_const_unstable(feature = "const_maybe_uninit_assume_init", issue = "none")]
#[inline(always)]
pub unsafe fn slice_assume_init_mut<T>(slice: &mut [MaybeUninit<T>]) -> &mut [T] {
    // SAFETY: similar to safety notes for `slice_get_ref`, but we have a
    // mutable reference which is also guaranteed to be valid for writes.
    unsafe { &mut *(slice as *mut [MaybeUninit<T>] as *mut [T]) }
}

/// Create a new array of `MaybeUninit<T>` items, in an uninitialized state.
///
/// Note: in a future Rust version this method may become unnecessary
/// when Rust allows
/// [inline const expressions](https://github.com/rust-lang/rust/issues/76001).
/// The example below could then use `let mut buf = [const { MaybeUninit::<u8>::uninit() }; 32];`.
///
/// # Examples
///
/// ```ignore
/// #![feature(maybe_uninit_uninit_array, maybe_uninit_extra, maybe_uninit_slice)]
///
/// use std::mem::MaybeUninit;
///
/// extern "C" {
///     fn read_into_buffer(ptr: *mut u8, max_len: usize) -> usize;
/// }
///
/// /// Returns a (possibly smaller) slice of data that was actually read
/// fn read(buf: &mut [MaybeUninit<u8>]) -> &[u8] {
///     unsafe {
///         let len = read_into_buffer(buf.as_mut_ptr() as *mut u8, buf.len());
///         MaybeUninit::slice_assume_init_ref(&buf[..len])
///     }
/// }
///
/// let mut buf: [MaybeUninit<u8>; 32] = MaybeUninit::uninit_array();
/// let data = read(&mut buf);
/// ```
// #[unstable(feature = "maybe_uninit_uninit_array", issue = "none")]
// #[rustc_const_unstable(feature = "maybe_uninit_uninit_array", issue = "none")]
#[inline(always)]
fn uninit_array<T, const LEN: usize>() -> [MaybeUninit<T>; LEN] {
    // SAFETY: An uninitialized `[MaybeUninit<_>; LEN]` is valid.
    unsafe { MaybeUninit::<[MaybeUninit<T>; LEN]>::uninit().assume_init() }
}

/// Extracts the values from an array of `MaybeUninit` containers.
///
/// # Safety
///
/// It is up to the caller to guarantee that all elements of the array are
/// in an initialized state.
///
/// # Examples
///
/// ```ignore
/// #![feature(maybe_uninit_uninit_array)]
/// #![feature(maybe_uninit_array_assume_init)]
/// use std::mem::MaybeUninit;
///
/// let mut array: [MaybeUninit<i32>; 3] = MaybeUninit::uninit_array();
/// array[0].write(0);
/// array[1].write(1);
/// array[2].write(2);
///
/// // SAFETY: Now safe as we initialised all elements
/// let array = unsafe {
///     MaybeUninit::array_assume_init(array)
/// };
///
/// assert_eq!(array, [0, 1, 2]);
/// ```
// #[unstable(feature = "maybe_uninit_array_assume_init", issue = "80908")]
#[inline(always)]
pub unsafe fn array_assume_init<T, const N: usize>(array: [MaybeUninit<T>; N]) -> [T; N] {
    // SAFETY:
    // * The caller guarantees that all elements of the array are initialized
    // * `MaybeUninit<T>` and T are guaranteed to have the same layout
    // * `MaybeUninit` does not drop, so there are no double-frees
    // And thus the conversion is safe
    unsafe {
        // intrinsics::assert_inhabited::<[T; N]>();
        (&array as *const _ as *const [T; N]).read()
    }
}
