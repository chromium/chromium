//! `strength_reduce` implements integer division and modulo via "arithmetic strength reduction"
//!
//! Modern processors can do multiplication and shifts much faster than division, and "arithmetic strength reduction" is an algorithm to transform divisions into multiplications and shifts.
//! Compilers already perform this optimization for divisors that are known at compile time; this library enables this optimization for divisors that are only known at runtime.
//!
//! Benchmarking shows a 5-10x speedup or integer division and modulo operations.
//!
//! # Example:
//! ```
//! use strength_reduce::StrengthReducedU64;
//! 
//! let mut my_array: Vec<u64> = (0..500).collect();
//! let divisor = 3;
//! let modulo = 14;
//!
//! // slow naive division and modulo
//! for element in &mut my_array {
//!     *element = (*element / divisor) % modulo;
//! }
//!
//! // fast strength-reduced division and modulo
//! let reduced_divisor = StrengthReducedU64::new(divisor);
//! let reduced_modulo = StrengthReducedU64::new(modulo);
//! for element in &mut my_array {
//!     *element = (*element / reduced_divisor) % reduced_modulo;
//! }
//! ```
//!
//! This library is intended for hot loops like the example above, where a division is repeated many times in a loop with the divisor remaining unchanged. 
//! There is a setup cost associated with creating stength-reduced division instances, so using strength-reduced division for 1-2 divisions is not worth the setup cost.
//! The break-even point differs by use-case, but is typically low: Benchmarking has shown that takes 3 to 4 repeated divisions with the same StengthReduced## instance to be worth it.
//! 
//! `strength_reduce` is `#![no_std]`
//!
//! The optimizations that this library provides are inherently dependent on architecture, compiler, and platform,
//! so test before you use. 
#![no_std]

#[cfg(test)]
extern crate num_bigint;
#[cfg(test)]
extern crate rand;

use core::ops::{Div, Rem};

mod long_division;
mod long_multiplication;

/// Implements unsigned division and modulo via mutiplication and shifts.
///
/// Creating a an instance of this struct is more expensive than a single division, but if the division is repeated,
/// this version will be several times faster than naive division.
#[derive(Clone, Copy, Debug)]
pub struct StrengthReducedU8 {
    multiplier: u16,
    divisor: u8,
}
impl StrengthReducedU8 {
    /// Creates a new divisor instance.
    ///
    /// If possible, avoid calling new() from an inner loop: The intended usage is to create an instance of this struct outside the loop, and use it for divison and remainders inside the loop.
    ///
    /// # Panics:
    /// 
    /// Panics if `divisor` is 0
    #[inline]
    pub fn new(divisor: u8) -> Self {
        assert!(divisor > 0);

        if divisor.is_power_of_two() { 
            Self{ multiplier: 0, divisor }
        } else {
            let divided = core::u16::MAX / (divisor as u16);
            Self{ multiplier: divided + 1, divisor }
        }
    }

    /// Simultaneous truncated integer division and modulus.
    /// Returns `(quotient, remainder)`.
    #[inline]
    pub fn div_rem(numerator: u8, denom: Self) -> (u8, u8) {
        let quotient = numerator / denom;
        let remainder = numerator % denom;
        (quotient, remainder)
    }

    /// Retrieve the value used to create this struct
    #[inline]
    pub fn get(&self) -> u8 {
        self.divisor
    }
}

impl Div<StrengthReducedU8> for u8 {
    type Output = u8;

    #[inline]
    fn div(self, rhs: StrengthReducedU8) -> Self::Output {
        if rhs.multiplier == 0 {
            (self as u16 >> rhs.divisor.trailing_zeros()) as u8
        } else {
            let numerator = self as u16;
            let multiplied_hi = numerator * (rhs.multiplier >> 8);
            let multiplied_lo = (numerator * rhs.multiplier as u8 as u16) >> 8;

            ((multiplied_hi + multiplied_lo) >> 8) as u8
        }
    }
}

impl Rem<StrengthReducedU8> for u8 {
    type Output = u8;

    #[inline]
    fn rem(self, rhs: StrengthReducedU8) -> Self::Output {
        if rhs.multiplier == 0 {
            self & (rhs.divisor - 1)
        } else {
            let product = rhs.multiplier.wrapping_mul(self as u16) as u32;
            let divisor = rhs.divisor as u32;

            let shifted = (product * divisor) >> 16;
            shifted as u8
        }
    }
}

// small types prefer to do work in the intermediate type
macro_rules! strength_reduced_u16 {
    ($struct_name:ident, $primitive_type:ident) => (
        /// Implements unsigned division and modulo via mutiplication and shifts.
        ///
        /// Creating a an instance of this struct is more expensive than a single division, but if the division is repeated,
        /// this version will be several times faster than naive division.
        #[derive(Clone, Copy, Debug)]
        pub struct $struct_name {
            multiplier: u32,
            divisor: $primitive_type,
        }
        impl $struct_name {
            /// Creates a new divisor instance.
            ///
            /// If possible, avoid calling new() from an inner loop: The intended usage is to create an instance of this struct outside the loop, and use it for divison and remainders inside the loop.
            ///
            /// # Panics:
            /// 
            /// Panics if `divisor` is 0
            #[inline]
            pub fn new(divisor: $primitive_type) -> Self {
                assert!(divisor > 0);

                if divisor.is_power_of_two() { 
                    Self{ multiplier: 0, divisor }
                } else {
                    let divided = core::u32::MAX / (divisor as u32);
                    Self{ multiplier: divided + 1, divisor }
                }
            }

            /// Simultaneous truncated integer division and modulus.
            /// Returns `(quotient, remainder)`.
            #[inline]
            pub fn div_rem(numerator: $primitive_type, denom: Self) -> ($primitive_type, $primitive_type) {
                let quotient = numerator / denom;
                let remainder = numerator - quotient * denom.divisor;
                (quotient, remainder)
            }

            /// Retrieve the value used to create this struct
            #[inline]
            pub fn get(&self) -> $primitive_type {
                self.divisor
            }
        }

        impl Div<$struct_name> for $primitive_type {
            type Output = $primitive_type;

            #[inline]
            fn div(self, rhs: $struct_name) -> Self::Output {
                if rhs.multiplier == 0 {
                    self >> rhs.divisor.trailing_zeros()
                } else {
                    let numerator = self as u32;
                    let multiplied_hi = numerator * (rhs.multiplier >> 16);
                    let multiplied_lo = (numerator * rhs.multiplier as u16 as u32) >> 16;

                    ((multiplied_hi + multiplied_lo) >> 16) as $primitive_type
                }
            }
        }

        impl Rem<$struct_name> for $primitive_type {
            type Output = $primitive_type;

            #[inline]
            fn rem(self, rhs: $struct_name) -> Self::Output {
                if rhs.multiplier == 0 {
                    self & (rhs.divisor - 1)
                } else {
                    let quotient = self / rhs;
                    self - quotient * rhs.divisor
                }
            }
        }
    )
}

// small types prefer to do work in the intermediate type
macro_rules! strength_reduced_u32 {
    ($struct_name:ident, $primitive_type:ident) => (
        /// Implements unsigned division and modulo via mutiplication and shifts.
        ///
        /// Creating a an instance of this struct is more expensive than a single division, but if the division is repeated,
        /// this version will be several times faster than naive division.
        #[derive(Clone, Copy, Debug)]
        pub struct $struct_name {
            multiplier: u64,
            divisor: $primitive_type,
        }
        impl $struct_name {
            /// Creates a new divisor instance.
            ///
            /// If possible, avoid calling new() from an inner loop: The intended usage is to create an instance of this struct outside the loop, and use it for divison and remainders inside the loop.
            ///
            /// # Panics:
            /// 
            /// Panics if `divisor` is 0
            #[inline]
            pub fn new(divisor: $primitive_type) -> Self {
                assert!(divisor > 0);

                if divisor.is_power_of_two() { 
                    Self{ multiplier: 0, divisor }
                } else {
                    let divided = core::u64::MAX / (divisor as u64);
                    Self{ multiplier: divided + 1, divisor }
                }
            }

            /// Simultaneous truncated integer division and modulus.
            /// Returns `(quotient, remainder)`.
            #[inline]
            pub fn div_rem(numerator: $primitive_type, denom: Self) -> ($primitive_type, $primitive_type) {
                if denom.multiplier == 0 {
                    (numerator >> denom.divisor.trailing_zeros(), numerator & (denom.divisor - 1))
                }
                else {
                    let numerator64 = numerator as u64;
                    let multiplied_hi = numerator64 * (denom.multiplier >> 32);
                    let multiplied_lo = numerator64 * (denom.multiplier as u32 as u64) >> 32;

                    let quotient = ((multiplied_hi + multiplied_lo) >> 32) as $primitive_type;
                    let remainder = numerator - quotient * denom.divisor;
                    (quotient, remainder)
                }
            }

            /// Retrieve the value used to create this struct
            #[inline]
            pub fn get(&self) -> $primitive_type {
                self.divisor
            }
        }

        impl Div<$struct_name> for $primitive_type {
            type Output = $primitive_type;

            #[inline]
            fn div(self, rhs: $struct_name) -> Self::Output {
                if rhs.multiplier == 0 {
                    self >> rhs.divisor.trailing_zeros()
                } else {
                    let numerator = self as u64;
                    let multiplied_hi = numerator * (rhs.multiplier >> 32);
                    let multiplied_lo = numerator * (rhs.multiplier as u32 as u64) >> 32;

                    ((multiplied_hi + multiplied_lo) >> 32) as $primitive_type
                }
            }
        }

        impl Rem<$struct_name> for $primitive_type {
            type Output = $primitive_type;

            #[inline]
            fn rem(self, rhs: $struct_name) -> Self::Output {
                if rhs.multiplier == 0 {
                    self & (rhs.divisor - 1)
                } else {
                    let product = rhs.multiplier.wrapping_mul(self as u64) as u128;
                    let divisor = rhs.divisor as u128;

                    let shifted = (product * divisor) >> 64;
                    shifted as $primitive_type
                }
            }
        }
    )
}

macro_rules! strength_reduced_u64 {
    ($struct_name:ident, $primitive_type:ident) => (
        /// Implements unsigned division and modulo via mutiplication and shifts.
        ///
        /// Creating a an instance of this struct is more expensive than a single division, but if the division is repeated,
        /// this version will be several times faster than naive division.
        #[derive(Clone, Copy, Debug)]
        pub struct $struct_name {
            multiplier: u128,
            divisor: $primitive_type,
        }
        impl $struct_name {
            /// Creates a new divisor instance.
            ///
            /// If possible, avoid calling new() from an inner loop: The intended usage is to create an instance of this struct outside the loop, and use it for divison and remainders inside the loop.
            ///
            /// # Panics:
            /// 
            /// Panics if `divisor` is 0
            #[inline]
            pub fn new(divisor: $primitive_type) -> Self {
                assert!(divisor > 0);

                if divisor.is_power_of_two() { 
                    Self{ multiplier: 0, divisor }
                } else {
                    let quotient = long_division::divide_128_max_by_64(divisor as u64);
                    Self{ multiplier: quotient + 1, divisor }
                }
            }
            /// Simultaneous truncated integer division and modulus.
            /// Returns `(quotient, remainder)`.
            #[inline]
            pub fn div_rem(numerator: $primitive_type, denom: Self) -> ($primitive_type, $primitive_type) {
                if denom.multiplier == 0 {
                    (numerator >> denom.divisor.trailing_zeros(), numerator & (denom.divisor - 1))
                }
                else {
                    let numerator128 = numerator as u128;
                    let multiplied_hi = numerator128 * (denom.multiplier >> 64);
                    let multiplied_lo = numerator128 * (denom.multiplier as u64 as u128) >> 64;

                    let quotient = ((multiplied_hi + multiplied_lo) >> 64) as $primitive_type;
                    let remainder = numerator - quotient * denom.divisor;
                    (quotient, remainder)
                }
            }

            /// Retrieve the value used to create this struct
            #[inline]
            pub fn get(&self) -> $primitive_type {
                self.divisor
            }
        }

        impl Div<$struct_name> for $primitive_type {
            type Output = $primitive_type;

            #[inline]
            fn div(self, rhs: $struct_name) -> Self::Output {
                if rhs.multiplier == 0 {
                    self >> rhs.divisor.trailing_zeros()
                } else {
                    let numerator = self as u128;
                    let multiplied_hi = numerator * (rhs.multiplier >> 64);
                    let multiplied_lo = numerator * (rhs.multiplier as u64 as u128) >> 64;

                    ((multiplied_hi + multiplied_lo) >> 64) as $primitive_type
                }
            }
        }

        impl Rem<$struct_name> for $primitive_type {
            type Output = $primitive_type;

            #[inline]
            fn rem(self, rhs: $struct_name) -> Self::Output {
                if rhs.multiplier == 0 {
                    self & (rhs.divisor - 1)
                } else {
                    let quotient = self / rhs;
                    self - quotient * rhs.divisor
                }
            }
        }
    )
}

/// Implements unsigned division and modulo via mutiplication and shifts.
///
/// Creating a an instance of this struct is more expensive than a single division, but if the division is repeated,
/// this version will be several times faster than naive division.
#[derive(Clone, Copy, Debug)]
pub struct StrengthReducedU128 {
    multiplier_hi: u128,
    multiplier_lo: u128,
    divisor: u128,
}
impl StrengthReducedU128 {
    /// Creates a new divisor instance.
    ///
    /// If possible, avoid calling new() from an inner loop: The intended usage is to create an instance of this struct outside the loop, and use it for divison and remainders inside the loop.
    ///
    /// # Panics:
    /// 
    /// Panics if `divisor` is 0
    #[inline]
    pub fn new(divisor: u128) -> Self {
        assert!(divisor > 0);

        if divisor.is_power_of_two() { 
            Self{ multiplier_hi: 0, multiplier_lo: 0, divisor }
        } else {
            let (quotient_hi, quotient_lo) = long_division::divide_256_max_by_128(divisor);
            let multiplier_lo = quotient_lo.wrapping_add(1);
            let multiplier_hi = if multiplier_lo == 0 { quotient_hi + 1 } else { quotient_hi };
            Self{ multiplier_hi, multiplier_lo, divisor }
        }
    }

    /// Simultaneous truncated integer division and modulus.
    /// Returns `(quotient, remainder)`.
    #[inline]
    pub fn div_rem(numerator: u128, denom: Self) -> (u128, u128) {
        let quotient = numerator / denom;
        let remainder = numerator - quotient * denom.divisor;
        (quotient, remainder)
    }

    /// Retrieve the value used to create this struct
    #[inline]
    pub fn get(&self) -> u128 {
        self.divisor
    }
}

impl Div<StrengthReducedU128> for u128 {
    type Output = u128;

    #[inline]
    fn div(self, rhs: StrengthReducedU128) -> Self::Output {
        if rhs.multiplier_hi == 0 {
            self >> rhs.divisor.trailing_zeros()
        } else {
            long_multiplication::multiply_256_by_128_upperbits(rhs.multiplier_hi, rhs.multiplier_lo, self)
        }
    }
}

impl Rem<StrengthReducedU128> for u128 {
    type Output = u128;

    #[inline]
    fn rem(self, rhs: StrengthReducedU128) -> Self::Output {
        if rhs.multiplier_hi == 0 {
            self & (rhs.divisor - 1)
        } else {
             let quotient = long_multiplication::multiply_256_by_128_upperbits(rhs.multiplier_hi, rhs.multiplier_lo, self);
             self - quotient * rhs.divisor
        }
    }
}

// We just hardcoded u8 and u128 since they will never be a usize. for the rest, we have macros, so we can reuse the same code for usize
strength_reduced_u16!(StrengthReducedU16, u16);
strength_reduced_u32!(StrengthReducedU32, u32);
strength_reduced_u64!(StrengthReducedU64, u64);

// Our definition for usize will depend on how big usize is
#[cfg(target_pointer_width = "16")]
strength_reduced_u16!(StrengthReducedUsize, usize);
#[cfg(target_pointer_width = "32")]
strength_reduced_u32!(StrengthReducedUsize, usize);
#[cfg(target_pointer_width = "64")]
strength_reduced_u64!(StrengthReducedUsize, usize);

#[cfg(test)]
mod unit_tests {
    use super::*;

    macro_rules! reduction_test {
        ($test_name:ident, $struct_name:ident, $primitive_type:ident) => (
            #[test]
            fn $test_name() {
                let max = core::$primitive_type::MAX;
                let divisors = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,max-1,max];
                let numerators = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20];

                for &divisor in &divisors {
                    let reduced_divisor = $struct_name::new(divisor);
                    for &numerator in &numerators {
                        let expected_div = numerator / divisor;
                        let expected_rem = numerator % divisor;

                        let reduced_div = numerator / reduced_divisor;

                        assert_eq!(expected_div, reduced_div, "Divide failed with numerator: {}, divisor: {}", numerator, divisor);
                        let reduced_rem = numerator % reduced_divisor;

                        let (reduced_combined_div, reduced_combined_rem) = $struct_name::div_rem(numerator, reduced_divisor);

                        
                        assert_eq!(expected_rem, reduced_rem, "Modulo failed with numerator: {}, divisor: {}", numerator, divisor);
                        assert_eq!(expected_div, reduced_combined_div, "div_rem divide failed with numerator: {}, divisor: {}", numerator, divisor);
                        assert_eq!(expected_rem, reduced_combined_rem, "div_rem modulo failed with numerator: {}, divisor: {}", numerator, divisor);
                    }
                }
            }
        )
    }

    reduction_test!(test_strength_reduced_u8, StrengthReducedU8, u8);
    reduction_test!(test_strength_reduced_u16, StrengthReducedU16, u16);
    reduction_test!(test_strength_reduced_u32, StrengthReducedU32, u32);
    reduction_test!(test_strength_reduced_u64, StrengthReducedU64, u64);
    reduction_test!(test_strength_reduced_usize, StrengthReducedUsize, usize);
    reduction_test!(test_strength_reduced_u128, StrengthReducedU128, u128);
}
