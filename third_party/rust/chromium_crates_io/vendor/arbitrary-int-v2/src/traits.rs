use crate::TryNewError;
use core::fmt::{Binary, Debug, Display, LowerHex, Octal, UpperHex};
use core::ops::{
    Add, AddAssign, BitAnd, BitAndAssign, BitOr, BitOrAssign, BitXor, BitXorAssign, Div, DivAssign,
    Mul, MulAssign, Neg, Not, Sub, SubAssign,
};

pub(crate) mod sealed {
    /// Ensures that outside users can not implement the traits provided by this crate.
    pub trait Sealed {}
}

/// Trait that is only implemented for `u8`, `u16`, `u32`, `u64`, `u128` and their signed
/// counterparts `i8`, `i16`, `i32`, `i64`, `i128`.
pub trait BuiltinInteger: sealed::Sealed {}

/// The base trait for integer numbers, either built-in (u8, i8, u16, i16, u32, i32, u64, i64,
/// u128, i128) or arbitrary-int (u1, i1, u7, i7 etc.).
pub trait Integer:
    Sized
    + Copy
    + Clone
    + PartialOrd
    + Ord
    + PartialEq
    + Eq
    + Debug
    + Display
    + LowerHex
    + UpperHex
    + Octal
    + Binary
    + sealed::Sealed
    + Add<Self, Output = Self>
    + AddAssign<Self>
    + Sub<Self, Output = Self>
    + SubAssign<Self>
    + Div<Self, Output = Self>
    + DivAssign<Self>
    + Mul<Self, Output = Self>
    + MulAssign<Self>
    + Not<Output = Self>
    + BitAnd<Self, Output = Self>
    + BitAndAssign<Self>
    + BitOr<Self, Output = Self>
    + BitOrAssign<Self>
    + BitXor<Self, Output = Self>
    + BitXorAssign<Self>
{
    type UnderlyingType: Integer
        + BuiltinInteger
        + Debug
        + TryFrom<u8>
        + TryFrom<u16>
        + TryFrom<u32>
        + TryFrom<u64>
        + TryFrom<u128>;

    /// An equivalent type with the same number of bits but unsigned. If
    /// the type is already unsigned, this is the same type.
    /// - i4::UnsignedInteger == u4
    /// - u60::SignedInteger == u60
    type UnsignedInteger: UnsignedInteger;

    /// An equivalent type with the same number of bits but signed. If
    /// the type is already signed, this is the same type.
    /// - i4::UnsignedInteger == i4
    /// - u60::SignedInteger == i60
    type SignedInteger: SignedInteger;

    /// Number of bits that can fit in this type
    const BITS: usize;

    /// The number 0
    const ZERO: Self;

    /// Minimum value that can be represented by this type
    const MIN: Self;

    /// Maximum value that can be represented by this type
    const MAX: Self;

    const IS_SIGNED: bool;

    /// Creates a number from the given value, throwing an error if the value is too large.
    /// This constructor is useful when creating a value from a literal.
    fn new(value: Self::UnderlyingType) -> Self;

    /// Creates a number from the given value, return None if the value is too large
    fn try_new(value: Self::UnderlyingType) -> Result<Self, TryNewError>;

    fn value(self) -> Self::UnderlyingType;

    /// Creates a number from the given value, throwing an error if the value is too large.
    /// This constructor is useful when the value is convertible to T. Use [`Self::new`] for literals.
    fn from_<T: Integer>(value: T) -> Self;

    /// Creates an instance from the given `value`. Unlike the various `new...` functions, this
    /// will never fail as the value is masked to the result size.
    ///
    /// If the source value is a signed integer and the target has more bits, the value is
    /// sign-extended. For example, `u5::masked_new(i4::new(-1)) == u5::new(0b1_1111)`.
    fn masked_new<T: Integer>(value: T) -> Self;

    fn as_u8(self) -> u8;

    fn as_u16(self) -> u16;

    fn as_u32(self) -> u32;

    fn as_u64(self) -> u64;

    fn as_u128(self) -> u128;

    fn as_usize(self) -> usize;

    fn as_i8(self) -> i8;

    fn as_i16(self) -> i16;

    fn as_i32(self) -> i32;

    fn as_i64(self) -> i64;

    fn as_i128(self) -> i128;

    fn as_isize(self) -> isize;

    /// Converts the number to its unsigned equivalent. For types that have fewer bits
    /// than the underlying type, this involves a zero extension. Types that are
    /// already unsigned will return themselves.
    fn to_unsigned(self) -> Self::UnsignedInteger;

    /// Converts the number from its unsigned equivalent. For types that have fewer bits
    /// than the underlying type, this involves a sign extension, if this type is a signed type.
    /// Types that are already unsigned will return themselves.
    fn from_unsigned(value: Self::UnsignedInteger) -> Self;

    #[inline]
    fn as_<T: Integer>(self) -> T {
        T::masked_new(self)
    }
}

/// The base trait for all signed numbers, either built-in (i8, i16, i32, i64, i128) or
/// arbitrary-int (i1, i7 etc.).
pub trait SignedInteger: Integer + Neg<Output = Self> {}

/// The base trait for all unsigned numbers, either built-in (u8, u16, u32, u64, u128) or
/// arbitrary-int (u1, u7 etc.).
pub trait UnsignedInteger: Integer {}
