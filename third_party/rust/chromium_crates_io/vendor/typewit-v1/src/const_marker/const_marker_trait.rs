/// A type that represents a constant
/// 
/// # Example
/// 
/// Emulating generic associated constants on stable
/// 
/// (this example Requires Rust 1.65.0)
/// 
#[cfg_attr(not(feature = "rust_1_65"), doc = "```ignore")]
#[cfg_attr(feature = "rust_1_65", doc = "```rust")]
/// use typewit::const_marker::{ConstMarker, ConstMarkerOf};
/// 
/// 
/// assert_eq!(make_array::<u8>(), ([3], [3, 3, 3]));
/// 
/// assert_eq!(make_array::<&str>(), (["hi"], ["hi", "hi", "hi"]));
/// 
/// const fn make_array<T: MakeArray>() -> ([T; 1], [T; 3]) {
///     (T::Make::<1>::VAL, T::Make::<3>::VAL)
/// }
/// 
/// // `MakeArray` can make arrays of any length without writing a bound for each length!!
/// trait MakeArray: Sized {
///     // emulates `const Make<const N: usize>: [Self; N];` (a generic associated constant)
///     type Make<const N: usize>: ConstMarkerOf<[Self; N]>;
/// }
/// 
/// 
/// impl MakeArray for u8 {
///     type Make<const N: usize> = MakeArrayConst<Self, N>;
/// }
///
/// impl<'a> MakeArray for &'a str {
///     type Make<const N: usize> = MakeArrayConst<Self, N>;
/// }
/// 
/// struct MakeArrayConst<T, const N: usize>(core::marker::PhantomData<T>);
/// 
/// impl<const N: usize> ConstMarker for MakeArrayConst<u8, N> {
///     type Of = [u8; N];
///     const VAL: Self::Of = [3; N];
/// }
/// 
/// impl<'a, const N: usize> ConstMarker for MakeArrayConst<&'a str, N> {
///     type Of = [&'a str; N];
///     const VAL: Self::Of = ["hi"; N];
/// }
/// 
/// ```
/// 
pub trait ConstMarker: Sized {
    /// The type of this constant
    type Of;

    /// The value of this constant
    const VAL: Self::Of;
}


/// Trait alias for `ConstMarker<Of = Of>`
/// 
/// This trait also shows the `Of` type in unsatisfied trait bound errors
/// (`ConstMarker<Of = ...>` bounds obfuscate this a bit as of 1.89.0) 
#[cfg_attr(feature = "rust_1_83", diagnostic::on_unimplemented(
    message = "`{Self}` is not a type-level constant of type `{Of}`",
))]
pub trait ConstMarkerOf<Of>: ConstMarker<Of = Of> {}

impl<C, Of> ConstMarkerOf<Of> for C
where
    C: ConstMarker<Of = Of>,
{}
