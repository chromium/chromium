use crate::{
    common::{
        bytes_operation_impl, from_arbitrary_int_impl, from_native_impl, impl_bin_proto,
        impl_extract, impl_num_traits, impl_schemars, impl_step, impl_sum_product,
    },
    traits::{sealed::Sealed, BuiltinInteger, Integer, SignedInteger},
    TryNewError,
};
use core::fmt::{Binary, Debug, Display, Formatter, LowerHex, Octal, UpperHex};
use core::ops::{
    Add, AddAssign, BitAnd, BitAndAssign, BitOr, BitOrAssign, BitXor, BitXorAssign, Div, DivAssign,
    Mul, MulAssign, Neg, Not, Shl, ShlAssign, Shr, ShrAssign, Sub, SubAssign,
};

macro_rules! impl_signed_integer_native {
    ($(($type:ident, $unsigned_type:ident)),+) => {
        $(
            impl Sealed for $type {}

            impl SignedInteger for $type {}

            impl BuiltinInteger for $type {}

            impl Integer for $type {
                type UnderlyingType = $type;
                type UnsignedInteger = $unsigned_type;
                type SignedInteger = $type;

                const BITS: usize = Self::BITS as usize;
                const ZERO: Self = 0;
                const MIN: Self = Self::MIN;
                const MAX: Self = Self::MAX;
                const IS_SIGNED: bool = true;

                #[inline]
                fn new(value: Self::UnderlyingType) -> Self { value }

                #[inline]
                fn try_new(value: Self::UnderlyingType) -> Result<Self, TryNewError> { Ok(value) }

                #[inline]
                fn value(self) -> Self::UnderlyingType { self }

                #[inline]
                fn from_<T: Integer>(value: T) -> Self {
                    if T::IS_SIGNED {
                        if (Self::BITS as usize) < T::BITS {
                            assert!(value >= T::masked_new(Self::MIN) && value <= T::masked_new(Self::MAX));
                        }
                    } else {
                        if (Self::BITS as usize) <= T::BITS {
                            assert!(value <= T::masked_new(Self::MAX));
                        }
                    }
                    Self::masked_new(value)
                }

                #[inline]
                fn masked_new<T: Integer>(value: T) -> Self {
                    // Primitive types don't need masking
                    match Self::BITS {
                        8 => value.as_i8() as Self,
                        16 => value.as_i16() as Self,
                        32 => value.as_i32() as Self,
                        64 => value.as_i64() as Self,
                        128 => value.as_i128() as Self,
                        _ => panic!("Unhandled Integer type")
                    }
                }

                #[inline]
                fn as_u8(self) -> u8 { self as u8 }

                #[inline]
                fn as_u16(self) -> u16 { self as u16 }

                #[inline]
                fn as_u32(self) -> u32 { self as u32 }

                #[inline]
                fn as_u64(self) -> u64 { self as u64 }

                #[inline]
                fn as_u128(self) -> u128 { self as u128 }

                #[inline]
                fn as_usize(self) -> usize { self as usize }

                #[inline]
                fn as_i8(self) -> i8 { self as i8 }

                #[inline]
                fn as_i16(self) -> i16 { self as i16 }

                #[inline]
                fn as_i32(self) -> i32 { self as i32 }

                #[inline]
                fn as_i64(self) -> i64 { self as i64 }

                #[inline]
                fn as_i128(self) -> i128 { self as i128 }

                #[inline]
                fn as_isize(self) -> isize { self as isize }

                #[inline]
                fn to_unsigned(self) -> Self::UnsignedInteger { self as Self::UnsignedInteger }

                #[inline]
                fn from_unsigned(value: Self::UnsignedInteger) -> Self { value as Self }
            }
        )+
    };
}

impl_signed_integer_native!((i8, u8), (i16, u16), (i32, u32), (i64, u64), (i128, u128));

/// A signed integer of arbitrary bit length.
///
/// # In-Memory Representation
/// The specific in-memory representation that would be seen by calling [`core::mem::transmute`] is unspecified.
/// but satisfies the following guarantees:
/// - An `Int<T, BITS>` has the same size/alignment as `T`
/// - An `Int` has no uninitialized bytes or padding (satisfies [`bytemuck::NoUninit`]).
/// - If the value of a `Int<T, BITS>` is non-negative,
///   then it will the same representation as `UInt<T, BITS>`
///
/// When `cfg(feature = "bytemuck")` is enabled, the appropriate traits are implemented
/// based on the above guarantees.
/// It is not possible to implement [`bytemuck::Contiguous`] because that would be
/// incompatible with a zero-extended memory representation.
///
/// Since the underlying representation is unspecified, it may change in a patch version
/// without being considered a breaking change.
///
/// [`bytemuck::NoUninit`]: https://docs.rs/bytemuck/1/bytemuck/trait.NoUninit.html
/// [`bytemuck::Contiguous`]: https://docs.rs/bytemuck/1/bytemuck/trait.Contiguous.html
#[derive(Copy, Clone, Eq, PartialEq, Default, Ord, PartialOrd, Hash)]
#[repr(transparent)]
pub struct Int<T: SignedInteger + BuiltinInteger, const BITS: usize> {
    value: T,
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> Int<T, BITS> {
    /// The number of bits in the underlying type that are not present in this type.
    const UNUSED_BITS: usize = (core::mem::size_of::<T>() << 3) - Self::BITS;

    pub const BITS: usize = BITS;

    /// Returns the type as a fundamental data type.
    ///
    /// Note that if negative, the returned value may span more bits than [`BITS`](Self::BITS),
    /// as it preserves the numeric value instead of the bitwise value:
    ///
    /// ```
    /// # use arbitrary_int::i3;
    /// let value: i8 = i3::new(-1).value();
    /// assert_eq!(value, -1);
    /// assert_eq!(value.count_ones(), 8);
    /// ```
    ///
    /// If you need a value within the specified bit range, use [`Self::to_bits`].
    #[cfg(not(feature = "hint"))]
    #[inline]
    pub const fn value(self) -> T {
        self.value
    }

    /// Initializes a new value without checking the bounds
    ///
    /// # Safety
    ///
    /// Must only be called with a value bigger or equal to [`Self::MIN`] and less than or equal to [`Self::MAX`].
    #[inline]
    pub const unsafe fn new_unchecked(value: T) -> Self {
        Self { value }
    }
}

macro_rules! int_impl_num {
    ($(($type:ident, $unsigned_type:ident)),+) => {
        $(
            impl<const BITS: usize> Sealed for Int<$type, BITS> {}

            impl<const BITS: usize> SignedInteger for Int<$type, BITS> {}

            impl<const BITS: usize> Integer for Int<$type, BITS> {
                type UnderlyingType = $type;
                type SignedInteger = Self;
                type UnsignedInteger = crate::UInt<$unsigned_type, BITS>;

                const BITS: usize = BITS;

                const ZERO: Self = Self { value: 0 };

                const MIN: Self = Self { value: -Self::MAX.value - 1 };

                // The existence of MAX also serves as a bounds check: If NUM_BITS is > available bits,
                // we will get a compiler error right here
                const MAX: Self = Self {
                    // MAX is always positive so we don't have to worry about the sign
                    value: (<$type as Integer>::MAX >> (<$type as Integer>::BITS - Self::BITS)),
                };

                const IS_SIGNED: bool = true;

                #[inline]
                fn try_new(value: Self::UnderlyingType) -> Result<Self, TryNewError> {
                    if value >= Self::MIN.value && value <= Self::MAX.value {
                        Ok(Self { value })
                    } else {
                        Err(TryNewError{})
                    }
                }

                #[inline]
                fn new(value: $type) -> Self {
                    assert!(value >= Self::MIN.value && value <= Self::MAX.value);

                    Self { value }
                }

                #[inline]
                fn from_<T: Integer>(value: T) -> Self {
                    if T::IS_SIGNED {
                        if Self::BITS < T::BITS {
                            assert!(value >= Self::MIN.value.as_() && value <= Self::MAX.value.as_());
                        }
                    } else {
                        if Self::BITS <= T::BITS {
                            assert!(value <= Self::MAX.value.as_());
                        }
                    }
                    Self { value: Self::UnderlyingType::masked_new(value) }
                }

                fn masked_new<T: Integer>(value: T) -> Self {
                    // If the source type is wider, we need to mask and sign-extend. If the source
                    // type is the same width but unsigned, we also need to sign-extend!
                    if Self::BITS < T::BITS || (Self::BITS == T::BITS && !T::IS_SIGNED) {
                        let value = (value.as_::<Self::UnderlyingType>() << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
                        Self { value: Self::UnderlyingType::masked_new(value) }
                    } else {
                        Self { value: Self::UnderlyingType::masked_new(value) }
                    }
                }

                fn as_u8(self) -> u8 {
                    self.value() as _
                }

                fn as_u16(self) -> u16 {
                    self.value() as _
                }

                fn as_u32(self) -> u32 {
                    self.value() as _
                }

                fn as_u64(self) -> u64 {
                    self.value() as _
                }

                fn as_u128(self) -> u128 {
                    self.value() as _
                }

                fn as_usize(self) -> usize {
                    self.value() as _
                }

                fn as_i8(self) -> i8 {
                    self.value() as _
                }

                fn as_i16(self) -> i16 {
                    self.value() as _
                }

                fn as_i32(self) -> i32 {
                    self.value() as _
                }

                fn as_i64(self) -> i64 {
                    self.value() as _
                }

                fn as_i128(self) -> i128 {
                    self.value() as _
                }

                fn as_isize(self) -> isize {
                    self.value() as _
                }

                #[inline]
                fn to_unsigned(self) -> Self::UnsignedInteger { Self::UnsignedInteger::masked_new(self.value as $unsigned_type) }

                #[inline]
                fn from_unsigned(value: Self::UnsignedInteger) -> Self {
                    Self::masked_new(value.value() as $type)
                }


                #[inline]
                fn value(self) -> $type {
                    #[cfg(feature = "hint")]
                    unsafe {
                        core::hint::assert_unchecked(self.value >= Self::MIN.value);
                        core::hint::assert_unchecked(self.value <= Self::MAX.value);
                    }

                    self.value
                }
            }
        )+
    };
}

int_impl_num!((i8, u8), (i16, u16), (i32, u32), (i64, u64), (i128, u128));

macro_rules! int_impl {
    ($(($type:ident, $unsigned_type:ident, doctest = $doctest_attr:literal)),+) => {
        $(
            impl<const BITS: usize> Int<$type, BITS> {
                pub const MASK: $type = (Self::MAX.value << 1) | 1;

                /// Creates an instance. Panics if the given value is outside of the valid range
                #[inline]
                pub const fn new(value: $type) -> Self {
                    assert!(value >= Self::MIN.value && value <= Self::MAX.value);

                    Self { value }
                }

                /// Creates an instance. Panics if the given value is outside of the valid range
                #[inline]
                pub const fn from_i8(value: i8) -> Self {
                    if Self::BITS < 8 {
                        assert!(value >= Self::MIN.value as i8 && value <= Self::MAX.value as i8);
                    }
                    Self { value: value as $type }
                }

                /// Creates an instance. Panics if the given value is outside of the valid range
                #[inline]
                pub const fn from_i16(value: i16) -> Self {
                    if Self::BITS < 16 {
                        assert!(value >= Self::MIN.value as i16 && value <= Self::MAX.value as i16);
                    }
                    Self { value: value as $type }
                }

                /// Creates an instance. Panics if the given value is outside of the valid range
                #[inline]
                pub const fn from_i32(value: i32) -> Self {
                    if Self::BITS < 32 {
                        assert!(value >= Self::MIN.value as i32 && value <= Self::MAX.value as i32);
                    }
                    Self { value: value as $type }
                }

                /// Creates an instance. Panics if the given value is outside of the valid range
                #[inline]
                pub const fn from_i64(value: i64) -> Self {
                    if Self::BITS < 64 {
                        assert!(value >= Self::MIN.value as i64 && value <= Self::MAX.value as i64);
                    }
                    Self { value: value as $type }
                }

                /// Creates an instance. Panics if the given value is outside of the valid range
                #[inline]
                pub const fn from_i128(value: i128) -> Self {
                    if Self::BITS < 128 {
                        assert!(value >= Self::MIN.value as i128 && value <= Self::MAX.value as i128);
                    }
                    Self { value: value as $type }
                }

                /// Creates an instance or an error if the given value is outside of the valid range
                #[inline]
                pub const fn try_new(value: $type) -> Result<Self, TryNewError> {
                    if value >= Self::MIN.value && value <= Self::MAX.value {
                        Ok(Self { value })
                    } else {
                        Err(TryNewError {})
                    }
                }

                /// Returns the bitwise representation of the value.
                ///
                /// As the bit width is limited to [`BITS`](Self::BITS) the numeric value may differ from [`value`](Self::value).
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::i3;
                /// let value = i3::new(-1);
                /// assert_eq!(value.to_bits(), 0b111); // 7
                /// assert_eq!(value.value(), -1);
                /// ```
                ///
                /// To convert from the bitwise representation back to an instance, use [`from_bits`](Self::from_bits).
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn to_bits(self) -> $unsigned_type {
                    (self.value() & Self::MASK) as $unsigned_type
                }

                /// Convert the bitwise representation from [`to_bits`](Self::to_bits) to an instance.
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::i3;
                /// let value = i3::from_bits(0b111);
                /// assert_eq!(value.value(), -1);
                /// assert_eq!(value.to_bits(), 0b111);
                /// ```
                ///
                /// If you want to convert a numeric value to an instance instead, use [`new`](Self::new).
                ///
                /// # Panics
                ///
                /// Panics if the given value exceeds the bit width specified by [`BITS`](Self::BITS).
                #[inline]
                pub const fn from_bits(value: $unsigned_type) -> Self {
                    assert!(value & (!Self::MASK as $unsigned_type) == 0);

                    // First do a logical left shift to put the sign bit at the underlying type's MSB (copying the sign),
                    // then an arithmetic right shift to sign-extend the value into its original position.
                    Self { value: ((value << Self::UNUSED_BITS) as $type) >> Self::UNUSED_BITS }
                }

                /// Tries to convert the bitwise representation from [`to_bits`](Self::to_bits) to an instance.
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::i3;
                /// i3::try_from_bits(0b1111).expect_err("value is > 3 bits");
                /// let value = i3::try_from_bits(0b111).expect("value is <= 3 bits");
                /// assert_eq!(value.value(), -1);
                /// assert_eq!(value.to_bits(), 0b111);
                /// ```
                ///
                /// If you want to convert a numeric value to an instance instead, use [`try_new`](Self::try_new).
                ///
                /// # Errors
                ///
                /// Returns an error if the given value exceeds the bit width specified by [`BITS`](Self::BITS).
                #[inline]
                pub const fn try_from_bits(value: $unsigned_type) -> Result<Self, TryNewError> {
                    if value & (!Self::MASK as $unsigned_type) == 0 {
                        // First do a logical left shift to put the sign bit at the underlying type's MSB (copying the sign),
                        // then an arithmetic right shift to sign-extend the value into its original position.
                        Ok(Self { value: ((value << Self::UNUSED_BITS) as $type) >> Self::UNUSED_BITS })
                    } else {
                        Err(TryNewError {})
                    }
                }

                /// Converts the bitwise representation from [`to_bits`](Self::to_bits) to an instance,
                /// without checking the bounds.
                ///
                /// # Safety
                ///
                /// The given value must not exceed the bit width specified by [`Self::BITS`].
                #[inline]
                pub const unsafe fn from_bits_unchecked(value: $unsigned_type) -> Self {
                    // First do a logical left shift to put the sign bit at the underlying type's MSB (copying the sign),
                    // then an arithmetic right shift to sign-extend the value into its original position.
                    Self { value: ((value << Self::UNUSED_BITS) as $type) >> Self::UNUSED_BITS }
                }

                /// Returns the type as a fundamental data type.
                ///
                /// Note that if negative, the returned value may span more bits than [`BITS`](Self::BITS)
                /// as it preserves the numeric value instead of the bitwise value:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::i3;
                /// let value: i8 = i3::new(-1).value();
                /// assert_eq!(value, -1);
                /// assert_eq!(value.count_ones(), 8);
                /// ```
                ///
                /// If you need a value within the specified bit range, use [`to_bits`](Self::to_bits).
                #[cfg(feature = "hint")]
                #[inline]
                pub const fn value(self) -> $type {
                    // The hint feature requires the type to be const-comparable,
                    // which isn't possible in the generic version above. So we have
                    // an entirely different function if this feature is enabled.
                    // It only works for primitive types, which should be ok in practice
                    // (but is technically an API change)
                    unsafe {
                        core::hint::assert_unchecked(self.value >= Self::MIN.value);
                        core::hint::assert_unchecked(self.value <= Self::MAX.value);
                    }
                    self.value
                }

                // Generate the `extract_{i,u}{8,16,32,64,128}` functions.
                impl_extract!(
                    $type,
                    "from_bits(value >> start_bit)",
                    |value| (value << Self::UNUSED_BITS) >> Self::UNUSED_BITS,

                    (8, (i8, extract_i8), (u8, extract_u8)),
                    (16, (i16, extract_i16), (u16, extract_u16)),
                    (32, (i32, extract_i32), (u32, extract_u32)),
                    (64, (i64, extract_i64), (u64, extract_u64)),
                    (128, (i128, extract_i128), (u128, extract_u128))
                );

                /// Returns an [`Int`] with a wider bit depth but with the same base data type
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn widen<const BITS_RESULT: usize>(self) -> Int<$type, BITS_RESULT> {
                    const { assert!(BITS < BITS_RESULT, "Can not call widen() with the given bit widths") };

                    // Query MAX of the result to ensure we get a compiler error if the current definition is bogus (e.g. <u8, 9>)
                    let _ = Int::<$type, BITS_RESULT>::MAX;
                    Int::<$type, BITS_RESULT> { value: self.value }
                }

                /// Wrapping (modular) addition. Computes `self + rhs`, wrapping around at the
                /// boundary of the type.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(100).wrapping_add(i14::new(27)), i14::new(127));
                /// assert_eq!(i14::MAX.wrapping_add(i14::new(2)), i14::MIN + i14::new(1));
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn wrapping_add(self, rhs: Self) -> Self {
                    let sum = self.value().wrapping_add(rhs.value());
                    Self {
                        value: (sum << Self::UNUSED_BITS) >> Self::UNUSED_BITS,
                    }
                }

                /// Wrapping (modular) subtraction. Computes `self - rhs`, wrapping around at the
                /// boundary of the type.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(0).wrapping_sub(i14::new(127)), i14::new(-127));
                /// assert_eq!(i14::new(-2).wrapping_sub(i14::MAX), i14::MAX);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn wrapping_sub(self, rhs: Self) -> Self {
                    let sum = self.value().wrapping_sub(rhs.value());
                    Self {
                        value: (sum << Self::UNUSED_BITS) >> Self::UNUSED_BITS,
                    }
                }

                /// Wrapping (modular) multiplication. Computes `self * rhs`, wrapping around at the
                /// boundary of the type.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(10).wrapping_mul(i14::new(12)), i14::new(120));
                /// assert_eq!(i14::new(12).wrapping_mul(i14::new(1024)), i14::new(-4096));
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn wrapping_mul(self, rhs: Self) -> Self {
                    let sum = self.value().wrapping_mul(rhs.value());
                    Self {
                        value: (sum << Self::UNUSED_BITS) >> Self::UNUSED_BITS,
                    }
                }

                /// Wrapping (modular) division. Computes `self / rhs`, wrapping around at the
                /// boundary of the type.
                ///
                /// The only case where such wrapping can occur is when one divides `MIN / -1` on a
                /// signed type (where `MIN` is the negative minimal value for the type); this is
                /// equivalent to `-MIN`, a positive value that is too large to represent in the type.
                /// In such a case, this function returns `MIN` itself.
                ///
                /// # Panics
                ///
                /// This function will panic if `rhs` is zero.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(100).wrapping_div(i14::new(10)), i14::new(10));
                /// assert_eq!(i14::MIN.wrapping_div(i14::new(-1)), i14::MIN);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn wrapping_div(self, rhs: Self) -> Self {
                    let sum = self.value().wrapping_div(rhs.value());
                    Self {
                        // Unlike the unsigned implementation we do need to account for overflow here,
                        // `Self::MIN / -1` is equal to `Self::MAX + 1`.
                        value: (sum << Self::UNUSED_BITS) >> Self::UNUSED_BITS,
                    }
                }

                /// Wrapping (modular) negation. Computes `-self`, wrapping around at the boundary of the type.
                ///
                /// The only case where such wrapping can occur is when one negates `MIN` on a signed type
                /// (where `MIN` is the negative minimal value for the type); this is a positive value that is
                /// too large to represent in the type. In such a case, this function returns `MIN` itself.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(100).wrapping_neg(), i14::new(-100));
                /// assert_eq!(i14::new(-100).wrapping_neg(), i14::new(100));
                /// assert_eq!(i14::MIN.wrapping_neg(), i14::MIN);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn wrapping_neg(self) -> Self {
                    let value = (self.value().wrapping_neg() << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
                    Self { value }
                }

                /// Panic-free bitwise shift-left; yields `self << mask(rhs)`, where mask removes any
                /// high-order bits of `rhs` that would cause the shift to exceed the bitwidth of the type.
                ///
                /// Note that this is not the same as a rotate-left; the RHS of a wrapping shift-left is
                /// restricted to the range of the type, rather than the bits shifted out of the LHS being
                /// returned to the other end.
                /// A [`rotate_left`](Self::rotate_left) function exists as well, which may be what you
                /// want instead.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(-1).wrapping_shl(7), i14::new(-128));
                /// assert_eq!(i14::new(-1).wrapping_shl(128), i14::new(-4));
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn wrapping_shl(self, rhs: u32) -> Self {
                    // modulo is expensive on some platforms, so only do it when necessary
                    let shift_amount = Self::UNUSED_BITS as u32 + (if rhs >= BITS as u32 {
                        rhs % (BITS as u32)
                    } else {
                        rhs
                    });

                    Self {
                        // We could use wrapping_shl here to make Debug builds slightly smaller;
                        // the downside would be that on weird CPUs that don't do wrapping_shl by
                        // default release builds would get slightly worse. Using << should give
                        // good release performance everywere
                        value: (self.value() << shift_amount) >> Self::UNUSED_BITS,
                    }
                }

                /// Panic-free bitwise shift-right; yields `self >> mask(rhs)`, where mask removes any
                /// high-order bits of `rhs` that would cause the shift to exceed the bitwidth of the type.
                ///
                /// Note that this is not the same as a rotate-right; the RHS of a wrapping shift-right is
                /// restricted to the range of the type, rather than the bits shifted out of the LHS being
                /// returned to the other end.
                /// A [`rotate_right`](Self::rotate_right) function exists as well, which may be what you
                /// want instead.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(-128).wrapping_shr(7), i14::new(-1));
                /// assert_eq!(i14::new(-128).wrapping_shr(60), i14::new(-8));
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn wrapping_shr(self, rhs: u32) -> Self {
                    // modulo is expensive on some platforms, so only do it when necessary
                    let shift_amount = if rhs >= (BITS as u32) {
                        rhs % (BITS as u32)
                    } else {
                        rhs
                    };

                    Self {
                        value: (self.value() >> shift_amount),
                    }
                }

                /// Saturating integer addition. Computes `self + rhs`, saturating at the numeric
                /// bounds instead of overflowing.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(100).saturating_add(i14::new(1)), i14::new(101));
                /// assert_eq!(i14::MAX.saturating_add(i14::new(100)), i14::MAX);
                /// assert_eq!(i14::MIN.saturating_add(i14::new(-1)), i14::MIN);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn saturating_add(self, rhs: Self) -> Self {
                    if Self::UNUSED_BITS == 0 {
                        // We are something like a Int::<i8; 8>, we can fallback to the base implementation.
                        // This is very unlikely to happen in practice, but checking allows us to use
                        // `wrapping_add` instead of `saturating_add` in the common case, which is faster.
                        let value = self.value().saturating_add(rhs.value());
                        Self { value }
                    } else {
                        // We're dealing with fewer bits than the underlying type (e.g. i7).
                        // That means the addition can never overflow the underlying type.
                        let value = self.value().wrapping_add(rhs.value());
                        if value > Self::MAX.value {
                            Self::MAX
                        } else if value < Self::MIN.value {
                            Self::MIN
                        } else {
                            Self { value }
                        }
                    }
                }

                /// Saturating integer subtraction. Computes `self - rhs`, saturating at the numeric
                /// bounds instead of overflowing.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(100).saturating_sub(i14::new(127)), i14::new(-27));
                /// assert_eq!(i14::MIN.saturating_sub(i14::new(100)), i14::MIN);
                /// assert_eq!(i14::MAX.saturating_sub(i14::new(-1)), i14::MAX);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn saturating_sub(self, rhs: Self) -> Self {
                    if Self::UNUSED_BITS == 0 {
                        // We are something like a Int::<i8; 8>, we can fallback to the base implementation.
                        // This is very unlikely to happen in practice, but checking allows us to use
                        // `wrapping_sub` instead of `saturating_sub` in the common case, which is faster.
                        let value = self.value().saturating_sub(rhs.value());
                        Self { value }
                    } else {
                        // We're dealing with fewer bits than the underlying type (e.g. i7).
                        // That means the subtraction can never overflow the underlying type.
                        let value = self.value().wrapping_sub(rhs.value());
                        if value > Self::MAX.value {
                            Self::MAX
                        } else if value < Self::MIN.value {
                            Self::MIN
                        } else {
                            Self { value }
                        }
                    }
                }

                /// Saturating integer multiplication. Computes `self * rhs`, saturating at the numeric
                /// bounds instead of overflowing.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(10).saturating_mul(i14::new(12)), i14::new(120));
                /// assert_eq!(i14::MAX.saturating_mul(i14::new(10)), i14::MAX);
                /// assert_eq!(i14::MIN.saturating_mul(i14::new(10)), i14::MIN);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn saturating_mul(self, rhs: Self) -> Self {
                    let value = if (BITS << 1) <= (core::mem::size_of::<$type>() << 3) {
                        // We have half the bits (e.g. i4 * i4) of the base type, so we can't overflow the base type
                        // `wrapping_mul` likely provides the best performance on all cpus
                        self.value().wrapping_mul(rhs.value())
                    } else {
                        // We have more than half the bits (e.g. i6 * i6)
                        self.value().saturating_mul(rhs.value())
                    };

                    if value > Self::MAX.value {
                        Self::MAX
                    } else if value < Self::MIN.value {
                        Self::MIN
                    } else {
                        Self { value }
                    }
                }

                /// Saturating integer division. Computes `self / rhs`, saturating at the numeric
                /// bounds instead of overflowing.
                ///
                /// # Panics
                ///
                /// This function will panic if rhs is zero.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(5).saturating_div(i14::new(2)), i14::new(2));
                /// assert_eq!(i14::MAX.saturating_div(i14::new(-1)), i14::MIN + i14::new(1));
                /// assert_eq!(i14::MIN.saturating_div(i14::new(-1)), i14::MAX);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn saturating_div(self, rhs: Self) -> Self {
                    // As `Self::MIN / -1` is equal to `Self::MAX + 1` we always need to check for overflow.
                    let value = self.value().saturating_div(rhs.value());

                    if value > Self::MAX.value {
                        Self::MAX
                    } else if value < Self::MIN.value {
                        Self::MIN
                    } else {
                        Self { value }
                    }
                }

                /// Saturating integer negation. Computes `-self`, returning `MAX` if `self == MIN`
                /// instead of overflowing.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(100).saturating_neg(), i14::new(-100));
                /// assert_eq!(i14::new(-100).saturating_neg(), i14::new(100));
                /// assert_eq!(i14::MIN.saturating_neg(), i14::MAX);
                /// assert_eq!(i14::MAX.saturating_neg(), i14::MIN + i14::new(1));
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn saturating_neg(self) -> Self {
                    if self.value() == Self::MIN.value() {
                        Self::MAX
                    } else {
                        // It is not possible for this to wrap as we've already checked for `MIN`.
                        let value = self.value().wrapping_neg();
                        Self { value }
                    }
                }

                /// Saturating integer exponentiation. Computes `self.pow(exp)`, saturating at the numeric
                /// bounds instead of overflowing.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(-4).saturating_pow(3), i14::new(-64));
                /// assert_eq!(i14::MIN.saturating_pow(2), i14::MAX);
                /// assert_eq!(i14::MIN.saturating_pow(3), i14::MIN);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn saturating_pow(self, exp: u32) -> Self {
                    // It might be possible to handwrite this to be slightly faster as both
                    // `saturating_pow` has to do a bounds-check and then we do second one.
                    let value = self.value().saturating_pow(exp);

                    if value > Self::MAX.value {
                        Self::MAX
                    } else if value < Self::MIN.value {
                        Self::MIN
                    } else {
                        Self { value }
                    }
                }

                /// Checked integer addition. Computes `self + rhs`, returning `None` if overflow occurred.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!((i14::MAX - i14::new(2)).checked_add(i14::new(1)), Some(i14::MAX - i14::new(1)));
                /// assert_eq!((i14::MAX - i14::new(2)).checked_add(i14::new(3)), None);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn checked_add(self, rhs: Self) -> Option<Self> {
                    if Self::UNUSED_BITS == 0 {
                        // We are something like a Int::<i8; 8>, we can fallback to the base implementation.
                        // This is very unlikely to happen in practice, but checking allows us to use
                        // `wrapping_add` instead of `checked_add` in the common case, which is faster.
                        match self.value().checked_add(rhs.value()) {
                            Some(value) => Some(Self { value }),
                            None => None
                        }
                    } else {
                        // We're dealing with fewer bits than the underlying type (e.g. i7).
                        // That means the addition can never overflow the underlying type
                        let value = self.value().wrapping_add(rhs.value());
                        if value < Self::MIN.value() || value > Self::MAX.value() {
                            None
                        } else {
                            Some(Self { value })
                        }
                    }
                }

                /// Checked integer subtraction. Computes `self - rhs`, returning `None` if overflow occurred.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!((i14::MIN + i14::new(2)).checked_sub(i14::new(1)), Some(i14::MIN + i14::new(1)));
                /// assert_eq!((i14::MIN + i14::new(2)).checked_sub(i14::new(3)), None);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn checked_sub(self, rhs: Self) -> Option<Self> {
                    if Self::UNUSED_BITS == 0 {
                        // We are something like a Int::<i8; 8>, we can fallback to the base implementation.
                        // This is very unlikely to happen in practice, but checking allows us to use
                        // `wrapping_sub` instead of `checked_sub` in the common case, which is faster.
                        match self.value().checked_sub(rhs.value()) {
                            Some(value) => Some(Self { value }),
                            None => None
                        }
                    } else {
                        // We're dealing with fewer bits than the underlying type (e.g. i7).
                        // That means the subtraction can never overflow the underlying type
                        let value = self.value().wrapping_sub(rhs.value());
                        if value < Self::MIN.value() || value > Self::MAX.value() {
                            None
                        } else {
                            Some(Self { value })
                        }
                    }
                }

                /// Checked integer multiplication. Computes `self * rhs`, returning `None` if overflow occurred.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::MAX.checked_mul(i14::new(1)), Some(i14::MAX));
                /// assert_eq!(i14::MAX.checked_mul(i14::new(2)), None);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn checked_mul(self, rhs: Self) -> Option<Self> {
                    let product = if (BITS << 1) <= (core::mem::size_of::<$type>() << 3) {
                        // We have half the bits (e.g. `i4 * i4`) of the base type, so we can't overflow the base type.
                        // `wrapping_mul` likely provides the best performance on all CPUs.
                        Some(self.value().wrapping_mul(rhs.value()))
                    } else {
                        // We have more than half the bits (e.g. u6 * u6)
                        self.value().checked_mul(rhs.value())
                    };

                    match product {
                        Some(value) if value >= Self::MIN.value() && value <= Self::MAX.value() => {
                            Some(Self { value })
                        }
                        _ => None
                    }
                }

                /// Checked integer division. Computes `self / rhs`, returning `None` if `rhs == 0`
                /// or the division results in overflow.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!((i14::MIN + i14::new(1)).checked_div(i14::new(-1)), Some(i14::new(8191)));
                /// assert_eq!(i14::MIN.checked_div(i14::new(-1)), None);
                /// assert_eq!((i14::new(1)).checked_div(i14::new(0)), None);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn checked_div(self, rhs: Self) -> Option<Self> {
                    // `checked_div` from the underlying type already catches division by zero,
                    // and the only way this can overflow is with `MIN / -1` (which equals `MAX + 1`).
                    // Because of this we only need to check if the value is larger than `MAX`.
                    match self.value().checked_div(rhs.value()) {
                        Some(value) if value <= Self::MAX.value() => Some(Self { value }),
                        _ => None
                    }
                }

                /// Checked negation. Computes `-self`, returning `None` if `self == MIN`.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(5).checked_neg(), Some(i14::new(-5)));
                /// assert_eq!(i14::MIN.checked_neg(), None);
                /// ```
                pub const fn checked_neg(self) -> Option<Self> {
                    if self.value() == Self::MIN.value() {
                        None
                    } else {
                        // It is not possible for this to wrap as we've already checked for `MIN`.
                        let value = self.value().wrapping_neg();
                        Some(Self { value })
                    }
                }

                /// Checked shift left. Computes `self << rhs`, returning `None` if `rhs` is larger than or
                /// equal to the number of bits in `self`.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::i14;
                /// assert_eq!(i14::new(0x1).checked_shl(4), Some(i14::new(0x10)));
                /// assert_eq!(i14::new(0x1).checked_shl(129), None);
                /// assert_eq!(i14::new(0x10).checked_shl(13), Some(i14::new(0)));
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn checked_shl(self, rhs: u32) -> Option<Self> {
                    if rhs >= (BITS as u32) {
                        None
                    } else {
                        let value = ((self.value() << rhs) << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
                        Some(Self { value })
                    }
                }

                /// Checked shift right. Computes `self >> rhs`, returning `None` if `rhs` is larger than
                /// or equal to the number of bits in `self`.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::i14;
                /// assert_eq!(i14::new(0x10).checked_shr(4), Some(i14::new(0x1)));
                /// assert_eq!(i14::new(0x10).checked_shr(129), None);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn checked_shr(self, rhs: u32) -> Option<Self> {
                    if rhs >= (BITS as u32) {
                        None
                    } else {
                        let value = ((self.value() >> rhs) << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
                        Some(Self { value })
                    }
                }

                /// Calculates `self + rhs`.
                ///
                /// Returns a tuple of the addition along with a boolean indicating whether an arithmetic
                /// overflow would occur. If an overflow would have occurred then the wrapped value is returned.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(5).overflowing_add(i14::new(2)), (i14::new(7), false));
                /// assert_eq!(i14::MAX.overflowing_add(i14::new(1)), (i14::MIN, true));
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn overflowing_add(self, rhs: Self) -> (Self, bool) {
                    let (value, overflow) = if Self::UNUSED_BITS == 0 {
                        // We are something like a Int::<i8; 8>, we can fallback to the base implementation.
                        // This is very unlikely to happen in practice, but checking allows us to use
                        // `wrapping_add` instead of `overflowing_add` in the common case, which is faster.
                        self.value().overflowing_add(rhs.value())
                    } else {
                        // We're dealing with fewer bits than the underlying type (e.g. i7).
                        // That means the addition can never overflow the underlying type.
                        let sum = self.value().wrapping_add(rhs.value());
                        let value = (sum << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
                        (value, value != sum)
                    };

                    (Self { value }, overflow)
                }

                /// Calculates `self - rhs`.
                ///
                /// Returns a tuple of the subtraction along with a boolean indicating whether an arithmetic
                /// overflow would occur. If an overflow would have occurred then the wrapped value is returned.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(5).overflowing_sub(i14::new(2)), (i14::new(3), false));
                /// assert_eq!(i14::MIN.overflowing_sub(i14::new(1)), (i14::MAX, true));
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn overflowing_sub(self, rhs: Self) -> (Self, bool) {
                    let (value, overflow) = if Self::UNUSED_BITS == 0 {
                        // We are something like a Int::<i8; 8>, we can fallback to the base implementation.
                        // This is very unlikely to happen in practice, but checking allows us to use
                        // `wrapping_sub` instead of `overflowing_sub` in the common case, which is faster.
                        self.value().overflowing_sub(rhs.value())
                    } else {
                        // We're dealing with fewer bits than the underlying type (e.g. i7).
                        // That means the subtraction can never overflow the underlying type
                        let sum = self.value().wrapping_sub(rhs.value());
                        let value = (sum << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
                        (value, value != sum)
                    };

                    (Self { value }, overflow)
                }

                /// Calculates the multiplication of `self` and `rhs`.
                ///
                /// Returns a tuple of the multiplication along with a boolean indicating whether an arithmetic
                /// overflow would occur. If an overflow would have occurred then the wrapped value is returned.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(5).overflowing_mul(i14::new(2)), (i14::new(10), false));
                /// assert_eq!(i14::new(1_000).overflowing_mul(i14::new(10)), (i14::new(-6384), true));
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn overflowing_mul(self, rhs: Self) -> (Self, bool) {
                    let (wrapping_product, overflow) = if (BITS << 1) <= (core::mem::size_of::<$type>() << 3) {
                        // We have half the bits (e.g. i4 * i4) of the base type, so we can't overflow the base type.
                        // `wrapping_mul` likely provides the best performance on all CPUs.
                        (self.value().wrapping_mul(rhs.value()), false)
                    } else {
                        // We have more than half the bits (e.g. i6 * i6)
                        self.value().overflowing_mul(rhs.value())
                    };

                    let value = (wrapping_product << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
                    let overflow2 = value != wrapping_product;
                    (Self { value }, overflow || overflow2)
                }

                /// Calculates the divisor when `self` is divided by `rhs`.
                ///
                /// Returns a tuple of the divisor along with a boolean indicating whether an arithmetic
                /// overflow would occur. If an overflow would occur then self is returned.
                ///
                /// # Panics
                ///
                /// This function will panic if `rhs` is zero.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(5).overflowing_div(i14::new(2)), (i14::new(2), false));
                /// assert_eq!(i14::MIN.overflowing_div(i14::new(-1)), (i14::MIN, true));
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn overflowing_div(self, rhs: Self) -> (Self, bool) {
                    let (value, overflow) = if Self::UNUSED_BITS == 0 {
                        // We are something like a Int::<i8; 8>, we can fallback to the base implementation.
                        // This is very unlikely to happen in practice, but checking allows us to use
                        // `wrapping_div` instead of `overflowing_div` in the common case, which is faster.
                        self.value().overflowing_div(rhs.value())
                    } else {
                        // We're dealing with fewer bits than the underlying type (e.g. i7).
                        // That means the division can never overflow the underlying type.
                        let quotient = self.value().wrapping_div(rhs.value());
                        let value = (quotient << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
                        (value, value != quotient)
                    };

                    (Self { value }, overflow)
                }

                /// Negates `self`, overflowing if this is equal to the minimum value.
                ///
                /// Returns a tuple of the negated version of self along with a boolean indicating whether an
                /// overflow happened. If `self` is the minimum value (e.g., `i14::MIN` for values of type `i14`),
                /// then the minimum value will be returned again and `true` will be returned for an overflow happening.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(2).overflowing_neg(), (i14::new(-2), false));
                /// assert_eq!(i14::MIN.overflowing_neg(), (i14::MIN, true));
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn overflowing_neg(self) -> (Self, bool) {
                    let (value, overflow) = if Self::UNUSED_BITS == 0 {
                        // We are something like a Int::<i8; 8>, we can fallback to the base implementation.
                        // This is very unlikely to happen in practice, but checking allows us to use
                        // `wrapping_neg` instead of `overflowing_neg` in the common case, which is faster.
                        self.value().overflowing_neg()
                    } else {
                        // We're dealing with fewer bits than the underlying type (e.g. i7).
                        // That means the negation can never overflow the underlying type.
                        let negated = self.value().wrapping_neg();
                        let value = (negated << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
                        (value, value != negated)
                    };

                    (Self { value }, overflow)
                }

                /// Shifts `self` left by `rhs` bits.
                ///
                /// Returns a tuple of the shifted version of `self` along with a boolean indicating whether
                /// the shift value was larger than or equal to the number of bits. If the shift value is too
                /// large, then value is masked (`N-1`) where `N` is the number of bits, and this value is then
                /// used to perform the shift.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(0x1).overflowing_shl(4), (i14::new(0x10), false));
                /// assert_eq!(i14::new(0x1).overflowing_shl(15), (i14::new(0x2), true));
                /// assert_eq!(i14::new(0x10).overflowing_shl(13), (i14::new(0), false));
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn overflowing_shl(self, rhs: u32) -> (Self, bool) {
                    let (shift, overflow) = if rhs >= (BITS as u32) {
                        (rhs % (BITS as u32), true)
                    } else {
                        (rhs, false)
                    };

                    // This cannot possibly wrap as we've already limited `shift` to `BITS`.
                    let value = (self.value().wrapping_shl(shift) << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
                    (Self { value }, overflow)
                }

                /// Shifts `self` right by `rhs` bits.
                ///
                /// Returns a tuple of the shifted version of `self` along with a boolean indicating whether
                /// the shift value was larger than or equal to the number of bits. If the shift value is too
                /// large, then value is masked (`N-1`) where `N` is the number of bits, and this value is then
                /// used to perform the shift.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i14::new(0x10).overflowing_shr(4), (i14::new(0x1), false));
                /// assert_eq!(i14::new(0x10).overflowing_shr(15), (i14::new(0x8), true));
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn overflowing_shr(self, rhs: u32) -> (Self, bool) {
                    let (shift, overflow) = if rhs >= (BITS as u32) {
                        (rhs % (BITS as u32), true)
                    } else {
                        (rhs, false)
                    };

                    // This cannot possibly wrap as we've already limited `shift` to `BITS`.
                    let value = (self.value().wrapping_shr(shift) << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
                    (Self { value }, overflow)
                }

                /// Returns `true` if `self` is positive and `false` if the number is zero or negative.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert!(i14::new(10).is_positive());
                /// assert!(!i14::new(-10).is_positive());
                /// ```
                #[inline]
                #[must_use]
                pub const fn is_positive(self) -> bool {
                    self.value() > 0
                }

                /// Returns `true` if `self` is negative and `false` if the number is zero or positive.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert!(i14::new(-10).is_negative());
                /// assert!(!i14::new(10).is_negative());
                /// ```
                #[inline]
                #[must_use]
                pub const fn is_negative(self) -> bool {
                    self.value() < 0
                }

                /// Reverses the order of bits in the integer. The least significant bit becomes the most
                /// significant bit, second least-significant bit becomes second most-significant bit, etc.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i6::from_bits(0b10_1010).reverse_bits(), i6::from_bits(0b01_0101));
                /// assert_eq!(i6::new(0), i6::new(0).reverse_bits());
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn reverse_bits(self) -> Self {
                    let value = self.value().reverse_bits() >> Self::UNUSED_BITS;
                    Self { value }
                }

                /// Returns the number of ones in the binary representation of `self`.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// let n = i6::from_bits(0b00_1000);
                /// assert_eq!(n.count_ones(), 1);
                /// ```
                #[inline]
                pub const fn count_ones(self) -> u32 {
                    // Due to sign-extension the unused bits may be either all ones or zeros, so we need to mask them off.
                    (self.value() & Self::MASK).count_ones()
                }

                /// Returns the number of zeros in the binary representation of `self`.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// assert_eq!(i6::MAX.count_zeros(), 1);
                /// ```
                #[inline]
                pub const fn count_zeros(self) -> u32 {
                    // Due to sign-extension the unused bits may be either all ones or zeros, so we need to mask them off.
                    // Afterwards the unused bits are all zero, so we can subtract them from the result.
                    // We can avoid a bounds check in debug builds with `wrapping_sub` since this cannot overflow.
                    (self.value() & Self::MASK).count_zeros().wrapping_sub(Self::UNUSED_BITS as u32)
                }

                /// Returns the number of leading ones in the binary representation of `self`.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// let n = i6::new(-1);
                /// assert_eq!(n.leading_ones(), 6);
                /// ```
                #[inline]
                pub const fn leading_ones(self) -> u32 {
                    (self.value() << Self::UNUSED_BITS).leading_ones()
                }

                /// Returns the number of leading zeros in the binary representation of `self`.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// let n = i6::new(-1);
                /// assert_eq!(n.leading_zeros(), 0);
                /// ```
                #[inline]
                pub const fn leading_zeros(self) -> u32 {
                    if Self::UNUSED_BITS == 0 {
                        self.value().leading_zeros()
                    } else {
                        // Prevent an all-zero value reporting the underlying type's entire bit width by setting
                        // the first unused bit to one, causing `leading_zeros()` to ignore all unused bits.
                        let first_unused_bit_set = const { 1 << (Self::UNUSED_BITS - 1) };
                        ((self.value() << Self::UNUSED_BITS) | first_unused_bit_set).leading_zeros()
                    }
                }

                /// Returns the number of trailing ones in the binary representation of `self`.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// let n = i6::new(3);
                /// assert_eq!(n.trailing_ones(), 2);
                /// ```
                #[inline]
                pub const fn trailing_ones(self) -> u32 {
                    // Prevent an all-ones value reporting the underlying type's entire bit width by masking
                    // off all the unused bits.
                    (self.value() & Self::MASK).trailing_ones()
                }

                /// Returns the number of trailing zeros in the binary representation of `self`.
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// let n = i6::new(-4);
                /// assert_eq!(n.trailing_zeros(), 2);
                /// ```
                #[inline]
                pub const fn trailing_zeros(self) -> u32 {
                    // Prevent an all-ones value reporting the underlying type's entire bit width by setting
                    // all the unused bits.
                    (self.value() | !Self::MASK).trailing_zeros()
                }

                /// Shifts the bits to the left by a specified amount, `n`, wrapping the truncated bits
                /// to the end of the resulting integer.
                ///
                /// Please note this isn’t the same operation as the `<<` shifting operator!
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// let n = i6::from_bits(0b10_1010);
                /// let m = i6::from_bits(0b01_0101);
                ///
                /// assert_eq!(n.rotate_left(1), m);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn rotate_left(self, n: u32) -> Self {
                    let b = BITS as u32;
                    let n = if n >= b { n % b } else { n };

                    // Temporarily switch to an unsigned type to prevent sign-extension with `>>`.
                    let moved_bits = ((self.value() << n) & Self::MASK) as $unsigned_type;
                    let truncated_bits = ((self.value() & Self::MASK) as $unsigned_type) >> (b - n);
                    let value = (((moved_bits | truncated_bits) << Self::UNUSED_BITS) as $type) >> Self::UNUSED_BITS;
                    Self { value }
                }

                /// Shifts the bits to the right by a specified amount, `n`, wrapping the truncated bits
                /// to the beginning of the resulting integer.
                ///
                /// Please note this isn’t the same operation as the `>>` shifting operator!
                ///
                /// # Examples
                ///
                /// Basic usage:
                ///
                #[doc = concat!(" ```", $doctest_attr)]
                /// # use arbitrary_int::prelude::*;
                /// let n = i6::from_bits(0b10_1010);
                /// let m = i6::from_bits(0b01_0101);
                ///
                /// assert_eq!(n.rotate_right(1), m);
                /// ```
                #[inline]
                #[must_use = "this returns the result of the operation, without modifying the original"]
                pub const fn rotate_right(self, n: u32) -> Self {
                    let b = BITS as u32;
                    let n = if n >= b { n % b } else { n };

                    // Temporarily switch to an unsigned type to prevent sign-extension with `>>`.
                    let moved_bits = (self.value() & Self::MASK) as $unsigned_type >> n;
                    let truncated_bits = ((self.value() << (b - n)) & Self::MASK) as $unsigned_type;
                    let value = (((moved_bits | truncated_bits) << Self::UNUSED_BITS) as $type) >> Self::UNUSED_BITS;
                    Self { value }
                }
            }
        )+
    };
}

// Because the methods within this macro are effectively copy-pasted for each underlying integer type,
// each documentation test gets executed five times (once for each underlying type), even though the
// tests themselves aren't specific to said underlying type. This severely slows down `cargo test`,
// so we ignore them for all but one (arbitrary) underlying type.
int_impl!(
    (i8, u8, doctest = "rust"),
    (i16, u16, doctest = "ignore"),
    (i32, u32, doctest = "ignore"),
    (i64, u64, doctest = "ignore"),
    (i128, u128, doctest = "ignore")
);

// Arithmetic operator implementations
impl<T: SignedInteger + BuiltinInteger, const BITS: usize> Add for Int<T, BITS>
where
    T: Shl<usize, Output = T> + Shr<usize, Output = T>,
{
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        let sum = self.value + rhs.value;
        let value = (sum << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
        debug_assert!(sum == value, "attempted to add with overflow");
        Self { value }
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> AddAssign for Int<T, BITS>
where
    T: Shl<usize, Output = T> + Shr<usize, Output = T>,
{
    fn add_assign(&mut self, rhs: Self) {
        // Delegate to the Add implementation above.
        *self = *self + rhs;
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> Sub for Int<T, BITS>
where
    T: Shl<usize, Output = T> + Shr<usize, Output = T>,
{
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        let difference = self.value - rhs.value;
        let value = (difference << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
        debug_assert!(difference == value, "attempted to subtract with overflow");
        Self { value }
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> SubAssign for Int<T, BITS>
where
    T: Shl<usize, Output = T> + Shr<usize, Output = T>,
{
    fn sub_assign(&mut self, rhs: Self) {
        // Delegate to the Sub implementation above.
        *self = *self - rhs;
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> Mul for Int<T, BITS>
where
    T: Shl<usize, Output = T> + Shr<usize, Output = T>,
{
    type Output = Self;

    fn mul(self, rhs: Self) -> Self::Output {
        let product = self.value * rhs.value;
        let value = (product << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
        debug_assert!(product == value, "attempted to multiply with overflow");
        Self { value }
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> MulAssign for Int<T, BITS>
where
    Self: Integer,
{
    fn mul_assign(&mut self, rhs: Self) {
        // Delegate to the Mul implementation above.
        *self = *self * rhs;
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> Div for Int<T, BITS>
where
    T: Shl<usize, Output = T> + Shr<usize, Output = T>,
{
    type Output = Self;

    fn div(self, rhs: Self) -> Self::Output {
        // Unlike the unsigned implementation we do need to account for overflow here,
        // `Self::MIN / -1` is equal to `Self::MAX + 1` and should therefore panic.
        let quotient = self.value / rhs.value;
        let value = (quotient << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
        debug_assert!(quotient == value, "attempted to divide with overflow");
        Self { value }
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> DivAssign for Int<T, BITS>
where
    T: Shl<usize, Output = T> + Shr<usize, Output = T>,
{
    fn div_assign(&mut self, rhs: Self) {
        // Delegate to the Div implementation above.
        *self = *self / rhs;
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> Neg for Int<T, BITS>
where
    Self: Integer<UnderlyingType = T>,
    T: Shl<usize, Output = T> + Shr<usize, Output = T>,
{
    type Output = Self;

    #[inline]
    fn neg(self) -> Self::Output {
        let negated = -self.value();
        let value = (negated << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
        debug_assert!(negated == value, "attempt to negate with overflow");
        Self { value }
    }
}

// Bitwise operator implementations
impl<T: SignedInteger + BuiltinInteger, const BITS: usize> BitAnd for Int<T, BITS> {
    type Output = Self;

    fn bitand(self, rhs: Self) -> Self::Output {
        let value = self.value & rhs.value;
        Self { value }
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> BitAndAssign for Int<T, BITS> {
    fn bitand_assign(&mut self, rhs: Self) {
        self.value &= rhs.value;
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> BitOr for Int<T, BITS> {
    type Output = Self;

    fn bitor(self, rhs: Self) -> Self::Output {
        let value = self.value | rhs.value;
        Self { value }
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> BitOrAssign for Int<T, BITS> {
    fn bitor_assign(&mut self, rhs: Self) {
        self.value |= rhs.value;
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> BitXor for Int<T, BITS> {
    type Output = Self;

    fn bitxor(self, rhs: Self) -> Self::Output {
        let value = self.value ^ rhs.value;
        Self { value }
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> BitXorAssign for Int<T, BITS> {
    fn bitxor_assign(&mut self, rhs: Self) {
        self.value ^= rhs.value;
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> Not for Int<T, BITS> {
    type Output = Self;

    fn not(self) -> Self::Output {
        let value = !self.value;
        Self { value }
    }
}

impl<T: SignedInteger + BuiltinInteger, TSHIFTBITS, const BITS: usize> Shl<TSHIFTBITS>
    for Int<T, BITS>
where
    T: Shl<TSHIFTBITS, Output = T> + Shl<usize, Output = T> + Shr<usize, Output = T>,
    TSHIFTBITS: TryInto<usize> + Copy,
{
    type Output = Self;

    fn shl(self, rhs: TSHIFTBITS) -> Self::Output {
        // With debug assertions, the << and >> operators throw an exception if the shift amount
        // is larger than the number of bits (in which case the result would always be 0)
        debug_assert!(
            rhs.try_into().unwrap_or(usize::MAX) < BITS,
            "attempted to shift left with overflow"
        );

        // Shift left twice to avoid needing an unnecessarily strict `TSHIFTBITS: Add<Self::UNUSED_BITS>` bound.
        // This should be optimised to a single shift.
        let value = ((self.value << rhs) << Self::UNUSED_BITS) >> Self::UNUSED_BITS;
        Self { value }
    }
}

impl<T: SignedInteger + BuiltinInteger, TSHIFTBITS, const BITS: usize> ShlAssign<TSHIFTBITS>
    for Int<T, BITS>
where
    Self: Integer,
    T: Shl<TSHIFTBITS, Output = T> + Shl<usize, Output = T> + Shr<usize, Output = T>,
    TSHIFTBITS: TryInto<usize> + Copy,
{
    fn shl_assign(&mut self, rhs: TSHIFTBITS) {
        // Delegate to the Shl implementation above.
        *self = *self << rhs;
    }
}

impl<T: SignedInteger + BuiltinInteger, TSHIFTBITS, const BITS: usize> Shr<TSHIFTBITS>
    for Int<T, BITS>
where
    Self: Integer,
    T: Shr<TSHIFTBITS, Output = T> + Shl<usize, Output = T> + Shr<usize, Output = T>,
    TSHIFTBITS: TryInto<usize> + Copy,
{
    type Output = Self;

    fn shr(self, rhs: TSHIFTBITS) -> Self::Output {
        // With debug assertions, the << and >> operators throw an exception if the shift amount
        // is larger than the number of bits (in which case the result would always be 0)
        debug_assert!(
            rhs.try_into().unwrap_or(usize::MAX) < BITS,
            "attempted to shift right with overflow"
        );

        Self {
            // Our unused bits can only ever all be 1 or 0, depending on the sign.
            // As right shifts on primitive types perform sign-extension anyways we don't need to do any extra work here.
            value: self.value >> rhs,
        }
    }
}

impl<T: SignedInteger + BuiltinInteger, TSHIFTBITS, const BITS: usize> ShrAssign<TSHIFTBITS>
    for Int<T, BITS>
where
    Self: Integer,
    T: Shr<TSHIFTBITS, Output = T> + Shl<usize, Output = T> + Shr<usize, Output = T>,
    TSHIFTBITS: TryInto<usize> + Copy,
{
    fn shr_assign(&mut self, rhs: TSHIFTBITS) {
        // Delegate to the Shr implementation above.
        *self = *self >> rhs;
    }
}

// Delegated trait implementations
impl<T: SignedInteger + BuiltinInteger, const BITS: usize> Display for Int<T, BITS> {
    #[inline]
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        Display::fmt(&self.value, f)
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> Debug for Int<T, BITS> {
    #[inline]
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        Debug::fmt(&self.value, f)
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> LowerHex for Int<T, BITS> {
    #[inline]
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        LowerHex::fmt(&self.value, f)
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> UpperHex for Int<T, BITS> {
    #[inline]
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        UpperHex::fmt(&self.value, f)
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> Octal for Int<T, BITS> {
    #[inline]
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        Octal::fmt(&self.value, f)
    }
}

impl<T: SignedInteger + BuiltinInteger, const BITS: usize> Binary for Int<T, BITS> {
    #[inline]
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        Binary::fmt(&self.value, f)
    }
}

impl_bytemuck_basic!(Int, SignedInteger {
    /// A [`Int`] initialized to zero has a zero value.
    impl Zeroable for ... {}
    /// A [`Int`] has no uninitialized bytes or padding.
    impl NoUninit for ... {}
    /// It is possible to check whether an in-memory representation of an [`Int`] is valid,
    /// although the specific meaning of that representation is not specified.
    impl CheckedBitPattern for ... {}
});

#[cfg(feature = "defmt")]
impl<T: SignedInteger + BuiltinInteger, const BITS: usize> defmt::Format for Int<T, BITS>
where
    T: defmt::Format,
{
    #[inline]
    fn format(&self, f: defmt::Formatter) {
        self.value.format(f)
    }
}

impl_borsh!(Int, "i", SignedInteger);

impl_bin_proto!(Int, SignedInteger);

// Serde's invalid_value error (https://rust-lang.github.io/hashbrown/serde/de/trait.Error.html#method.invalid_value)
// takes an Unexpected (https://rust-lang.github.io/hashbrown/serde/de/enum.Unexpected.html) which only accepts a 64 bit
// integer. This is a problem for us because we want to support 128 bit integers. To work around this we define our own
// error type using the Int's underlying type which implements Display and then use serde::de::Error::custom to create
// an error with our custom type.
#[cfg(feature = "serde")]
struct InvalidIntValueError<T: SignedInteger> {
    value: T::UnderlyingType,
}

#[cfg(feature = "serde")]
impl<T: SignedInteger> Display for InvalidIntValueError<T> {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "invalid value: integer `{}`, expected a value between `{}` and `{}`",
            self.value,
            T::MIN.value(),
            T::MAX.value()
        )
    }
}

#[cfg(feature = "serde")]
impl<T: SignedInteger + BuiltinInteger, const BITS: usize> serde::Serialize for Int<T, BITS>
where
    T: serde::Serialize,
{
    fn serialize<S: serde::Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        self.value.serialize(serializer)
    }
}

#[cfg(feature = "serde")]
impl<'de, T: SignedInteger + BuiltinInteger, const BITS: usize> serde::Deserialize<'de>
    for Int<T, BITS>
where
    Self: SignedInteger<UnderlyingType = T>,
    T: serde::Deserialize<'de>,
{
    fn deserialize<D: serde::Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        let value = T::deserialize(deserializer)?;

        if value >= Self::MIN.value && value <= Self::MAX.value {
            Ok(Self { value })
        } else {
            let err = InvalidIntValueError::<Self> { value };
            Err(serde::de::Error::custom(err))
        }
    }
}

// Implement `core::iter::Sum` and `core::iter::Product`.
impl_sum_product!(Int, 1_i8, SignedInteger);

// Implement `core::iter::Step` (if the `step_trait` feature is enabled).
impl_step!(Int, SignedInteger);

// Implement support for the `num-traits` crate, if the feature is enabled.
impl_num_traits!(Int, SignedInteger, i8, |value| (
    (value << Self::UNUSED_BITS) >> Self::UNUSED_BITS,
    value.clamp(Self::MIN.value(), Self::MAX.value())
));

// Support for the `schemars` crate, if the feature is enabled.
impl_schemars!(Int, "int", SignedInteger);

// Implement byte operations for Int's with a bit width aligned to a byte boundary.
bytes_operation_impl!(Int<i32, 24>, i32);
bytes_operation_impl!(Int<i64, 24>, i64);
bytes_operation_impl!(Int<i128, 24>, i128);
bytes_operation_impl!(Int<i64, 40>, i64);
bytes_operation_impl!(Int<i128, 40>, i128);
bytes_operation_impl!(Int<i64, 48>, i64);
bytes_operation_impl!(Int<i128, 48>, i128);
bytes_operation_impl!(Int<i64, 56>, i64);
bytes_operation_impl!(Int<i128, 56>, i128);
bytes_operation_impl!(Int<i128, 72>, i128);
bytes_operation_impl!(Int<i128, 80>, i128);
bytes_operation_impl!(Int<i128, 88>, i128);
bytes_operation_impl!(Int<i128, 96>, i128);
bytes_operation_impl!(Int<i128, 104>, i128);
bytes_operation_impl!(Int<i128, 112>, i128);
bytes_operation_impl!(Int<i128, 120>, i128);

// Conversions
from_arbitrary_int_impl!(Int(i8), [i16, i32, i64, i128]);
from_arbitrary_int_impl!(Int(i16), [i8, i32, i64, i128]);
from_arbitrary_int_impl!(Int(i32), [i8, i16, i64, i128]);
from_arbitrary_int_impl!(Int(i64), [i8, i16, i32, i128]);
from_arbitrary_int_impl!(Int(i128), [i8, i32, i64, i16]);

from_native_impl!(Int(i8), [i8, i16, i32, i64, i128]);
from_native_impl!(Int(i16), [i8, i16, i32, i64, i128]);
from_native_impl!(Int(i32), [i8, i16, i32, i64, i128]);
from_native_impl!(Int(i64), [i8, i16, i32, i64, i128]);
from_native_impl!(Int(i128), [i8, i16, i32, i64, i128]);

use crate::common::{impl_borsh, impl_bytemuck_basic};
pub use aliases::*;

#[allow(non_camel_case_types)]
#[rustfmt::skip]
pub(crate) mod aliases {
    use crate::common::type_alias;

    type_alias!(Int(i8), (i1, 1), (i2, 2), (i3, 3), (i4, 4), (i5, 5), (i6, 6), (i7, 7));
    type_alias!(Int(i16), (i9, 9), (i10, 10), (i11, 11), (i12, 12), (i13, 13), (i14, 14), (i15, 15));
    type_alias!(Int(i32), (i17, 17), (i18, 18), (i19, 19), (i20, 20), (i21, 21), (i22, 22), (i23, 23), (i24, 24), (i25, 25), (i26, 26), (i27, 27), (i28, 28), (i29, 29), (i30, 30), (i31, 31));
    type_alias!(Int(i64), (i33, 33), (i34, 34), (i35, 35), (i36, 36), (i37, 37), (i38, 38), (i39, 39), (i40, 40), (i41, 41), (i42, 42), (i43, 43), (i44, 44), (i45, 45), (i46, 46), (i47, 47), (i48, 48), (i49, 49), (i50, 50), (i51, 51), (i52, 52), (i53, 53), (i54, 54), (i55, 55), (i56, 56), (i57, 57), (i58, 58), (i59, 59), (i60, 60), (i61, 61), (i62, 62), (i63, 63));
    type_alias!(Int(i128), (i65, 65), (i66, 66), (i67, 67), (i68, 68), (i69, 69), (i70, 70), (i71, 71), (i72, 72), (i73, 73), (i74, 74), (i75, 75), (i76, 76), (i77, 77), (i78, 78), (i79, 79), (i80, 80), (i81, 81), (i82, 82), (i83, 83), (i84, 84), (i85, 85), (i86, 86), (i87, 87), (i88, 88), (i89, 89), (i90, 90), (i91, 91), (i92, 92), (i93, 93), (i94, 94), (i95, 95), (i96, 96), (i97, 97), (i98, 98), (i99, 99), (i100, 100), (i101, 101), (i102, 102), (i103, 103), (i104, 104), (i105, 105), (i106, 106), (i107, 107), (i108, 108), (i109, 109), (i110, 110), (i111, 111), (i112, 112), (i113, 113), (i114, 114), (i115, 115), (i116, 116), (i117, 117), (i118, 118), (i119, 119), (i120, 120), (i121, 121), (i122, 122), (i123, 123), (i124, 124), (i125, 125), (i126, 126), (i127, 127));
}
