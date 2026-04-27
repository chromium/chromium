//! Const equivalents of array functions.

/// Const equivalent of
/// [`array::map`](https://doc.rust-lang.org/std/primitive.array.html#method.map).
///
/// **Limitation:** requires `$array` and the elements
/// returned by the passed-in function to be `Copy`.
///
/// # Example
///
/// ```rust
/// use konst::array;
///
/// const TRIMMED: [&str; 3] = array::map!(["  foo", "bar  ", "  baz  "], konst::string::trim);
/// assert_eq!(TRIMMED, ["foo", "bar", "baz"]);
///
/// const LENGTHS: [usize; 3] = array::map!(["foo", "hello", "bar baz"], |s| s.len());
/// assert_eq!(LENGTHS, [3, 5, 7]);
///
/// const SQUARED: [u32; 6] = array::map!([1, 2, 3, 4, 5, 6], |x: u32| x.pow(2));
/// assert_eq!(SQUARED, [1, 4, 9, 16, 25, 36]);
///
/// ```
///
#[cfg(feature = "rust_1_56")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_56")))]
pub use konst_macro_rules::array_map as map;
