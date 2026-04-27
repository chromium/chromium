//! Miscelaneous functions.

use core::ops::Range;

/// Newtype wrapper to get around limitations in `const fn`s
pub(crate) struct Constructor<T>(#[allow(dead_code)] fn() -> T);

pub use crate::slice_cmp::{str_eq, u8_slice_eq};

#[doc(hidden)]
#[inline]
pub const fn saturate_range(s: &[u8], range: &Range<usize>) -> Range<usize> {
    let len = s.len();
    let end = min_usize(range.end, len);
    min_usize(range.start, end)..end
}

#[doc(hidden)]
#[repr(C)]
pub union Dereference<'a, T: ?Sized> {
    pub ptr: *const T,
    pub reff: &'a T,
}

////////////////////////////////////////////////////////////////////////////////

/// The same as [`slice_up_to_len`]
///
/// (this function only exists for backwards compatibility)
#[deprecated(since = "0.2.36", note = "redundant, same as `slice_up_to_len`")]
#[inline(always)]
pub const fn slice_up_to_len_alt<T>(slice: &[T], len: usize) -> &[T] {
    slice_up_to_len(slice, len)
}

////////////////////////////////////////////////////////////////////////////////

/// A const equivalent of `&slice[..len]`.
///
/// If `slice.len() < len`, this simply returns `slice` back.
///
/// # Example
///
/// ```rust
/// use const_format::utils::slice_up_to_len;
///
/// const FIBB: &[u16] = &[3, 5, 8, 13, 21, 34, 55, 89];
///
/// const TWO: &[u16] = slice_up_to_len(FIBB, 2);
/// const FOUR: &[u16] = slice_up_to_len(FIBB, 4);
/// const ALL: &[u16] = slice_up_to_len(FIBB, usize::MAX);
///
/// assert_eq!(TWO, &[3, 5]);
/// assert_eq!(FOUR, &[3, 5, 8, 13]);
/// assert_eq!(FIBB, ALL);
///
/// ```
#[inline(always)]
pub const fn slice_up_to_len<T>(slice: &[T], len: usize) -> &[T] {
    if len > slice.len() {
        return slice;
    }

    slice.split_at(len).0
}

////////////////////////////////////////////////////////////////////////////////

pub(crate) const fn min_usize(l: usize, r: usize) -> usize {
    if l < r {
        l
    } else {
        r
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_slice_up_to_len_alt() {
        let mut list = [0u16; 256];

        (100..).zip(list.iter_mut()).for_each(|(i, m)| *m = i);

        for i in 0..list.len() {
            assert_eq!(slice_up_to_len_alt(&list, i), &list[..i]);
        }
    }

    #[test]
    fn slice_in_bounds() {
        assert_eq!(slice_up_to_len(&[3, 5], 0), []);
        assert_eq!(slice_up_to_len(&[3, 5], 1), [3]);
        assert_eq!(slice_up_to_len(&[3, 5], 2), [3, 5]);
        assert_eq!(slice_up_to_len(&[3, 5], 3), [3, 5]);
        assert_eq!(slice_up_to_len(&[3, 5], 4), [3, 5]);
    }
}
