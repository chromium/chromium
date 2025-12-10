#![no_std]

//! The `array-init` crate allows you to initialize arrays
//! with an initializer closure that will be called
//! once for each element until the array is filled.
//!
//! This way you do not need to default-fill an array
//! before running initializers. Rust currently only
//! lets you either specify all initializers at once,
//! individually (`[a(), b(), c(), ...]`), or specify
//! one initializer for a `Copy` type (`[a(); N]`),
//! which will be called once with the result copied over.
//!
//! Care is taken not to leak memory shall the initialization
//! fail.
//!
//! # Examples:
//! ```rust
//! # #![allow(unused)]
//! # extern crate array_init;
//! #
//! // Initialize an array of length 50 containing
//! // successive squares
//!
//! let arr: [u32; 50] = array_init::array_init(|i: usize| (i * i) as u32);
//!
//! // Initialize an array from an iterator
//! // producing an array of [1,2,3,4] repeated
//!
//! let four = [1,2,3,4];
//! let mut iter = four.iter().copied().cycle();
//! let arr: [u32; 50] = array_init::from_iter(iter).unwrap();
//!
//! // Closures can also mutate state. We guarantee that they will be called
//! // in order from lower to higher indices.
//!
//! let mut last = 1u64;
//! let mut secondlast = 0;
//! let fibonacci: [u64; 50] = array_init::array_init(|_| {
//!     let this = last + secondlast;
//!     secondlast = last;
//!     last = this;
//!     this
//! });
//! ```

use ::core::{
    mem::{self, MaybeUninit},
    ptr, slice,
};

#[inline]
/// Initialize an array given an initializer expression.
///
/// The initializer is given the index of the element. It is allowed
/// to mutate external state; we will always initialize the elements in order.
///
/// # Examples
///
/// ```rust
/// # #![allow(unused)]
/// # extern crate array_init;
/// #
/// // Initialize an array of length 50 containing
/// // successive squares
/// let arr: [usize; 50] = array_init::array_init(|i| i * i);
///
/// assert!(arr.iter().enumerate().all(|(i, &x)| x == i * i));
/// ```
pub fn array_init<F, T, const N: usize>(mut initializer: F) -> [T; N]
where
    F: FnMut(usize) -> T,
{
    enum Unreachable {}

    try_array_init(
        // monomorphise into an infallible version
        move |i| -> Result<T, Unreachable> { Ok(initializer(i)) },
    )
    .unwrap_or_else(
        // zero-cost unwrap
        |unreachable| match unreachable { /* ! */ },
    )
}

#[inline]
/// Initialize an array given an iterator
///
/// We will iterate until the array is full or the iterator is exhausted. Returns
/// `None` if the iterator is exhausted before we can fill the array.
///
///   - Once the array is full, extra elements from the iterator (if any)
///     won't be consumed.
///
/// # Examples
///
/// ```rust
/// # #![allow(unused)]
/// # extern crate array_init;
/// #
/// // Initialize an array from an iterator
/// // producing an array of [1,2,3,4] repeated
///
/// let four = [1,2,3,4];
/// let mut iter = four.iter().copied().cycle();
/// let arr: [u32; 10] = array_init::from_iter(iter).unwrap();
/// assert_eq!(arr, [1, 2, 3, 4, 1, 2, 3, 4, 1, 2]);
/// ```
pub fn from_iter<Iterable, T, const N: usize>(iterable: Iterable) -> Option<[T; N]>
where
    Iterable: IntoIterator<Item = T>,
{
    try_array_init_impl::<_, _, T, N, 1>({
        let mut iterator = iterable.into_iter();
        move |_| iterator.next().ok_or(())
    })
    .ok()
}

#[inline]
/// Initialize an array in reverse given an iterator
///
/// We will iterate until the array is full or the iterator is exhausted. Returns
/// `None` if the iterator is exhausted before we can fill the array.
///
///   - Once the array is full, extra elements from the iterator (if any)
///     won't be consumed.
///
/// # Examples
///
/// ```rust
/// # #![allow(unused)]
/// # extern crate array_init;
/// #
/// // Initialize an array from an iterator
/// // producing an array of [4,3,2,1] repeated, finishing with 1.
///
/// let four = [1,2,3,4];
/// let mut iter = four.iter().copied().cycle();
/// let arr: [u32; 10] = array_init::from_iter_reversed(iter).unwrap();
/// assert_eq!(arr, [2, 1, 4, 3, 2, 1, 4, 3, 2, 1]);
/// ```
pub fn from_iter_reversed<Iterable, T, const N: usize>(iterable: Iterable) -> Option<[T; N]>
where
    Iterable: IntoIterator<Item = T>,
{
    try_array_init_impl::<_, _, T, N, -1>({
        let mut iterator = iterable.into_iter();
        move |_| iterator.next().ok_or(())
    })
    .ok()
}

#[inline]
/// Initialize an array given an initializer expression that may fail.
///
/// The initializer is given the index (between 0 and `N - 1` included) of the element, and returns a `Result<T, Err>,`. It is allowed
/// to mutate external state; we will always initialize from lower to higher indices.
///
/// # Examples
///
/// ```rust
/// # #![allow(unused)]
/// # extern crate array_init;
/// #
/// #[derive(PartialEq,Eq,Debug)]
/// struct DivideByZero;
///
/// fn inv(i : usize) -> Result<f64,DivideByZero> {
///     if i == 0 {
///         Err(DivideByZero)
///     } else {
///         Ok(1./(i as f64))
///     }
/// }
///
/// // If the initializer does not fail, we get an initialized array
/// let arr: [f64; 3] = array_init::try_array_init(|i| inv(3-i)).unwrap();
/// assert_eq!(arr,[1./3., 1./2., 1./1.]);
///
/// // The initializer fails
/// let res : Result<[f64;4], DivideByZero> = array_init::try_array_init(|i| inv(3-i));
/// assert_eq!(res,Err(DivideByZero));
/// ```
pub fn try_array_init<Err, F, T, const N: usize>(initializer: F) -> Result<[T; N], Err>
where
    F: FnMut(usize) -> Result<T, Err>,
{
    try_array_init_impl::<Err, F, T, N, 1>(initializer)
}

#[inline]
/// Initialize an array given a source array and a mapping expression. The size of the source array
/// is the same as the size of the returned array.
///
/// The mapper is given an element from the source array and maps it to an element in the
/// destination.
///
/// # Examples
///
/// ```rust
/// # #![allow(unused)]
/// # extern crate array_init;
/// #
/// // Initialize an array of length 50 containing successive squares
/// let arr: [usize; 50] = array_init::array_init(|i| i * i);
///
/// // Map each usize element to a u64 element.
/// let u64_arr: [u64; 50] = array_init::map_array_init(&arr, |element| *element as u64);
///
/// assert!(u64_arr.iter().enumerate().all(|(i, &x)| x == (i * i) as u64));
/// ```
pub fn map_array_init<M, T, U, const N: usize>(source: &[U; N], mut mapper: M) -> [T; N]
where
    M: FnMut(&U) -> T,
{
    // # Safety
    //   - The array size is known at compile time so we are certain that both the source and
    //     desitination have the same size. If the two arrays are of the same size we know that a
    //     valid index for one would be a valid index for the other.
    array_init(|index| unsafe { mapper(source.get_unchecked(index)) })
}

#[inline]
fn try_array_init_impl<Err, F, T, const N: usize, const D: i8>(
    mut initializer: F,
) -> Result<[T; N], Err>
where
    F: FnMut(usize) -> Result<T, Err>,
{
    // The implementation differentiates two cases:
    //   A) `T` does not need to be dropped. Even if the initializer panics
    //      or returns `Err` we will not leak memory.
    //   B) `T` needs to be dropped. We must keep track of which elements have
    //      been initialized so far, and drop them if we encounter a panic or `Err` midway.
    if !mem::needs_drop::<T>() {
        let mut array: MaybeUninit<[T; N]> = MaybeUninit::uninit();
        // pointer to array = *mut [T; N] <-> *mut T = pointer to first element
        let mut ptr_i = array.as_mut_ptr() as *mut T;

        // # Safety
        //
        //   - for D > 0, we are within the array since we start from the
        //     beginning of the array, and we have `0 <= i < N`.
        //   - for D < 0, we start at the end of the array and go back one
        //     place before writing, going back N times in total, finishing
        //     at the start of the array.
        unsafe {
            if D < 0 {
                ptr_i = ptr_i.add(N);
            }
            for i in 0..N {
                let value_i = initializer(i)?;
                // We overwrite *ptr_i previously undefined value without reading or dropping it.
                if D < 0 {
                    ptr_i = ptr_i.sub(1);
                }
                ptr_i.write(value_i);
                if D > 0 {
                    ptr_i = ptr_i.add(1);
                }
            }
            Ok(array.assume_init())
        }
    } else {
        // else: `mem::needs_drop::<T>()`

        /// # Safety
        ///
        ///   - `base_ptr[.. initialized_count]` must be a slice of init elements...
        ///
        ///   - ... that must be sound to `ptr::drop_in_place` if/when
        ///     `UnsafeDropSliceGuard` is dropped: "symbolic ownership"
        struct UnsafeDropSliceGuard<Item> {
            base_ptr: *mut Item,
            initialized_count: usize,
        }

        impl<Item> Drop for UnsafeDropSliceGuard<Item> {
            fn drop(self: &'_ mut Self) {
                unsafe {
                    // # Safety
                    //
                    //   - the contract of the struct guarantees that this is sound
                    ptr::drop_in_place(slice::from_raw_parts_mut(
                        self.base_ptr,
                        self.initialized_count,
                    ));
                }
            }
        }

        //  If the `initializer(i)` call panics, `panic_guard` is dropped,
        //  dropping `array[.. initialized_count]` => no memory leak!
        //
        // # Safety
        //
        //  1. - For D > 0, by construction, array[.. initiliazed_count] only
        //       contains init elements, thus there is no risk of dropping
        //       uninit data;
        //     - For D < 0, by construction, array[N - initialized_count..] only
        //       contains init elements.
        //
        //  2. - for D > 0, we are within the array since we start from the
        //       beginning of the array, and we have `0 <= i < N`.
        //     - for D < 0, we start at the end of the array and go back one
        //       place before writing, going back N times in total, finishing
        //       at the start of the array.
        //
        unsafe {
            let mut array: MaybeUninit<[T; N]> = MaybeUninit::uninit();
            // pointer to array = *mut [T; N] <-> *mut T = pointer to first element
            let mut ptr_i = array.as_mut_ptr() as *mut T;
            if D < 0 {
                ptr_i = ptr_i.add(N);
            }
            let mut panic_guard = UnsafeDropSliceGuard {
                base_ptr: ptr_i,
                initialized_count: 0,
            };

            for i in 0..N {
                // Invariant: `i` elements have already been initialized
                panic_guard.initialized_count = i;
                // If this panics or fails, `panic_guard` is dropped, thus
                // dropping the elements in `base_ptr[.. i]` for D > 0 or
                // `base_ptr[N - i..]` for D < 0.
                let value_i = initializer(i)?;
                // this cannot panic
                // the previously uninit value is overwritten without being read or dropped
                if D < 0 {
                    ptr_i = ptr_i.sub(1);
                    panic_guard.base_ptr = ptr_i;
                }
                ptr_i.write(value_i);
                if D > 0 {
                    ptr_i = ptr_i.add(1);
                }
            }
            // From now on, the code can no longer `panic!`, let's take the
            // symbolic ownership back
            mem::forget(panic_guard);

            Ok(array.assume_init())
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn seq() {
        let seq: [usize; 5] = array_init(|i| i);
        assert_eq!(&[0, 1, 2, 3, 4], &seq);
    }

    #[test]
    fn array_from_iter() {
        let array = [0, 1, 2, 3, 4];
        let seq: [usize; 5] = from_iter(array.iter().copied()).unwrap();
        assert_eq!(array, seq,);
    }

    #[test]
    fn array_init_no_drop() {
        DropChecker::with(|drop_checker| {
            let result: Result<[_; 5], ()> = try_array_init(|i| {
                if i < 3 {
                    Ok(drop_checker.new_element())
                } else {
                    Err(())
                }
            });
            assert!(result.is_err());
        });
    }

    #[test]
    fn from_iter_no_drop() {
        DropChecker::with(|drop_checker| {
            let iterator = (0..3).map(|_| drop_checker.new_element());
            let result: Option<[_; 5]> = from_iter(iterator);
            assert!(result.is_none());
        });
    }

    #[test]
    fn from_iter_reversed_no_drop() {
        DropChecker::with(|drop_checker| {
            let iterator = (0..3).map(|_| drop_checker.new_element());
            let result: Option<[_; 5]> = from_iter_reversed(iterator);
            assert!(result.is_none());
        });
    }

    #[test]
    fn test_513_seq() {
        let seq: [usize; 513] = array_init(|i| i);
        assert_eq!(
            [
                0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
                44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
                65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85,
                86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104,
                105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120,
                121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136,
                137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152,
                153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168,
                169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184,
                185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200,
                201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216,
                217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232,
                233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248,
                249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264,
                265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280,
                281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296,
                297, 298, 299, 300, 301, 302, 303, 304, 305, 306, 307, 308, 309, 310, 311, 312,
                313, 314, 315, 316, 317, 318, 319, 320, 321, 322, 323, 324, 325, 326, 327, 328,
                329, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 343, 344,
                345, 346, 347, 348, 349, 350, 351, 352, 353, 354, 355, 356, 357, 358, 359, 360,
                361, 362, 363, 364, 365, 366, 367, 368, 369, 370, 371, 372, 373, 374, 375, 376,
                377, 378, 379, 380, 381, 382, 383, 384, 385, 386, 387, 388, 389, 390, 391, 392,
                393, 394, 395, 396, 397, 398, 399, 400, 401, 402, 403, 404, 405, 406, 407, 408,
                409, 410, 411, 412, 413, 414, 415, 416, 417, 418, 419, 420, 421, 422, 423, 424,
                425, 426, 427, 428, 429, 430, 431, 432, 433, 434, 435, 436, 437, 438, 439, 440,
                441, 442, 443, 444, 445, 446, 447, 448, 449, 450, 451, 452, 453, 454, 455, 456,
                457, 458, 459, 460, 461, 462, 463, 464, 465, 466, 467, 468, 469, 470, 471, 472,
                473, 474, 475, 476, 477, 478, 479, 480, 481, 482, 483, 484, 485, 486, 487, 488,
                489, 490, 491, 492, 493, 494, 495, 496, 497, 498, 499, 500, 501, 502, 503, 504,
                505, 506, 507, 508, 509, 510, 511, 512
            ],
            seq
        );
    }

    use self::drop_checker::DropChecker;
    mod drop_checker {
        use ::core::cell::Cell;

        pub(super) struct DropChecker {
            slots: [Cell<bool>; 512],
            next_uninit_slot: Cell<usize>,
        }

        pub(super) struct Element<'drop_checker> {
            slot: &'drop_checker Cell<bool>,
        }

        impl Drop for Element<'_> {
            fn drop(self: &'_ mut Self) {
                assert!(self.slot.replace(false), "Double free!");
            }
        }

        impl DropChecker {
            pub(super) fn with(f: impl FnOnce(&Self)) {
                let drop_checker = Self::new();
                f(&drop_checker);
                drop_checker.assert_no_leaks();
            }

            pub(super) fn new_element(self: &'_ Self) -> Element<'_> {
                let i = self.next_uninit_slot.get();
                self.next_uninit_slot.set(i + 1);
                self.slots[i].set(true);
                Element {
                    slot: &self.slots[i],
                }
            }

            fn new() -> Self {
                Self {
                    slots: crate::array_init(|_| Cell::new(false)),
                    next_uninit_slot: Cell::new(0),
                }
            }

            fn assert_no_leaks(self: Self) {
                let leak_count: usize = self.slots[..self.next_uninit_slot.get()]
                    .iter()
                    .map(|slot| usize::from(slot.get() as u8))
                    .sum();
                assert_eq!(leak_count, 0, "{} elements leaked", leak_count);
            }
        }
    }
}
