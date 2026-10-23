// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Options for [`CurrencyFormatter`](crate::dimension::currency::formatter::CurrencyFormatter).

#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};

/// A collection of configuration options that determine the formatting behavior of
/// [`CurrencyFormatter`](crate::dimension::currency::formatter::CurrencyFormatter).
#[derive(Copy, Debug, Eq, PartialEq, Clone, Default, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[non_exhaustive]
pub struct CurrencyFormatterOptions {
    /// Whether to use standard or accounting currency patterns.
    pub usage: CurrencyUsage,
}

impl From<CurrencyUsage> for CurrencyFormatterOptions {
    fn from(usage: CurrencyUsage) -> Self {
        Self { usage }
    }
}

/// Controls whether currency formatting uses standard or accounting patterns.
///
/// Corresponds to ECMA-402 `currencySign`.
#[derive(Copy, Debug, Eq, PartialEq, Clone, Default, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[non_exhaustive]
pub enum CurrencyUsage {
    /// Standard currency formatting (default).
    ///
    /// Negative values typically use a leading minus sign, e.g. `-$1,234.56`.
    #[default]
    Standard,

    /// Accounting currency formatting.
    ///
    /// Negative values may use locale-specific accounting patterns such as
    /// parentheses, e.g. `($1,234.56)` in `en-US`.
    Accounting,
}
