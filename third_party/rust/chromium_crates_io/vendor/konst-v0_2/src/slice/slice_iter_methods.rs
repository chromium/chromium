use crate::{
    iter::{IntoIterKind, IsIteratorKind},
    option, slice,
};

use konst_macro_rules::iterator_shared;

/// Gets a const iterator over `slice`, const equivalent of
/// [`<[T]>::iter`
/// ](https://doc.rust-lang.org/std/primitive.slice.html#method.iter)
///
/// # Example
///
/// ### Normal
///
/// ```rust
/// use konst::iter::for_each;
/// use konst::slice;
///
/// const ARR: &[usize] = &{
///     let mut arr = [0usize; 3];
///     // the `slice::iter` call here is unnecessary,
///     // you can pass a slice reference to `for_each*`
///     for_each!{(i, elem) in slice::iter(&["foo", "hello", "That box"]), enumerate() =>
///         arr[i] = elem.len();
///     }
///     arr
/// };
///
/// assert_eq!(ARR, [3, 5, 8]);
///
/// ```
///
/// ### Reversed
///
/// ```rust
/// use konst::iter::for_each;
/// use konst::slice;
///
/// const ARR: &[usize] = &{
///     let mut arr = [0usize; 3];
///     for_each!{(i, elem) in slice::iter(&["foo", "hello", "That box"]).rev(),enumerate() =>
///         arr[i] = elem.len();
///     }
///     arr
/// };
///
/// assert_eq!(ARR, [8, 5, 3]);
///
/// ```
pub use konst_macro_rules::into_iter::slice_into_iter::iter;

/// Const equivalent of [`core::slice::Iter`].
///
/// This is constructed in either of these ways:
/// ```rust
/// # let a_slice = &[3];
/// # let _ = (
/// konst::slice::iter(a_slice)
/// # ,
/// konst::iter::into_iter!(a_slice)
/// # );
/// ```
pub use konst_macro_rules::into_iter::slice_into_iter::Iter;

/// Const equivalent of `core::iter::Rev<core::slice::Iter<_>>`
///
/// This is constructed in either of these ways:
/// ```rust
/// # let a_slice = &[3];
/// # let _ = (
/// konst::slice::iter(a_slice).rev()
/// # ,
/// konst::iter::into_iter!(a_slice).rev()
/// # );
/// ```
pub use konst_macro_rules::into_iter::slice_into_iter::IterRev;

/// A const equivalent of `slice.iter().copied()`
///
/// # Version compatibility
///
/// This requires the `"rust_1_61"` feature.
///
/// # Example
///
/// ```rust
/// use konst::{iter, slice};
///
/// const fn find_even(slice: &[u32]) -> Option<u32> {
///     iter::eval!(slice::iter_copied(slice),find(|elem| *elem % 2 == 0))
/// }
///
/// assert_eq!(find_even(&[]), None);
/// assert_eq!(find_even(&[1]), None);
/// assert_eq!(find_even(&[1, 2]), Some(2));
/// assert_eq!(find_even(&[5, 4, 3, 2, 1]), Some(4));
///
/// ```
///
#[cfg(feature = "rust_1_61")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_61")))]
pub use konst_macro_rules::into_iter::slice_into_iter::iter_copied;

/// A const equivalent of `iter::Copied<slice::Iter<'a, T>>`.
///
/// This const iterator can be created with [`iter_copied`].
///
/// # Version compatibility
///
/// This requires the `"rust_1_61"` feature.
///
#[cfg(feature = "rust_1_61")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_61")))]
pub use konst_macro_rules::into_iter::slice_into_iter::IterCopied;

/// A const equivalent of `iter::Rev<iter::Copied<slice::Iter<'a, T>>>`
///
/// This const iterator can be created with
/// ```rust
/// # let slice = &[3, 5, 8];
/// # let _: konst::slice::IterCopiedRev<'_, u32> =
/// konst::slice::iter_copied(slice).rev()
/// # ;
/// ```
///
/// # Version compatibility
///
/// This requires the `"rust_1_61"` feature.
///
/// # Example
///
/// ```rust
/// use konst::iter;
/// use konst::slice::{self, IterCopiedRev};
///
/// const fn rfind_even(slice: &[u32]) -> Option<u32> {
///     let iter: IterCopiedRev<'_, u32> = slice::iter_copied(slice).rev();
///     iter::eval!(iter,find(|&elem| elem % 2 == 0))
/// }
///
/// assert_eq!(rfind_even(&[]), None);
/// assert_eq!(rfind_even(&[1]), None);
/// assert_eq!(rfind_even(&[1, 2]), Some(2));
/// assert_eq!(rfind_even(&[1, 2, 3, 4, 5]), Some(4));
///
/// ```
///
#[cfg(feature = "rust_1_61")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_61")))]
pub use konst_macro_rules::into_iter::slice_into_iter::IterCopiedRev;

///////////////////////////////////////////////////////////////////////////////

#[cfg(feature = "rust_1_64")]
mod requires_rust_1_64 {
    use super::*;

    #[inline(always)]
    pub(crate) const fn some_if_nonempty<T>(slice: &[T]) -> Option<&[T]> {
        if let [] = slice {
            None
        } else {
            Some(slice)
        }
    }

    /// Const equivalent of
    /// [`<[T]>::windows`
    /// ](https://doc.rust-lang.org/std/primitive.slice.html#method.windows)
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::{iter, slice};
    ///
    /// const fn is_sorted(slice: &[u8]) -> bool {
    ///     iter::eval!(slice::windows(slice, 2),all(|w| w[1] > w[0]))
    /// }
    ///
    /// assert!(is_sorted(&[3, 5, 8]));
    /// assert!(!is_sorted(&[8, 13, 0]));
    ///
    ///
    ///
    /// ```
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
    #[track_caller]
    pub const fn windows<T>(slice: &[T], size: usize) -> Windows<'_, T> {
        assert!(size != 0, "window size must be non-zero");

        Windows { slice, size }
    }

    macro_rules! windows_shared {
        (is_forward = $is_forward:ident) => {
            iterator_shared! {
                is_forward = $is_forward,
                item = &'a [T],
                iter_forward = Windows<'a, T>,
                iter_reversed = WindowsRev<'a, T>,
                next(self){
                    if self.slice.len() < self.size {
                        None
                    } else {
                        let up_to = slice::slice_up_to(self.slice, self.size);
                        self.slice = slice::slice_from(self.slice, 1);
                        Some((up_to, self))
                    }
                },
                next_back {
                    let len = self.slice.len();
                    if len < self.size {
                        None
                    } else {
                        let up_to = slice::slice_from(self.slice, len - self.size);
                        self.slice = slice::slice_up_to(self.slice, len - 1);
                        Some((up_to, self))
                    }
                },
                fields = {slice, size},
            }
        };
    }

    /// Const equivalent of [`core::slice::Windows`]
    ///
    /// This is constructed with [`windows`] like this:
    /// ```rust
    /// # let slice = &[3];
    /// # let _ =
    /// konst::slice::windows(slice, 1)
    /// # ;
    /// ```
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
    pub struct Windows<'a, T> {
        slice: &'a [T],
        size: usize,
    }
    impl<T> IntoIterKind for Windows<'_, T> {
        type Kind = IsIteratorKind;
    }

    /// Const equivalent of `core::iter::Rev<core::slice::Windows>`
    ///
    /// This is constructed with [`windows`] like this:
    /// ```rust
    /// # let slice = &[3];
    /// # let _ =
    /// konst::slice::windows(slice, 1).rev()
    /// # ;
    /// ```
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
    pub struct WindowsRev<'a, T> {
        slice: &'a [T],
        size: usize,
    }
    impl<T> IntoIterKind for WindowsRev<'_, T> {
        type Kind = IsIteratorKind;
    }

    impl<'a, T> Windows<'a, T> {
        windows_shared! {is_forward = true}
    }

    impl<'a, T> WindowsRev<'a, T> {
        windows_shared! {is_forward = false}
    }

    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    /// Const equivalent of
    /// [`<[T]>::chunks`](https://doc.rust-lang.org/std/primitive.slice.html#method.chunks)
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::iter::for_each;
    /// use konst::slice;
    ///
    /// const CHUNKS: &[&[u8]] = &{
    ///     let mut out = [&[] as &[u8]; 3] ;
    ///     let fibb = &[3, 5, 8, 13, 21, 34, 55, 89];
    ///     for_each!{(i, chunk) in slice::chunks(fibb, 3),enumerate() =>
    ///         out[i] = chunk;
    ///     }
    ///     out
    /// };
    ///
    /// let expected: &[&[u8]] = &[&[3, 5, 8], &[13, 21, 34], &[55, 89]];
    ///
    /// assert_eq!(CHUNKS, expected)
    ///
    /// ```
    ///
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
    #[track_caller]
    pub const fn chunks<T>(slice: &[T], size: usize) -> Chunks<'_, T> {
        assert!(size != 0, "chunk size must be non-zero");

        Chunks {
            slice: some_if_nonempty(slice),
            size,
        }
    }

    macro_rules! chunks_shared {
        (is_forward = $is_forward:ident) => {
            iterator_shared! {
                is_forward = $is_forward,
                item = &'a [T],
                iter_forward = Chunks<'a, T>,
                iter_reversed = ChunksRev<'a, T>,
                next(self) {
                    option::map!(self.slice, |slice| {
                        let (ret, next) = slice::split_at(slice, self.size);
                        self.slice = some_if_nonempty(next);
                        (ret, self)
                    })
                },
                next_back{
                    option::map!(self.slice, |slice| {
                        let at = (slice.len() - 1) / self.size * self.size;
                        let (next, ret) = slice::split_at(slice, at);
                        self.slice = some_if_nonempty(next);
                        (ret, self)
                    })
                },
                fields = {slice, size},
            }
        };
    }

    /// Const equivalent of [`core::slice::Chunks`]
    ///
    /// This is constructed with [`chunks`] like this:
    /// ```rust
    /// # let slice = &[3];
    /// # let _ =
    /// konst::slice::chunks(slice, 1)
    /// # ;
    /// ```
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
    pub struct Chunks<'a, T> {
        slice: Option<&'a [T]>,
        size: usize,
    }
    impl<T> IntoIterKind for Chunks<'_, T> {
        type Kind = IsIteratorKind;
    }

    /// Const equivalent of `core::iter::Rev<core::slice::Chunks>`
    ///
    /// This is constructed with [`chunks`] like this:
    /// ```rust
    /// # let slice = &[3];
    /// # let _ =
    /// konst::slice::chunks(slice, 1).rev()
    /// # ;
    /// ```
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
    pub struct ChunksRev<'a, T> {
        slice: Option<&'a [T]>,
        size: usize,
    }
    impl<T> IntoIterKind for ChunksRev<'_, T> {
        type Kind = IsIteratorKind;
    }

    impl<'a, T> Chunks<'a, T> {
        chunks_shared! {is_forward = true}
    }

    impl<'a, T> ChunksRev<'a, T> {
        chunks_shared! {is_forward = false}
    }

    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    /// Const equivalent of
    /// [`<[T]>::chunks_exact`
    /// ](https://doc.rust-lang.org/std/primitive.slice.html#method.chunks_exact)
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::{for_range, option, slice};
    ///
    /// const FOUND: [&[u8]; 3] = {
    ///     let iter = slice::chunks_exact(&[3, 5, 8, 13, 21, 34, 55, 89], 3);
    ///     let (elem0, iter) = option::unwrap!(iter.next());
    ///     let (elem1, iter) = option::unwrap!(iter.next());
    ///     [elem0, elem1, iter.remainder()]
    /// };
    ///
    /// let expected: [&[u8]; 3] = [&[3u8, 5, 8], &[13, 21, 34], &[55, 89]];
    ///
    /// assert_eq!(FOUND, expected);
    ///
    /// ```
    ///
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
    #[track_caller]
    pub const fn chunks_exact<T>(slice: &[T], size: usize) -> ChunksExact<'_, T> {
        assert!(size != 0, "chunk size must be non-zero");

        ChunksExact { slice, size }
    }

    macro_rules! chunks_exact_shared {
        (is_forward = $is_forward:ident) => {
            iterator_shared! {
                is_forward = $is_forward,
                item = &'a [T],
                iter_forward = ChunksExact<'a, T>,
                iter_reversed = ChunksExactRev<'a, T>,
                next(self) {
                    if self.slice.len() < self.size {
                        None
                    } else {
                        let (ret, next) = slice::split_at(self.slice, self.size);
                        self.slice = next;
                        Some((ret, self))
                    }
                },
                next_back {
                    if let Some(mut at) = self.slice.len().checked_sub(self.size) {
                        at = at / self.size * self.size;
                        let (next, ret) = slice::split_at(self.slice, at);
                        self.slice = next;
                        Some((slice::slice_up_to(ret, self.size), self))
                    } else {
                        None
                    }
                },
                fields = {slice, size},
            }

            /// Returns the remainder of the slice that not returned by [`next`](Self::next),
            /// because it is shorter than the chunk size.
            pub const fn remainder(&self) -> &'a [T] {
                self.slice
            }
        };
    }

    /// Const equivalent of [`core::slice::ChunksExact`]
    ///
    /// This is constructed with [`chunks_exact`] like this:
    /// ```rust
    /// # let slice = &[3];
    /// # let _ =
    /// konst::slice::chunks_exact(slice, 1)
    /// # ;
    /// ```
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
    pub struct ChunksExact<'a, T> {
        slice: &'a [T],
        size: usize,
    }
    impl<T> IntoIterKind for ChunksExact<'_, T> {
        type Kind = IsIteratorKind;
    }

    /// Const equivalent of `core::iter::Rev<core::slice::ChunksExact>`
    ///
    /// This is constructed with [`chunks_exact`] like this:
    /// ```rust
    /// # let slice = &[3];
    /// # let _ =
    /// konst::slice::chunks_exact(slice, 1).rev()
    /// # ;
    /// ```
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
    pub struct ChunksExactRev<'a, T> {
        slice: &'a [T],
        size: usize,
    }
    impl<T> IntoIterKind for ChunksExactRev<'_, T> {
        type Kind = IsIteratorKind;
    }

    impl<'a, T> ChunksExact<'a, T> {
        chunks_exact_shared! {is_forward = true}
    }

    impl<'a, T> ChunksExactRev<'a, T> {
        chunks_exact_shared! {is_forward = false}
    }
}

#[cfg(feature = "rust_1_64")]
pub use requires_rust_1_64::*;
