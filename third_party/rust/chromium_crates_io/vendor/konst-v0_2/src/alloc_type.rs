//! Generic constants for types from the [`alloc`] crate, including `String` and `Vec`.
//!
//! [`alloc`]: https://doc.rust-lang.org/alloc/
//!
use alloc::{borrow::Cow, string::String, vec::Vec};

/// An empty `Cow<'_, str>`. Usable to construct a `[Cow<'_, str>; N]`.
///
/// As of Rust 1.51.0, `[Cow::Borrowed(""); LEN]` is not valid,
/// because `Cow<'_, str>` isn't copy,
/// but `[COW_STR_NEW; LEN]` does work, like in the example below.
///
/// # Example
///
/// ```rust
/// use konst::alloc_type::COW_STR_NEW;
///
/// use std::borrow::Cow;
///
/// const SIX_COWS: [Cow<'_, str>; 6] = [COW_STR_NEW; 6];
///
/// let mut cows = SIX_COWS;
///
/// ('a'..='f')
///     .enumerate()
///     .filter(|(i, _)| i % 2 == 0 )
///     .for_each(|(i, c)|{
///         cows[i].to_mut().push(c)
///     });
///
/// assert_eq!(cows, ["a", "", "c", "", "e", ""])
///
/// ```
///
pub const COW_STR_NEW: Cow<'_, str> = Cow::Borrowed("");

/// An empty `String`. Usable to construct a `[String; N]`.
///
/// As of Rust 1.51.0, `[String::new(); LEN]` is not valid, because `String` isn't copy,
/// but `[STRING_NEW; LEN]` does work, like in the example below.
///
/// # Example
///
/// ```rust
/// use konst::alloc_type::STRING_NEW;
///
/// const STRINGS: [String; 3] = [STRING_NEW; 3];
///
/// let mut strings = STRINGS;
///
/// strings[0].push_str("foo");
/// strings[1].push_str("bar");
/// strings[2].push_str("baz");
///
/// assert_eq!(strings, ["foo", "bar", "baz"]);
///
///
/// ```
///
pub const STRING_NEW: String = String::new();

declare_generic_const! {
    /// An empty `Cow<'_, [T]>`. Usable to construct a `[Cow<'_, [T]>; N]`.
    ///
    /// As of Rust 1.51.0, `[Cow::Borrowed(&[][..]); LEN]` is not valid,
    /// because `Cow<'_, [T]>` isn't copy,
    /// but `[CONST; LEN]` does work, like in the example below.
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::alloc_type::COW_SLICE_NEW;
    ///
    /// use std::borrow::Cow;
    ///
    /// const SLICES: [Cow<'_, [u64]>; 6] = [COW_SLICE_NEW::<u64>::V; 6];
    ///
    /// let mut cows = SLICES;
    ///
    /// [3, 5, 8, 13, 21, 34].iter().copied()
    ///     .enumerate()
    ///     .filter(|(i, _)| i % 2 != 0 )
    ///     .for_each(|(i, v)|{
    ///         cows[i].to_mut().push(v)
    ///     });
    ///
    /// assert_eq!(cows, [&[][..], &[5], &[], &[13], &[], &[34]])
    ///
    /// ```
    ///
    for['a, T: Clone + 'a]
    pub const COW_SLICE_NEW['a, T]: Cow<'a, [T]> = Cow::Borrowed(&[]);
}

declare_generic_const! {
    /// An empty `Vec<T>`. Usable to construct a `[Vec<T>; N]`.
    ///
    /// As of Rust 1.51.0, `[Vec::new(); LEN]` is not valid,
    /// because `Vec<T>` isn't copy,
    /// but `[CONST; LEN]` does work, like in the example below.
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::alloc_type::VEC_NEW;
    ///
    /// use std::borrow::Cow;
    ///
    /// const VECS: [Vec<u64>; 3] = [VEC_NEW::<u64>::V; 3];
    ///
    /// let mut vecs = VECS;
    ///
    /// vecs[0].extend_from_slice(&[3]);
    /// vecs[1].extend_from_slice(&[5, 8]);
    /// vecs[2].extend_from_slice(&[13, 21, 34]);
    ///
    /// assert_eq!(vecs, [&[3][..], &[5, 8], &[13, 21, 34]]);
    ///
    /// ```
    ///
    for[T]
    pub const VEC_NEW[T]: Vec<T> = Vec::new();
}
