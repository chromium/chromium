use crate::PanicFmt;

/// A wrapper type used to define methods for std types.
///
/// Std types are coerced to this type through the approach used in the
/// [`coerce_fmt`] macro.
///
/// # Example
///
/// Formatting std types with this type's `to_panicvals` methods,
/// without using macros.
///
#[cfg_attr(feature = "non_basic", doc = "```rust")]
#[cfg_attr(not(feature = "non_basic"), doc = "```ignore")]
/// use const_panic::{ArrayString, FmtArg, StdWrapper};
///
/// assert_eq!(
///     ArrayString::<99>::from_panicvals(
///         &StdWrapper("hello").to_panicvals(FmtArg::DEBUG)
///     ).unwrap(),
///     r#""hello""#
/// );
///
/// assert_eq!(
///     ArrayString::<99>::from_panicvals(
///         &StdWrapper(&[3u8, 5, 8]).to_panicvals(FmtArg::ALT_DEBUG)
///     ).unwrap(),
///     "[\n    3,\n    5,\n    8,\n]"
/// );
///
/// ```
#[derive(Copy, Clone)]
#[repr(transparent)]
pub struct StdWrapper<T>(pub T);

impl<T> PanicFmt for StdWrapper<T>
where
    T: PanicFmt,
{
    type This = Self;
    type Kind = crate::fmt::IsCustomType;

    const PV_COUNT: usize = T::PV_COUNT;
}
