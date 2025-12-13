// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Experimental.

use fixed_decimal::Decimal;
use icu_plurals::{PluralRules, PluralRulesPreferences};
use icu_provider::prelude::*;

use crate::{
    compactdecimal::CompactDecimalFormatter,
    compactdecimal::CompactDecimalFormatterOptions,
    compactdecimal::CompactDecimalFormatterPreferences,
    dimension::provider::currency::{
        extended::CurrencyExtendedDataV1, patterns::CurrencyPatternsDataV1,
    },
};
use icu_locale_core::preferences::{define_preferences, prefs_convert};

use super::{long_compact_format::FormattedLongCompactCurrency, CurrencyCode};

extern crate alloc;

define_preferences!(
    /// The preferences for currency formatting.
    [Copy]
    LongCompactCurrencyFormatterPreferences,
    {
        numbering_system: super::super::preferences::NumberingSystem
    }
);

prefs_convert!(
    LongCompactCurrencyFormatterPreferences,
    CompactDecimalFormatterPreferences,
    { numbering_system }
);
prefs_convert!(
    LongCompactCurrencyFormatterPreferences,
    PluralRulesPreferences
);

/// A formatter for monetary values.
///
/// [`LongCompactCurrencyFormatter`] supports:
///   1. Rendering in the locale's currency system.
///   2. Locale-sensitive grouping separator positions.
pub struct LongCompactCurrencyFormatter {
    /// Extended data for the currency formatter.
    extended: DataPayload<CurrencyExtendedDataV1>,

    /// Formatting patterns for each currency plural category.
    patterns: DataPayload<CurrencyPatternsDataV1>,

    /// A [`CompactDecimalFormatter`] to format the currency value in compact form.
    compact_decimal_formatter: CompactDecimalFormatter,

    /// A [`PluralRules`] to determine the plural category of the unit.
    plural_rules: PluralRules,
}

impl LongCompactCurrencyFormatter {
    icu_provider::gen_buffer_data_constructors!(
        (
            prefs: LongCompactCurrencyFormatterPreferences,
            currency_code: &CurrencyCode
        ) -> error: DataError,
        functions: [
            try_new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    /// Creates a new [`LongCompactCurrencyFormatter`] from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new(
        prefs: LongCompactCurrencyFormatterPreferences,
        currency_code: &CurrencyCode,
    ) -> Result<Self, DataError> {
        let compact_decimal_formatter = CompactDecimalFormatter::try_new_long(
            (&prefs).into(),
            CompactDecimalFormatterOptions::default(),
        )?;

        let marker_attributes = DataMarkerAttributes::try_from_str(currency_code.0.as_str())
            .map_err(|_| {
                DataErrorKind::IdentifierNotFound
                    .into_error()
                    .with_debug_context("failed to get data marker attribute from a `CurrencyCode`")
            })?;

        let locale = &CurrencyPatternsDataV1::make_locale(prefs.locale_preferences);

        let extended = crate::provider::Baked
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    marker_attributes,
                    locale,
                ),
                ..Default::default()
            })?
            .payload;

        let patterns = crate::provider::Baked.load(Default::default())?.payload;

        let plural_rules = PluralRules::try_new_cardinal((&prefs).into())?;

        Ok(Self {
            extended,
            patterns,
            compact_decimal_formatter,
            plural_rules,
        })
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D>(
        provider: &D,
        prefs: LongCompactCurrencyFormatterPreferences,
        currency_code: &CurrencyCode,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<crate::dimension::provider::currency::extended::CurrencyExtendedDataV1>
            + DataProvider<crate::dimension::provider::currency::patterns::CurrencyPatternsDataV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>
            + DataProvider<crate::compactdecimal::provider::LongCompactDecimalFormatDataV1>,
    {
        let locale = CurrencyPatternsDataV1::make_locale(prefs.locale_preferences);

        let marker_attributes = DataMarkerAttributes::try_from_str(currency_code.0.as_str())
            .map_err(|_| {
                DataErrorKind::IdentifierNotFound
                    .into_error()
                    .with_debug_context("failed to get data marker attribute from a `CurrencyCode`")
            })?;

        let extended = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    marker_attributes,
                    &locale,
                ),
                ..Default::default()
            })?
            .payload;

        let patterns = provider.load(Default::default())?.payload;

        let plural_rules = PluralRules::try_new_cardinal_unstable(provider, (&prefs).into())?;

        let compact_decimal_formatter = CompactDecimalFormatter::try_new_long_unstable(
            provider,
            (&prefs).into(),
            CompactDecimalFormatterOptions::default(),
        )?;

        Ok(Self {
            extended,
            patterns,
            compact_decimal_formatter,
            plural_rules,
        })
    }

    /// Formats in the long format a [`Decimal`] value for the given currency code.
    ///
    /// # Examples
    /// ```
    /// use icu::experimental::dimension::currency::long_compact_formatter::LongCompactCurrencyFormatter;
    /// use icu::experimental::dimension::currency::CurrencyCode;
    /// use icu::locale::locale;
    /// use tinystr::*;
    /// use writeable::Writeable;
    ///
    /// let currency_prefs = locale!("en-US").into();
    /// let currency_code = CurrencyCode(tinystr!(3, "USD"));
    /// let fmt = LongCompactCurrencyFormatter::try_new(currency_prefs, &currency_code).unwrap();
    /// let value = "12345.67".parse().unwrap();
    /// let formatted_currency = fmt.format_fixed_decimal(&value, currency_code);
    /// let mut sink = String::new();
    /// formatted_currency.write_to(&mut sink).unwrap();
    /// assert_eq!(sink.as_str(), "12 thousand US dollars");
    /// ```
    pub fn format_fixed_decimal<'l>(
        &'l self,
        value: &'l Decimal,
        currency_code: CurrencyCode,
    ) -> FormattedLongCompactCurrency<'l> {
        FormattedLongCompactCurrency {
            signed_fixed_decimal: value,
            _currency_code: currency_code,
            extended: self.extended.get(),
            patterns: self.patterns.get(),
            compact_decimal_formatter: &self.compact_decimal_formatter,
            plural_rules: &self.plural_rules,
        }
    }
}
