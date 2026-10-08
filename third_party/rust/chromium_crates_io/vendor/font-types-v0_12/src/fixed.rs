//! fixed-point numerical types

use std::ops::{Add, AddAssign, Div, DivAssign, Mul, MulAssign, Neg, Sub, SubAssign};

// shared between Fixed, F26Dot6, F2Dot14, F4Dot12, F6Dot10
macro_rules! fixed_impl {
    ($name:ident, $bits:literal, $fract_bits:literal, $ty:ty) => {
        #[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash, Default)]
        #[cfg_attr(feature = "bytemuck", derive(bytemuck::AnyBitPattern, bytemuck::NoUninit))]
        #[repr(transparent)]
        #[doc = concat!(stringify!($bits), "-bit signed fixed point number with ", stringify!($fract_bits), " bits of fraction." )]
        pub struct $name($ty);
        impl $name {
            /// Minimum value.
            pub const MIN: Self = Self(<$ty>::MIN);

            /// Maximum value.
            pub const MAX: Self = Self(<$ty>::MAX);

            /// This type's smallest representable value
            pub const EPSILON: Self = Self(1);

            /// Representation of 0.0.
            pub const ZERO: Self = Self(0);

            /// Representation of 1.0.
            pub const ONE: Self = Self(1 << $fract_bits);

            /// Representation of -1.0.
            pub const NEG_ONE: Self = Self((!0 << $fract_bits) as $ty);

            const INT_MASK: $ty = !0 << $fract_bits;
            const ROUND: $ty = 1 << ($fract_bits - 1);
            const FRACT_BITS: usize = $fract_bits;

            /// Creates a new fixed point value from the underlying bit representation.
            #[inline(always)]
            pub const fn from_bits(bits: $ty) -> Self {
                Self(bits)
            }

            /// Returns the underlying bit representation of the value.
            #[inline(always)]
            pub const fn to_bits(self) -> $ty {
                self.0
            }

            //TODO: is this actually useful?
            /// Returns the nearest integer value.
            #[inline(always)]
            pub const fn round(self) -> Self {
                Self(self.0.wrapping_add(Self::ROUND) & Self::INT_MASK)
            }

            /// Returns the absolute value of the number.
            #[inline(always)]
            pub const fn abs(self) -> Self {
                Self(self.0.wrapping_abs())
            }

            /// Returns the largest integer less than or equal to the number.
            #[inline(always)]
            pub const fn floor(self) -> Self {
                Self(self.0 & Self::INT_MASK)
            }

            /// Returns the fractional part of the number.
            #[inline(always)]
            pub const fn fract(self) -> Self {
                Self(self.0 - self.floor().0)
            }

            /// Wrapping addition.
            #[inline(always)]
            pub fn wrapping_add(self, other: Self) -> Self {
                Self(self.0.wrapping_add(other.0))
            }

            /// Saturating addition.
            #[inline(always)]
            pub const fn saturating_add(self, other: Self) -> Self {
                Self(self.0.saturating_add(other.0))
            }

            /// Checked addition.
            #[inline(always)]
            pub fn checked_add(self, other: Self) -> Option<Self> {
                self.0.checked_add(other.0).map(|inner| Self(inner))
            }

            /// Wrapping substitution.
            #[inline(always)]
            pub const fn wrapping_sub(self, other: Self) -> Self {
                Self(self.0.wrapping_sub(other.0))
            }

            /// Saturating substitution.
            #[inline(always)]
            pub const fn saturating_sub(self, other: Self) -> Self {
                Self(self.0.saturating_sub(other.0))
            }

            /// The representation of this number as a big-endian byte array.
            #[inline(always)]
            pub const fn to_be_bytes(self) -> [u8; $bits / 8] {
                self.0.to_be_bytes()
            }
        }

        impl Add for $name {
            type Output = Self;
            #[inline(always)]
            fn add(self, other: Self) -> Self {
                Self(self.0.wrapping_add(other.0))
            }
        }

        impl AddAssign for $name {
            #[inline(always)]
            fn add_assign(&mut self, other: Self) {
                *self = *self + other;
            }
        }

        impl Sub for $name {
            type Output = Self;
            #[inline(always)]
            fn sub(self, other: Self) -> Self {
                Self(self.0.wrapping_sub(other.0))
            }
        }

        impl SubAssign for $name {
            #[inline(always)]
            fn sub_assign(&mut self, other: Self) {
                *self = *self - other;
            }
        }

        impl Neg for $name {
            type Output = Self;
            #[inline(always)]
            fn neg(self) -> Self {
                Self(self.0.wrapping_neg())
            }
        }
    };
}

impl Fixed {
    /// Multiplies `self` by `a` and divides the product by `b`.
    // This one is specifically not always inlined due to size and
    // frequency of use. We leave it to compiler discretion.
    #[inline]
    pub const fn mul_div(&self, a: Self, b: Self) -> Self {
        let mut sign = 1;
        let mut su = self.0 as u64;
        let mut au = a.0 as u64;
        let mut bu = b.0 as u64;
        if self.0 < 0 {
            su = 0u64.wrapping_sub(su);
            sign = -1;
        }
        if a.0 < 0 {
            au = 0u64.wrapping_sub(au);
            sign = -sign;
        }
        if b.0 < 0 {
            bu = 0u64.wrapping_sub(bu);
            sign = -sign;
        }
        let result = if bu > 0 {
            su.wrapping_mul(au).wrapping_add(bu >> 1) / bu
        } else {
            0x7FFFFFFF
        };
        Self(if sign < 0 {
            (result as i32).wrapping_neg()
        } else {
            result as i32
        })
    }
}

impl Mul for Fixed {
    type Output = Self;

    #[inline(always)]
    fn mul(self, other: Self) -> Self::Output {
        let ab = self.0 as i64 * other.0 as i64;
        Self(((ab + 0x8000 - i64::from(ab < 0)) >> 16) as i32)
    }
}

impl Div for Fixed {
    type Output = Self;

    fn div(self, other: Self) -> Self {
        let sign = (self.0 < 0) ^ (other.0 < 0);
        let au = self.0.unsigned_abs() as u64;
        let bu = other.0.unsigned_abs() as u64;
        let q = if bu == 0 {
            0x7FFFFFFF_u32
        } else {
            (((au << 16) + (bu >> 1)) / bu) as u32
        };
        Self(if sign {
            (q as i32).wrapping_neg()
        } else {
            q as i32
        })
    }
}

impl Mul for F26Dot6 {
    type Output = Self;

    #[inline(always)]
    fn mul(self, other: Self) -> Self::Output {
        let ab = self.0 as i64 * other.0 as i64;
        Self(((ab + 32 - i64::from(ab < 0)) >> 6) as i32)
    }
}

impl Div for F26Dot6 {
    type Output = Self;

    fn div(self, other: Self) -> Self {
        let sign = (self.0 < 0) ^ (other.0 < 0);
        let au = self.0.unsigned_abs() as u64;
        let bu = other.0.unsigned_abs() as u64;
        let q = if bu == 0 {
            0x7FFFFFFF_u32
        } else {
            (((au << 6) + (bu >> 1)) / bu) as u32
        };
        Self(if sign {
            (q as i32).wrapping_neg()
        } else {
            q as i32
        })
    }
}

/// Implements multiplication and division assignment operators for fixed
/// types.
macro_rules! fixed_mul_div_assign {
    ($ty:ty) => {
        impl MulAssign for $ty {
            #[inline(always)]
            fn mul_assign(&mut self, rhs: Self) {
                *self = *self * rhs;
            }
        }

        impl DivAssign for $ty {
            #[inline(always)]
            fn div_assign(&mut self, rhs: Self) {
                *self = *self / rhs;
            }
        }
    };
}

/// impl float conversion methods.
///
/// We convert to different float types in order to ensure we can roundtrip
/// without floating point error.
macro_rules! float_conv {
    // default invocation: we will impl Display/Default/Serialize/Deserialize
    ($name:ident, $to:ident, $from:ident, $ty:ty) => {
        float_conv!($name, $to, $from, $ty, no_fmt);

        impl std::fmt::Display for $name {
            fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
                if f.precision().is_some() {
                    return std::fmt::Display::fmt(&self.$to(), f);
                }
                fmt_shortest(self.0 as i64, Self::FRACT_BITS as u32, f)
            }
        }

        impl std::fmt::Debug for $name {
            fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
                std::fmt::Display::fmt(self, f)
            }
        }

        #[cfg(feature = "serde")]
        impl ::serde::Serialize for $name {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: ::serde::Serializer,
            {
                <$ty>::serialize(&$name::$to(*self), serializer)
            }
        }

        #[cfg(feature = "serde")]
        impl<'de> ::serde::Deserialize<'de> for $name {
            fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
            where
                D: ::serde::Deserializer<'de>,
            {
                <$ty>::deserialize(deserializer).map($name::$from)
            }
        }
    };
    // explicitly opt out of Display/Default/Serialize/Deserialize
    // (for types that get both f32 & f64)
    ($name:ident, $to:ident, $from:ident, $ty:ty, no_fmt) => {
        impl $name {
            #[doc = concat!("Creates a fixed point value from a ", stringify!($ty), ".")]
            ///
            /// This operation is lossy; the float will be rounded to the nearest
            /// representable value.
            #[inline(always)]
            pub fn $from(x: $ty) -> Self {
                // When x is positive: 1.0 - 0.5 =  0.5
                // When x is negative: 0.0 - 0.5 = -0.5
                let frac = (x.is_sign_positive() as u8 as $ty) - 0.5;
                Self((x * Self::ONE.0 as $ty + frac) as _)
            }

            #[doc = concat!("Returns the value as an ", stringify!($ty), ".")]
            ///
            /// This operation is lossless: all representable values can be
            /// round-tripped.
            #[inline(always)]
            pub fn $to(self) -> $ty {
                let int = ((self.0 & Self::INT_MASK) >> Self::FRACT_BITS) as $ty;
                let fract = (self.0 & !Self::INT_MASK) as $ty / Self::ONE.0 as $ty;
                int + fract
            }
        }
    };
}

/// Format a fixed-point value as the shortest decimal that parses back to it.
///
/// The value is exactly `raw / 2^fract_bits`. We pick the fewest fractional
/// digits whose nearest decimal is strictly within half a step of it, as
/// `fontTools`' `fixedToStr` does, and omit the decimal point for integral
/// values. Width, fill, alignment and sign flags behave as they do for
/// floats.
fn fmt_shortest(raw: i64, fract_bits: u32, f: &mut std::fmt::Formatter) -> std::fmt::Result {
    let one = 1u128 << fract_bits;
    let magnitude = u128::from(raw.unsigned_abs());
    for places in 0..=5 {
        let pow = 10u128.pow(places);
        let digits = (2 * magnitude * pow + one) / (2 * one);
        if (digits * one).abs_diff(magnitude * pow) * 2 < pow {
            let mut buf = [0u8; 32];
            return f.pad_integral(raw >= 0, "", write_decimal(digits, places, &mut buf));
        }
    }
    // Rounding leaves `digits * one` within `one / 2` of `magnitude * pow`,
    // so five places always pass since `one <= 2^16 < 10^5`.
    unreachable!()
}

/// Write `digits / 10^places` as a decimal at the end of `buf` and return it.
fn write_decimal(mut digits: u128, places: u32, buf: &mut [u8; 32]) -> &str {
    let mut pos = buf.len();
    let mut written = 0;
    while written <= places || digits != 0 {
        if written == places && places != 0 {
            pos -= 1;
            buf[pos] = b'.';
        }
        pos -= 1;
        buf[pos] = b'0' + (digits % 10) as u8;
        digits /= 10;
        written += 1;
    }
    std::str::from_utf8(&buf[pos..]).expect("only ASCII digits and '.'")
}

fixed_impl!(F2Dot14, 16, 14, i16);
fixed_impl!(F4Dot12, 16, 12, i16);
fixed_impl!(F6Dot10, 16, 10, i16);
fixed_impl!(Fixed, 32, 16, i32);
fixed_impl!(F26Dot6, 32, 6, i32);
fixed_impl!(F48Dot16, 64, 16, i64);

fixed_mul_div_assign!(Fixed);
fixed_mul_div_assign!(F26Dot6);

float_conv!(F2Dot14, to_f32, from_f32, f32);
float_conv!(F4Dot12, to_f32, from_f32, f32);
float_conv!(F6Dot10, to_f32, from_f32, f32);
float_conv!(F48Dot16, to_f64, from_f64, f64);
float_conv!(F2Dot14, to_f64, from_f64, f64, no_fmt);
float_conv!(F4Dot12, to_f64, from_f64, f64, no_fmt);
float_conv!(F6Dot10, to_f64, from_f64, f64, no_fmt);

float_conv!(Fixed, to_f64, from_f64, f64);
float_conv!(F26Dot6, to_f64, from_f64, f64);
crate::newtype_scalar!(F2Dot14, [u8; 2]);
crate::newtype_scalar!(F4Dot12, [u8; 2]);
crate::newtype_scalar!(F6Dot10, [u8; 2]);
crate::newtype_scalar!(Fixed, [u8; 4]);

impl Fixed {
    /// Creates a 16.16 fixed point value from a 32 bit integer.
    #[inline(always)]
    pub const fn from_i32(i: i32) -> Self {
        Self(i << 16)
    }

    /// Converts a 16.16 fixed point value to a 32 bit integer, rounding off
    /// the fractional bits.
    #[inline(always)]
    pub const fn to_i32(self) -> i32 {
        self.0.wrapping_add(0x8000) >> 16
    }

    /// Converts a 16.16 to 26.6 fixed point value.
    #[inline(always)]
    pub const fn to_f26dot6(self) -> F26Dot6 {
        F26Dot6(self.0.wrapping_add(0x200) >> 10)
    }

    /// Converts a 16.16 to 2.14 fixed point value.
    ///
    /// This specific conversion is defined by the spec:
    /// <https://learn.microsoft.com/en-us/typography/opentype/spec/otvaroverview#coordinate-scales-and-normalization>
    ///
    /// "5. Convert the final, normalized 16.16 coordinate value to 2.14 by this method: add 0x00000002,
    /// and sign-extend shift to the right by 2."
    #[inline(always)]
    pub const fn to_f2dot14(self) -> F2Dot14 {
        F2Dot14((self.0.wrapping_add(2) >> 2) as _)
    }

    /// Converts a 16.16 fixed point value to a single precision floating
    /// point value.
    ///
    /// This operation is lossy. Use `to_f64()` for a lossless conversion.
    #[inline(always)]
    pub fn to_f32(self) -> f32 {
        const SCALE_FACTOR: f32 = 1.0 / 65536.0;
        self.0 as f32 * SCALE_FACTOR
    }

    /// Converts a 16.16 to a 48.16 fixed point value.
    ///
    /// This conversion is exact.
    #[inline(always)]
    pub const fn to_f48dot16(self) -> F48Dot16 {
        F48Dot16(self.0 as i64)
    }

    /// Applies an item variation delta, returning the varied value as a
    /// single precision floating point number.
    ///
    /// A delta for a 16.16 valued target is a raw integer count of the
    /// target's own quantum, so the accumulated 48.16 delta is scaled by
    /// 1/65536 on application. The result is intentionally not rounded
    /// back to 16.16.
    #[inline(always)]
    pub fn apply_delta(self, delta: F48Dot16) -> f32 {
        self.to_f32() + (delta.to_f64() / 65536.0) as f32
    }

    /// Multiplies by a 32 bit integer, producing the product as a 48.16
    /// fixed point value.
    ///
    /// This is the shape of variation delta accumulation: a raw integer
    /// delta scaled by a 16.16 scalar. The arithmetic wraps rather than
    /// panicking, following this module's style -- though the widened
    /// product of two 32 bit values is at most 2^62 and cannot actually
    /// wrap.
    #[inline(always)]
    pub const fn mul_i32(self, value: i32) -> F48Dot16 {
        F48Dot16((self.0 as i64).wrapping_mul(value as i64))
    }
}

impl From<i32> for Fixed {
    fn from(value: i32) -> Self {
        Self::from_i32(value)
    }
}

impl F26Dot6 {
    /// Creates a 26.6 fixed point value from a 32 bit integer.
    #[inline(always)]
    pub const fn from_i32(i: i32) -> Self {
        Self(i << 6)
    }

    /// Converts a 26.6 fixed point value to a 32 bit integer, rounding off
    /// the fractional bits.
    #[inline(always)]
    pub const fn to_i32(self) -> i32 {
        self.0.wrapping_add(32) >> 6
    }

    /// Converts a 26.6 fixed point value to a single precision floating
    /// point value.
    ///
    /// This operation is lossy. Use `to_f64()` for a lossless conversion.
    #[inline(always)]
    pub fn to_f32(self) -> f32 {
        const SCALE_FACTOR: f32 = 1.0 / 64.0;
        self.0 as f32 * SCALE_FACTOR
    }
}

impl F48Dot16 {
    /// Creates a 48.16 fixed point value from a 64 bit integer.
    #[inline(always)]
    pub const fn from_i64(i: i64) -> Self {
        Self(i << 16)
    }

    /// Creates a 48.16 fixed point value from a 32 bit integer.
    #[inline(always)]
    pub const fn from_i32(i: i32) -> Self {
        Self((i as i64) << 16)
    }

    /// Converts a 48.16 fixed point value to a 64 bit integer, rounding off
    /// the fractional bits.
    #[inline(always)]
    pub const fn to_i64(self) -> i64 {
        self.0.wrapping_add(0x8000) >> 16
    }

    /// Converts a 48.16 fixed point value to a 32 bit integer, rounding off
    /// the fractional bits.
    ///
    /// This truncates the integral bits if the value is too large to fit.
    #[inline(always)]
    pub const fn to_i32(self) -> i32 {
        self.to_i64() as i32
    }

    /// Converts a 48.16 fixed point value to a single precision floating
    /// point value.
    ///
    /// This operation is lossy.
    #[inline(always)]
    pub const fn to_f32(self) -> f32 {
        const SCALE_FACTOR: f64 = 1.0 / 65536.0;
        (self.0 as f64 * SCALE_FACTOR) as f32
    }

    /// Converts a 48.16 to a 16.16 fixed point value.
    ///
    /// The fractional bits carry over exactly; this truncates the integral
    /// bits if the value is too large to fit.
    #[inline(always)]
    pub const fn to_fixed(self) -> Fixed {
        Fixed(self.0 as i32)
    }
}

impl F2Dot14 {
    /// Applies an item variation delta, returning the varied value as a
    /// single precision floating point number.
    ///
    /// A delta for a 2.14 valued target is a raw integer count of the
    /// target's own quantum, so the accumulated 48.16 delta is scaled by
    /// one quarter of one percent -- 1/16384 -- on application. The result
    /// is intentionally not rounded back to 2.14.
    #[inline(always)]
    pub fn apply_delta(self, delta: F48Dot16) -> f32 {
        self.to_f32() + (delta.to_f64() / 16384.0) as f32
    }

    /// Converts a 2.14 to 16.16 fixed point value.
    #[inline(always)]
    pub const fn to_fixed(self) -> Fixed {
        Fixed(self.0 as i32 * 4)
    }
}

#[cfg(test)]
mod tests {
    #![allow(overflowing_literals)] // we want to specify byte values directly
    use super::*;

    #[test]
    fn f2dot14_floats() {
        // Examples from https://docs.microsoft.com/en-us/typography/opentype/spec/otff#data-types
        assert_eq!(F2Dot14(0x7fff), F2Dot14::from_f32(1.999939));
        assert_eq!(F2Dot14(0x7000), F2Dot14::from_f32(1.75));
        assert_eq!(F2Dot14(0x0001), F2Dot14::from_f32(0.0000610356));
        assert_eq!(F2Dot14(0x0000), F2Dot14::from_f32(0.0));
        assert_eq!(F2Dot14(0xffff), F2Dot14::from_f32(-0.000061));
        assert_eq!(F2Dot14(0x8000), F2Dot14::from_f32(-2.0));
    }

    #[test]
    fn roundtrip_f2dot14() {
        for i in i16::MIN..=i16::MAX {
            let val = F2Dot14(i);
            assert_eq!(val, F2Dot14::from_f32(val.to_f32()));
        }
    }

    #[test]
    fn round_f2dot14() {
        assert_eq!(F2Dot14(0x7000).round(), F2Dot14::from_f32(-2.0));
        assert_eq!(F2Dot14(0x1F00).round(), F2Dot14::from_f32(0.0));
        assert_eq!(F2Dot14(0x2000).round(), F2Dot14::from_f32(1.0));
    }

    #[test]
    fn round_fixed() {
        //TODO: make good test cases
        assert_eq!(Fixed(0x0001_7FFE).round(), Fixed(0x0001_0000));
        assert_eq!(Fixed(0x0001_7FFF).round(), Fixed(0x0001_0000));
        assert_eq!(Fixed(0x0001_8000).round(), Fixed(0x0002_0000));
    }

    // disabled because it's slow; these were just for my edification anyway
    //#[test]
    //fn roundtrip_fixed() {
    //for i in i32::MIN..=i32::MAX {
    //let val = Fixed(i);
    //assert_eq!(val, Fixed::from_f64(val.to_f64()));
    //}
    //}

    #[test]
    fn fixed_floats() {
        assert_eq!(Fixed(0x7fff_0000), Fixed::from_f64(32767.));
        assert_eq!(Fixed(0x7000_0001), Fixed::from_f64(28672.00001525879));
        assert_eq!(Fixed(0x0001_0000), Fixed::from_f64(1.0));
        assert_eq!(Fixed(0x0000_0000), Fixed::from_f64(0.0));
        assert_eq!(
            Fixed(i32::from_be_bytes([0xff; 4])),
            Fixed::from_f64(-0.000015259)
        );
        assert_eq!(Fixed(0x7fff_ffff), Fixed::from_f64(32768.0));
    }

    // We lost the f64::round() intrinsic when dropping std and the
    // alternative implementation was very slightly incorrect, throwing
    // off some tests. This makes sure we match.
    #[test]
    fn fixed_floats_rounding() {
        fn with_round_intrinsic(x: f64) -> Fixed {
            Fixed((x * 65536.0).round() as i32)
        }
        // These particular values were tripping up tests
        let inputs = [0.05, 0.6, 0.2, 0.4, 0.67755];
        for input in inputs {
            assert_eq!(Fixed::from_f64(input), with_round_intrinsic(input));
            // Test negated values as well for good measure
            assert_eq!(Fixed::from_f64(-input), with_round_intrinsic(-input));
        }
    }

    #[test]
    fn fixed_to_int() {
        assert_eq!(Fixed::from_f64(1.0).to_i32(), 1);
        assert_eq!(Fixed::from_f64(1.5).to_i32(), 2);
        assert_eq!(F26Dot6::from_f64(1.0).to_i32(), 1);
        assert_eq!(F26Dot6::from_f64(1.5).to_i32(), 2);
    }

    #[test]
    fn fixed_from_int() {
        assert_eq!(Fixed::from_i32(1000).to_bits(), 1000 << 16);
        assert_eq!(F26Dot6::from_i32(1000).to_bits(), 1000 << 6);
    }

    #[test]
    fn fixed_to_f26dot6() {
        assert_eq!(Fixed::from_f64(42.5).to_f26dot6(), F26Dot6::from_f64(42.5));
    }

    #[test]
    fn fixed_muldiv() {
        assert_eq!(
            Fixed::from_f64(0.5) * Fixed::from_f64(2.0),
            Fixed::from_f64(1.0)
        );
        assert_eq!(
            Fixed::from_f64(0.5) / Fixed::from_f64(2.0),
            Fixed::from_f64(0.25)
        );
    }

    // OSS Fuzz caught panic with overflow in fixed point division.
    // See <https://oss-fuzz.com/testcase-detail/5666843647082496> and
    // <https://issues.oss-fuzz.com/issues/443104630>
    #[test]
    fn fixed_div_neg_overflow() {
        let a = Fixed::from_f64(-92.5);
        let b = Fixed::from_f64(0.0028228759765625);
        // Just don't panic with overflow
        let _ = a / b;
    }

    #[test]
    fn fixed_mul_div_neg_overflow() {
        let a = Fixed::from_f64(-92.5);
        let b = Fixed::from_f64(0.0028228759765625);
        // Just don't panic with overflow
        let _ = a.mul_div(Fixed::ONE, b);
    }

    #[test]
    fn fixed_div_min_value() {
        // i32::MIN.abs() overflows i32, unsigned_abs() handles this correctly
        let min = Fixed(i32::MIN);
        let one = Fixed::ONE;
        // Just don't panic with overflow
        let _ = min / one;
        // Dividing by -1 is also an edge case
        let neg_one = Fixed(-Fixed::ONE.0);
        let _ = min / neg_one;
    }

    #[test]
    fn fixed_abs_min_value() {
        // Just don't panic with overflow; we use wrapping arithmetic to
        // match FT.
        assert_eq!(Fixed(i32::MIN).abs(), Fixed(i32::MIN));
    }

    #[test]
    fn fixed_neg_min_value() {
        // Just don't panic with overflow; we use wrapping arithmetic to
        // match FT.
        assert_eq!(-Fixed(i32::MIN), Fixed(i32::MIN));
    }

    #[test]
    fn f48dot16_floats() {
        assert_eq!(F48Dot16(0x0001_0000), F48Dot16::from_f64(1.0));
        assert_eq!(F48Dot16(0x0000_0000), F48Dot16::from_f64(0.0));
        assert_eq!(F48Dot16(0x0001_8000), F48Dot16::from_f64(1.5));
        assert_eq!(F48Dot16(-0x0001_8000), F48Dot16::from_f64(-1.5));
        assert_eq!(F48Dot16(0x0000_0001), F48Dot16::EPSILON);
        // Values far outside the 16.16 range are representable.
        assert_eq!(
            F48Dot16((1i64 << 40) * 65536),
            F48Dot16::from_f64((1u64 << 40) as f64)
        );
        assert_eq!(
            F48Dot16::from_f64(1099511627776.5).to_f64(),
            1099511627776.5
        );
    }

    #[test]
    fn f48dot16_round() {
        assert_eq!(F48Dot16(0x0001_7FFF).round(), F48Dot16(0x0001_0000));
        assert_eq!(F48Dot16(0x0001_8000).round(), F48Dot16(0x0002_0000));
        assert_eq!(F48Dot16::from_f64(-1.5).round(), F48Dot16::from_f64(-1.0));
    }

    #[test]
    fn f48dot16_to_int() {
        assert_eq!(F48Dot16::from_f64(1.0).to_i64(), 1);
        assert_eq!(F48Dot16::from_f64(1.5).to_i64(), 2);
        assert_eq!(F48Dot16::from_f64(-1.5).to_i64(), -1);
        assert_eq!(F48Dot16::from_f64(1099511627776.25).to_i64(), 1099511627776);
    }

    #[test]
    fn f48dot16_from_int() {
        assert_eq!(F48Dot16::from_i64(1000).to_bits(), 1000 << 16);
        assert_eq!(F48Dot16::from_i64(1 << 40).to_bits(), 1 << 56);
    }

    /// Widening from 16.16 is exact over the whole range, and narrowing
    /// returns exactly when the value fits.
    #[test]
    fn f48dot16_fixed_conversions() {
        for bits in [0, 1, -1, 0x1234_5678, i32::MAX, i32::MIN] {
            let fixed = Fixed(bits);
            let wide = fixed.to_f48dot16();
            assert_eq!(wide.to_bits(), bits as i64);
            assert_eq!(wide.to_fixed(), fixed);
        }
        // Out of range narrows truncate, wrapping like the sibling
        // conversions rather than saturating.
        assert_eq!(F48Dot16(i32::MAX as i64 + 1).to_fixed(), Fixed(i32::MIN));
        assert_eq!(F48Dot16(i32::MIN as i64 - 1).to_fixed(), Fixed(i32::MAX));
    }

    #[test]
    fn f48dot16_mul_i32() {
        assert_eq!(Fixed::from_f64(0.5).mul_i32(3), F48Dot16::from_f64(1.5));
        assert_eq!(Fixed::ONE.mul_i32(-7), F48Dot16::from_i64(-7));
        assert_eq!(Fixed::ZERO.mul_i32(i32::MAX), F48Dot16::ZERO);
        // The extreme product fits without overflow: |i32::MIN| * |i32::MIN|
        // is 2^62, inside i64.
        assert_eq!(Fixed(i32::MIN).mul_i32(i32::MIN), F48Dot16(1i64 << 62));
    }

    /// The variation delta shape: raw deltas scaled and summed exactly,
    /// then rounded once at the end.
    #[test]
    fn f48dot16_delta_accumulation() {
        let deltas = [100i32, -250, 37];
        let scalars = [
            Fixed::from_f64(1.0),
            Fixed::from_f64(0.5),
            Fixed::from_f64(0.25),
        ];
        let mut accum = F48Dot16::ZERO;
        for (delta, scalar) in deltas.iter().zip(&scalars) {
            accum += scalar.mul_i32(*delta);
        }
        // 100 - 125 + 9.25 = -15.75, which rounds to the nearest integer.
        assert_eq!(accum, F48Dot16::from_f64(-15.75));
        assert_eq!(accum.to_i64(), -16);
        assert_eq!(accum.round(), F48Dot16::from_f64(-16.0));
    }

    /// The delta application methods must agree exactly with the former
    /// `FloatItemDeltaTarget` impls in read-fonts that they replaced, which
    /// computed base + raw * (1 / quantum) through an f64 raw count.
    #[test]
    fn f48dot16_apply_delta() {
        let raws = [0i64, 1, -1, 100, -32768, 65536, 1 << 40, -(1 << 40)];
        for raw_bits in raws {
            let delta = F48Dot16(raw_bits);
            let raw = delta.to_f64();
            // Fixed targets scale by 1/65536.
            let base = Fixed::from_f64(1.5);
            assert_eq!(
                base.apply_delta(delta),
                base.to_f32() + (raw * (1.0 / 65536.0)) as f32
            );
            // 2.14 targets scale by 1/16384.
            let base = F2Dot14::from_f32(0.25);
            assert_eq!(
                base.apply_delta(delta),
                base.to_f32() + (raw * (1.0 / 16384.0)) as f32
            );
        }
        // Spot values: one quantum of delta moves a target by one quantum.
        assert_eq!(
            Fixed::ZERO.apply_delta(F48Dot16::from_i64(1)),
            1.0 / 65536.0
        );
        assert_eq!(
            F2Dot14::from_f32(0.0).apply_delta(F48Dot16::from_i64(1)),
            1.0 / 16384.0
        );
        assert_eq!(
            F2Dot14::from_f32(0.5).apply_delta(F48Dot16::from_f64(-0.5)),
            0.5 - 0.5 / 16384.0
        );
    }

    #[test]
    fn f26dot6_muldiv() {
        assert_eq!(
            F26Dot6::from_f64(0.5) * F26Dot6::from_f64(2.0),
            F26Dot6::from_f64(1.0)
        );
        assert_eq!(
            F26Dot6::from_f64(0.5) * F26Dot6::from_f64(-2.4),
            F26Dot6::from_f64(-1.2)
        );
        assert_eq!(F26Dot6::ONE * F26Dot6::ONE, F26Dot6::ONE);
        assert_eq!(
            F26Dot6::from_f64(0.5) / F26Dot6::from_f64(2.0),
            F26Dot6::from_f64(0.25)
        );
        assert_eq!(
            F26Dot6::from_f64(0.5) / F26Dot6::from_f64(-2.4),
            F26Dot6::from_f64(-0.20833333333333334)
        );
        assert_eq!(
            F26Dot6::from_f64(2.0) / F26Dot6::from_f64(3.0),
            F26Dot6::from_f64(0.6666666666666666)
        );
        assert_eq!(F26Dot6::ONE / F26Dot6::ONE, F26Dot6::ONE);
        assert_eq!(F26Dot6::ONE / F26Dot6::ZERO, F26Dot6(0x7FFFFFFF));
        assert_eq!(-F26Dot6::ONE / F26Dot6::ZERO, F26Dot6(-0x7FFFFFFF));
    }

    #[cfg(feature = "std")]
    mod display {
        use super::*;

        #[test]
        fn f2dot14() {
            let cases = [
                (F2Dot14(9830), "0.6"),
                (F2Dot14(-10139), "-0.61884"),
                (F2Dot14::ONE, "1"),
                (F2Dot14::NEG_ONE, "-1"),
                (F2Dot14::ZERO, "0"),
                (F2Dot14::MIN, "-2"),
                (F2Dot14::MAX, "1.99994"),
                (F2Dot14::EPSILON, "0.00006"),
                (F2Dot14(-1), "-0.00006"),
                (F2Dot14::from_f32(0.5), "0.5"),
                (F2Dot14::from_f32(-0.25), "-0.25"),
            ];
            for (value, expected) in cases {
                assert_eq!(value.to_string(), expected, "{:?}", value.to_bits());
            }
        }

        #[test]
        fn f4dot12_f6dot10() {
            assert_eq!(F4Dot12::from_f32(0.6).to_string(), "0.6");
            assert_eq!(F4Dot12::MIN.to_string(), "-8");
            assert_eq!(F4Dot12::MAX.to_string(), "7.9998");
            assert_eq!(F6Dot10::from_f32(-0.6).to_string(), "-0.6");
            assert_eq!(F6Dot10::MIN.to_string(), "-32");
            assert_eq!(F6Dot10::MAX.to_string(), "31.999");
        }

        #[test]
        fn fixed() {
            let cases = [
                (Fixed::from_f64(0.6), "0.6"),
                (Fixed::from_f64(-0.6), "-0.6"),
                (Fixed::from_f64(-1.5), "-1.5"),
                (Fixed::from_i32(1000), "1000"),
                (Fixed::from_f64(32767.5), "32767.5"),
                (Fixed::MAX, "32767.99998"),
                (Fixed::MIN, "-32768"),
                (Fixed::EPSILON, "0.00002"),
                (Fixed(-1), "-0.00002"),
            ];
            for (value, expected) in cases {
                assert_eq!(value.to_string(), expected, "{:?}", value.to_bits());
            }
        }

        #[test]
        fn f26dot6() {
            let cases = [
                (F26Dot6(1), "0.02"),
                (F26Dot6(-3), "-0.05"),
                (F26Dot6(32), "0.5"),
                (F26Dot6(65), "1.02"),
                (F26Dot6(-96), "-1.5"),
                (F26Dot6(100 * 64 + 16), "100.25"),
                (F26Dot6::from_i32(-7), "-7"),
                (F26Dot6::MIN, "-33554432"),
                (F26Dot6::MAX, "33554431.98"),
            ];
            for (value, expected) in cases {
                assert_eq!(value.to_string(), expected, "{:?}", value.to_bits());
            }
        }

        #[test]
        fn f48dot16() {
            let cases = [
                (F48Dot16::from_f64(0.6), "0.6"),
                (F48Dot16::from_i64(1 << 40), "1099511627776"),
                (F48Dot16::from_f64(-1099511627776.5), "-1099511627776.5"),
                (F48Dot16::MAX, "140737488355327.99998"),
                (F48Dot16::MIN, "-140737488355328"),
                (F48Dot16(i64::MIN + 1), "-140737488355327.99998"),
            ];
            for (value, expected) in cases {
                assert_eq!(value.to_string(), expected, "{:?}", value.to_bits());
            }
        }

        /// Checks that `text` parses back to `raw` and is as short as possible.
        ///
        /// A decimal with one fewer place that also round-trips would lie
        /// within half a step of the value, and then so would one of the two
        /// such decimals nearest it. `from_f64` returns `None` for inputs it
        /// would saturate, since those don't round-trip in any real sense.
        fn check_shortest(
            raw: i64,
            text: &str,
            fract_bits: u32,
            from_f64: impl Fn(f64) -> Option<i64>,
        ) {
            assert_ne!(text, "-0");
            let parsed: f64 = text.parse().unwrap();
            assert_eq!(from_f64(parsed), Some(raw), "{text} does not round-trip");
            let places = text
                .split_once('.')
                .map_or(0, |(_, fract)| fract.len() as u32);
            if places == 0 {
                return;
            }
            let pow = 10i128.pow(places - 1);
            let floor = (i128::from(raw) * pow).div_euclid(1 << fract_bits);
            for shorter in [floor, floor + 1] {
                let shorter = shorter as f64 / pow as f64;
                assert_ne!(from_f64(shorter), Some(raw), "{text} could be {shorter}");
            }
        }

        macro_rules! check_all {
            ($fixed:ident, $raws:expr) => {
                let one = $fixed::ONE.to_bits() as f64;
                let min = ($fixed::MIN.to_bits() as f64 - 0.5) / one;
                let max = ($fixed::MAX.to_bits() as f64 + 0.5) / one;
                for raw in $raws {
                    check_shortest(
                        raw as i64,
                        &$fixed(raw).to_string(),
                        $fixed::FRACT_BITS as u32,
                        |x| (min < x && x < max).then(|| $fixed::from_f64(x).to_bits() as i64),
                    );
                }
            };
        }

        #[test]
        fn shortest_all_f2dot14() {
            check_all!(F2Dot14, i16::MIN..=i16::MAX);
        }

        #[test]
        fn shortest_all_f4dot12() {
            check_all!(F4Dot12, i16::MIN..=i16::MAX);
        }

        #[test]
        fn shortest_all_f6dot10() {
            check_all!(F6Dot10, i16::MIN..=i16::MAX);
        }

        fn sampled_i32() -> impl Iterator<Item = i32> {
            (i32::MIN..=i32::MIN + 2000)
                .chain(-200_000..=200_000)
                .chain((i32::MIN..=i32::MAX).step_by(65_521))
                .chain(i32::MAX - 2000..=i32::MAX)
        }

        #[test]
        fn shortest_sampled_fixed() {
            check_all!(Fixed, sampled_i32());
        }

        #[test]
        fn shortest_sampled_f26dot6() {
            check_all!(F26Dot6, sampled_i32());
        }

        #[test]
        fn formatter_flags() {
            let value = F2Dot14(9830);
            assert_eq!(format!("{value:.2}"), "0.60");
            assert_eq!(format!("{value:.2?}"), "0.60");
            assert_eq!(format!("{value:>8}"), "     0.6");
            assert_eq!(format!("{value:8}"), "     0.6");
            assert_eq!(format!("{value:<8}|"), "0.6     |");
            assert_eq!(format!("{value:*^9}"), "***0.6***");
            assert_eq!(format!("{value:08}"), "000000.6");
            assert_eq!(format!("{value:+}"), "+0.6");
            assert_eq!(format!("{value:?}"), "0.6");
            // Where the shortest decimal is also what f64 prints, every flag
            // should agree with it.
            for (value, float) in [(F2Dot14::from_f32(-1.5), -1.5f64), (F2Dot14::ONE, 1.0)] {
                assert_eq!(format!("{value}"), format!("{float}"));
                assert_eq!(format!("{value:.3}"), format!("{float:.3}"));
                assert_eq!(format!("{value:>8}"), format!("{float:>8}"));
                assert_eq!(format!("{value:<8}"), format!("{float:<8}"));
                assert_eq!(format!("{value:08}"), format!("{float:08}"));
                assert_eq!(format!("{value:+09}"), format!("{float:+09}"));
            }
        }
    }

    #[cfg(feature = "serde")]
    mod serde {
        use super::*;

        macro_rules! roundtrip_one {
            ($fixed:ident) => {{
                let before = <$fixed>::ONE;
                let serialized = ::serde_json::to_string(&before).expect("should serialize");
                assert_eq!(&serialized, "1.0");
                let after = ::serde_json::from_str(&serialized).expect("should deserialize");
                assert_eq!(before, after);
            }};
        }

        #[test]
        fn one_is_one_f2dot14() {
            roundtrip_one!(F2Dot14);
        }

        #[test]
        fn one_is_one_f4dot12() {
            roundtrip_one!(F4Dot12);
        }

        #[test]
        fn one_is_one_f6dot10() {
            roundtrip_one!(F6Dot10);
        }

        #[test]
        fn one_is_one_fixed() {
            roundtrip_one!(Fixed);
        }

        #[test]
        fn one_is_one_f26dot6() {
            roundtrip_one!(F26Dot6);
        }

        macro_rules! roundtrip_all {
            ($fixed:ident, $ty:ty) => {
                for raw in <$ty>::MIN..=<$ty>::MAX {
                    let fixed = $fixed(raw);
                    let fixed_float = fixed.to_f64();

                    let json_value = ::serde_json::to_value(&fixed).expect("should serialize");
                    let json_float = json_value
                        .as_f64()
                        .expect("serde didn't serialize the value to a float");

                    // Normally directly comparing floats is flawed, but these
                    // should have been converted to float using the exact same
                    // method each, so I wouldn't expect them to be different
                    assert_eq!(
                        fixed_float,
                        json_float,
                        "failed on {raw} ({fixed_type}({raw:#X})): {json_float} != {fixed_float}",
                        fixed_type = ::std::stringify!($fixed),
                    );
                }
            };
        }

        #[test]
        fn roundtrip_all_f2dot14() {
            roundtrip_all!(F2Dot14, i16);
        }

        #[test]
        fn roundtrip_all_f4dot12() {
            roundtrip_all!(F4Dot12, i16);
        }

        #[test]
        fn roundtrip_all_f6dot10() {
            roundtrip_all!(F6Dot10, i16);
        }

        #[test]
        #[ignore = "enumerating all i32 values takes a while"]
        fn roundtrip_all_fixed() {
            roundtrip_all!(Fixed, i32);
        }

        #[test]
        #[ignore = "enumerating all i32 values takes a while"]
        fn roundtrip_all_f26dot6() {
            roundtrip_all!(F26Dot6, i32);
        }
    }
}
