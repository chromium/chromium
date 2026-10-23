// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::marker::PhantomData;

use crate::dimension::provider::units::categorized_display_names::{
    UnitsNamesDurationCoreV1, UnitsNamesDurationExtendedV1, UnitsNamesDurationOutlierV1,
};
use crate::dimension::units::categorized_formatter::{
    CategorizedFormatter, CategorizedUnitsFormatterPreferences,
};
use crate::dimension::units::options::{UnitsFormatterOptions, Width};
use crate::duration::options::FieldStyle;
use crate::measure::category::CategorizedMeasureUnit;
use crate::measure::measureunit::MeasureUnit;

use super::format::FormattedDuration;
use super::options::BaseStyle;
use super::validated_options::Unit;
use super::{Duration, provider};

pub use super::validated_options::ValidatedDurationFormatterOptions;
use icu_decimal::provider::{DecimalDigitsV1, DecimalSymbolsV1};
use icu_decimal::{DecimalFormatter, DecimalFormatterPreferences};
use icu_list::{ListFormatter, ListFormatterPreferences, options::ListLength};
use icu_locale_core::preferences::{
    define_preferences, extensions::unicode::keywords::NumberingSystem, prefs_convert,
};
use icu_provider::prelude::*;

define_preferences!(
    /// The preferences for duration formatting.
    [Copy]
    DurationFormatterPreferences,
    {
        /// The user's preferred numbering system.
        ///
        /// Corresponds to the `-u-nu` in Unicode Locale Identifier.
        numbering_system: NumberingSystem
    }
);

prefs_convert!(
    DurationFormatterPreferences,
    CategorizedUnitsFormatterPreferences,
    { numbering_system }
);
prefs_convert!(DurationFormatterPreferences, DecimalFormatterPreferences, {
    numbering_system
});
prefs_convert!(DurationFormatterPreferences, ListFormatterPreferences);

/// A formatter for [`Duration`](crate::duration::Duration)s.
///
/// [`DurationFormatter`] supports:
///
/// 1. Rendering with different styles for each unit
/// 2. Digital formatting style
/// 3. Positive and negative duraitons
///
/// Read more about the options in the [`options`](super::options) module.
///
/// See the crate-level documentation for examples.
#[derive(Debug)]
pub struct DurationFormatter {
    /// Options for configuring the formatter.
    pub(crate) options: ValidatedDurationFormatterOptions,
    pub(crate) digital: DataPayload<provider::UnitsDurationDigitalV1>,
    pub(crate) unit: DurationUnitFormatter,
    pub(crate) list: ListFormatter,
    pub(crate) fdf: DecimalFormatter,
}

#[derive(Debug)]
pub(crate) struct DurationUnitFormatter {
    pub(crate) year: CategorizedFormatter<crate::measure::category::Duration>,
    pub(crate) month: CategorizedFormatter<crate::measure::category::Duration>,
    pub(crate) week: CategorizedFormatter<crate::measure::category::Duration>,
    pub(crate) day: CategorizedFormatter<crate::measure::category::Duration>,
    pub(crate) hour: CategorizedFormatter<crate::measure::category::Duration>,
    pub(crate) minute: CategorizedFormatter<crate::measure::category::Duration>,
    pub(crate) second: CategorizedFormatter<crate::measure::category::Duration>,
    pub(crate) millisecond: CategorizedFormatter<crate::measure::category::Duration>,
    pub(crate) microsecond: CategorizedFormatter<crate::measure::category::Duration>,
    pub(crate) nanosecond: CategorizedFormatter<crate::measure::category::Duration>,
}

impl core::ops::Index<Unit> for DurationUnitFormatter {
    type Output = CategorizedFormatter<crate::measure::category::Duration>;

    fn index(&self, index: Unit) -> &Self::Output {
        match index {
            Unit::Year => &self.year,
            Unit::Month => &self.month,
            Unit::Week => &self.week,
            Unit::Day => &self.day,
            Unit::Hour => &self.hour,
            Unit::Minute => &self.minute,
            Unit::Second => &self.second,
            Unit::Millisecond => &self.millisecond,
            Unit::Microsecond => &self.microsecond,
            Unit::Nanosecond => &self.nanosecond,
        }
    }
}

impl DurationUnitFormatter {
    fn field_style_to_unit_width(style: FieldStyle, base: BaseStyle) -> Width {
        match style {
            FieldStyle::Long => Width::Long,
            FieldStyle::Short => Width::Short,
            FieldStyle::Narrow => Width::Narrow,
            _ => match base {
                BaseStyle::Long => Width::Long,
                BaseStyle::Short | BaseStyle::Digital => Width::Short,
                BaseStyle::Narrow => Width::Narrow,
            },
        }
    }

    #[cfg(feature = "compiled_data")]
    fn try_new(
        prefs: DurationFormatterPreferences,
        options: ValidatedDurationFormatterOptions,
    ) -> Result<Self, DataError> {
        let get_unit_formatter = |unit: Unit, style| {
            let w = DurationUnitFormatter::field_style_to_unit_width(style, options.base);
            let options = UnitsFormatterOptions { width: w };

            CategorizedFormatter::<crate::measure::category::Duration>::try_new_outlier(
                (&prefs).into(),
                match unit {
                    Unit::Year => crate::measure::category::Duration::year(),
                    Unit::Month => crate::measure::category::Duration::month(),
                    Unit::Week => crate::measure::category::Duration::week(),
                    Unit::Day => crate::measure::category::Duration::day(),
                    Unit::Hour => crate::measure::category::Duration::hour(),
                    Unit::Minute => crate::measure::category::Duration::minute(),
                    Unit::Second => crate::measure::category::Duration::second(),
                    Unit::Millisecond => crate::measure::category::Duration::millisecond(),
                    Unit::Microsecond => crate::measure::category::Duration::microsecond(),
                    Unit::Nanosecond => crate::measure::category::Duration::nanosecond(),
                },
                options,
            )
        };

        Ok(DurationUnitFormatter {
            year: get_unit_formatter(Unit::Year, options.year)?,
            month: get_unit_formatter(Unit::Month, options.month)?,
            week: get_unit_formatter(Unit::Week, options.week)?,
            day: get_unit_formatter(Unit::Day, options.day)?,
            hour: get_unit_formatter(Unit::Hour, options.hour)?,
            minute: get_unit_formatter(Unit::Minute, options.minute)?,
            second: get_unit_formatter(Unit::Second, options.second)?,
            millisecond: get_unit_formatter(Unit::Millisecond, options.millisecond)?,
            microsecond: get_unit_formatter(Unit::Microsecond, options.microsecond)?,
            nanosecond: get_unit_formatter(Unit::Nanosecond, options.nanosecond)?,
        })
    }

    fn try_new_unstable<
        D: ?Sized
            + DataProvider<UnitsNamesDurationCoreV1>
            + DataProvider<UnitsNamesDurationExtendedV1>
            + DataProvider<UnitsNamesDurationOutlierV1>
            + DataProvider<DecimalSymbolsV1>
            + DataProvider<DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    >(
        provider: &D,
        prefs: DurationFormatterPreferences,
        options: ValidatedDurationFormatterOptions,
    ) -> Result<Self, DataError> {
        let get_unit_formatter = |unit: Unit, style| {
            let w = DurationUnitFormatter::field_style_to_unit_width(style, options.base);
            let options = UnitsFormatterOptions { width: w };

            CategorizedFormatter::<crate::measure::category::Duration>::try_new_outlier_unstable(
                provider,
                (&prefs).into(),
                CategorizedMeasureUnit {
                    _category: PhantomData,
                    unit: MeasureUnit::try_from_str(unit.as_unit_formatter_name()).unwrap(),
                },
                options,
            )
        };

        Ok(DurationUnitFormatter {
            year: get_unit_formatter(Unit::Year, options.year)?,
            month: get_unit_formatter(Unit::Month, options.month)?,
            week: get_unit_formatter(Unit::Week, options.week)?,
            day: get_unit_formatter(Unit::Day, options.day)?,
            hour: get_unit_formatter(Unit::Hour, options.hour)?,
            minute: get_unit_formatter(Unit::Minute, options.minute)?,
            second: get_unit_formatter(Unit::Second, options.second)?,
            millisecond: get_unit_formatter(Unit::Millisecond, options.millisecond)?,
            microsecond: get_unit_formatter(Unit::Microsecond, options.microsecond)?,
            nanosecond: get_unit_formatter(Unit::Nanosecond, options.nanosecond)?,
        })
    }
}

impl From<BaseStyle> for icu_list::options::ListFormatterOptions {
    fn from(style: BaseStyle) -> Self {
        // Section 1.1.13
        // 1. Let lfOpts be OrdinaryObjectCreate(null).
        // 2. Perform ! CreateDataPropertyOrThrow(lfOpts, "type", "unit").
        // 3. Let listStyle be durationFormat.[[Style]].
        // 4. If listStyle is "digital", then
        //     a. Set listStyle to "short".
        // 5. Perform ! CreateDataPropertyOrThrow(lfOpts, "style", listStyle).
        // 6. Let lf be ! Construct(%ListFormat%, « durationFormat.[[Locale]], lfOpts »).
        let length = match style {
            BaseStyle::Long => ListLength::Wide,
            BaseStyle::Short | BaseStyle::Digital => ListLength::Short,
            BaseStyle::Narrow => ListLength::Narrow,
        };
        Self::default().with_length(length)
    }
}

impl DurationFormatter {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: DurationFormatterPreferences, options: ValidatedDurationFormatterOptions) -> error: DataError,
        functions: [
            try_new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    /// Creates a new [`DurationFormatter`] from compiled locale data and an options bag.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new(
        prefs: DurationFormatterPreferences,
        options: ValidatedDurationFormatterOptions,
    ) -> Result<Self, DataError> {
        let locale = provider::UnitsDurationDigitalV1::make_locale(prefs.locale_preferences);
        let digital = crate::provider::Baked
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                ..Default::default()
            })?
            .payload;

        Ok(Self {
            digital,
            options,
            unit: DurationUnitFormatter::try_new(prefs, options)?,
            list: ListFormatter::try_new_unit((&prefs).into(), options.base.into())?,
            fdf: DecimalFormatter::try_new((&prefs).into(), Default::default())?,
        })
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<
        D: DataProvider<provider::UnitsDurationDigitalV1>
            + DataProvider<UnitsNamesDurationCoreV1>
            + DataProvider<UnitsNamesDurationExtendedV1>
            + DataProvider<UnitsNamesDurationOutlierV1>
            + DataProvider<DecimalSymbolsV1>
            + DataProvider<DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>
            + DataProvider<icu_list::provider::ListUnitV1>
            + ?Sized,
    >(
        provider: &D,
        prefs: DurationFormatterPreferences,
        options: ValidatedDurationFormatterOptions,
    ) -> Result<Self, DataError> {
        let locale = provider::UnitsDurationDigitalV1::make_locale(prefs.locale_preferences);
        let digital = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                ..Default::default()
            })?
            .payload;

        Ok(Self {
            digital,
            options,
            unit: DurationUnitFormatter::try_new_unstable(provider, prefs, options)?,
            list: ListFormatter::try_new_unit_unstable(
                provider,
                (&prefs).into(),
                options.base.into(),
            )?,
            fdf: DecimalFormatter::try_new_unstable(provider, (&prefs).into(), Default::default())?,
        })
    }

    /// Formats a [`Duration`](crate::duration::Duration) into a [`FormattedDuration`].
    pub fn format<'l>(&'l self, duration: &'l Duration) -> FormattedDuration<'l> {
        FormattedDuration {
            fmt: self,
            duration,
        }
    }
}
