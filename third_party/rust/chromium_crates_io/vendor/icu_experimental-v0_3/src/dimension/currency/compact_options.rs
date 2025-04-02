// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Options for [`CompactCurrencyFormatter`](crate::dimension::currency::compact_formatter::CompactCurrencyFormatter).

/// A collection of configuration options that determine the formatting behavior of
/// [`CompactCurrencyFormatter`](crate::dimension::currency::compact_formatter::CompactCurrencyFormatter).
#[derive(Copy, Debug, Eq, PartialEq, Clone, Default)]
#[non_exhaustive]
pub struct CompactCurrencyFormatterOptions {
    /// The width of the currency format.
    pub width: Width,
}

impl From<Width> for CompactCurrencyFormatterOptions {
    fn from(width: Width) -> Self {
        Self { width }
    }
}

#[derive(Debug, Eq, PartialEq, Clone, Copy, Default)]
#[non_exhaustive]
pub enum Width {
    /// Format the currency with the standard (short) currency symbol.
    ///
    /// For example, 1 USD formats as "$1.00" in en-US and "US$1" in most other locales. // TODO: check this
    #[default]
    Short,

    /// Format the currency with the narrow currency symbol.
    ///
    /// The narrow symbol may be ambiguous, so it should be evident from context which
    /// currency is being represented.
    ///
    /// For example, 1 USD formats as "$1.00" in most locales. // TODO: check this
    Narrow,
}
