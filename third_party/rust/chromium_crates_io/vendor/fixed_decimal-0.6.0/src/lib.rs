// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! `fixed_decimal` is a utility crate of the [`ICU4X`] project.
//!
//! It includes [`FixedDecimal`], a core API for representing numbers in a human-readable form
//! appropriate for formatting and plural rule selection. It is optimized for operations involving
//! the individual digits of a number.
//!
//! # Examples
//!
//! ```
//! use fixed_decimal::FixedDecimal;
//!
//! let dec = FixedDecimal::from(250).multiplied_pow10(-2);
//! assert_eq!("2.50", format!("{}", dec));
//!
//! #[derive(Debug, PartialEq)]
//! struct MagnitudeAndDigit(i16, u8);
//!
//! let digits: Vec<MagnitudeAndDigit> = dec
//!     .magnitude_range()
//!     .map(|m| MagnitudeAndDigit(m, dec.digit_at(m)))
//!     .collect();
//!
//! assert_eq!(
//!     vec![
//!         MagnitudeAndDigit(-2, 0),
//!         MagnitudeAndDigit(-1, 5),
//!         MagnitudeAndDigit(0, 2)
//!     ],
//!     digits
//! );
//! ```
//!
//! [`ICU4X`]: ../icu/index.html

// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
#![cfg_attr(not(any(test, feature = "std")), no_std)]
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
        clippy::exhaustive_structs,
        clippy::exhaustive_enums,
        missing_debug_implementations,
    )
)]

mod compact;
mod decimal;
mod integer;
mod ops;
mod scientific;
mod uint_iterator;

#[cfg(feature = "ryu")]
pub use decimal::FloatPrecision;

#[cfg(feature = "ryu")]
#[doc(no_inline)]
pub use FloatPrecision as DoublePrecision;

pub use compact::CompactDecimal;
pub use decimal::FixedDecimal;
pub use decimal::RoundingIncrement;
pub use decimal::RoundingMode;
pub use decimal::Sign;
pub use decimal::SignDisplay;
use displaydoc::Display;
pub use integer::FixedInteger;
pub use scientific::ScientificDecimal;

/// The magnitude or number of digits exceeds the limit of the [`FixedDecimal`].
///
/// The highest
/// magnitude of the most significant digit is [`i16::MAX`], and the lowest magnitude of the
/// least significant digit is [`i16::MIN`].
///
/// This error is also returned when constructing a [`FixedInteger`] from a [`FixedDecimal`] with a
/// fractional part.
///
/// # Examples
///
/// ```
/// use fixed_decimal::FixedDecimal;
/// use fixed_decimal::LimitError;
///
/// let mut dec1 = FixedDecimal::from(123);
/// dec1.multiply_pow10(i16::MAX);
/// assert!(dec1.is_zero());
/// ```
#[derive(Display, Debug, Copy, Clone, PartialEq)]
#[allow(clippy::exhaustive_structs)]
#[displaydoc("Magnitude or number of digits exceeded")]
pub struct LimitError;

/// An error involving FixedDecimal operations or conversion.
#[derive(Display, Debug, Copy, Clone, PartialEq)]
#[non_exhaustive]
pub enum ParseError {
    /// See [`LimitError`].
    #[displaydoc("Magnitude or number of digits exceeded")]
    Limit,
    /// The input of a string that is supposed to be converted to FixedDecimal is not accepted.
    ///
    /// Any string with non-digit characters (except for one '.' and one '-' at the beginning of the string) is not accepted.
    /// Also, empty string ("") and its negation ("-") are not accepted.
    /// Strings of form "12_345_678" are not accepted, the accepted format is "12345678".
    /// Also '.' shouldn't be first or the last characters, i. e. .123 and 123. are not accepted, and instead 0.123 and
    /// 123 (or 123.0) must be used.
    #[displaydoc("Failed to parse the input string")]
    Syntax,
}

#[cfg(feature = "std")]
impl std::error::Error for ParseError {}
