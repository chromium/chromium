// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

//! IEEE 754 half-precision (binary16) floating-point type.
//!
//! This is a minimal implementation providing only the operations needed for JPEG XL decoding,
//! avoiding external dependencies like `half` which pulls in `zerocopy`.

/// IEEE 754 binary16 half-precision floating-point type.
///
/// Format: 1 sign bit, 5 exponent bits (bias 15), 10 mantissa bits.
#[allow(non_camel_case_types)]
#[derive(Copy, Clone, Default, PartialEq, Eq, Hash)]
#[repr(transparent)]
pub struct f16(u16);

impl f16 {
    /// Positive zero.
    pub const ZERO: Self = Self(0);

    /// Creates an f16 from its raw bit representation.
    #[inline]
    pub const fn from_bits(bits: u16) -> Self {
        Self(bits)
    }

    /// Returns the raw bit representation.
    #[inline]
    pub const fn to_bits(self) -> u16 {
        self.0
    }

    /// Converts to f32.
    #[inline]
    pub fn to_f32(self) -> f32 {
        let bits = self.0;
        let sign = ((bits >> 15) & 1) as u32;
        let exp = ((bits >> 10) & 0x1F) as u32;
        let mant = (bits & 0x3FF) as u32;

        let f32_bits = if exp == 0 {
            if mant == 0 {
                // Zero (signed)
                sign << 31
            } else {
                // Denormal f16 -> normalized f32
                // Find the leading 1 bit in mantissa
                let mut m = mant;
                let mut e = 0u32;
                while (m & 0x400) == 0 {
                    m <<= 1;
                    e += 1;
                }
                m &= 0x3FF; // Remove the implicit leading 1
                // f16 denormal exponent is -14 (not -15), adjust by shift count
                let new_exp = 127 - 14 - e;
                (sign << 31) | (new_exp << 23) | (m << 13)
            }
        } else if exp == 31 {
            // Infinity or NaN
            if mant == 0 {
                // Infinity
                (sign << 31) | (0xFF << 23)
            } else {
                // NaN - preserve some payload bits, ensure quiet NaN
                (sign << 31) | (0xFF << 23) | (mant << 13) | 0x0040_0000
            }
        } else {
            // Normal number
            // Rebias: f16 uses bias 15, f32 uses bias 127
            // new_exp = exp - 15 + 127 = exp + 112
            let new_exp = exp + 112;
            (sign << 31) | (new_exp << 23) | (mant << 13)
        };

        f32::from_bits(f32_bits)
    }

    /// Creates an f16 from an f32.
    #[inline]
    pub fn from_f32(f: f32) -> Self {
        let bits = f.to_bits();
        let sign = ((bits >> 31) & 1) as u16;
        let exp = ((bits >> 23) & 0xFF) as i32;
        let mant = bits & 0x007F_FFFF;

        let h_bits = if exp == 0 {
            // Zero or f32 denormal -> f16 zero (too small)
            sign << 15
        } else if exp == 255 {
            // Infinity or NaN
            if mant == 0 {
                (sign << 15) | (0x1F << 10) // Infinity
            } else {
                (sign << 15) | (0x1F << 10) | 0x0200 // Quiet NaN
            }
        } else {
            let unbiased = exp - 127;

            if unbiased < -24 {
                // Too small, underflow to zero
                sign << 15
            } else if unbiased < -14 {
                // Denormal f16
                let shift = (-14 - unbiased) as u32;
                let m = ((mant | 0x0080_0000) >> (shift + 14)) as u16;
                (sign << 15) | m
            } else if unbiased > 15 {
                // Overflow to infinity
                (sign << 15) | (0x1F << 10)
            } else {
                // Normal f16
                let h_exp = (unbiased + 15) as u16;
                let h_mant = (mant >> 13) as u16;

                // Round to nearest, ties to even
                let round_bit = (mant >> 12) & 1;
                let sticky = mant & 0x0FFF;
                let h_mant = if round_bit == 1 && (sticky != 0 || (h_mant & 1) == 1) {
                    h_mant + 1
                } else {
                    h_mant
                };

                // Handle mantissa overflow from rounding
                if h_mant > 0x3FF {
                    if h_exp >= 30 {
                        // Overflow to infinity
                        (sign << 15) | (0x1F << 10)
                    } else {
                        (sign << 15) | ((h_exp + 1) << 10)
                    }
                } else {
                    (sign << 15) | (h_exp << 10) | h_mant
                }
            }
        };

        Self(h_bits)
    }

    /// Creates an f16 from an f64.
    #[inline]
    pub fn from_f64(f: f64) -> Self {
        // Convert via f32 - sufficient precision for f16
        Self::from_f32(f as f32)
    }

    /// Converts to f64.
    #[inline]
    pub fn to_f64(self) -> f64 {
        self.to_f32() as f64
    }

    /// Returns true if this is neither infinite nor NaN.
    #[inline]
    pub fn is_finite(self) -> bool {
        // Exponent of 31 means infinity or NaN
        ((self.0 >> 10) & 0x1F) != 31
    }

    /// Returns the bytes in little-endian order.
    #[inline]
    pub const fn to_le_bytes(self) -> [u8; 2] {
        self.0.to_le_bytes()
    }

    /// Returns the bytes in big-endian order.
    #[inline]
    pub const fn to_be_bytes(self) -> [u8; 2] {
        self.0.to_be_bytes()
    }
}

impl From<f16> for f32 {
    #[inline]
    fn from(f: f16) -> f32 {
        f.to_f32()
    }
}

impl From<f16> for f64 {
    #[inline]
    fn from(f: f16) -> f64 {
        f.to_f64()
    }
}

impl core::fmt::Debug for f16 {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{}", self.to_f32())
    }
}

impl core::fmt::Display for f16 {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{}", self.to_f32())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_zero() {
        let z = f16::ZERO;
        assert_eq!(z.to_bits(), 0);
        assert_eq!(z.to_f32(), 0.0);
        assert!(z.is_finite());
    }

    #[test]
    fn test_one() {
        // 1.0 in f16: sign=0, exp=15 (biased), mant=0 -> 0x3C00
        let one = f16::from_bits(0x3C00);
        assert!((one.to_f32() - 1.0).abs() < 1e-6);
        assert!(one.is_finite());
    }

    #[test]
    fn test_negative_one() {
        // -1.0 in f16: sign=1, exp=15, mant=0 -> 0xBC00
        let neg_one = f16::from_bits(0xBC00);
        assert!((neg_one.to_f32() - (-1.0)).abs() < 1e-6);
    }

    #[test]
    fn test_infinity() {
        // +Inf: sign=0, exp=31, mant=0 -> 0x7C00
        let inf = f16::from_bits(0x7C00);
        assert!(inf.to_f32().is_infinite());
        assert!(!inf.is_finite());

        // -Inf: 0xFC00
        let neg_inf = f16::from_bits(0xFC00);
        assert!(neg_inf.to_f32().is_infinite());
        assert!(!neg_inf.is_finite());
    }

    #[test]
    fn test_nan() {
        // NaN: exp=31, mant!=0 -> 0x7C01 (or any mant != 0)
        let nan = f16::from_bits(0x7C01);
        assert!(nan.to_f32().is_nan());
        assert!(!nan.is_finite());
    }

    #[test]
    fn test_denormal() {
        // Smallest positive denormal: 0x0001
        let tiny = f16::from_bits(0x0001);
        let val = tiny.to_f32();
        assert!(val > 0.0);
        assert!(val < 1e-6);
        assert!(tiny.is_finite());
    }

    #[test]
    fn test_roundtrip_normal() {
        let test_values: [f32; 8] = [0.5, 1.0, 2.0, 100.0, 0.001, -0.5, -1.0, -100.0];
        for &v in &test_values {
            let h = f16::from_f32(v);
            let back = h.to_f32();
            // f16 has limited precision, allow ~0.1% error for normal values
            let rel_err = ((v - back) / v).abs();
            assert!(
                rel_err < 0.002,
                "Roundtrip failed for {}: got {}, rel_err {}",
                v,
                back,
                rel_err
            );
        }
    }

    #[test]
    fn test_roundtrip_special() {
        // Zero
        assert_eq!(f16::from_f32(0.0).to_f32(), 0.0);

        // Infinity
        assert!(f16::from_f32(f32::INFINITY).to_f32().is_infinite());
        assert!(f16::from_f32(f32::NEG_INFINITY).to_f32().is_infinite());

        // NaN
        assert!(f16::from_f32(f32::NAN).to_f32().is_nan());
    }

    #[test]
    fn test_overflow_to_infinity() {
        // f16 max is ~65504, values above should overflow to infinity
        let big = f16::from_f32(100000.0);
        assert!(big.to_f32().is_infinite());
    }

    #[test]
    fn test_underflow_to_zero() {
        // Very small values should underflow to zero
        let tiny = f16::from_f32(1e-10);
        assert_eq!(tiny.to_f32(), 0.0);
    }

    #[test]
    fn test_bytes() {
        let h = f16::from_bits(0x1234);
        assert_eq!(h.to_le_bytes(), [0x34, 0x12]);
        assert_eq!(h.to_be_bytes(), [0x12, 0x34]);
    }
}
