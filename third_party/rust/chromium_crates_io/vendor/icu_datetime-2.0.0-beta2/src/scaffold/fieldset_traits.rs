// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::{
    provider::{neo::*, time_zones::tz, *},
    scaffold::*,
};
use icu_calendar::{
    provider::{
        CalendarChineseV1, CalendarDangiV1, CalendarIslamicObservationalV1,
        CalendarIslamicUmmalquraV1, CalendarJapaneseExtendedV1, CalendarJapaneseModernV1,
    },
    types::{DayOfMonth, DayOfYearInfo, MonthInfo, Weekday, YearInfo},
    Date, Iso,
};
use icu_decimal::provider::{DecimalDigitsV1, DecimalSymbolsV2};
use icu_provider::{marker::NeverMarker, prelude::*};
use icu_time::scaffold::IntoOption;
use icu_time::{
    zone::{TimeZoneVariant, UtcOffset},
    Hour, Minute, Nanosecond, Second, Time, TimeZone,
};

// TODO: Add WeekCalculator and DecimalFormatter optional bindings here

/// A trait associating types for date formatting in any calendar
/// (input types only).
///
/// This is a sealed trait implemented on field set markers.
///
/// <div class="stab unstable">
/// 🚧 This trait is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not implement this trait in userland unless you are prepared for things to occasionally break.
/// </div>
pub trait DateInputMarkers: UnstableSealed {
    /// Marker for resolving the year input field.
    type YearInput: IntoOption<YearInfo>;
    /// Marker for resolving the month input field.
    type MonthInput: IntoOption<MonthInfo>;
    /// Marker for resolving the day-of-month input field.
    type DayOfMonthInput: IntoOption<DayOfMonth>;
    /// Marker for resolving the day-of-year input field.
    type DayOfYearInput: IntoOption<DayOfYearInfo>;
    /// Marker for resolving the day-of-week input field.
    type DayOfWeekInput: IntoOption<Weekday>;
}

/// A trait associating types for date formatting in a specific calendar
/// (data markers only).
///
/// This is a sealed trait implemented on field set markers.
///
/// <div class="stab unstable">
/// 🚧 This trait is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not implement this trait in userland unless you are prepared for things to occasionally break.
/// </div>
pub trait TypedDateDataMarkers<C>: UnstableSealed {
    /// Marker for loading date skeleton patterns.
    type DateSkeletonPatternsV1: DataMarker<DataStruct = PackedPatterns<'static>>;
    /// Marker for loading year names.
    type YearNamesV1: DataMarker<DataStruct = YearNames<'static>>;
    /// Marker for loading month names.
    type MonthNamesV1: DataMarker<DataStruct = MonthNames<'static>>;
    /// Marker for loading weekday names.
    type WeekdayNamesV1: DataMarker<DataStruct = LinearNames<'static>>;
}

/// A trait associating types for date formatting in any calendar
/// (data markers only).
///
/// This is a sealed trait implemented on field set markers.
///
/// <div class="stab unstable">
/// 🚧 This trait is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not implement this trait in userland unless you are prepared for things to occasionally break.
/// </div>
pub trait DateDataMarkers: UnstableSealed {
    /// Cross-calendar data markers for date skeleta.
    type Skel: CalMarkers<ErasedPackedPatterns>;
    /// Cross-calendar data markers for year names.
    type Year: CalMarkers<YearNamesV1>;
    /// Cross-calendar data markers for month names.
    type Month: CalMarkers<MonthNamesV1>;
    /// Marker for loading weekday names.
    type WeekdayNamesV1: DataMarker<DataStruct = LinearNames<'static>>;
}

/// A trait associating types for time formatting
/// (input types and data markers).
///
/// This is a sealed trait implemented on field set markers.
///
/// <div class="stab unstable">
/// 🚧 This trait is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not implement this trait in userland unless you are prepared for things to occasionally break.
/// </div>
pub trait TimeMarkers: UnstableSealed {
    /// Marker for resolving the day-of-month input field.
    type HourInput: IntoOption<Hour>;
    /// Marker for resolving the day-of-week input field.
    type MinuteInput: IntoOption<Minute>;
    /// Marker for resolving the day-of-year input field.
    type SecondInput: IntoOption<Second>;
    /// Marker for resolving the any-calendar-kind input field.
    type NanosecondInput: IntoOption<Nanosecond>;
    /// Marker for loading time skeleton patterns.
    type TimeSkeletonPatternsV1: DataMarker<DataStruct = PackedPatterns<'static>>;
    /// Marker for loading day period names.
    type DayPeriodNamesV1: DataMarker<DataStruct = LinearNames<'static>>;
}

/// A trait associating types for time zone formatting
/// (input types and data markers).
///
/// This is a sealed trait implemented on field set markers.
///
/// <div class="stab unstable">
/// 🚧 This trait is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not implement this trait in userland unless you are prepared for things to occasionally break.
/// </div>
pub trait ZoneMarkers: UnstableSealed {
    /// Marker for resolving the time zone id input field.
    type TimeZoneIdInput: IntoOption<TimeZone>;
    /// Marker for resolving the time zone offset input field.
    type TimeZoneOffsetInput: IntoOption<UtcOffset>;
    /// Marker for resolving the time zone variant input field.
    type TimeZoneVariantInput: IntoOption<TimeZoneVariant>;
    /// Marker for resolving the time zone non-location display names, which depend on the datetime.
    type TimeZoneLocalTimeInput: IntoOption<(Date<Iso>, Time)>;
    /// Marker for loading core time zone data.
    type EssentialsV1: DataMarker<DataStruct = tz::Essentials<'static>>;
    /// Marker for loading location names for time zone formatting
    type LocationsV1: DataMarker<DataStruct = tz::Locations<'static>>;
    /// Marker for loading location names for time zone formatting
    type LocationsRootV1: DataMarker<DataStruct = tz::Locations<'static>>;
    /// Marker for loading exemplar city names for time zone formatting
    type ExemplarCitiesV1: DataMarker<DataStruct = tz::ExemplarCities<'static>>;
    /// Marker for loading exemplar city names for time zone formatting
    type ExemplarCitiesRootV1: DataMarker<DataStruct = tz::ExemplarCities<'static>>;
    /// Marker for loading generic long time zone names.
    type GenericLongV1: DataMarker<DataStruct = tz::MzGeneric<'static>>;
    /// Marker for loading generic short time zone names.
    type GenericShortV1: DataMarker<DataStruct = tz::MzGeneric<'static>>;
    /// Marker for loading standard long time zone names.
    type StandardLongV1: DataMarker<DataStruct = tz::MzGeneric<'static>>;
    /// Marker for loading specific long time zone names.
    type SpecificLongV1: DataMarker<DataStruct = tz::MzSpecific<'static>>;
    /// Marker for loading generic short time zone names.
    type SpecificShortV1: DataMarker<DataStruct = tz::MzSpecific<'static>>;
    /// Marker for loading metazone periods.
    type MetazonePeriodV1: DataMarker<DataStruct = tz::MzPeriod<'static>>;
}

/// A trait associating constants and types implementing various other traits
/// required for datetime formatting.
///
/// This is a sealed trait implemented on field set markers.
///
/// <div class="stab unstable">
/// 🚧 This trait is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not implement this trait in userland unless you are prepared for things to occasionally break.
/// </div>
pub trait DateTimeMarkers: UnstableSealed + DateTimeNamesMarker {
    /// Associated types for date formatting.
    ///
    /// Should implement [`DateDataMarkers`], [`TypedDateDataMarkers`], and [`DateInputMarkers`].
    type D;
    /// Associated types for time formatting.
    ///
    /// Should implement [`TimeMarkers`].
    type T;
    /// Associated types for time zone formatting.
    ///
    /// Should implement [`ZoneMarkers`].
    type Z;
    /// Marker for loading the date/time glue pattern.
    type GluePatternV1: DataMarker<DataStruct = GluePattern<'static>>;
}

/// Trait implemented on formattable datetime types.
///
/// This trait allows for only those types compatible with a particular field set to be passed
/// as arguments to the formatting function for that field set. For example, this trait prevents
/// [`Time`] from being passed to a formatter parameterized with [`fieldsets::YMD`].
///
/// The following types implement this trait:
///
/// - [`Date`](icu_calendar::Date)
/// - [`Time`](icu_time::Time)
/// - [`DateTime`](icu_time::DateTime)
/// - [`ZonedDateTime`](icu_time::ZonedDateTime)
/// - [`UtcOffset`](icu_time::zone::UtcOffset)
/// - [`TimeZoneInfo`](icu_time::TimeZoneInfo)
///
/// [`fieldsets::YMD`]: crate::fieldsets::YMD
// This trait is implicitly sealed due to sealed supertraits
pub trait AllInputMarkers<R: DateTimeMarkers>:
    GetField<<R::D as DateInputMarkers>::YearInput>
    + GetField<<R::D as DateInputMarkers>::MonthInput>
    + GetField<<R::D as DateInputMarkers>::DayOfMonthInput>
    + GetField<<R::D as DateInputMarkers>::DayOfWeekInput>
    + GetField<<R::D as DateInputMarkers>::DayOfYearInput>
    + GetField<<R::T as TimeMarkers>::HourInput>
    + GetField<<R::T as TimeMarkers>::MinuteInput>
    + GetField<<R::T as TimeMarkers>::SecondInput>
    + GetField<<R::T as TimeMarkers>::NanosecondInput>
    + GetField<<R::Z as ZoneMarkers>::TimeZoneIdInput>
    + GetField<<R::Z as ZoneMarkers>::TimeZoneOffsetInput>
    + GetField<<R::Z as ZoneMarkers>::TimeZoneVariantInput>
    + GetField<<R::Z as ZoneMarkers>::TimeZoneLocalTimeInput>
where
    R::D: DateInputMarkers,
    R::T: TimeMarkers,
    R::Z: ZoneMarkers,
{
}

impl<T, R> AllInputMarkers<R> for T
where
    R: DateTimeMarkers,
    R::D: DateInputMarkers,
    R::T: TimeMarkers,
    R::Z: ZoneMarkers,
    T: GetField<<R::D as DateInputMarkers>::YearInput>
        + GetField<<R::D as DateInputMarkers>::MonthInput>
        + GetField<<R::D as DateInputMarkers>::DayOfMonthInput>
        + GetField<<R::D as DateInputMarkers>::DayOfWeekInput>
        + GetField<<R::D as DateInputMarkers>::DayOfYearInput>
        + GetField<<R::T as TimeMarkers>::HourInput>
        + GetField<<R::T as TimeMarkers>::MinuteInput>
        + GetField<<R::T as TimeMarkers>::SecondInput>
        + GetField<<R::T as TimeMarkers>::NanosecondInput>
        + GetField<<R::Z as ZoneMarkers>::TimeZoneIdInput>
        + GetField<<R::Z as ZoneMarkers>::TimeZoneOffsetInput>
        + GetField<<R::Z as ZoneMarkers>::TimeZoneVariantInput>
        + GetField<<R::Z as ZoneMarkers>::TimeZoneLocalTimeInput>,
{
}

/// Trait to consolidate data provider markers defined by this crate
/// for datetime skeleton patterns with a fixed calendar.
///
/// This trait is implemented on all providers that support datetime skeleton patterns,
/// including [`crate::provider::Baked`].
// This trait is implicitly sealed due to sealed supertraits
#[rustfmt::skip]
pub trait AllFixedCalendarPatternDataMarkers<C: CldrCalendar, FSet: DateTimeMarkers>:
DataProvider<<FSet::D as TypedDateDataMarkers<C>>::DateSkeletonPatternsV1>
    + DataProvider<<FSet::T as TimeMarkers>::TimeSkeletonPatternsV1>
    + DataProvider<FSet::GluePatternV1>
where
    FSet::D: TypedDateDataMarkers<C>,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
{
}

#[rustfmt::skip]
impl<T, C, FSet> AllFixedCalendarPatternDataMarkers<C, FSet> for T
where
    C: CldrCalendar,
    FSet: DateTimeMarkers,
    FSet::D: TypedDateDataMarkers<C>,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
    T: ?Sized
        + DataProvider<<FSet::D as TypedDateDataMarkers<C>>::DateSkeletonPatternsV1>
        + DataProvider<<FSet::T as TimeMarkers>::TimeSkeletonPatternsV1>
        + DataProvider<FSet::GluePatternV1>,
{
}

/// Trait to consolidate data provider markers defined by this crate
/// for datetime formatting with a fixed calendar.
///
/// This trait is implemented on all providers that support datetime formatting,
/// including [`crate::provider::Baked`].
// This trait is implicitly sealed due to sealed supertraits
#[rustfmt::skip]
pub trait AllFixedCalendarFormattingDataMarkers<C: CldrCalendar, FSet: DateTimeMarkers>:
    DataProvider<<FSet::D as TypedDateDataMarkers<C>>::YearNamesV1>
    + DataProvider<<FSet::D as TypedDateDataMarkers<C>>::MonthNamesV1>
    + DataProvider<<FSet::D as TypedDateDataMarkers<C>>::WeekdayNamesV1>
    + DataProvider<<FSet::T as TimeMarkers>::DayPeriodNamesV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::EssentialsV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::LocationsV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::LocationsRootV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::ExemplarCitiesV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::ExemplarCitiesRootV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::GenericLongV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::GenericShortV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::StandardLongV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::SpecificLongV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::SpecificShortV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::MetazonePeriodV1>
    + AllFixedCalendarPatternDataMarkers<C, FSet>
where
    FSet::D: TypedDateDataMarkers<C>,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
{
}

#[rustfmt::skip]
impl<T, C, FSet> AllFixedCalendarFormattingDataMarkers<C, FSet> for T
where
    C: CldrCalendar,
    FSet: DateTimeMarkers,
    FSet::D: TypedDateDataMarkers<C>,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
    T: ?Sized
        + DataProvider<<FSet::D as TypedDateDataMarkers<C>>::YearNamesV1>
        + DataProvider<<FSet::D as TypedDateDataMarkers<C>>::MonthNamesV1>
        + DataProvider<<FSet::D as TypedDateDataMarkers<C>>::DateSkeletonPatternsV1>
        + DataProvider<<FSet::D as TypedDateDataMarkers<C>>::WeekdayNamesV1>
        + DataProvider<<FSet::T as TimeMarkers>::DayPeriodNamesV1>
        + DataProvider<<FSet::T as TimeMarkers>::TimeSkeletonPatternsV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::EssentialsV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::LocationsV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::LocationsRootV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::ExemplarCitiesV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::ExemplarCitiesRootV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::GenericLongV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::GenericShortV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::StandardLongV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::SpecificLongV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::SpecificShortV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::MetazonePeriodV1>
        + DataProvider<FSet::GluePatternV1>
        + AllFixedCalendarPatternDataMarkers<C, FSet>
{
}

/// Trait to consolidate data provider markers defined by this crate
/// for datetime skeleton patterns with any calendar.
///
/// This trait is implemented on all providers that support datetime skeleton patterns,
/// including [`crate::provider::Baked`].
// This trait is implicitly sealed due to sealed supertraits
#[rustfmt::skip]
pub trait AllAnyCalendarPatternDataMarkers<FSet: DateTimeMarkers>:
    DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Buddhist>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Chinese>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Coptic>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Dangi>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Ethiopian>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::EthiopianAmeteAlem>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Gregorian>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Hebrew>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Indian>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::IslamicCivil>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::IslamicObservational>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::IslamicTabular>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::IslamicUmmAlQura>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Japanese>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::JapaneseExtended>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Persian>
    + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Roc>
    + DataProvider<<FSet::T as TimeMarkers>::TimeSkeletonPatternsV1>
    + DataProvider<FSet::GluePatternV1>
where
    FSet::D: DateDataMarkers,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
{
}

#[rustfmt::skip]
impl<T, FSet> AllAnyCalendarPatternDataMarkers<FSet> for T
where
    FSet: DateTimeMarkers,
    FSet::D: DateDataMarkers,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
    T: ?Sized
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Buddhist>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Chinese>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Coptic>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Dangi>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Ethiopian>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::EthiopianAmeteAlem>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Gregorian>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Hebrew>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Indian>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::IslamicCivil>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::IslamicObservational>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::IslamicTabular>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::IslamicUmmAlQura>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Japanese>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::JapaneseExtended>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Persian>
        + DataProvider<<<FSet::D as DateDataMarkers>::Skel as CalMarkers<ErasedPackedPatterns>>::Roc>
        + DataProvider<<FSet::T as TimeMarkers>::TimeSkeletonPatternsV1>
        + DataProvider<FSet::GluePatternV1>
{
}

/// Trait to consolidate data provider markers defined by this crate
/// for datetime formatting with any calendar.
///
/// This trait is implemented on all providers that support datetime formatting,
/// including [`crate::provider::Baked`].
// This trait is implicitly sealed due to sealed supertraits
#[rustfmt::skip]
pub trait AllAnyCalendarFormattingDataMarkers<FSet: DateTimeMarkers>:
    DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Buddhist>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Chinese>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Coptic>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Dangi>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Ethiopian>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::EthiopianAmeteAlem>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Gregorian>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Hebrew>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Indian>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::IslamicCivil>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::IslamicObservational>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::IslamicTabular>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::IslamicUmmAlQura>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Japanese>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::JapaneseExtended>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Persian>
    + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Roc>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Buddhist>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Chinese>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Coptic>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Dangi>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Ethiopian>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::EthiopianAmeteAlem>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Gregorian>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Hebrew>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Indian>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::IslamicCivil>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::IslamicObservational>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::IslamicTabular>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::IslamicUmmAlQura>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Japanese>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::JapaneseExtended>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Persian>
    + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Roc>
    + DataProvider<<FSet::D as DateDataMarkers>::WeekdayNamesV1>
    + DataProvider<<FSet::T as TimeMarkers>::DayPeriodNamesV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::EssentialsV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::LocationsV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::LocationsRootV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::ExemplarCitiesV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::ExemplarCitiesRootV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::GenericLongV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::GenericShortV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::StandardLongV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::SpecificLongV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::SpecificShortV1>
    + DataProvider<<FSet::Z as ZoneMarkers>::MetazonePeriodV1>
    + AllAnyCalendarPatternDataMarkers<FSet>
where
    FSet::D: DateDataMarkers,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
{
}

#[rustfmt::skip]
impl<T, FSet> AllAnyCalendarFormattingDataMarkers<FSet> for T
where
    FSet: DateTimeMarkers,
    FSet::D: DateDataMarkers,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
    T: ?Sized
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Buddhist>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Chinese>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Coptic>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Dangi>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Ethiopian>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::EthiopianAmeteAlem>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Gregorian>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Hebrew>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Indian>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::IslamicCivil>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::IslamicObservational>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::IslamicTabular>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::IslamicUmmAlQura>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Japanese>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::JapaneseExtended>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Persian>
        + DataProvider<<<FSet::D as DateDataMarkers>::Year as CalMarkers<YearNamesV1>>::Roc>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Buddhist>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Chinese>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Coptic>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Dangi>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Ethiopian>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::EthiopianAmeteAlem>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Gregorian>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Hebrew>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Indian>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::IslamicCivil>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::IslamicObservational>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::IslamicTabular>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::IslamicUmmAlQura>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Japanese>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::JapaneseExtended>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Persian>
        + DataProvider<<<FSet::D as DateDataMarkers>::Month as CalMarkers<MonthNamesV1>>::Roc>
        + DataProvider<<FSet::D as DateDataMarkers>::WeekdayNamesV1>
        + DataProvider<<FSet::T as TimeMarkers>::DayPeriodNamesV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::EssentialsV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::LocationsV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::LocationsRootV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::ExemplarCitiesV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::ExemplarCitiesRootV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::GenericLongV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::GenericShortV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::StandardLongV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::SpecificLongV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::SpecificShortV1>
        + DataProvider<<FSet::Z as ZoneMarkers>::MetazonePeriodV1>
        + AllAnyCalendarPatternDataMarkers<FSet>
{
}

/// Trait to consolidate data provider markers external to this crate
/// for datetime formatting with a fixed calendar.
// This trait is implicitly sealed due to sealed supertraits
pub trait AllFixedCalendarExternalDataMarkers:
    DataProvider<DecimalSymbolsV2> + DataProvider<DecimalDigitsV1>
{
}

impl<T> AllFixedCalendarExternalDataMarkers for T where
    T: ?Sized + DataProvider<DecimalSymbolsV2> + DataProvider<DecimalDigitsV1>
{
}

/// Trait to consolidate data provider markers external to this crate
/// for datetime formatting with any calendar.
// This trait is implicitly sealed due to sealed supertraits
pub trait AllAnyCalendarExternalDataMarkers:
    DataProvider<CalendarChineseV1>
    + DataProvider<CalendarDangiV1>
    + DataProvider<CalendarIslamicObservationalV1>
    + DataProvider<CalendarIslamicUmmalquraV1>
    + DataProvider<CalendarJapaneseModernV1>
    + DataProvider<CalendarJapaneseExtendedV1>
    + DataProvider<DecimalSymbolsV2>
    + DataProvider<DecimalDigitsV1>
{
}

impl<T> AllAnyCalendarExternalDataMarkers for T where
    T: ?Sized
        + DataProvider<CalendarChineseV1>
        + DataProvider<CalendarDangiV1>
        + DataProvider<CalendarIslamicObservationalV1>
        + DataProvider<CalendarIslamicUmmalquraV1>
        + DataProvider<CalendarJapaneseModernV1>
        + DataProvider<CalendarJapaneseExtendedV1>
        + DataProvider<DecimalSymbolsV2>
        + DataProvider<DecimalDigitsV1>
{
}

impl DateInputMarkers for () {
    type YearInput = ();
    type MonthInput = ();
    type DayOfMonthInput = ();
    type DayOfYearInput = ();
    type DayOfWeekInput = ();
}

impl<C> TypedDateDataMarkers<C> for () {
    type DateSkeletonPatternsV1 = NeverMarker<PackedPatterns<'static>>;
    type YearNamesV1 = NeverMarker<YearNames<'static>>;
    type MonthNamesV1 = NeverMarker<MonthNames<'static>>;
    type WeekdayNamesV1 = NeverMarker<LinearNames<'static>>;
}

impl DateDataMarkers for () {
    type Skel = NoDataCalMarkers;
    type Year = NoDataCalMarkers;
    type Month = NoDataCalMarkers;
    type WeekdayNamesV1 = NeverMarker<LinearNames<'static>>;
}

impl TimeMarkers for () {
    type HourInput = ();
    type MinuteInput = ();
    type SecondInput = ();
    type NanosecondInput = ();
    type TimeSkeletonPatternsV1 = NeverMarker<PackedPatterns<'static>>;
    type DayPeriodNamesV1 = NeverMarker<LinearNames<'static>>;
}

impl ZoneMarkers for () {
    type TimeZoneIdInput = ();
    type TimeZoneOffsetInput = ();
    type TimeZoneVariantInput = ();
    type TimeZoneLocalTimeInput = ();
    type EssentialsV1 = NeverMarker<tz::Essentials<'static>>;
    type LocationsV1 = NeverMarker<tz::Locations<'static>>;
    type LocationsRootV1 = NeverMarker<tz::Locations<'static>>;
    type ExemplarCitiesV1 = NeverMarker<tz::ExemplarCities<'static>>;
    type ExemplarCitiesRootV1 = NeverMarker<tz::ExemplarCities<'static>>;
    type GenericLongV1 = NeverMarker<tz::MzGeneric<'static>>;
    type GenericShortV1 = NeverMarker<tz::MzGeneric<'static>>;
    type StandardLongV1 = NeverMarker<tz::MzGeneric<'static>>;
    type SpecificLongV1 = NeverMarker<tz::MzSpecific<'static>>;
    type SpecificShortV1 = NeverMarker<tz::MzSpecific<'static>>;
    type MetazonePeriodV1 = NeverMarker<tz::MzPeriod<'static>>;
}

macro_rules! datetime_marker_helper {
    (@years/typed, yes) => {
        C::YearNamesV1
    };
    (@years/typed,) => {
        NeverMarker<YearNames<'static>>
    };
    (@months/typed, yes) => {
        C::MonthNamesV1
    };
    (@months/typed,) => {
        NeverMarker<MonthNames<'static>>
    };
    (@dates/typed, yes) => {
        C::SkeletaV1
    };
    (@dates/typed,) => {
        NeverMarker<PackedPatterns<'static>>
    };
    (@calmarkers, yes) => {
        FullDataCalMarkers
    };
    (@calmarkers,) => {
        NoDataCalMarkers
    };
    (@weekdays, yes) => {
        WeekdayNamesV1
    };
    (@weekdays,) => {
        NeverMarker<LinearNames<'static>>
    };
    (@dayperiods, yes) => {
        DayPeriodNamesV1
    };
    (@dayperiods,) => {
        NeverMarker<LinearNames<'static>>
    };
    (@times, yes) => {
        TimeNeoSkeletonPatternsV1
    };
    (@times,) => {
        NeverMarker<ErasedPackedPatterns>
    };
    (@glue, yes) => {
        GluePatternV1
    };
    (@glue,) => {
        NeverMarker<GluePattern<'static>>
    };
    (@option/length, yes) => {
        Length
    };
    (@option/length, long) => {
        Length
    };
    (@option/length, medium) => {
        Length
    };
    (@option/length, short) => {
        Length
    };
    (@option/yearstyle, yes) => {
        Option<YearStyle>
    };
    (@option/alignment, yes) => {
        Option<Alignment>
    };
    (@option/timeprecision, yes) => {
        Option<TimePrecision>
    };
    (@option/$any:ident,) => {
        ()
    };
    (@input/year, yes) => {
        YearInfo
    };
    (@input/month, yes) => {
        MonthInfo
    };
    (@input/day_of_month, yes) => {
        DayOfMonth
    };
    (@input/day_of_week, yes) => {
        Weekday
    };
    (@input/day_of_year, yes) => {
        DayOfYearInfo
    };
    (@input/hour, yes) => {
        Hour
    };
    (@input/minute, yes) => {
        Minute
    };
    (@input/second, yes) => {
        Second
    };
    (@input/Nanosecond, yes) => {
        Nanosecond
    };
    (@input/timezone/id, yes) => {
        TimeZone
    };
    (@input/timezone/offset, yes) => {
        Option<UtcOffset>
    };
    (@input/timezone/variant, yes) => {
        TimeZoneVariant
    };
    (@input/timezone/local_time, yes) => {
        (Date<Iso>, Time)
    };
    (@input/timezone/$any:ident,) => {
        ()
    };
    (@input/$any:ident,) => {
        ()
    };
    (@data/zone/essentials, yes) => {
        tz::EssentialsV1
    };
    (@data/zone/locations, yes) => {
        tz::LocationsV1
    };
    (@data/zone/locations_root, yes) => {
        tz::LocationsRootV1
    };
    (@data/zone/exemplars, yes) => {
        tz::ExemplarCitiesV1
    };
    (@data/zone/exemplars_root, yes) => {
        tz::ExemplarCitiesRootV1
    };
    (@data/zone/generic_long, yes) => {
        tz::MzGenericLongV1
    };
    (@data/zone/generic_short, yes) => {
        tz::MzGenericShortV1
    };
    (@data/zone/standard_long, yes) => {
        tz::MzStandardLongV1
    };
    (@data/zone/specific_long, yes) => {
        tz::MzSpecificLongV1
    };
    (@data/zone/specific_short, yes) => {
        tz::MzSpecificShortV1
    };
    (@data/zone/metazone_periods, yes) => {
        tz::MzPeriodV1
    };
    (@data/zone/essentials,) => {
        NeverMarker<tz::Essentials<'static>>
    };
    (@data/zone/locations,) => {
        NeverMarker<tz::Locations<'static>>
    };
    (@data/zone/locations_root,) => {
        NeverMarker<tz::Locations<'static>>
    };
    (@data/zone/exemplars,) => {
        NeverMarker<tz::ExemplarCities<'static>>
    };
    (@data/zone/exemplars_root,) => {
        NeverMarker<tz::ExemplarCities<'static>>
    };
    (@data/zone/generic_long,) => {
        NeverMarker<tz::MzGeneric<'static>>
    };
    (@data/zone/generic_short,) => {
        NeverMarker<tz::MzGeneric<'static>>
    };
    (@data/zone/standard_long,) => {
        NeverMarker<tz::MzGeneric<'static>>
    };
    (@data/zone/specific_long,) => {
        NeverMarker<tz::MzSpecific<'static>>
    };
    (@data/zone/specific_short,) => {
        NeverMarker<tz::MzSpecific<'static>>
    };
    (@data/zone/metazone_periods,) => {
        NeverMarker<tz::MzPeriod<'static>>
    };
    (@names/year, yes) => {
        YearNamesV1
    };
    (@names/month, yes) => {
        MonthNamesV1
    };
    (@names/weekday, yes) => {
        WeekdayNamesV1
    };
    (@names/dayperiod, yes) => {
        DayPeriodNamesV1
    };
    (@names/zone/essentials, yes) => {
        tz::EssentialsV1
    };
    (@names/zone/locations, yes) => {
        tz::LocationsV1
    };
    (@names/zone/locations_root, yes) => {
        tz::LocationsRootV1
    };
    (@names/zone/exemplars, yes) => {
        tz::ExemplarCitiesV1
    };
    (@names/zone/exemplars_root, yes) => {
        tz::ExemplarCitiesRootV1
    };
    (@names/zone/generic_long, yes) => {
        tz::MzGenericLongV1
    };
    (@names/zone/generic_short, yes) => {
        tz::MzGenericShortV1
    };
    (@names/zone/standard_long, yes) => {
        tz::MzStandardLongV1
    };
    (@names/zone/specific_long, yes) => {
        tz::MzSpecificLongV1
    };
    (@names/zone/specific_short, yes) => {
        tz::MzSpecificShortV1
    };
    (@names/zone/metazone_periods, yes) => {
        tz::MzPeriodV1
    };
    (@names/$any:ident,) => {
        ()
    };
    (@names/zone/$any:ident,) => {
        ()
    };
    () => {
        unreachable!() // prevent bugs
    };
}
pub(crate) use datetime_marker_helper;
