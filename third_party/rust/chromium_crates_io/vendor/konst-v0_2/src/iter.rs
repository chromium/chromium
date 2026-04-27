//! Const equivalent of iterators with a specific `next` function signature.
//!
//! The docs for [`IntoIterKind`] has more information on
//! const equivalents of IntoIterator and Iterator.
//!

#[cfg(any(not(doctest), feature = "rust_1_56"))]
pub mod iterator_dsl;

/// Iterates over all elements of an [iterator](crate::iter#iterator),
/// const equivalent of [`Iterator::for_each`]
///
/// # Iterator methods
///
/// This macro supports emulating iterator methods by expanding to equivalent code.
///
/// The supported iterator methods are documented in the [`iterator_dsl`] module,
/// because they are also supported by other `konst::iter` macros.
///
/// # Examples
///
/// ### Custom iterator
///
/// ```rust
/// use konst::iter::{IntoIterKind, IsIteratorKind};
///
/// struct Upto10(u8);
///
/// impl IntoIterKind for Upto10 { type Kind = IsIteratorKind; }
///
/// impl Upto10 {
///     const fn next(mut self) -> Option<(u8, Self)> {
///         if self.0 < 10 {
///             let ret = self.0;
///             self.0 += 1;
///             Some((ret, self))
///         } else {
///             None
///         }
///     }
/// }
///
/// const N: u32 = {
///     let mut n = 0u32;
///     konst::iter::for_each!{elem in Upto10(7) =>
///         n = n * 10 + elem as u32;
///     }
///     n
/// };
///
/// assert_eq!(N, 789);
///
/// ```
///
/// ### Summing pairs
///
/// This example requires the `"rust_1_51"` feature, because it uses const generics.
///
#[cfg_attr(feature = "rust_1_51", doc = "```rust")]
#[cfg_attr(not(feature = "rust_1_51"), doc = "```ignore")]
/// use konst::iter::for_each;
///     
/// const fn add_pairs<const N: usize>(l: [u32; N], r: [u32; N]) -> [u32; N] {
///     let mut out = [0u32; N];
///
///     for_each!{(i, val) in &l,zip(&r),map(|(l, r)| *l + *r),enumerate() =>
///         out[i] = val;
///     }
///
///     out
/// }
///
/// assert_eq!(add_pairs([], []), []);
/// assert_eq!(add_pairs([3], [5]), [8]);
/// assert_eq!(add_pairs([3, 5], [8, 13]), [11, 18]);
///
/// ```
///
/// [`iterator_dsl`]: crate::iter::iterator_dsl
pub use konst_macro_rules::for_each;

/// Wrapper for `IntoIterKind` implementors,
/// that defines different methods depending on the
/// value of `<T as IntoIterKind>::Kind`.
#[doc(inline)]
pub use konst_macro_rules::into_iter::IntoIterWrapper;

/// Marker type for proving that `T: IntoIterKind<Kind = K>`
#[doc(inline)]
pub use konst_macro_rules::into_iter::IsIntoIterKind;

/// Macro for converting [`IntoIterKind`] implementors into const iterators.
///
#[doc(inline)]
pub use konst_macro_rules::into_iter_macro as into_iter;

/// Const analog of the [`IntoIterator`] trait.
///
/// # Implementor
///
/// Implementors are expected to be:
///
/// - [Types that have an associated iterator](#isnoniteratorkind),
///   that have [`IsNonIteratorKind`](crate::iter::IsNonIteratorKind)
///   as the [`IntoIterKind::Kind`] associated type.
///
/// - [Iterators themselves](#isiteratorkind),
/// that have [`IsIteratorKind`](crate::iter::IsIteratorKind)
/// as the [`IntoIterKind::Kind`] associated type.
///
/// - Standard library types, of the [`IsStdKind`] kind
///
/// ### `IsNonIteratorKind`
///
/// These types are expected to define this inherent method for converting to
/// a const iterator:
///
/// ```rust
/// # struct II;
/// # struct SomeIterator;
/// # impl II {
/// const fn const_into_iter(self) -> SomeIterator {
/// #   loop{}
/// # }
/// # }
/// ```
///
/// [full example below](#non-iter-example)
///
/// ### `IsIteratorKind`
///
/// These types are expected to have this inherent method:
///
/// ```rust
/// # struct SomeIterator;
/// # type Item = u8;
/// # impl SomeIterator {
/// // Equivalent to `Iterator::next`
/// const fn next(self) -> Option<(Item, Self)> {
/// #   loop{}
/// # }
/// # }
/// ```
/// Where `Item` can be any type.
///
/// These are other methods that you can optionaly define,
/// which most iterators from the `konst` crate define:
/// ```rust
/// # struct SomeIterator;
/// # struct SomeIteratorRev;
/// # type Item = u8;
/// # impl SomeIterator {
/// // equivalent to `DoubleEndedÌterator::mext_back`
/// const fn next_back(self) -> Option<(Item, Self)> {
/// #   loop{}
///     // ... some code...
/// }
///
/// // Reverses the itereator, equivalent to `Iterator::rev`
/// const fn rev(self) -> SomeIteratorRev {
/// #   loop{}
///     // ... some code...
/// }
///
/// // Clones the iterator, equivalent to `Clone::clone`
/// const fn copy(&self) -> Self {
/// #   loop{}
///     // ... some code...
/// }
/// # }
/// ```
/// Where `SomeIteratorRev` should be a `IntoIterKind<Kind = IsIteratorKind>`
/// which has the same inherent methods for iteration.
///
/// [full example below](#iter-example)
///
/// # Examples
///
/// <span id = "non-iter-example"></span>
/// ### Implementing for a non-iterator
///
/// ```rust
/// use konst::{iter, slice};
///
/// struct GetSlice<'a, T>{
///     slice: &'a [T],
///     up_to: usize,
/// }
///
/// impl<T> iter::IntoIterKind for GetSlice<'_, T> {
///     type Kind = iter::IsNonIteratorKind;
/// }
///
/// impl<'a, T> GetSlice<'a, T> {
///     const fn const_into_iter(self) -> konst::slice::Iter<'a, T> {
///         slice::iter(slice::slice_up_to(self.slice, self.up_to))
///     }
/// }
///
/// const fn sum_powers(up_to: usize) -> u64 {
///     let gs = GetSlice{slice: &[1, 2, 4, 8, 16, 32, 64, 128], up_to};
///
///     iter::eval!(gs,fold(0, |l, &r| l + r))
/// }
///
/// assert_eq!(sum_powers(0), 0);
/// assert_eq!(sum_powers(1), 1);
/// assert_eq!(sum_powers(2), 3);
/// assert_eq!(sum_powers(3), 7);
/// assert_eq!(sum_powers(4), 15);
/// assert_eq!(sum_powers(5), 31);
///
/// ```
///
/// <span id = "iter-example"></span>
/// ### Implementing for an iterator
///
/// This example requires Rust 1.47.0 (because of `u8::checked_sub`)
///
#[cfg_attr(feature = "rust_1_51", doc = "```rust")]
#[cfg_attr(not(feature = "rust_1_51"), doc = "```ignore")]
/// use konst::iter::{self, IntoIterKind};
///
/// struct Countdown(u8);
///
/// impl IntoIterKind for Countdown { type Kind = iter::IsIteratorKind; }
///
/// impl Countdown {
///     const fn next(mut self) -> Option<(u8, Self)> {
///         konst::option::map!(self.0.checked_sub(1), |ret| {
///             self.0 = ret;
///             (ret, self)
///         })
///     }
/// }
///
/// const fn sum(initial: u8) -> u16 {
///     iter::eval!(Countdown(initial),fold(0u16, |accum, elem| accum + elem as u16))
/// }
///
/// assert_eq!(sum(0), 0);
/// assert_eq!(sum(1), 0);
/// assert_eq!(sum(2), 1);
/// assert_eq!(sum(3), 3);
/// assert_eq!(sum(4), 6);
/// assert_eq!(sum(5), 10);
///
/// ```
///
/// ### Implementing for a double-ended iterator
///
/// ```rust
/// use konst::iter;
///
/// assert_eq!(HOURS, [1, 2, 3, 4, 5, 6, 12, 11, 10, 9, 8, 7]);
///
/// const HOURS: [u8; 12] = {
///     let mut arr = [0; 12];
///     let hours = Hours::new();
///
///     iter::for_each!{(i, hour) in 0..6,zip(hours.copy()) =>
///         arr[i] = hour;
///     }
///     iter::for_each!{(i, hour) in 6..12,zip(hours.rev()) =>
///         arr[i] = hour;
///     }
///
///     arr
/// };
///
///
/// struct Hours{
///     start: u8,
///     end: u8,
/// }
///
/// impl iter::IntoIterKind for Hours {
///     type Kind = iter::IsIteratorKind;
/// }
///
/// impl Hours {
///     const fn new() -> Self {
///         Self {start: 1, end: 13}
///     }
///
///     const fn next(mut self) -> Option<(u8, Self)> {
///         if self.start == self.end {
///             None
///         } else {
///             let ret = self.start;
///             self.start += 1;
///             Some((ret, self))
///         }
///     }
///
///     const fn next_back(mut self) -> Option<(u8, Self)> {
///         if self.start == self.end {
///             None
///         } else {
///             self.end -= 1;
///             Some((self.end, self))
///         }
///     }
///
///     const fn rev(self) -> HoursRev {
///         HoursRev(self)
///     }
///
///     /// Since `Clone::clone` isn't const callable on stable,
///     /// clonable iterators must define an inherent method to be cloned
///     const fn copy(&self) -> Self {
///         let Self{start, end} = *self;
///         Self{start, end}
///     }
/// }
///
/// struct HoursRev(Hours);
///
/// impl iter::IntoIterKind for HoursRev {
///     type Kind = iter::IsIteratorKind;
/// }
///
/// impl HoursRev {
///     const fn next(self) -> Option<(u8, Self)> {
///         konst::option::map!(self.0.next_back(), |(a, h)| (a, HoursRev(h)))
///     }
///
///     const fn next_back(self) -> Option<(u8, Self)> {
///         konst::option::map!(self.0.next(), |(a, h)| (a, HoursRev(h)))
///     }
///
///     const fn rev(self) -> Hours {
///         self.0
///     }
///
///     const fn copy(&self) -> Self {
///         Self(self.0.copy())
///     }
/// }
///
///
/// ```
///
#[doc(inline)]
pub use konst_macro_rules::into_iter::IntoIterKind;

/// For marking some type as being from std
/// in its [`IntoIterKind::Kind`] associated type.
#[doc(inline)]
pub use konst_macro_rules::into_iter::IsStdKind;

/// For marking some type as being convertible to an iterator
/// in its [`IntoIterKind::Kind`] associated type.
#[doc(inline)]
pub use konst_macro_rules::into_iter::IsNonIteratorKind;

/// For marking some type as being an iterator
/// in its [`IntoIterKind::Kind`] associated type.
#[doc(inline)]
pub use konst_macro_rules::into_iter::IsIteratorKind;

include! {"./iter/collect_const.rs"}
include! {"./iter/iter_eval.rs"}
