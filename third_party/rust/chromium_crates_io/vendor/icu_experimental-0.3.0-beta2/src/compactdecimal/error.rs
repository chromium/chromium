// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use displaydoc::Display;

/// An error due to a [`CompactDecimal`](fixed_decimal::CompactDecimal) with an
/// exponent inconsistent with the compact decimal data for the locale, e.g.,
/// when formatting 1c5 in English (US).
#[derive(Display, Copy, Clone, Debug)]
#[displaydoc("Expected compact exponent {expected} for 10^{log10_type}, got {actual}")]
pub struct ExponentError {
    /// The compact decimal exponent passed to the formatter.
    pub(crate) actual: u8,
    /// The appropriate compact decimal exponent for a number of the given magnitude.
    pub(crate) expected: u8,
    /// The magnitude of the number being formatted.
    pub(crate) log10_type: i16,
}

impl core::error::Error for ExponentError {}
