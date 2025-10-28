// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::compactdecimal::{
    format::FormattedCompactDecimal,
    options::CompactDecimalFormatterOptions,
    provider::{
        CompactDecimalPatternData, Count, LongCompactDecimalFormatDataV1, PatternULE,
        ShortCompactDecimalFormatDataV1,
    },
    ExponentError,
};
use alloc::borrow::Cow;
use core::convert::TryFrom;
use fixed_decimal::{CompactDecimal, Decimal};
use icu_decimal::{DecimalFormatter, DecimalFormatterPreferences};
use icu_locale_core::preferences::{define_preferences, prefs_convert};
use icu_plurals::{PluralRules, PluralRulesPreferences};
use icu_provider::DataError;
use icu_provider::{marker::ErasedMarker, prelude::*};
use zerovec::maps::ZeroMap2dCursor;

define_preferences!(
    /// The preferences for compact decimal formatting.
    [Copy]
    CompactDecimalFormatterPreferences,
    {
        /// The user's preferred numbering system.
        ///
        /// Corresponds to the `-u-nu` in Unicode Locale Identifier.
        numbering_system: super::preferences::NumberingSystem
    }
);

prefs_convert!(
    CompactDecimalFormatterPreferences,
    DecimalFormatterPreferences,
    { numbering_system }
);
prefs_convert!(CompactDecimalFormatterPreferences, PluralRulesPreferences);

/// A formatter that renders locale-sensitive compact numbers.
///
/// # Examples
///
/// ```
/// use icu::experimental::compactdecimal::CompactDecimalFormatter;
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// let short_french = CompactDecimalFormatter::try_new_short(
///    locale!("fr").into(),
///    Default::default(),
/// ).unwrap();
///
/// let [long_french, long_japanese, long_bangla] = [locale!("fr"), locale!("ja"), locale!("bn")]
///     .map(|locale| {
///         CompactDecimalFormatter::try_new_long(
///             locale.into(),
///             Default::default(),
///         )
///         .unwrap()
///     });
///
/// /// Supports short and long notations:
/// # // The following line contains U+00A0 NO-BREAK SPACE.
/// assert_writeable_eq!(short_french.format_i64(35_357_670), "35 M");
/// assert_writeable_eq!(long_french.format_i64(35_357_670), "35 millions");
/// /// The powers of ten used are locale-dependent:
/// assert_writeable_eq!(long_japanese.format_i64(3535_7670), "3536万");
/// /// So are the digits:
/// assert_writeable_eq!(long_bangla.format_i64(3_53_57_670), "৩.৫ কোটি");
///
/// /// The output does not always contain digits:
/// assert_writeable_eq!(long_french.format_i64(1000), "mille");
/// ```
#[derive(Debug)]
pub struct CompactDecimalFormatter {
    pub(crate) plural_rules: PluralRules,
    pub(crate) decimal_formatter: DecimalFormatter,
    pub(crate) compact_data: DataPayload<ErasedMarker<CompactDecimalPatternData<'static>>>,
}

impl CompactDecimalFormatter {
    /// Constructor that takes a selected locale and a list of preferences,
    /// then collects all compiled data necessary to format numbers in short compact
    /// decimal notation for the given locale.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::experimental::compactdecimal::CompactDecimalFormatter;
    /// use icu::locale::locale;
    ///
    /// CompactDecimalFormatter::try_new_short(
    ///     locale!("sv").into(),
    ///     Default::default(),
    /// );
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn try_new_short(
        prefs: CompactDecimalFormatterPreferences,
        options: CompactDecimalFormatterOptions,
    ) -> Result<Self, DataError> {
        let locale = ShortCompactDecimalFormatDataV1::make_locale(prefs.locale_preferences);
        Ok(Self {
            decimal_formatter: DecimalFormatter::try_new(
                (&prefs).into(),
                options.decimal_formatter_options,
            )?,
            plural_rules: PluralRules::try_new_cardinal((&prefs).into())?,
            compact_data: DataProvider::<ShortCompactDecimalFormatDataV1>::load(
                &crate::provider::Baked,
                DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&locale),
                    ..Default::default()
                },
            )?
            .payload
            .cast(),
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: CompactDecimalFormatterPreferences, options: CompactDecimalFormatterOptions) -> error: DataError,
        functions: [
            try_new_short: skip,
            try_new_short_with_buffer_provider,
            try_new_short_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_short)]
    pub fn try_new_short_unstable<D>(
        provider: &D,
        prefs: CompactDecimalFormatterPreferences,
        options: CompactDecimalFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<ShortCompactDecimalFormatDataV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>
            + ?Sized,
    {
        let locale = ShortCompactDecimalFormatDataV1::make_locale(prefs.locale_preferences);
        Ok(Self {
            decimal_formatter: DecimalFormatter::try_new_unstable(
                provider,
                (&prefs).into(),
                options.decimal_formatter_options,
            )?,
            plural_rules: PluralRules::try_new_cardinal_unstable(provider, (&prefs).into())?,
            compact_data: DataProvider::<ShortCompactDecimalFormatDataV1>::load(
                provider,
                DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&locale),
                    ..Default::default()
                },
            )?
            .payload
            .cast(),
        })
    }

    /// Constructor that takes a selected locale and a list of preferences,
    /// then collects all compiled data necessary to format numbers in short compact
    /// decimal notation for the given locale.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::experimental::compactdecimal::CompactDecimalFormatter;
    /// use icu::locale::locale;
    ///
    /// CompactDecimalFormatter::try_new_long(
    ///     locale!("sv").into(),
    ///     Default::default(),
    /// );
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn try_new_long(
        prefs: CompactDecimalFormatterPreferences,
        options: CompactDecimalFormatterOptions,
    ) -> Result<Self, DataError> {
        let locale = LongCompactDecimalFormatDataV1::make_locale(prefs.locale_preferences);
        Ok(Self {
            decimal_formatter: DecimalFormatter::try_new(
                (&prefs).into(),
                options.decimal_formatter_options,
            )?,
            plural_rules: PluralRules::try_new_cardinal((&prefs).into())?,
            compact_data: DataProvider::<LongCompactDecimalFormatDataV1>::load(
                &crate::provider::Baked,
                DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&locale),
                    ..Default::default()
                },
            )?
            .payload
            .cast(),
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: CompactDecimalFormatterPreferences, options: CompactDecimalFormatterOptions) -> error: DataError,
        functions: [
            try_new_long: skip,
            try_new_long_with_buffer_provider,
            try_new_long_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_long)]
    pub fn try_new_long_unstable<D>(
        provider: &D,
        prefs: CompactDecimalFormatterPreferences,
        options: CompactDecimalFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<LongCompactDecimalFormatDataV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>
            + ?Sized,
    {
        let locale = LongCompactDecimalFormatDataV1::make_locale(prefs.locale_preferences);
        Ok(Self {
            decimal_formatter: DecimalFormatter::try_new_unstable(
                provider,
                (&prefs).into(),
                options.decimal_formatter_options,
            )?,
            plural_rules: PluralRules::try_new_cardinal_unstable(provider, (&prefs).into())?,
            compact_data: DataProvider::<LongCompactDecimalFormatDataV1>::load(
                provider,
                DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&locale),
                    ..Default::default()
                },
            )?
            .payload
            .cast(),
        })
    }

    /// Formats an integer in compact decimal notation using the default
    /// precision settings.
    ///
    /// The result may have a fractional digit only if it is compact and its
    /// significand is less than 10. Trailing fractional 0s are omitted, and
    /// a sign is shown only for negative values.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::experimental::compactdecimal::CompactDecimalFormatter;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let short_english = CompactDecimalFormatter::try_new_short(
    ///     locale!("en").into(),
    ///     Default::default(),
    /// )
    /// .unwrap();
    ///
    /// assert_writeable_eq!(short_english.format_i64(0), "0");
    /// assert_writeable_eq!(short_english.format_i64(2), "2");
    /// assert_writeable_eq!(short_english.format_i64(843), "843");
    /// assert_writeable_eq!(short_english.format_i64(2207), "2.2K");
    /// assert_writeable_eq!(short_english.format_i64(15_127), "15K");
    /// assert_writeable_eq!(short_english.format_i64(3_010_349), "3M");
    /// assert_writeable_eq!(short_english.format_i64(-13_132), "-13K");
    /// ```
    ///
    /// The result is the nearest such compact number, with halfway cases-
    /// rounded towards the number with an even least significant digit.
    ///
    /// ```
    /// # use icu::experimental::compactdecimal::CompactDecimalFormatter;
    /// # use icu::locale::locale;
    /// # use writeable::assert_writeable_eq;
    /// #
    /// # let short_english = CompactDecimalFormatter::try_new_short(
    /// #    locale!("en").into(),
    /// #    Default::default(),
    /// # ).unwrap();
    /// assert_writeable_eq!(short_english.format_i64(999_499), "999K");
    /// assert_writeable_eq!(short_english.format_i64(999_500), "1M");
    /// assert_writeable_eq!(short_english.format_i64(1650), "1.6K");
    /// assert_writeable_eq!(short_english.format_i64(1750), "1.8K");
    /// assert_writeable_eq!(short_english.format_i64(1950), "2K");
    /// assert_writeable_eq!(short_english.format_i64(-1_172_700), "-1.2M");
    /// ```
    pub fn format_i64(&self, value: i64) -> FormattedCompactDecimal<'_> {
        let unrounded = Decimal::from(value);
        self.format_fixed_decimal(&unrounded)
    }

    /// Formats a floating-point number in compact decimal notation using the default
    /// precision settings.
    ///
    /// The result may have a fractional digit only if it is compact and its
    /// significand is less than 10. Trailing fractional 0s are omitted, and
    /// a sign is shown only for negative values.
    ///
    /// ✨ *Enabled with the `ryu` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::experimental::compactdecimal::CompactDecimalFormatter;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let short_english = CompactDecimalFormatter::try_new_short(
    ///     locale!("en").into(),
    ///     Default::default(),
    /// )
    /// .unwrap();
    ///
    /// assert_writeable_eq!(short_english.format_f64(0.0).unwrap(), "0");
    /// assert_writeable_eq!(short_english.format_f64(2.0).unwrap(), "2");
    /// assert_writeable_eq!(short_english.format_f64(843.0).unwrap(), "843");
    /// assert_writeable_eq!(short_english.format_f64(2207.0).unwrap(), "2.2K");
    /// assert_writeable_eq!(short_english.format_f64(15_127.0).unwrap(), "15K");
    /// assert_writeable_eq!(short_english.format_f64(3_010_349.0).unwrap(), "3M");
    /// assert_writeable_eq!(short_english.format_f64(-13_132.0).unwrap(), "-13K");
    /// ```
    ///
    /// The result is the nearest such compact number, with halfway cases-
    /// rounded towards the number with an even least significant digit.
    ///
    /// ```
    /// # use icu::experimental::compactdecimal::CompactDecimalFormatter;
    /// # use icu::locale::locale;
    /// # use writeable::assert_writeable_eq;
    /// #
    /// # let short_english = CompactDecimalFormatter::try_new_short(
    /// #    locale!("en").into(),
    /// #    Default::default(),
    /// # ).unwrap();
    /// assert_writeable_eq!(short_english.format_f64(999_499.99).unwrap(), "999K");
    /// assert_writeable_eq!(short_english.format_f64(999_500.00).unwrap(), "1M");
    /// assert_writeable_eq!(short_english.format_f64(1650.0).unwrap(), "1.6K");
    /// assert_writeable_eq!(short_english.format_f64(1750.0).unwrap(), "1.8K");
    /// assert_writeable_eq!(short_english.format_f64(1950.0).unwrap(), "2K");
    /// assert_writeable_eq!(
    ///     short_english.format_f64(-1_172_700.0).unwrap(),
    ///     "-1.2M"
    /// );
    /// ```
    #[cfg(feature = "ryu")]
    pub fn format_f64(
        &self,
        value: f64,
    ) -> Result<FormattedCompactDecimal<'_>, fixed_decimal::LimitError> {
        use fixed_decimal::FloatPrecision::RoundTrip;
        // NOTE: This first gets the shortest representation of the f64, which
        // manifests as double rounding.
        let partly_rounded = Decimal::try_from_f64(value, RoundTrip)?;
        Ok(self.format_fixed_decimal(&partly_rounded))
    }

    /// Formats a [`Decimal`] by automatically scaling and rounding it.
    ///
    /// The result may have a fractional digit only if it is compact and its
    /// significand is less than 10. Trailing fractional 0s are omitted.
    ///
    /// Because the Decimal is mutated before formatting, this function
    /// takes ownership of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::Decimal;
    /// use icu::experimental::compactdecimal::CompactDecimalFormatter;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let short_english = CompactDecimalFormatter::try_new_short(
    ///     locale!("en").into(),
    ///     Default::default(),
    /// )
    /// .unwrap();
    ///
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(&Decimal::from(0)),
    ///     "0"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(&Decimal::from(2)),
    ///     "2"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(&Decimal::from(843)),
    ///     "843"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(&Decimal::from(2207)),
    ///     "2.2K"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(&Decimal::from(15127)),
    ///     "15K"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(&Decimal::from(3010349)),
    ///     "3M"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(&Decimal::from(-13132)),
    ///     "-13K"
    /// );
    ///
    /// // The sign display on the Decimal is respected:
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(
    ///         &Decimal::from(2500)
    ///             .with_sign_display(fixed_decimal::SignDisplay::ExceptZero)
    ///     ),
    ///     "+2.5K"
    /// );
    /// ```
    ///
    /// The result is the nearest such compact number, with halfway cases-
    /// rounded towards the number with an even least significant digit.
    ///
    /// ```
    /// # use icu::experimental::compactdecimal::CompactDecimalFormatter;
    /// # use icu::locale::locale;
    /// # use writeable::assert_writeable_eq;
    /// #
    /// # let short_english = CompactDecimalFormatter::try_new_short(
    /// #    locale!("en").into(),
    /// #    Default::default(),
    /// # ).unwrap();
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(&"999499.99".parse().unwrap()),
    ///     "999K"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(&"999500.00".parse().unwrap()),
    ///     "1M"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(&"1650".parse().unwrap()),
    ///     "1.6K"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(&"1750".parse().unwrap()),
    ///     "1.8K"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(&"1950".parse().unwrap()),
    ///     "2K"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format_fixed_decimal(&"-1172700".parse().unwrap()),
    ///     "-1.2M"
    /// );
    /// ```
    pub fn format_fixed_decimal(&self, value: &Decimal) -> FormattedCompactDecimal<'_> {
        let log10_type = value.absolute.nonzero_magnitude_start();
        let (mut plural_map, mut exponent) = self.plural_map_and_exponent_for_magnitude(log10_type);
        let mut significand = value.clone();
        significand.multiply_pow10(-i16::from(exponent));
        // If we have just one digit before the decimal point…
        if significand.absolute.nonzero_magnitude_start() == 0 {
            // …round to one fractional digit…
            significand.round(-1);
        } else {
            // …otherwise, we have at least 2 digits before the decimal point,
            // so round to eliminate the fractional part.
            significand.round(0);
        }
        let rounded_magnitude =
            significand.absolute.nonzero_magnitude_start() + i16::from(exponent);
        if rounded_magnitude > log10_type {
            // We got bumped up a magnitude by rounding.
            // This means that `significand` is a power of 10.
            let old_exponent = exponent;
            // NOTE(egg): We could inline `plural_map_and_exponent_for_magnitude`
            // to avoid iterating twice (we only need to look at the next key),
            // but this obscures the logic and the map is tiny.
            (plural_map, exponent) = self.plural_map_and_exponent_for_magnitude(rounded_magnitude);
            significand = significand.clone();
            significand.multiply_pow10(i16::from(old_exponent) - i16::from(exponent));
            // There is no need to perform any rounding: `significand`, being
            // a power of 10, is as round as it gets, and since `exponent` can
            // only have become larger, it is already the correct rounding of
            // `unrounded` to the precision we want to show.
        }
        significand.absolute.trim_end();
        FormattedCompactDecimal {
            formatter: self,
            plural_map,
            value: Cow::Owned(CompactDecimal::from_significand_and_exponent(
                significand,
                exponent,
            )),
        }
    }

    /// Formats a [`CompactDecimal`] object according to locale data.
    ///
    /// This is an advanced API; prefer using [`Self::format_i64()`] in simple
    /// cases.
    ///
    /// Since the caller specifies the exact digits that are displayed, this
    /// allows for arbitrarily complex rounding rules.
    /// However, contrary to [`DecimalFormatter::format()`], this operation
    /// can fail, because the given [`CompactDecimal`] can be inconsistent with
    /// the locale data; for instance, if the locale uses lakhs and crores and
    /// millions are requested, or vice versa, this function returns an error.
    ///
    /// The given [`CompactDecimal`] should be constructed using
    /// [`Self::compact_exponent_for_magnitude()`] on the same
    /// [`CompactDecimalFormatter`] object.
    /// Specifically, `formatter.format_compact_decimal(n)` requires that `n.exponent()`
    /// be equal to `formatter.compact_exponent_for_magnitude(n.significand().nonzero_magnitude_start() + n.exponent())`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use icu::experimental::compactdecimal::CompactDecimalFormatter;
    /// # use icu::locale::locale;
    /// # use writeable::assert_writeable_eq;
    /// # use std::str::FromStr;
    /// use fixed_decimal::CompactDecimal;
    ///
    /// # let short_french = CompactDecimalFormatter::try_new_short(
    /// #    locale!("fr").into(),
    /// #    Default::default(),
    /// # ).unwrap();
    /// # let long_french = CompactDecimalFormatter::try_new_long(
    /// #    locale!("fr").into(),
    /// #    Default::default()
    /// # ).unwrap();
    /// # let long_bangla = CompactDecimalFormatter::try_new_long(
    /// #    locale!("bn").into(),
    /// #    Default::default()
    /// # ).unwrap();
    /// #
    /// let about_a_million = CompactDecimal::from_str("1.20c6").unwrap();
    /// let three_million = CompactDecimal::from_str("+3c6").unwrap();
    /// let ten_lakhs = CompactDecimal::from_str("10c5").unwrap();
    /// # // The following line contains U+00A0 NO-BREAK SPACE.
    /// assert_writeable_eq!(
    ///     short_french
    ///         .format_compact_decimal(&about_a_million)
    ///         .unwrap(),
    ///     "1,20 M"
    /// );
    /// assert_writeable_eq!(
    ///     long_french
    ///         .format_compact_decimal(&about_a_million)
    ///         .unwrap(),
    ///     "1,20 million"
    /// );
    ///
    /// # // The following line contains U+00A0 NO-BREAK SPACE.
    /// assert_writeable_eq!(
    ///     short_french.format_compact_decimal(&three_million).unwrap(),
    ///     "+3 M"
    /// );
    /// assert_writeable_eq!(
    ///     long_french.format_compact_decimal(&three_million).unwrap(),
    ///     "+3 millions"
    /// );
    ///
    /// assert_writeable_eq!(
    ///     long_bangla.format_compact_decimal(&ten_lakhs).unwrap(),
    ///     "১০ লাখ"
    /// );
    ///
    /// assert_eq!(
    ///     long_bangla
    ///         .format_compact_decimal(&about_a_million)
    ///         .err()
    ///         .unwrap()
    ///         .to_string(),
    ///     "Expected compact exponent 5 for 10^6, got 6",
    /// );
    /// assert_eq!(
    ///     long_french
    ///         .format_compact_decimal(&ten_lakhs)
    ///         .err()
    ///         .unwrap()
    ///         .to_string(),
    ///     "Expected compact exponent 6 for 10^6, got 5",
    /// );
    ///
    /// /// Some patterns omit the digits; in those cases, the output does not
    /// /// contain the sequence of digits specified by the CompactDecimal.
    /// let a_thousand = CompactDecimal::from_str("1c3").unwrap();
    /// assert_writeable_eq!(
    ///     long_french.format_compact_decimal(&a_thousand).unwrap(),
    ///     "mille"
    /// );
    /// ```
    pub fn format_compact_decimal<'l>(
        &'l self,
        value: &'l CompactDecimal,
    ) -> Result<FormattedCompactDecimal<'l>, ExponentError> {
        let log10_type =
            value.significand().absolute.nonzero_magnitude_start() + i16::from(value.exponent());

        let (plural_map, expected_exponent) =
            self.plural_map_and_exponent_for_magnitude(log10_type);
        if value.exponent() != expected_exponent {
            return Err(ExponentError {
                actual: value.exponent(),
                expected: expected_exponent,
                log10_type,
            });
        }

        Ok(FormattedCompactDecimal {
            formatter: self,
            plural_map,
            value: Cow::Borrowed(value),
        })
    }

    /// Returns the compact decimal exponent that should be used for a number of
    /// the given magnitude when using this formatter.
    ///
    /// # Examples
    /// ```
    /// use icu::experimental::compactdecimal::CompactDecimalFormatter;
    /// use icu::locale::locale;
    ///
    /// let [long_french, long_japanese, long_bangla] = [
    ///     locale!("fr").into(),
    ///     locale!("ja").into(),
    ///     locale!("bn").into(),
    /// ]
    /// .map(|locale| {
    ///     CompactDecimalFormatter::try_new_long(locale, Default::default())
    ///         .unwrap()
    /// });
    /// /// French uses millions.
    /// assert_eq!(long_french.compact_exponent_for_magnitude(6), 6);
    /// /// Bangla uses lakhs.
    /// assert_eq!(long_bangla.compact_exponent_for_magnitude(6), 5);
    /// /// Japanese uses myriads.
    /// assert_eq!(long_japanese.compact_exponent_for_magnitude(6), 4);
    /// ```
    pub fn compact_exponent_for_magnitude(&self, magnitude: i16) -> u8 {
        let (_, exponent) = self.plural_map_and_exponent_for_magnitude(magnitude);
        exponent
    }

    fn plural_map_and_exponent_for_magnitude(
        &self,
        magnitude: i16,
    ) -> (Option<ZeroMap2dCursor<'_, '_, i8, Count, PatternULE>>, u8) {
        let plural_map = self
            .compact_data
            .get()
            .patterns
            .iter0()
            .filter(|cursor| i16::from(*cursor.key0()) <= magnitude)
            .last();
        let exponent = plural_map
            .as_ref()
            .and_then(|map| {
                map.get1(&Count::Other)
                    .and_then(|pattern| u8::try_from(pattern.exponent).ok())
            })
            .unwrap_or(0);
        (plural_map, exponent)
    }
}

#[cfg(feature = "serde")]
#[cfg(test)]
mod tests {
    use super::*;
    use icu_decimal::options::GroupingStrategy;
    use icu_locale_core::locale;
    use writeable::assert_writeable_eq;

    #[allow(non_snake_case)]
    #[test]
    fn test_grouping() {
        // https://unicode-org.atlassian.net/browse/ICU-22254
        #[derive(Debug)]
        struct TestCase<'a> {
            short: bool,
            options: CompactDecimalFormatterOptions,
            expected1T: &'a str,
            expected10T: &'a str,
        }
        let cases = [
            TestCase {
                short: true,
                options: Default::default(),
                expected1T: "1000T",
                expected10T: "10,000T",
            },
            TestCase {
                short: true,
                options: GroupingStrategy::Always.into(),
                expected1T: "1,000T",
                expected10T: "10,000T",
            },
            TestCase {
                short: true,
                options: GroupingStrategy::Never.into(),
                expected1T: "1000T",
                expected10T: "10000T",
            },
            TestCase {
                short: false,
                options: Default::default(),
                expected1T: "1000 trillion",
                expected10T: "10,000 trillion",
            },
            TestCase {
                short: false,
                options: GroupingStrategy::Always.into(),
                expected1T: "1,000 trillion",
                expected10T: "10,000 trillion",
            },
            TestCase {
                short: false,
                options: GroupingStrategy::Never.into(),
                expected1T: "1000 trillion",
                expected10T: "10000 trillion",
            },
        ];
        for case in cases {
            let formatter = if case.short {
                CompactDecimalFormatter::try_new_short(locale!("en").into(), case.options.clone())
            } else {
                CompactDecimalFormatter::try_new_long(locale!("en").into(), case.options.clone())
            }
            .unwrap();
            let result1T = formatter.format_i64(1_000_000_000_000_000);
            assert_writeable_eq!(result1T, case.expected1T, "{:?}", case);
            let result10T = formatter.format_i64(10_000_000_000_000_000);
            assert_writeable_eq!(result10T, case.expected10T, "{:?}", case);
        }
    }
}
