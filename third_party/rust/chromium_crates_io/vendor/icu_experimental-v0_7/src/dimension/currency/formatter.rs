// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::fmt::Display;

use super::super::provider::currency::{
    essentials::CurrencyEssentialsV1,
    extended::CurrencyExtendedDataV1,
    fractions::{CurrencyFractionsV1, FractionInfo, Rounding},
    no_currency::{CurrencyPatternsNoCurrency, CurrencyPatternsNoCurrencyV1},
    patterns::CurrencyPatternsDataV1,
    symbols::{CurrencyDecimalSymbolsV1, CurrencySymbolWidth, CurrencySymbolsV1},
};
use super::CurrencyType;
use fixed_decimal::{
    Decimal as FixedDecimal, RoundingIncrement, Sign, SignedRoundingMode, UnsignedRoundingMode,
};
use icu_decimal::preferences::CompactDecimalFormatterPreferences;
use icu_decimal::{
    AbstractFormatter, CompactDecimalFormatter, DecimalFormatter, DecimalFormatterPreferences,
};
use icu_locale_core::preferences::{define_preferences, prefs_convert};
use icu_plurals::{PluralRules, PluralRulesPreferences};
use icu_provider::prelude::*;
use tinystr::TinyAsciiStr;
use writeable::Writeable;

use super::options::{CurrencyFormatterOptions, CurrencyUsage};

extern crate alloc;

define_preferences!(
    /// The preferences for currency formatting.
    [Copy]
    CurrencyFormatterPreferences,
    {
        /// The user's preferred numbering system.
        ///
        /// Corresponds to the `-u-nu` in Unicode Locale Identifier.
        numbering_system: crate::dimension::preferences::NumberingSystem
    }
);

prefs_convert!(CurrencyFormatterPreferences, DecimalFormatterPreferences, {
    numbering_system
});
prefs_convert!(CurrencyFormatterPreferences, PluralRulesPreferences);
prefs_convert!(
    CurrencyFormatterPreferences,
    CompactDecimalFormatterPreferences,
    { numbering_system }
);

#[derive(Debug)]
pub(crate) enum CurrencyFormatterData {
    /// Formats using the ISO currency code while following the symbol pattern.
    IsoCodeSymbol {
        essential: DataPayload<CurrencyEssentialsV1>,
        iso_code: TinyAsciiStr<3>,
    },
    /// Formats using the ISO currency code while following the name pattern.
    IsoCodeName {
        patterns: DataPayload<CurrencyPatternsDataV1>,
        iso_code: TinyAsciiStr<3>,
    },
    Symbol {
        essential: DataPayload<CurrencyEssentialsV1>,
        symbol: DataPayload<CurrencySymbolsV1>,
    },
    Name {
        extended: DataPayload<CurrencyExtendedDataV1>,
        patterns: DataPayload<CurrencyPatternsDataV1>,
        plural_rules: PluralRules,
    },
    NoCurrency {
        patterns: DataPayload<CurrencyPatternsNoCurrencyV1>,
    },
}

/// A formatter for monetary values.
///
/// [`CurrencyFormatter`] supports:
///   1. Rendering in the locale's currency system.
///   2. Locale-sensitive grouping separator positions.
///
/// Read more about the options in the [`super::options`] module.
#[derive(Debug)]
pub struct CurrencyFormatter<V: AbstractFormatter> {
    value_formatter: V,
    currency_data: CurrencyFormatterData,
    usage: CurrencyUsage,
    fraction_info: FractionInfo,
}

impl<V: AbstractFormatter> CurrencyFormatter<V> {
    #[cfg(feature = "compiled_data")]
    pub(crate) fn try_new_essential(
        value_formatter: V,
        prefs: CurrencyFormatterPreferences,
        currency: CurrencyType,
        width: CurrencySymbolWidth,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        let locale = CurrencyEssentialsV1::make_locale(prefs.locale_preferences);
        let decimal_prefs = DecimalFormatterPreferences::from(&prefs);

        let req_id = decimal_prefs.nu_id(&locale);
        let default_id = DataIdentifierBorrowed::for_locale(&locale);
        let ids = req_id.into_iter().chain(core::iter::once(default_id));
        let essential =
            load_with_fallback::<CurrencyEssentialsV1>(&crate::provider::Baked, ids.clone())?
                .payload;
        let fractions: DataPayload<CurrencyFractionsV1> =
            crate::provider::Baked.load(Default::default())?.payload;
        let fraction_info = fractions.get().resolve(currency);
        let currency_data = match DataProvider::<CurrencySymbolsV1>::load(
            &crate::provider::Baked,
            DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    CurrencySymbolsV1::make_attributes(currency, width, &mut Default::default()),
                    &locale,
                ),
                ..Default::default()
            },
        )
        .allow_identifier_not_found()?
        {
            // An empty symbol means the locale explicitly has no symbol for this
            // currency, so format the amount with the no-currency pattern. This is
            // different from a missing entry (`None`), which falls back to the ISO code.
            Some(res) if res.payload.get().as_str().is_empty() => {
                let patterns = load_with_fallback::<CurrencyPatternsNoCurrencyV1>(
                    &crate::provider::Baked,
                    ids,
                )?
                .payload;
                CurrencyFormatterData::NoCurrency { patterns }
            }
            Some(res) => CurrencyFormatterData::Symbol {
                essential,
                symbol: res.payload,
            },
            None => CurrencyFormatterData::IsoCodeSymbol {
                essential,
                iso_code: currency.iso_code(),
            },
        };

        Ok(Self {
            value_formatter,
            currency_data,
            usage: options.usage,
            fraction_info,
        })
    }

    pub(crate) fn try_new_essential_unstable<D>(
        provider: &D,
        value_formatter: V,
        prefs: CurrencyFormatterPreferences,
        currency: CurrencyType,
        width: CurrencySymbolWidth,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyEssentialsV1>
            + DataProvider<CurrencySymbolsV1>
            + DataProvider<CurrencyPatternsNoCurrencyV1>
            + DataProvider<CurrencyFractionsV1>,
    {
        let locale = CurrencyEssentialsV1::make_locale(prefs.locale_preferences);
        let decimal_prefs = DecimalFormatterPreferences::from(&prefs);

        let req_id = decimal_prefs.nu_id(&locale);
        let default_id = DataIdentifierBorrowed::for_locale(&locale);
        let ids = req_id.into_iter().chain(core::iter::once(default_id));
        let essential = load_with_fallback::<CurrencyEssentialsV1>(provider, ids.clone())?.payload;
        let fractions: DataPayload<CurrencyFractionsV1> =
            provider.load(Default::default())?.payload;
        let fraction_info = fractions.get().resolve(currency);
        let currency_data = match DataProvider::<CurrencySymbolsV1>::load(
            provider,
            DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    CurrencySymbolsV1::make_attributes(currency, width, &mut Default::default()),
                    &locale,
                ),
                ..Default::default()
            },
        )
        .allow_identifier_not_found()?
        {
            // An empty symbol means the locale explicitly has no symbol for this
            // currency, so format the amount with the no-currency pattern. This is
            // different from a missing entry (`None`), which falls back to the ISO code.
            Some(res) if res.payload.get().as_str().is_empty() => {
                let patterns =
                    load_with_fallback::<CurrencyPatternsNoCurrencyV1>(provider, ids)?.payload;
                CurrencyFormatterData::NoCurrency { patterns }
            }
            Some(res) => CurrencyFormatterData::Symbol {
                essential,
                symbol: res.payload,
            },
            None => CurrencyFormatterData::IsoCodeSymbol {
                essential,
                iso_code: currency.iso_code(),
            },
        };

        Ok(Self {
            value_formatter,
            currency_data,
            usage: options.usage,
            fraction_info,
        })
    }

    #[cfg(feature = "compiled_data")]
    pub(crate) fn try_new_code_internal(
        value_formatter: V,
        prefs: CurrencyFormatterPreferences,
        currency: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        let locale = CurrencyEssentialsV1::make_locale(prefs.locale_preferences);
        let decimal_prefs = DecimalFormatterPreferences::from(&prefs);

        let req_id = decimal_prefs.nu_id(&locale);
        let default_id = DataIdentifierBorrowed::for_locale(&locale);
        let ids = req_id.into_iter().chain(core::iter::once(default_id));
        let essential =
            load_with_fallback::<CurrencyEssentialsV1>(&crate::provider::Baked, ids.clone())?
                .payload;
        let fractions: DataPayload<CurrencyFractionsV1> =
            crate::provider::Baked.load(Default::default())?.payload;
        let fraction_info = fractions.get().resolve(currency);

        Ok(Self {
            value_formatter,
            currency_data: CurrencyFormatterData::IsoCodeSymbol {
                essential,
                iso_code: currency.iso_code(),
            },
            usage: options.usage,
            fraction_info,
        })
    }

    pub(crate) fn try_new_code_internal_unstable<D>(
        provider: &D,
        value_formatter: V,
        prefs: CurrencyFormatterPreferences,
        currency: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized + DataProvider<CurrencyEssentialsV1> + DataProvider<CurrencyFractionsV1>,
    {
        let locale = CurrencyEssentialsV1::make_locale(prefs.locale_preferences);
        let decimal_prefs = DecimalFormatterPreferences::from(&prefs);

        let req_id = decimal_prefs.nu_id(&locale);
        let default_id = DataIdentifierBorrowed::for_locale(&locale);
        let ids = req_id.into_iter().chain(core::iter::once(default_id));
        let essential = load_with_fallback::<CurrencyEssentialsV1>(provider, ids.clone())?.payload;
        let fractions: DataPayload<CurrencyFractionsV1> =
            provider.load(Default::default())?.payload;
        let fraction_info = fractions.get().resolve(currency);

        Ok(Self {
            value_formatter,
            currency_data: CurrencyFormatterData::IsoCodeSymbol {
                essential,
                iso_code: currency.iso_code(),
            },
            usage: options.usage,
            fraction_info,
        })
    }

    #[cfg(feature = "compiled_data")]
    pub(crate) fn try_new_name_internal(
        value_formatter: V,
        prefs: CurrencyFormatterPreferences,
        currency: CurrencyType,
    ) -> Result<Self, DataError> {
        let locale = CurrencyPatternsDataV1::make_locale(prefs.locale_preferences);
        let iso_code = currency.iso_code();
        let marker_attributes =
            DataMarkerAttributes::try_from_str(iso_code.as_str()).map_err(|_| {
                DataErrorKind::IdentifierNotFound
                    .into_error()
                    .with_debug_context("failed to get data marker attribute from a `CurrencyType`")
            })?;
        // According to UTS #35, if no displayName is found, the currency code itself should be used.
        // https://www.unicode.org/reports/tr35/tr35-numbers.html#Plural_Rules_in_Currency_Formatting
        let extended_opt = crate::provider::Baked
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    marker_attributes,
                    &locale,
                ),
                ..Default::default()
            })
            .allow_identifier_not_found()?
            .map(|res| res.payload);

        let patterns = crate::provider::Baked.load(Default::default())?.payload;
        let fractions: DataPayload<CurrencyFractionsV1> =
            crate::provider::Baked.load(Default::default())?.payload;
        let fraction_info = fractions.get().resolve(currency);

        let currency_data = match extended_opt {
            Some(extended) => {
                let plural_rules = PluralRules::try_new_cardinal((&prefs).into())?;
                CurrencyFormatterData::Name {
                    extended,
                    patterns,
                    plural_rules,
                }
            }
            None => CurrencyFormatterData::IsoCodeName {
                patterns,
                iso_code: currency.iso_code(),
            },
        };

        Ok(Self {
            value_formatter,
            currency_data,
            usage: CurrencyUsage::default(),
            fraction_info,
        })
    }

    pub(crate) fn try_new_name_internal_unstable<D>(
        provider: &D,
        value_formatter: V,
        prefs: CurrencyFormatterPreferences,
        currency: CurrencyType,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyExtendedDataV1>
            + DataProvider<CurrencyPatternsDataV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        let locale = CurrencyPatternsDataV1::make_locale(prefs.locale_preferences);
        let iso_code = currency.iso_code();
        let marker_attributes =
            DataMarkerAttributes::try_from_str(iso_code.as_str()).map_err(|_| {
                DataErrorKind::IdentifierNotFound
                    .into_error()
                    .with_debug_context("failed to get data marker attribute from a `CurrencyType`")
            })?;
        // According to UTS #35, if no displayName is found, the currency code itself should be used.
        // https://www.unicode.org/reports/tr35/tr35-numbers.html#Plural_Rules_in_Currency_Formatting
        let extended_opt = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    marker_attributes,
                    &locale,
                ),
                ..Default::default()
            })
            .allow_identifier_not_found()?
            .map(|res| res.payload);

        let patterns = provider.load(Default::default())?.payload;
        let fractions: DataPayload<CurrencyFractionsV1> =
            provider.load(Default::default())?.payload;
        let fraction_info = fractions.get().resolve(currency);

        let currency_data = match extended_opt {
            Some(extended) => {
                let plural_rules =
                    PluralRules::try_new_cardinal_unstable(provider, (&prefs).into())?;
                CurrencyFormatterData::Name {
                    extended,
                    patterns,
                    plural_rules,
                }
            }
            None => CurrencyFormatterData::IsoCodeName {
                patterns,
                iso_code: currency.iso_code(),
            },
        };

        Ok(Self {
            value_formatter,
            currency_data,
            usage: CurrencyUsage::default(),
            fraction_info,
        })
    }

    #[cfg(feature = "compiled_data")]
    pub(crate) fn try_new_no_currency_internal(
        value_formatter: V,
        prefs: CurrencyFormatterPreferences,
        currency: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        let locale = CurrencyEssentialsV1::make_locale(prefs.locale_preferences);
        let decimal_prefs = DecimalFormatterPreferences::from(&prefs);

        let req_id = decimal_prefs.nu_id(&locale);
        let default_id = DataIdentifierBorrowed::for_locale(&locale);
        let ids = req_id.into_iter().chain(core::iter::once(default_id));
        let patterns =
            load_with_fallback::<CurrencyPatternsNoCurrencyV1>(&crate::provider::Baked, ids)?
                .payload;
        let fractions: DataPayload<CurrencyFractionsV1> =
            crate::provider::Baked.load(Default::default())?.payload;
        let fraction_info = fractions.get().resolve(currency);

        Ok(Self {
            value_formatter,
            currency_data: CurrencyFormatterData::NoCurrency { patterns },
            usage: options.usage,
            fraction_info,
        })
    }

    pub(crate) fn try_new_no_currency_internal_unstable<D>(
        provider: &D,
        value_formatter: V,
        prefs: CurrencyFormatterPreferences,
        currency: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized + DataProvider<CurrencyPatternsNoCurrencyV1> + DataProvider<CurrencyFractionsV1>,
    {
        let locale = CurrencyEssentialsV1::make_locale(prefs.locale_preferences);
        let decimal_prefs = DecimalFormatterPreferences::from(&prefs);

        let req_id = decimal_prefs.nu_id(&locale);
        let default_id = DataIdentifierBorrowed::for_locale(&locale);
        let ids = req_id.into_iter().chain(core::iter::once(default_id));
        let patterns = load_with_fallback::<CurrencyPatternsNoCurrencyV1>(provider, ids)?.payload;
        let fractions: DataPayload<CurrencyFractionsV1> =
            provider.load(Default::default())?.payload;
        let fraction_info = fractions.get().resolve(currency);

        Ok(Self {
            value_formatter,
            currency_data: CurrencyFormatterData::NoCurrency { patterns },
            usage: options.usage,
            fraction_info,
        })
    }
}

impl CurrencyFormatter<DecimalFormatter> {
    icu_provider::gen_buffer_data_constructors!(
        (
            prefs: CurrencyFormatterPreferences,
            currency_code: CurrencyType,
            options: CurrencyFormatterOptions
        ) -> error: DataError,
        functions: [
            try_new_symbol: skip,
            try_new_symbol_with_buffer_provider,
            try_new_symbol_unstable,
            Self
        ]
    );

    icu_provider::gen_buffer_data_constructors!(
        (
            prefs: CurrencyFormatterPreferences,
            currency_code: CurrencyType,
            options: CurrencyFormatterOptions
        ) -> error: DataError,
        functions: [
            try_new_symbol_narrow: skip,
            try_new_symbol_narrow_with_buffer_provider,
            try_new_symbol_narrow_unstable,
            Self
        ]
    );

    // We manually implement the compiled constructors because of the cross-crate dependency

    /// Creates a new [`CurrencyFormatter`] for formatting with short currency symbols from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_symbol(
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        Self::try_new_essential(
            DecimalFormatter::try_new_unstable(
                &WrappingProvider(
                    &crate::provider::Baked,
                    &icu_decimal::provider::Baked,
                    currency_code,
                ),
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            CurrencySymbolsV1::SHORT,
            options,
        )
    }

    /// Creates a new [`CurrencyFormatter`] for formatting with narrow currency symbols from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_symbol_narrow(
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        Self::try_new_essential(
            DecimalFormatter::try_new_unstable(
                &WrappingProvider(
                    &crate::provider::Baked,
                    &icu_decimal::provider::Baked,
                    currency_code,
                ),
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            CurrencySymbolsV1::NARROW,
            options,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_symbol)]
    pub fn try_new_symbol_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyEssentialsV1>
            + DataProvider<CurrencySymbolsV1>
            + DataProvider<CurrencyPatternsNoCurrencyV1>
            + DataProvider<CurrencyDecimalSymbolsV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>,
    {
        Self::try_new_essential_unstable(
            provider,
            DecimalFormatter::try_new_unstable(
                &WrappingProvider(provider, provider, currency_code),
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            CurrencySymbolsV1::SHORT,
            options,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_symbol_narrow)]
    pub fn try_new_symbol_narrow_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyEssentialsV1>
            + DataProvider<CurrencySymbolsV1>
            + DataProvider<CurrencyPatternsNoCurrencyV1>
            + DataProvider<CurrencyDecimalSymbolsV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>,
    {
        Self::try_new_essential_unstable(
            provider,
            DecimalFormatter::try_new_unstable(
                &WrappingProvider(provider, provider, currency_code),
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            CurrencySymbolsV1::NARROW,
            options,
        )
    }

    icu_provider::gen_buffer_data_constructors!(
        (
            prefs: CurrencyFormatterPreferences,
            currency_code: CurrencyType,
            options: CurrencyFormatterOptions
        ) -> error: DataError,
        functions: [
            try_new_code: skip,
            try_new_code_with_buffer_provider,
            try_new_code_unstable,
            Self
        ]
    );

    /// Creates a new [`CurrencyFormatter`] for formatting using the 3-letter ISO currency code from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    ///
    /// # Examples
    /// ```
    /// use icu::experimental::dimension::currency::CurrencyType;
    /// use icu::experimental::dimension::currency::formatter::CurrencyFormatter;
    /// use icu::locale::locale;
    /// use icu::locale::preferences::extensions::unicode::keywords::currency;
    /// use writeable::assert_writeable_eq;
    ///
    /// let currency_preferences = locale!("en-US").into();
    /// let currency_code = currency!("USD");
    /// let fmt = CurrencyFormatter::try_new_code(
    ///     currency_preferences,
    ///     currency_code,
    ///     Default::default(),
    /// )
    /// .unwrap();
    /// let value = "12345.67".parse().unwrap();
    /// assert_writeable_eq!(
    ///     fmt.format_fixed_decimal(&value),
    ///     "USD\u{a0}12,345.67"
    /// );
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn try_new_code(
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        Self::try_new_code_internal(
            DecimalFormatter::try_new_unstable(
                &WrappingProvider(
                    &crate::provider::Baked,
                    &icu_decimal::provider::Baked,
                    currency_code,
                ),
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            options,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_code)]
    pub fn try_new_code_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyEssentialsV1>
            + DataProvider<CurrencyDecimalSymbolsV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>,
    {
        Self::try_new_code_internal_unstable(
            provider,
            DecimalFormatter::try_new_unstable(
                &WrappingProvider(provider, provider, currency_code),
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            options,
        )
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: CurrencyFormatterPreferences, currency_code: CurrencyType) -> error: DataError,
        functions: [
            try_new_name: skip,
            try_new_name_with_buffer_provider,
            try_new_name_unstable,
            Self
        ]
    );

    /// Creates a new [`CurrencyFormatter`] for formatting with full currency display names from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    ///
    /// # Examples
    /// ```
    /// use icu::experimental::dimension::currency::CurrencyType;
    /// use icu::experimental::dimension::currency::formatter::CurrencyFormatter;
    /// use icu::locale::locale;
    /// use icu::locale::preferences::extensions::unicode::keywords::currency;
    /// use writeable::assert_writeable_eq;
    ///
    /// let currency_preferences = locale!("en-US").into();
    /// let currency_code = currency!("USD");
    /// let fmt =
    ///     CurrencyFormatter::try_new_name(currency_preferences, currency_code)
    ///         .unwrap();
    /// let value = "12345.67".parse().unwrap();
    /// assert_writeable_eq!(
    ///     fmt.format_fixed_decimal(&value),
    ///     "12,345.67 US dollars"
    /// );
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn try_new_name(
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
    ) -> Result<Self, DataError> {
        Self::try_new_name_internal(
            DecimalFormatter::try_new_unstable(
                &WrappingProvider(
                    &crate::provider::Baked,
                    &icu_decimal::provider::Baked,
                    currency_code,
                ),
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_name)]
    pub fn try_new_name_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyExtendedDataV1>
            + DataProvider<CurrencyPatternsDataV1>
            + DataProvider<CurrencyDecimalSymbolsV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        Self::try_new_name_internal_unstable(
            provider,
            DecimalFormatter::try_new_unstable(
                &WrappingProvider(provider, provider, currency_code),
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
        )
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: CurrencyFormatterPreferences, currency_code: CurrencyType, options: CurrencyFormatterOptions) -> error: DataError,
        functions: [
            try_new_no_currency: skip,
            try_new_no_currency_with_buffer_provider,
            try_new_no_currency_unstable,
            Self
        ]
    );

    /// Creates a new [`CurrencyFormatter`] for formatting currency amounts without a currency symbol, code, or name from compiled locale data.
    ///
    /// The currency code is used to determine the number of decimal digits and rounding for the format.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    ///
    /// # Examples
    /// ```
    /// use icu::experimental::dimension::currency::CurrencyType;
    /// use icu::experimental::dimension::currency::formatter::CurrencyFormatter;
    /// use icu::locale::locale;
    /// use icu::locale::preferences::extensions::unicode::keywords::currency;
    /// use writeable::assert_writeable_eq;
    ///
    /// let currency_preferences = locale!("en-US").into();
    /// let currency_code = currency!("USD");
    /// let fmt = CurrencyFormatter::try_new_no_currency(
    ///     currency_preferences,
    ///     currency_code,
    ///     Default::default(),
    /// )
    /// .unwrap();
    /// let value = "12345.67".parse().unwrap();
    /// assert_writeable_eq!(fmt.format_fixed_decimal(&value), "12,345.67");
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn try_new_no_currency(
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        Self::try_new_no_currency_internal(
            DecimalFormatter::try_new_unstable(
                &WrappingProvider(
                    &crate::provider::Baked,
                    &icu_decimal::provider::Baked,
                    currency_code,
                ),
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            options,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_no_currency)]
    pub fn try_new_no_currency_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyPatternsNoCurrencyV1>
            + DataProvider<CurrencyDecimalSymbolsV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>,
    {
        Self::try_new_no_currency_internal_unstable(
            provider,
            DecimalFormatter::try_new_unstable(
                &WrappingProvider(provider, provider, currency_code),
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            options,
        )
    }
}

struct WrappingProvider<'a, D1: ?Sized, D2: ?Sized>(&'a D1, &'a D2, CurrencyType);

impl<
    'a,
    D1: ?Sized + DataProvider<CurrencyDecimalSymbolsV1>,
    D2: ?Sized + DataProvider<icu_decimal::provider::DecimalSymbolsV1>,
> DataProvider<icu_decimal::provider::DecimalSymbolsV1> for WrappingProvider<'a, D1, D2>
{
    fn load(
        &self,
        req: DataRequest,
    ) -> Result<DataResponse<icu_decimal::provider::DecimalSymbolsV1>, DataError> {
        if let Some(x) = self
            .0
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    CurrencyDecimalSymbolsV1::make_attributes(
                        icu_decimal::provider::DecimalSymbolsV1::parse_attributes(
                            req.id.marker_attributes,
                        ),
                        self.2,
                        &mut Default::default(),
                    ),
                    req.id.locale,
                ),
                metadata: {
                    let mut m = req.metadata;
                    m.silent = true;
                    m
                },
            })
            .allow_identifier_not_found()?
        {
            return Ok(x.cast());
        }

        self.1.load(req)
    }
}

impl<'a, D1: ?Sized, D2: ?Sized + DataProvider<icu_decimal::provider::DecimalDigitsV1>>
    DataProvider<icu_decimal::provider::DecimalDigitsV1> for WrappingProvider<'a, D1, D2>
{
    fn load(
        &self,
        req: DataRequest,
    ) -> Result<DataResponse<icu_decimal::provider::DecimalDigitsV1>, DataError> {
        self.1.load(req)
    }
}

impl CurrencyFormatter<CompactDecimalFormatter> {
    icu_provider::gen_buffer_data_constructors!(
        (
            prefs: CurrencyFormatterPreferences,
            currency_code: CurrencyType,
            options: CurrencyFormatterOptions
        ) -> error: DataError,
        functions: [
            try_new_compact_symbol: skip,
            try_new_compact_symbol_with_buffer_provider,
            try_new_compact_symbol_unstable,
            Self
        ]
    );

    icu_provider::gen_buffer_data_constructors!(
        (
            prefs: CurrencyFormatterPreferences,
            currency_code: CurrencyType,
            options: CurrencyFormatterOptions
        ) -> error: DataError,
        functions: [
            try_new_compact_symbol_narrow: skip,
            try_new_compact_symbol_narrow_with_buffer_provider,
            try_new_compact_symbol_narrow_unstable,
            Self
        ]
    );

    icu_provider::gen_buffer_data_constructors!(
        (
            prefs: CurrencyFormatterPreferences,
            currency_code: CurrencyType,
            options: CurrencyFormatterOptions
        ) -> error: DataError,
        functions: [
            try_new_compact_code: skip,
            try_new_compact_code_with_buffer_provider,
            try_new_compact_code_unstable,
            Self
        ]
    );

    icu_provider::gen_buffer_data_constructors!(
        (prefs: CurrencyFormatterPreferences, currency_code: CurrencyType) -> error: DataError,
        functions: [
            try_new_compact_name: skip,
            try_new_compact_name_with_buffer_provider,
            try_new_compact_name_unstable,
            Self
        ]
    );

    icu_provider::gen_buffer_data_constructors!(
        (
            prefs: CurrencyFormatterPreferences,
            currency_code: CurrencyType,
            options: CurrencyFormatterOptions
        ) -> error: DataError,
        functions: [
            try_new_compact_long_symbol: skip,
            try_new_compact_long_symbol_with_buffer_provider,
            try_new_compact_long_symbol_unstable,
            Self
        ]
    );

    icu_provider::gen_buffer_data_constructors!(
        (
            prefs: CurrencyFormatterPreferences,
            currency_code: CurrencyType,
            options: CurrencyFormatterOptions
        ) -> error: DataError,
        functions: [
            try_new_compact_long_symbol_narrow: skip,
            try_new_compact_long_symbol_narrow_with_buffer_provider,
            try_new_compact_long_symbol_narrow_unstable,
            Self
        ]
    );

    icu_provider::gen_buffer_data_constructors!(
        (
            prefs: CurrencyFormatterPreferences,
            currency_code: CurrencyType,
            options: CurrencyFormatterOptions
        ) -> error: DataError,
        functions: [
            try_new_compact_long_code: skip,
            try_new_compact_long_code_with_buffer_provider,
            try_new_compact_long_code_unstable,
            Self
        ]
    );

    icu_provider::gen_buffer_data_constructors!(
        (prefs: CurrencyFormatterPreferences, currency_code: CurrencyType) -> error: DataError,
        functions: [
            try_new_compact_long_name: skip,
            try_new_compact_long_name_with_buffer_provider,
            try_new_compact_long_name_unstable,
            Self
        ]
    );

    /// Creates a new [`CurrencyFormatter`] for compact short number formatting with short currency symbols from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_compact_symbol(
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        Self::try_new_essential(
            CompactDecimalFormatter::try_new_short((&prefs).into(), Default::default())?,
            prefs,
            currency_code,
            CurrencySymbolsV1::SHORT,
            options,
        )
    }

    /// Creates a new [`CurrencyFormatter`] for compact short number formatting with narrow currency symbols from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_compact_symbol_narrow(
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        Self::try_new_essential(
            CompactDecimalFormatter::try_new_short((&prefs).into(), Default::default())?,
            prefs,
            currency_code,
            CurrencySymbolsV1::NARROW,
            options,
        )
    }

    /// Creates a new [`CurrencyFormatter`] for compact short number formatting using the 3-letter ISO currency code from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_compact_code(
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        Self::try_new_code_internal(
            CompactDecimalFormatter::try_new_short((&prefs).into(), Default::default())?,
            prefs,
            currency_code,
            options,
        )
    }

    /// Creates a new [`CurrencyFormatter`] for compact short number formatting with full currency display names from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_compact_name(
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
    ) -> Result<Self, DataError> {
        Self::try_new_name_internal(
            CompactDecimalFormatter::try_new_short((&prefs).into(), Default::default())?,
            prefs,
            currency_code,
        )
    }

    /// Creates a new [`CurrencyFormatter`] for compact long number formatting with short currency symbols from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_compact_long_symbol(
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        Self::try_new_essential(
            CompactDecimalFormatter::try_new_long((&prefs).into(), Default::default())?,
            prefs,
            currency_code,
            CurrencySymbolsV1::SHORT,
            options,
        )
    }

    /// Creates a new [`CurrencyFormatter`] for compact long number formatting with narrow currency symbols from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_compact_long_symbol_narrow(
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        Self::try_new_essential(
            CompactDecimalFormatter::try_new_long((&prefs).into(), Default::default())?,
            prefs,
            currency_code,
            CurrencySymbolsV1::NARROW,
            options,
        )
    }

    /// Creates a new [`CurrencyFormatter`] for compact long number formatting using the 3-letter ISO currency code from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_compact_long_code(
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        Self::try_new_code_internal(
            CompactDecimalFormatter::try_new_long((&prefs).into(), Default::default())?,
            prefs,
            currency_code,
            options,
        )
    }

    /// Creates a new [`CurrencyFormatter`] for compact long number formatting with full currency display names from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_compact_long_name(
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
    ) -> Result<Self, DataError> {
        Self::try_new_name_internal(
            CompactDecimalFormatter::try_new_long((&prefs).into(), Default::default())?,
            prefs,
            currency_code,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_compact_symbol)]
    pub fn try_new_compact_symbol_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyEssentialsV1>
            + DataProvider<CurrencySymbolsV1>
            + DataProvider<CurrencyPatternsNoCurrencyV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_decimal::provider::DecimalCompactShortV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        Self::try_new_essential_unstable(
            provider,
            CompactDecimalFormatter::try_new_short_unstable(
                provider,
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            CurrencySymbolsV1::SHORT,
            options,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_compact_symbol_narrow)]
    pub fn try_new_compact_symbol_narrow_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyEssentialsV1>
            + DataProvider<CurrencySymbolsV1>
            + DataProvider<CurrencyPatternsNoCurrencyV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_decimal::provider::DecimalCompactShortV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        Self::try_new_essential_unstable(
            provider,
            CompactDecimalFormatter::try_new_short_unstable(
                provider,
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            CurrencySymbolsV1::NARROW,
            options,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_compact_name)]
    pub fn try_new_compact_name_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyExtendedDataV1>
            + DataProvider<CurrencyPatternsDataV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>
            + DataProvider<icu_decimal::provider::DecimalCompactShortV1>,
    {
        Self::try_new_name_internal_unstable(
            provider,
            CompactDecimalFormatter::try_new_short_unstable(
                provider,
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_compact_long_symbol)]
    pub fn try_new_compact_long_symbol_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyEssentialsV1>
            + DataProvider<CurrencySymbolsV1>
            + DataProvider<CurrencyPatternsNoCurrencyV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_decimal::provider::DecimalCompactLongV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        Self::try_new_essential_unstable(
            provider,
            CompactDecimalFormatter::try_new_long_unstable(
                provider,
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            CurrencySymbolsV1::SHORT,
            options,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_compact_long_symbol_narrow)]
    pub fn try_new_compact_long_symbol_narrow_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyEssentialsV1>
            + DataProvider<CurrencySymbolsV1>
            + DataProvider<CurrencyPatternsNoCurrencyV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_decimal::provider::DecimalCompactLongV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        Self::try_new_essential_unstable(
            provider,
            CompactDecimalFormatter::try_new_long_unstable(
                provider,
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            CurrencySymbolsV1::NARROW,
            options,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_compact_long_name)]
    pub fn try_new_compact_long_name_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyExtendedDataV1>
            + DataProvider<CurrencyPatternsDataV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>
            + DataProvider<icu_decimal::provider::DecimalCompactLongV1>,
    {
        Self::try_new_name_internal_unstable(
            provider,
            CompactDecimalFormatter::try_new_long_unstable(
                provider,
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_compact_code)]
    pub fn try_new_compact_code_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyEssentialsV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_decimal::provider::DecimalCompactShortV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        Self::try_new_code_internal_unstable(
            provider,
            CompactDecimalFormatter::try_new_short_unstable(
                provider,
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            options,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_compact_long_code)]
    pub fn try_new_compact_long_code_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: CurrencyType,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyEssentialsV1>
            + DataProvider<CurrencyFractionsV1>
            + DataProvider<icu_decimal::provider::DecimalCompactLongV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        Self::try_new_code_internal_unstable(
            provider,
            CompactDecimalFormatter::try_new_long_unstable(
                provider,
                (&prefs).into(),
                Default::default(),
            )?,
            prefs,
            currency_code,
            options,
        )
    }
}

impl<V: AbstractFormatter> CurrencyFormatter<V> {
    /// Formats a [`FixedDecimal`] value.
    ///
    /// # Examples
    /// ```
    /// use icu::experimental::dimension::currency::CurrencyType;
    /// use icu::experimental::dimension::currency::formatter::CurrencyFormatter;
    /// use icu::locale::locale;
    /// use icu::locale::preferences::extensions::unicode::keywords::currency;
    /// use writeable::assert_writeable_eq;
    ///
    /// let currency_preferences = locale!("en-US").into();
    /// let currency_code = currency!("USD");
    /// let fmt = CurrencyFormatter::try_new_symbol(
    ///     currency_preferences,
    ///     currency_code,
    ///     Default::default(),
    /// )
    /// .unwrap();
    /// let value = "12345.67".parse().unwrap();
    /// assert_writeable_eq!(fmt.format_fixed_decimal(&value), "$12,345.67");
    /// ```
    ///
    /// ```
    /// use icu::experimental::dimension::currency::CurrencyType;
    /// use icu::experimental::dimension::currency::formatter::CurrencyFormatter;
    /// use icu::locale::locale;
    /// use icu::locale::preferences::extensions::unicode::keywords::currency;
    /// use writeable::assert_writeable_eq;
    ///
    /// let currency_preferences = locale!("en-US").into();
    /// let currency_code = currency!("USD");
    /// let fmt = CurrencyFormatter::try_new_compact_symbol(
    ///     currency_preferences,
    ///     currency_code,
    ///     Default::default(),
    /// )
    /// .unwrap();
    /// let value = "12345.67".parse().unwrap();
    /// assert_writeable_eq!(fmt.format_fixed_decimal(&value), "$12K");
    /// ```
    ///
    /// ```
    /// use icu::experimental::dimension::currency::CurrencyType;
    /// use icu::experimental::dimension::currency::formatter::CurrencyFormatter;
    /// use icu::locale::locale;
    /// use icu::locale::preferences::extensions::unicode::keywords::currency;
    /// use writeable::assert_writeable_eq;
    ///
    /// let currency_preferences = locale!("en-US").into();
    /// let currency_code = currency!("USD");
    /// let fmt = CurrencyFormatter::try_new_compact_long_symbol(
    ///     currency_preferences,
    ///     currency_code,
    ///     Default::default(),
    /// )
    /// .unwrap();
    /// let value = "12345.67".parse().unwrap();
    /// assert_writeable_eq!(fmt.format_fixed_decimal(&value), "$12 thousand");
    /// ```
    pub fn format_fixed_decimal<'l>(
        &'l self,
        value: &'l FixedDecimal,
    ) -> impl Writeable + Display + 'l {
        // TODO(#8146): Evaluate if FixedDecimal is the correct input type or if we should use
        // an exact decimal/money representation.
        // Per UTS #35 (LDML Part 3: Numbers, Section 3.8, Rule 3 in Compact Number Formatting):
        // "If the element value of P is '0', then use the corresponding non-compact number formatting instead,"
        //
        // Specifically, when compact formatters handle uncompacted fallback values below 1,000 ("0" pattern):
        // * Per UTS #35: Suffix abbreviation and division steps ("K", "M") are skipped in favor of standard currency format.
        // * Per current ICU4X implementation: `CompactDecimalFormatter` trims trailing fractional zeros by default
        //   (rendering e.g. "$12" instead of "$12.00").
        // * Currency fraction precision is applied uniformly without type-level switches, relying on the underlying
        //   formatter (such as `CompactDecimalFormatter`) to handle magnitude-based trailing zero trimming.
        let rounded_value = apply_precision(value.clone(), self.fraction_info);
        let formatted_value = V::format_unsigned(&self.value_formatter, rounded_value.absolute);
        let accounting = self.usage == CurrencyUsage::Accounting;

        let (pattern, currency_str, sign) = match &self.currency_data {
            CurrencyFormatterData::IsoCodeSymbol {
                essential,
                iso_code,
            } => {
                let (pattern, sign) = select_essentials_pattern(
                    essential.get(),
                    accounting,
                    rounded_value.sign,
                    true,
                    true,
                );
                (pattern, iso_code.as_str(), sign)
            }
            CurrencyFormatterData::IsoCodeName { patterns, iso_code } => {
                let pattern = patterns.get().elements.get_default().1;
                (pattern, iso_code.as_str(), rounded_value.sign)
            }
            CurrencyFormatterData::Symbol { essential, symbol } => {
                let symbol = symbol.get();
                let (pattern, sign) = select_essentials_pattern(
                    essential.get(),
                    accounting,
                    rounded_value.sign,
                    symbol.starts_with_letter(),
                    symbol.ends_with_letter(),
                );
                (pattern, symbol.as_str(), sign)
            }
            CurrencyFormatterData::Name {
                extended,
                patterns,
                plural_rules,
            } => {
                let operands = V::plural_operands(&formatted_value);
                let currency_str = extended.get().get(operands, plural_rules);
                let pattern = patterns.get().get(operands, plural_rules);
                (pattern, currency_str, rounded_value.sign)
            }
            CurrencyFormatterData::NoCurrency { patterns } => {
                let (pattern, sign) =
                    select_no_currency_pattern(patterns.get(), accounting, rounded_value.sign);
                (pattern, "", sign)
            }
        };

        // Per UTS #35 (Section 3.2.1), when no explicit negative subpattern exists, the negative format
        // is formed by prepending the localized minus sign to the entire positive pattern (e.g., `-$12K`).
        // `format_sign` is applied as the outermost wrapper around the interpolated currency string.
        // (If an explicit negative pattern was selected, `sign` is `Sign::None` so this is a no-op).
        V::format_sign(
            &self.value_formatter,
            pattern.interpolate((formatted_value, currency_str)),
            sign,
        )
    }
}

/// Selects the pattern for no-currency formatting for the given sign.
///
/// Returns [`Sign::None`] if an explicit negative pattern is matched (which already encodes the sign).
fn select_no_currency_pattern<'a>(
    patterns: &'a CurrencyPatternsNoCurrency<'_>,
    accounting: bool,
    sign: Sign,
) -> (&'a icu_pattern::DoublePlaceholderPattern, Sign) {
    if accounting {
        if sign == Sign::Negative
            && let Some(pattern) = patterns.get_negative_accounting()
        {
            return (pattern, Sign::None);
        }
        return (patterns.get_positive_accounting(), sign);
    }

    if sign == Sign::Negative
        && let Some(pattern) = patterns.get_negative()
    {
        return (pattern, Sign::None);
    }

    (patterns.get_positive(), sign)
}

/// Selects the pattern from the currency essentials for the given sign.
///
/// If an explicit negative pattern exists for a negative value, it is returned along with
/// [`Sign::None`]: such patterns already encode the sign (e.g. parentheses, or a minus sign
/// placed by the pattern), so the pattern is interpolated with the absolute value and no
/// sign is applied on top of it. Otherwise, per UTS #35 (LDML Part 3: Numbers, Section 3.2.1),
/// the positive pattern of the same category is returned with the original sign, and the sign is
/// rendered by the value formatter.
fn select_essentials_pattern<'a>(
    essentials: &'a super::super::provider::currency::essentials::CurrencyEssentials<'_>,
    accounting: bool,
    sign: Sign,
    symbol_starts_with_letter: bool,
    symbol_ends_with_letter: bool,
) -> (&'a icu_pattern::DoublePlaceholderPattern, Sign) {
    if sign == Sign::Negative {
        let negative_pattern = if accounting {
            essentials.get_negative_accounting(symbol_starts_with_letter, symbol_ends_with_letter)
        } else {
            essentials.get_negative(symbol_starts_with_letter, symbol_ends_with_letter)
        };
        if let Some(pattern) = negative_pattern {
            return (pattern, Sign::None);
        }
    }

    let positive_pattern = if accounting {
        essentials.get_positive_accounting(symbol_starts_with_letter, symbol_ends_with_letter)
    } else {
        essentials.get_positive(symbol_starts_with_letter, symbol_ends_with_letter)
    };
    (positive_pattern, sign)
}

// TODO: Discuss reusing the `load_with_fallback` helper from `icu_decimal`
// (or moving it to a shared location) instead of duplicating it here.
pub(crate) fn load_with_fallback<'a, M: DataMarker>(
    provider: &(impl DataProvider<M> + ?Sized),
    ids: impl Iterator<Item = DataIdentifierBorrowed<'a>>,
) -> Result<DataResponse<M>, DataError> {
    let mut ids = ids.peekable();

    while let Some(id) = ids.next() {
        if ids.peek().is_some() {
            if let Some(r) = provider
                .load(DataRequest {
                    id,
                    metadata: {
                        let mut m = DataRequestMetadata::default();
                        m.silent = true;
                        m
                    },
                })
                .allow_identifier_not_found()?
            {
                return Ok(r);
            }
        } else {
            return provider.load(DataRequest {
                id,
                metadata: DataRequestMetadata::default(),
            });
        }
    }

    Err(DataErrorKind::InvalidRequest.into_error())
}

pub(crate) fn apply_precision(value: FixedDecimal, fraction_info: FractionInfo) -> FixedDecimal {
    let precision = fraction_info.digits as i16;
    let rounding = fraction_info.rounding;

    let (magnitude, increment) = match rounding {
        Rounding::R50 => (-precision + 1, RoundingIncrement::MultiplesOf5),
        Rounding::R20 => (-precision + 1, RoundingIncrement::MultiplesOf2),
        Rounding::R5 => (-precision, RoundingIncrement::MultiplesOf5),
        Rounding::R1 => (-precision, RoundingIncrement::MultiplesOf1),
    };

    value.rounded_with_mode_and_increment(
        magnitude,
        SignedRoundingMode::Unsigned(UnsignedRoundingMode::HalfExpand),
        increment,
    )
}
