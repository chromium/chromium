// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data structs and markers for semantic skeletons and datetime names.

mod adapter;

use crate::provider::pattern::runtime::{self, PatternULE};
use crate::size_test_macro::size_test;
use alloc::borrow::Cow;
use icu_pattern::SinglePlaceholderPattern;
use icu_provider::prelude::*;
use potential_utf::PotentialUtf8;
use zerovec::{VarZeroVec, ZeroMap};

/// Helpers involving the data marker attributes used for date names.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[allow(missing_docs)]
pub mod marker_attrs {
    use icu_provider::DataMarkerAttributes;

    pub const NUMERIC: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("1");
    pub const ABBR: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("3");
    pub const NARROW: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("4");
    pub const WIDE: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("5");
    pub const SHORT: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("6");
    pub const ABBR_STANDALONE: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("3s");
    pub const NARROW_STANDALONE: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("4s");
    pub const WIDE_STANDALONE: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("5s");
    pub const SHORT_STANDALONE: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("6s");

    pub const PATTERN_LONG: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("l");
    pub const PATTERN_MEDIUM: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("m");
    pub const PATTERN_SHORT: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("s");

    // TODO: The 12-hour and 24-hour DataMarkerAttributes can probably be deleted

    pub const PATTERN_LONG12: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("l12");
    pub const PATTERN_MEDIUM12: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("m12");
    pub const PATTERN_SHORT12: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("s12");

    pub const PATTERN_LONG24: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("l24");
    pub const PATTERN_MEDIUM24: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("m24");
    pub const PATTERN_SHORT24: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("s24");

    pub const PATTERN_LONG_DT: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("ldt");
    pub const PATTERN_MEDIUM_DT: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("mdt");
    pub const PATTERN_SHORT_DT: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("sdt");

    pub const PATTERN_LONG_DZ: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("ldz");
    pub const PATTERN_MEDIUM_DZ: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("mdz");
    pub const PATTERN_SHORT_DZ: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("sdz");

    pub const PATTERN_LONG_TZ: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("ltz");
    pub const PATTERN_MEDIUM_TZ: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("mtz");
    pub const PATTERN_SHORT_TZ: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("stz");

    pub const PATTERN_LONG_DTZ: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("ldtz");
    pub const PATTERN_MEDIUM_DTZ: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("mdtz");
    pub const PATTERN_SHORT_DTZ: &DataMarkerAttributes =
        DataMarkerAttributes::from_str_or_panic("sdtz");

    pub const NUMERIC_STR: &str = NUMERIC.as_str();
    pub const ABBR_STR: &str = ABBR.as_str();
    pub const NARROW_STR: &str = NARROW.as_str();
    pub const WIDE_STR: &str = WIDE.as_str();
    pub const SHORT_STR: &str = SHORT.as_str();
    pub const ABBR_STANDALONE_STR: &str = ABBR_STANDALONE.as_str();
    pub const NARROW_STANDALONE_STR: &str = NARROW_STANDALONE.as_str();
    pub const WIDE_STANDALONE_STR: &str = WIDE_STANDALONE.as_str();
    pub const SHORT_STANDALONE_STR: &str = SHORT_STANDALONE.as_str();

    pub const PATTERN_LONG_STR: &str = PATTERN_LONG.as_str();
    pub const PATTERN_MEDIUM_STR: &str = PATTERN_MEDIUM.as_str();
    pub const PATTERN_SHORT_STR: &str = PATTERN_SHORT.as_str();

    pub const PATTERN_LONG12_STR: &str = PATTERN_LONG12.as_str();
    pub const PATTERN_MEDIUM12_STR: &str = PATTERN_MEDIUM12.as_str();
    pub const PATTERN_SHORT12_STR: &str = PATTERN_SHORT12.as_str();

    pub const PATTERN_LONG24_STR: &str = PATTERN_LONG24.as_str();
    pub const PATTERN_MEDIUM24_STR: &str = PATTERN_MEDIUM24.as_str();
    pub const PATTERN_SHORT24_STR: &str = PATTERN_SHORT24.as_str();

    pub const PATTERN_LONG_DT_STR: &str = PATTERN_LONG_DT.as_str();
    pub const PATTERN_MEDIUM_DT_STR: &str = PATTERN_MEDIUM_DT.as_str();
    pub const PATTERN_SHORT_DT_STR: &str = PATTERN_SHORT_DT.as_str();

    pub const PATTERN_LONG_DZ_STR: &str = PATTERN_LONG_DZ.as_str();
    pub const PATTERN_MEDIUM_DZ_STR: &str = PATTERN_MEDIUM_DZ.as_str();
    pub const PATTERN_SHORT_DZ_STR: &str = PATTERN_SHORT_DZ.as_str();

    pub const PATTERN_LONG_TZ_STR: &str = PATTERN_LONG_TZ.as_str();
    pub const PATTERN_MEDIUM_TZ_STR: &str = PATTERN_MEDIUM_TZ.as_str();
    pub const PATTERN_SHORT_TZ_STR: &str = PATTERN_SHORT_TZ.as_str();

    pub const PATTERN_LONG_DTZ_STR: &str = PATTERN_LONG_DTZ.as_str();
    pub const PATTERN_MEDIUM_DTZ_STR: &str = PATTERN_MEDIUM_DTZ.as_str();
    pub const PATTERN_SHORT_DTZ_STR: &str = PATTERN_SHORT_DTZ.as_str();

    /// Field lengths supported in data marker attribute.
    ///
    /// For a stable version of this enum, use [`FieldLength`].
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
    /// to be stable, their Rust representation might not be. Use with caution.
    /// </div>
    ///
    /// [`FieldLength`]: crate::fields::FieldLength
    #[derive(Copy, Clone, Debug, PartialEq, Eq)]
    #[allow(clippy::exhaustive_enums)] // documented as unstable
    pub enum Length {
        Abbr,
        Narrow,
        Wide,
        Short,
        Numeric,
    }

    /// Pattern lengths supported in data marker attributes.
    ///
    /// For a stable version of this enum, use [`Length`].
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
    /// to be stable, their Rust representation might not be. Use with caution.
    /// </div>
    ///
    /// [`Length`]: crate::options::Length
    #[derive(Copy, Clone, Debug, PartialEq, Eq)]
    pub enum PatternLength {
        Long,
        Medium,
        Short,
    }

    /// Field contexts supported in data marker attributes.
    ///
    /// For a stable version of this enum, use one of the specific field symbol enums in [`fields`].
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
    /// to be stable, their Rust representation might not be. Use with caution.
    /// </div>
    ///
    /// [`fields`]: crate::fields
    #[derive(Copy, Clone, Debug, PartialEq, Eq)]
    #[allow(clippy::exhaustive_enums)] // documented as unstable
    pub enum Context {
        Format,
        Standalone,
    }

    /// Date, time, and time zone combinations supported in data marker attributes.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
    /// to be stable, their Rust representation might not be. Use with caution.
    /// </div>
    ///
    /// [`fields`]: crate::fields
    #[derive(Copy, Clone, Debug, PartialEq, Eq)]
    #[allow(clippy::exhaustive_enums)] // documented as unstable
    pub enum GlueType {
        DateTime,
        DateZone,
        TimeZone,
        DateTimeZone,
    }

    /// Parses a name data marker attribute to enum values.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
    /// to be stable, their Rust representation might not be. Use with caution.
    /// </div>
    pub fn name_marker_attr_info(marker_attr: &DataMarkerAttributes) -> Option<(Context, Length)> {
        use {Context::*, Length::*};
        match &**marker_attr {
            NUMERIC_STR => Some((Format, Numeric)),
            ABBR_STR => Some((Format, Abbr)),
            NARROW_STR => Some((Format, Narrow)),
            WIDE_STR => Some((Format, Wide)),
            SHORT_STR => Some((Format, Short)),
            ABBR_STANDALONE_STR => Some((Standalone, Abbr)),
            NARROW_STANDALONE_STR => Some((Standalone, Narrow)),
            WIDE_STANDALONE_STR => Some((Standalone, Wide)),
            SHORT_STANDALONE_STR => Some((Standalone, Short)),
            _ => None,
        }
    }

    /// Parses a pattern data marker attribute to enum values.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
    /// to be stable, their Rust representation might not be. Use with caution.
    /// </div>
    pub fn pattern_marker_attr_info_for_glue(
        marker_attr: &DataMarkerAttributes,
    ) -> Option<(PatternLength, GlueType)> {
        use {GlueType::*, PatternLength::*};
        match &**marker_attr {
            PATTERN_LONG_DT_STR => Some((Long, DateTime)),
            PATTERN_MEDIUM_DT_STR => Some((Medium, DateTime)),
            PATTERN_SHORT_DT_STR => Some((Short, DateTime)),

            PATTERN_LONG_DZ_STR => Some((Long, DateZone)),
            PATTERN_MEDIUM_DZ_STR => Some((Medium, DateZone)),
            PATTERN_SHORT_DZ_STR => Some((Short, DateZone)),

            PATTERN_LONG_TZ_STR => Some((Long, TimeZone)),
            PATTERN_MEDIUM_TZ_STR => Some((Medium, TimeZone)),
            PATTERN_SHORT_TZ_STR => Some((Short, TimeZone)),

            PATTERN_LONG_DTZ_STR => Some((Long, DateTimeZone)),
            PATTERN_MEDIUM_DTZ_STR => Some((Medium, DateTimeZone)),
            PATTERN_SHORT_DTZ_STR => Some((Short, DateTimeZone)),

            _ => None,
        }
    }

    /// Creates a name data marker attribute from the enum values.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
    /// to be stable, their Rust representation might not be. Use with caution.
    /// </div>
    pub fn name_attr_for(context: Context, length: Length) -> &'static DataMarkerAttributes {
        use {Context::*, Length::*};
        match (context, length) {
            (Format, Numeric) => NUMERIC,
            (Format, Abbr) => ABBR,
            (Format, Narrow) => NARROW,
            (Format, Wide) => WIDE,
            (Format, Short) => SHORT,
            (Standalone, Numeric) => NUMERIC,
            (Standalone, Abbr) => ABBR_STANDALONE,
            (Standalone, Narrow) => NARROW_STANDALONE,
            (Standalone, Wide) => WIDE_STANDALONE,
            (Standalone, Short) => SHORT_STANDALONE,
        }
    }

    pub fn pattern_marker_attr_for_glue(
        length: PatternLength,
        glue_type: GlueType,
    ) -> &'static DataMarkerAttributes {
        use {GlueType::*, PatternLength::*};
        match (length, glue_type) {
            (Long, DateTime) => PATTERN_LONG_DT,
            (Medium, DateTime) => PATTERN_MEDIUM_DT,
            (Short, DateTime) => PATTERN_SHORT_DT,

            (Long, DateZone) => PATTERN_LONG_DZ,
            (Medium, DateZone) => PATTERN_MEDIUM_DZ,
            (Short, DateZone) => PATTERN_SHORT_DZ,

            (Long, TimeZone) => PATTERN_LONG_TZ,
            (Medium, TimeZone) => PATTERN_MEDIUM_TZ,
            (Short, TimeZone) => PATTERN_SHORT_TZ,

            (Long, DateTimeZone) => PATTERN_LONG_DTZ,
            (Medium, DateTimeZone) => PATTERN_MEDIUM_DTZ,
            (Short, DateTimeZone) => PATTERN_SHORT_DTZ,
        }
    }
}

size_test!(YearNamesV1, year_names_v1_size, 48);

/// Names used for representing the year.
///
/// This uses a data marker attribute for length. The value is simply the number of
/// characters in the equivalent CLDR field syntax name, plus "s" for standalone contexts. For example,
/// "abbreviated" (e.g. `MMM`) is `3` or `3s` depending on whether it is format or standalone
/// respectively.
///
/// The full list is:
/// - 3 is "abbreviated"
/// - 4 is "narrow"
/// - 5 is "wide"
/// - 6 is "short" (weekdays only)
#[doc = year_names_v1_size!()]
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(
    marker(BuddhistYearNamesV1Marker, "datetime/names/buddhist/years@1"),
    marker(ChineseYearNamesV1Marker, "datetime/names/chinese/years@1"),
    marker(CopticYearNamesV1Marker, "datetime/names/coptic/years@1"),
    marker(DangiYearNamesV1Marker, "datetime/names/dangi/years@1"),
    marker(EthiopianYearNamesV1Marker, "datetime/names/ethiopic/years@1"),
    marker(GregorianYearNamesV1Marker, "datetime/names/gregory/years@1"),
    marker(HebrewYearNamesV1Marker, "datetime/names/hebrew/years@1"),
    marker(IndianYearNamesV1Marker, "datetime/names/indian/years@1"),
    marker(IslamicYearNamesV1Marker, "datetime/names/islamic/years@1"),
    marker(JapaneseYearNamesV1Marker, "datetime/names/japanese/years@1"),
    marker(JapaneseExtendedYearNamesV1Marker, "datetime/names/japanext/years@1"),
    marker(PersianYearNamesV1Marker, "datetime/names/persian/years@1"),
    marker(RocYearNamesV1Marker, "datetime/names/roc/years@1")
)]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::neo))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub enum YearNamesV1<'data> {
    /// This calendar has a small, fixed set of eras with numeric years, this stores the era names in chronological order.
    ///
    /// See FormattableEra for a definition of what chronological order is in this context.
    FixedEras(#[cfg_attr(feature = "serde", serde(borrow))] VarZeroVec<'data, str>),
    /// This calendar has a variable set of eras with numeric years, this stores the era names mapped from
    /// era code to the name.
    ///
    /// Only the Japanese calendars need this
    VariableEras(#[cfg_attr(feature = "serde", serde(borrow))] ZeroMap<'data, PotentialUtf8, str>),
    /// This calendar is cyclic (Chinese, Dangi), so it uses cyclic year names without any eras
    Cyclic(#[cfg_attr(feature = "serde", serde(borrow))] VarZeroVec<'data, str>),
}

size_test!(MonthNamesV1, month_names_v1_size, 32);

/// Names used for representing the month.
///
/// This uses a data marker attribute for length. See [`YearNamesV1`] for more information on the scheme. This
/// has an additional `1` value used for numeric names, only found for calendars with leap months.
#[doc = month_names_v1_size!()]
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(
    marker(BuddhistMonthNamesV1Marker, "datetime/names/buddhist/months@1"),
    marker(ChineseMonthNamesV1Marker, "datetime/names/chinese/months@1"),
    marker(CopticMonthNamesV1Marker, "datetime/names/coptic/months@1"),
    marker(DangiMonthNamesV1Marker, "datetime/names/dangi/months@1"),
    marker(EthiopianMonthNamesV1Marker, "datetime/names/ethiopic/months@1"),
    marker(GregorianMonthNamesV1Marker, "datetime/names/gregory/months@1"),
    marker(HebrewMonthNamesV1Marker, "datetime/names/hebrew/months@1"),
    marker(IndianMonthNamesV1Marker, "datetime/names/indian/months@1"),
    marker(IslamicMonthNamesV1Marker, "datetime/names/islamic/months@1"),
    marker(JapaneseMonthNamesV1Marker, "datetime/names/japanese/months@1"),
    marker(JapaneseExtendedMonthNamesV1Marker, "datetime/names/japanext/months@1"),
    marker(PersianMonthNamesV1Marker, "datetime/names/persian/months@1"),
    marker(RocMonthNamesV1Marker, "datetime/names/roc/months@1")
)]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::neo))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub enum MonthNamesV1<'data> {
    /// Month codes M01, M02, M03, .. (can allow for M13 onwards)
    ///
    /// Found for solar and pure lunar calendars
    Linear(#[cfg_attr(feature = "serde", serde(borrow))] VarZeroVec<'data, str>),
    /// Month codes M01, M02, M03, .. M01L, M02L, ...
    ///
    /// Empty entries for non-present month codes. Will have an equal number of leap and non-leap
    /// entries.
    ///
    /// Found for lunisolar and lunisidereal calendars
    LeapLinear(#[cfg_attr(feature = "serde", serde(borrow))] VarZeroVec<'data, str>),

    /// This represents the formatting to apply to numeric values to produce the corresponding
    /// leap month symbol.
    ///
    /// For numeric formatting only, on calendars with leap months
    LeapNumeric(
        #[cfg_attr(
            feature = "serde",
            serde(
                borrow,
                deserialize_with = "icu_pattern::deserialize_borrowed_cow::<icu_pattern::SinglePlaceholder, _>"
            )
        )]
        Cow<'data, SinglePlaceholderPattern>,
    ),
}

size_test!(LinearNamesV1, linear_names_v1_size, 24);

/// Names that can be stored as a simple linear array.
///
/// - For weekdays, element 0 is Sunday
/// - For dayperiods, the elements are in order: AM, PM, (noon), (midnight), where the latter two are optional.
///   In the case noon is missing but midnight is present, the noon value can be the empty string. This is unlikely.
/// - For day names element 0 is the first day of the month
///
/// This uses a data marker attribute for length. See [`YearNamesV1`] for more information on the scheme.
#[doc = linear_names_v1_size!()]
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(
    marker(WeekdayNamesV1Marker, "datetime/names/weekdays@1"),
    marker(DayPeriodNamesV1Marker, "datetime/names/dayperiods@1"),

    // We're not producing or using day names yet, but this is where they would go
    marker(ChineseDayNamesV1Marker, "datetime/names/chinese/days@1"),
    marker(DangiDayNamesV1Marker, "datetime/names/dangi/days@1"),
    // for calendars that don't use day names
    marker(PlaceholderDayNamesV1Marker, "datetime/names/placeholder/days@1"),
)]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::neo))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct LinearNamesV1<'data> {
    #[cfg_attr(feature = "serde", serde(borrow))]
    /// The names, in order. Order specified on the struct docs.
    // This uses a VarZeroVec rather than a fixed-size array for weekdays to save stack space
    pub names: VarZeroVec<'data, str>,
}

impl LinearNamesV1<'_> {
    /// Gets the 'am' name assuming this struct contains day period data.
    pub(crate) fn am(&self) -> Option<&str> {
        self.names.get(0)
    }
    /// Gets the 'pm' name assuming this struct contains day period data.
    pub(crate) fn pm(&self) -> Option<&str> {
        self.names.get(1)
    }
    /// Gets the 'noon' name assuming this struct contains day period data.
    pub(crate) fn noon(&self) -> Option<&str> {
        self.names
            .get(2)
            .and_then(|s| if s.is_empty() { None } else { Some(s) })
    }
    /// Gets the 'midnight' name assuming this struct contains day period data.
    pub(crate) fn midnight(&self) -> Option<&str> {
        self.names.get(3)
    }
}

// TODO: We may need to support plural forms here. Something like
// pub enum NeoPatternPlurals<'data> {
//     SingleDate(runtime::Pattern<'data>),
//     WeekPlurals(ZeroMap<'data, PluralCategory, runtime::PatternULE>),
// }

size_test!(GluePatternV1, glue_pattern_v1_size, 24);

/// The default per-length patterns used for combining dates, times, and timezones into formatted strings.
#[doc = glue_pattern_v1_size!()]
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(marker(GluePatternV1Marker, "datetime/patterns/glue@1"))]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::neo))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct GluePatternV1<'data> {
    /// The pattern
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub pattern: runtime::GenericPattern<'data>,
}

#[icu_provider::data_struct(marker(
    DateTimeSkeletonPatternsV1Marker,
    "datetime/patterns/datetime_skeleton@1"
))]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::neo))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
#[allow(missing_docs)] // TODO
pub struct DateTimeSkeletonsV1<'data> {
    // will typically be small, there are only a couple special cases like E B h m
    // TODO: This should support plurals
    // TODO: The key of this map should be Skeleton
    #[allow(missing_docs)] // TODO
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub map: ZeroMap<'data, str, PatternULE>,
}

/// Calendar-agnostic year name data marker
#[derive(Debug)]
pub struct YearNamesV1Marker;
impl DynamicDataMarker for YearNamesV1Marker {
    type DataStruct = YearNamesV1<'static>;
}

/// Calendar-agnostic month name data marker
#[derive(Debug)]
pub struct MonthNamesV1Marker;
impl DynamicDataMarker for MonthNamesV1Marker {
    type DataStruct = MonthNamesV1<'static>;
}
