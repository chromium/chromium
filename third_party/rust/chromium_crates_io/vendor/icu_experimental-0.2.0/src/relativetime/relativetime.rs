// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use fixed_decimal::{FixedDecimal, Sign};
use icu_decimal::{
    options::FixedDecimalFormatterOptions, provider::DecimalDigitsV1Marker,
    provider::DecimalSymbolsV2Marker, FixedDecimalFormatter, FixedDecimalFormatterPreferences,
};
use icu_locale_core::preferences::{define_preferences, prefs_convert};
use icu_plurals::PluralRulesPreferences;
use icu_plurals::{provider::CardinalV1Marker, PluralRules};
use icu_provider::marker::ErasedMarker;
use icu_provider::prelude::*;

use crate::relativetime::format::FormattedRelativeTime;
use crate::relativetime::options::RelativeTimeFormatterOptions;
use crate::relativetime::provider::*;

define_preferences!(
    /// The preferences for relative time formatting.
    [Copy]
    RelativeTimeFormatterPreferences,
    {}
);
prefs_convert!(
    RelativeTimeFormatterPreferences,
    FixedDecimalFormatterPreferences
);
prefs_convert!(RelativeTimeFormatterPreferences, PluralRulesPreferences);

/// A formatter to render locale-sensitive relative time.
///
/// # Example
///
/// ```
/// use fixed_decimal::FixedDecimal;
/// use icu::experimental::relativetime::{
///     RelativeTimeFormatter, RelativeTimeFormatterOptions,
/// };
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// let relative_time_formatter = RelativeTimeFormatter::try_new_long_second(
///     locale!("en").into(),
///     RelativeTimeFormatterOptions::default(),
/// )
/// .expect("locale should be present");
///
/// assert_writeable_eq!(
///     relative_time_formatter.format(FixedDecimal::from(5i8)),
///     "in 5 seconds"
/// );
/// assert_writeable_eq!(
///     relative_time_formatter.format(FixedDecimal::from(-10i8)),
///     "10 seconds ago"
/// );
/// ```
///
/// # Example
///
/// ```
/// use fixed_decimal::FixedDecimal;
/// use icu::experimental::relativetime::options::Numeric;
/// use icu::experimental::relativetime::{
///     RelativeTimeFormatter, RelativeTimeFormatterOptions,
/// };
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// let relative_time_formatter = RelativeTimeFormatter::try_new_short_day(
///     locale!("es").into(),
///     RelativeTimeFormatterOptions {
///         numeric: Numeric::Auto,
///     },
/// )
/// .expect("locale should be present");
///
/// assert_writeable_eq!(
///     relative_time_formatter.format(FixedDecimal::from(0u8)),
///     "hoy"
/// );
/// assert_writeable_eq!(
///     relative_time_formatter.format(FixedDecimal::from(-2i8)),
///     "anteayer"
/// );
/// assert_writeable_eq!(
///     relative_time_formatter.format(FixedDecimal::from(2u8)),
///     "pasado mañana"
/// );
/// assert_writeable_eq!(
///     relative_time_formatter.format(FixedDecimal::from(15i8)),
///     "dentro de 15 d"
/// );
/// ```
///
/// # Example
/// ```
/// use fixed_decimal::FixedDecimal;
/// use icu::experimental::relativetime::{
///     RelativeTimeFormatter, RelativeTimeFormatterOptions,
/// };
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// let relative_time_formatter = RelativeTimeFormatter::try_new_narrow_year(
///     locale!("bn").into(),
///     RelativeTimeFormatterOptions::default(),
/// )
/// .expect("locale should be present");
///
/// assert_writeable_eq!(
///     relative_time_formatter.format(FixedDecimal::from(3u8)),
///     "৩ বছরে"
/// );
/// assert_writeable_eq!(
///     relative_time_formatter.format(FixedDecimal::from(-15i8)),
///     "১৫ বছর পূর্বে"
/// );
/// ```
pub struct RelativeTimeFormatter {
    pub(crate) plural_rules: PluralRules,
    pub(crate) rt: DataPayload<ErasedMarker<RelativeTimePatternDataV1<'static>>>,
    pub(crate) options: RelativeTimeFormatterOptions,
    pub(crate) fixed_decimal_format: FixedDecimalFormatter,
}

macro_rules! constructor {
    ($unstable: ident, $baked: ident, $any: ident, $buffer: ident, $marker: ty) => {

        /// Create a new [`RelativeTimeFormatter`] from compiled data.
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// [📚 Help choosing a constructor](icu_provider::constructors)
        #[cfg(feature = "compiled_data")]
        pub fn $baked(
            prefs: RelativeTimeFormatterPreferences,
            options: RelativeTimeFormatterOptions,
        ) -> Result<Self, DataError> {
            let locale = DataLocale::from_preferences_locale::<$marker>(prefs.locale_prefs);
            let plural_rules = PluralRules::try_new_cardinal((&prefs).into())?;
            // Initialize FixedDecimalFormatter with default options
            let fixed_decimal_format = FixedDecimalFormatter::try_new(
                (&prefs).into(),
                FixedDecimalFormatterOptions::default(),
            )?;
            let rt: DataResponse<$marker> = crate::provider::Baked
                .load(DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&locale),
                    ..Default::default()
                })?;
            let rt = rt.payload.cast();
            Ok(RelativeTimeFormatter {
                plural_rules,
                options,
                rt,
                fixed_decimal_format,
            })
        }

        icu_provider::gen_any_buffer_data_constructors!(
            (prefs: RelativeTimeFormatterPreferences, options: RelativeTimeFormatterOptions) -> error: DataError,
            functions: [
                $baked: skip,
                $any,
                $buffer,
                $unstable,
                Self,
            ]
        );


        #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::$baked)]
        pub fn $unstable<D>(
            provider: &D,
            prefs: RelativeTimeFormatterPreferences,
            options: RelativeTimeFormatterOptions,
        ) -> Result<Self, DataError>
        where
            D: DataProvider<CardinalV1Marker>
                + DataProvider<$marker>
                + DataProvider<DecimalSymbolsV2Marker> + DataProvider<DecimalDigitsV1Marker>
                + ?Sized,
        {
            let locale = DataLocale::from_preferences_locale::<$marker>(prefs.locale_prefs);
            let plural_rules = PluralRules::try_new_cardinal_unstable(provider, (&prefs).into())?;
            // Initialize FixedDecimalFormatter with default options
            let fixed_decimal_format = FixedDecimalFormatter::try_new_unstable(
                provider,
                (&prefs).into(),
                FixedDecimalFormatterOptions::default(),
            )?;
            let rt: DataResponse<$marker> = provider
                .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                    ..Default::default()
                })?;
            let rt = rt.payload.cast();
            Ok(RelativeTimeFormatter {
                plural_rules,
                options,
                rt,
                fixed_decimal_format,
            })
        }
    };
}

impl RelativeTimeFormatter {
    constructor!(
        try_new_long_second_unstable,
        try_new_long_second,
        try_new_long_second_with_any_provider,
        try_new_long_second_with_buffer_provider,
        LongSecondRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_long_minute_unstable,
        try_new_long_minute,
        try_new_long_minute_with_any_provider,
        try_new_long_minute_with_buffer_provider,
        LongMinuteRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_long_hour_unstable,
        try_new_long_hour,
        try_new_long_hour_with_any_provider,
        try_new_long_hour_with_buffer_provider,
        LongHourRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_long_day_unstable,
        try_new_long_day,
        try_new_long_day_with_any_provider,
        try_new_long_day_with_buffer_provider,
        LongDayRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_long_week_unstable,
        try_new_long_week,
        try_new_long_week_with_any_provider,
        try_new_long_week_with_buffer_provider,
        LongWeekRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_long_month_unstable,
        try_new_long_month,
        try_new_long_month_with_any_provider,
        try_new_long_month_with_buffer_provider,
        LongMonthRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_long_quarter_unstable,
        try_new_long_quarter,
        try_new_long_quarter_with_any_provider,
        try_new_long_quarter_with_buffer_provider,
        LongQuarterRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_long_year_unstable,
        try_new_long_year,
        try_new_long_year_with_any_provider,
        try_new_long_year_with_buffer_provider,
        LongYearRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_short_second_unstable,
        try_new_short_second,
        try_new_short_second_with_any_provider,
        try_new_short_second_with_buffer_provider,
        ShortSecondRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_short_minute_unstable,
        try_new_short_minute,
        try_new_short_minute_with_any_provider,
        try_new_short_minute_with_buffer_provider,
        ShortMinuteRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_short_hour_unstable,
        try_new_short_hour,
        try_new_short_hour_with_any_provider,
        try_new_short_hour_with_buffer_provider,
        ShortHourRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_short_day_unstable,
        try_new_short_day,
        try_new_short_day_with_any_provider,
        try_new_short_day_with_buffer_provider,
        ShortDayRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_short_week_unstable,
        try_new_short_week,
        try_new_short_week_with_any_provider,
        try_new_short_week_with_buffer_provider,
        ShortWeekRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_short_month_unstable,
        try_new_short_month,
        try_new_short_month_with_any_provider,
        try_new_short_month_with_buffer_provider,
        ShortMonthRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_short_quarter_unstable,
        try_new_short_quarter,
        try_new_short_quarter_with_any_provider,
        try_new_short_quarter_with_buffer_provider,
        ShortQuarterRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_short_year_unstable,
        try_new_short_year,
        try_new_short_year_with_any_provider,
        try_new_short_year_with_buffer_provider,
        ShortYearRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_narrow_second_unstable,
        try_new_narrow_second,
        try_new_narrow_second_with_any_provider,
        try_new_narrow_second_with_buffer_provider,
        NarrowSecondRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_narrow_minute_unstable,
        try_new_narrow_minute,
        try_new_narrow_minute_with_any_provider,
        try_new_narrow_minute_with_buffer_provider,
        NarrowMinuteRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_narrow_hour_unstable,
        try_new_narrow_hour,
        try_new_narrow_hour_with_any_provider,
        try_new_narrow_hour_with_buffer_provider,
        NarrowHourRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_narrow_day_unstable,
        try_new_narrow_day,
        try_new_narrow_day_with_any_provider,
        try_new_narrow_day_with_buffer_provider,
        NarrowDayRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_narrow_week_unstable,
        try_new_narrow_week,
        try_new_narrow_week_with_any_provider,
        try_new_narrow_week_with_buffer_provider,
        NarrowWeekRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_narrow_month_unstable,
        try_new_narrow_month,
        try_new_narrow_month_with_any_provider,
        try_new_narrow_month_with_buffer_provider,
        NarrowMonthRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_narrow_quarter_unstable,
        try_new_narrow_quarter,
        try_new_narrow_quarter_with_any_provider,
        try_new_narrow_quarter_with_buffer_provider,
        NarrowQuarterRelativeTimeFormatDataV1Marker
    );
    constructor!(
        try_new_narrow_year_unstable,
        try_new_narrow_year,
        try_new_narrow_year_with_any_provider,
        try_new_narrow_year_with_buffer_provider,
        NarrowYearRelativeTimeFormatDataV1Marker
    );

    /// Format a `value` according to the locale and formatting options of
    /// [`RelativeTimeFormatter`].
    pub fn format(&self, value: FixedDecimal) -> FormattedRelativeTime<'_> {
        let is_negative = value.sign() == Sign::Negative;
        FormattedRelativeTime {
            options: &self.options,
            formatter: self,
            value: value.with_sign(Sign::None),
            is_negative,
        }
    }
}
