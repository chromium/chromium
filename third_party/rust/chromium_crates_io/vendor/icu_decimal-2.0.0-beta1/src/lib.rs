// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Formatting basic decimal numbers.
//!
//! This module is published as its own crate ([`icu_decimal`](https://docs.rs/icu_decimal/latest/icu_decimal/))
//! and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.
//!
//! Support for currencies, measurement units, and compact notation is planned. To track progress,
//! follow [icu4x#275](https://github.com/unicode-org/icu4x/issues/275).
//!
//! # Examples
//!
//! ## Format a number with Bangla digits
//!
//! ```
//! use fixed_decimal::FixedDecimal;
//! use icu::decimal::FixedDecimalFormatter;
//! use icu::locale::locale;
//! use writeable::assert_writeable_eq;
//!
//! let fdf = FixedDecimalFormatter::try_new(
//!     locale!("bn").into(),
//!     Default::default(),
//! )
//! .expect("locale should be present");
//!
//! let fixed_decimal = FixedDecimal::from(1000007);
//!
//! assert_writeable_eq!(fdf.format(&fixed_decimal), "১০,০০,০০৭");
//! ```
//!
//! ## Format a number with digits after the decimal separator
//!
//! ```
//! use fixed_decimal::FixedDecimal;
//! use icu::decimal::FixedDecimalFormatter;
//! use icu::locale::Locale;
//! use writeable::assert_writeable_eq;
//!
//! let fdf =
//!     FixedDecimalFormatter::try_new(Default::default(), Default::default())
//!         .expect("locale should be present");
//!
//! let fixed_decimal = FixedDecimal::from(200050).multiplied_pow10(-2);
//!
//! assert_writeable_eq!(fdf.format(&fixed_decimal), "2,000.50");
//! ```
//!
//! ### Format a number using an alternative numbering system
//!
//! Numbering systems specified in the `-u-nu` subtag will be followed as long as the locale has
//! symbols for that numbering system.
//!
//! ```
//! use fixed_decimal::FixedDecimal;
//! use icu::decimal::FixedDecimalFormatter;
//! use icu::locale::locale;
//! use writeable::assert_writeable_eq;
//!
//! let fdf = FixedDecimalFormatter::try_new(
//!     locale!("th-u-nu-thai").into(),
//!     Default::default(),
//! )
//! .expect("locale should be present");
//!
//! let fixed_decimal = FixedDecimal::from(1000007);
//!
//! assert_writeable_eq!(fdf.format(&fixed_decimal), "๑,๐๐๐,๐๐๗");
//! ```
//!
//! [`FixedDecimalFormatter`]: FixedDecimalFormatter

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
#![warn(missing_docs)]

extern crate alloc;

mod format;
mod grouper;
pub mod options;
pub mod provider;
pub(crate) mod size_test_macro;

pub use format::FormattedFixedDecimal;

use alloc::string::String;
use fixed_decimal::FixedDecimal;
use icu_locale_core::locale;
use icu_locale_core::preferences::{
    define_preferences, extensions::unicode::keywords::NumberingSystem,
};
use icu_provider::prelude::*;
use size_test_macro::size_test;
use writeable::Writeable;

size_test!(FixedDecimalFormatter, fixed_decimal_formatter_size, 96);

define_preferences!(
    /// The preferences for fixed decimal formatting.
    [Copy]
    FixedDecimalFormatterPreferences,
    {
        /// Numbering System. Corresponds to the `-u-nu` in Unicode Locale Identifier.
        numbering_system: NumberingSystem
    }
);

/// A formatter for [`FixedDecimal`], rendering decimal digits in an i18n-friendly way.
///
/// [`FixedDecimalFormatter`] supports:
///
/// 1. Rendering in the local numbering system
/// 2. Locale-sensitive grouping separator positions
/// 3. Locale-sensitive plus and minus signs
///
/// Read more about the options in the [`options`] module.
///
/// See the crate-level documentation for examples.
#[doc = fixed_decimal_formatter_size!()]
#[derive(Debug)]
pub struct FixedDecimalFormatter {
    options: options::FixedDecimalFormatterOptions,
    symbols: DataPayload<provider::DecimalSymbolsV2Marker>,
    digits: DataPayload<provider::DecimalDigitsV1Marker>,
}

impl AsRef<FixedDecimalFormatter> for FixedDecimalFormatter {
    fn as_ref(&self) -> &FixedDecimalFormatter {
        self
    }
}

impl FixedDecimalFormatter {
    icu_provider::gen_any_buffer_data_constructors!(
        (prefs: FixedDecimalFormatterPreferences, options: options::FixedDecimalFormatterOptions) -> error: DataError,
        /// Creates a new [`FixedDecimalFormatter`] from compiled data and an options bag.
    );

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<
        D: DataProvider<provider::DecimalSymbolsV2Marker>
            + DataProvider<provider::DecimalDigitsV1Marker>
            + ?Sized,
    >(
        provider: &D,
        prefs: FixedDecimalFormatterPreferences,
        options: options::FixedDecimalFormatterOptions,
    ) -> Result<Self, DataError> {
        let locale = DataLocale::from_preferences_locale::<provider::DecimalSymbolsV2Marker>(
            prefs.locale_prefs,
        );
        let nu: &str = prefs
            .numbering_system
            .as_ref()
            .map(|s| s.as_str())
            .unwrap_or("");
        let symbols: DataPayload<provider::DecimalSymbolsV2Marker> = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    DataMarkerAttributes::from_str_or_panic(nu),
                    &locale,
                ),
                ..Default::default()
            })?
            .payload;

        let digits = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    DataMarkerAttributes::from_str_or_panic(symbols.get().numsys()),
                    &locale!("und").into(),
                ),
                ..Default::default()
            })?
            .payload;

        Ok(Self {
            options,
            symbols,
            digits,
        })
    }

    /// Formats a [`FixedDecimal`], returning a [`FormattedFixedDecimal`].
    pub fn format<'l>(&'l self, value: &'l FixedDecimal) -> FormattedFixedDecimal<'l> {
        FormattedFixedDecimal {
            value,
            options: &self.options,
            symbols: self.symbols.get(),
            digits: self.digits.get(),
        }
    }

    /// Formats a [`FixedDecimal`], returning a [`String`].
    pub fn format_to_string(&self, value: &FixedDecimal) -> String {
        self.format(value).write_to_string().into_owned()
    }
}
