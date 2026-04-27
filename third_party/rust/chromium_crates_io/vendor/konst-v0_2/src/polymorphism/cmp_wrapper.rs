use crate::polymorphism::{ConstCmpMarker, IsAConstCmpMarker, IsNotStdKind, IsStdKind};

/// A wrapper type for std types, which defines `const_eq` and `const_cmp` methods for them.
///
/// This is what [`coerce_to_cmp`] and the comparison macros convert standard library types into.
///
/// # Example
///
/// ```rust
/// use konst::{
///     polymorphism::CmpWrapper,
///     coerce_to_cmp,
/// };
///
/// use std::cmp::Ordering;
///
/// {
///     // The `CmpWrapper<u32>` type annotation is just for the reader
///     let foo: CmpWrapper<u32> = coerce_to_cmp!(10u32);
///     assert!( foo.const_eq(&10));
///     assert!(!foo.const_eq(&20));
///     
///     assert_eq!(foo.const_cmp(&5), Ordering::Greater);
///     assert_eq!(foo.const_cmp(&10), Ordering::Equal);
///     assert_eq!(foo.const_cmp(&15), Ordering::Less);
/// }
/// {
///     let bar = CmpWrapper(Ordering::Equal);
///     assert!( bar.const_eq(&Ordering::Equal));
///     assert!(!bar.const_eq(&Ordering::Less));
///     
///     assert_eq!(bar.const_cmp(&Ordering::Less), Ordering::Greater);
///     assert_eq!(bar.const_cmp(&Ordering::Equal), Ordering::Equal);
///     assert_eq!(bar.const_cmp(&Ordering::Greater), Ordering::Less);
/// }
///
/// ```
#[derive(Debug, Copy, Clone, PartialEq)]
pub struct CmpWrapper<T>(pub T);

impl<'a, T> CmpWrapper<&'a [T]> {
    /// For constructing from a reference to an array.
    ///
    /// With slices you can do `CmpWrapper(slice)` as well.
    #[inline(always)]
    pub const fn slice(x: &'a [T]) -> Self {
        Self { 0: x }
    }
}

impl<P> ConstCmpMarker for CmpWrapper<P> {
    type Kind = IsNotStdKind;
    type This = Self;
}

macro_rules! std_kind_impls {
    ($($ty:ty),* $(,)* ) => (
        $(
            impl ConstCmpMarker for $ty {
                type Kind = IsStdKind;
                type This = Self;
            }

            impl<T> IsAConstCmpMarker<IsStdKind, $ty, T> {
                /// Copies the value from `reference`, and wraps it in a `CmpWrapper`
                #[inline(always)]
                pub const fn coerce(self, reference: &$ty) -> CmpWrapper<$ty> {
                    CmpWrapper(*reference)
                }
            }

            impl CmpWrapper<$ty> {
                /// Compares `self` and `other` for equality.
                #[inline(always)]
                pub const fn const_eq(self, other: &$ty) -> bool {
                    self.0 == *other
                }
            }
        )*
    )
}

std_kind_impls! {
    i8, u8,
    i16, u16,
    i32, u32,
    i64, u64,
    i128, u128,
    isize, usize,
    bool, char,
}
