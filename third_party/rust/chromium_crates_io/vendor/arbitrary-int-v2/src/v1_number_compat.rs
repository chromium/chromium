use crate::traits::{Integer, UnsignedInteger};
use crate::TryNewError;
use core::fmt::Debug;

/// Compatibility with arbitrary-int 1.x, which didn't support signed integers.
///
/// Going forward, use [`UnsignedInteger`] (to allow only unsigned integers) or [`Integer`] (to
/// support either signed or unsigned).
///
/// It is suggested to import via `use arbitrary_int::prelude::*` as `use arbitrary_int::*` will
/// pull in this trait as well, which causes clashes with `Integer`.
#[deprecated(
    since = "2.0.0",
    note = "Use [`UnsignedInteger`] or [`Integer`] instead. Suggested to import via `use arbitrary_int::prelude::*`."
)]
pub trait Number: UnsignedInteger<UnderlyingType = <Self as Number>::UnderlyingType> {
    type UnderlyingType: Integer
        + Debug
        + From<u8>
        + TryFrom<u16>
        + TryFrom<u32>
        + TryFrom<u64>
        + TryFrom<u128>;

    /// Number of bits that can fit in this type
    const BITS: usize = <Self as Integer>::BITS;

    /// Minimum value that can be represented by this type
    const MIN: Self = <Self as Integer>::MIN;

    /// Maximum value that can be represented by this type
    const MAX: Self = <Self as Integer>::MAX;

    /// Creates a number from the given value, throwing an error if the value is too large.
    /// This constructor is useful when creating a value from a literal.
    #[inline]
    fn new(value: <Self as Number>::UnderlyingType) -> Self {
        Integer::new(value)
    }

    /// Creates a number from the given value, return None if the value is too large
    #[inline]
    fn try_new(value: <Self as Number>::UnderlyingType) -> Result<Self, TryNewError> {
        Integer::try_new(value)
    }

    #[inline]
    fn value(self) -> <Self as Number>::UnderlyingType {
        Integer::value(self)
    }

    /// Creates a number from the given value, throwing an error if the value is too large.
    /// This constructor is useful when the value is convertible to T. Use [`Self::new`] for literals.
    #[inline]
    fn from_<T: Number>(value: T) -> Self {
        Integer::from_(value)
    }

    /// Creates an instance from the given `value`. Unlike the various `new...` functions, this
    /// will never fail as the value is masked to the result size.
    #[inline]
    fn masked_new<T: Number>(value: T) -> Self {
        Integer::masked_new(value)
    }

    #[inline]
    fn as_u8(&self) -> u8 {
        Integer::as_u8(*self)
    }

    #[inline]
    fn as_u16(&self) -> u16 {
        Integer::as_u16(*self)
    }

    #[inline]
    fn as_u32(&self) -> u32 {
        Integer::as_u32(*self)
    }

    #[inline]
    fn as_u64(&self) -> u64 {
        Integer::as_u64(*self)
    }

    #[inline]
    fn as_u128(&self) -> u128 {
        Integer::as_u128(*self)
    }

    #[inline]
    fn as_usize(&self) -> usize {
        Integer::as_usize(*self)
    }

    #[inline]
    fn as_<T: Number>(self) -> T {
        Integer::as_(self)
    }
}
