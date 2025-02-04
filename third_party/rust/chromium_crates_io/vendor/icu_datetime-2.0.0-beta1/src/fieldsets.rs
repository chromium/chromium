// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! All available field sets for datetime formatting.
//!
//! Each field set is a struct containing options specified to that field set.
//! The fields can either be set directly or via helper functions.
//!
//! This module contains _static_ field sets, which deliver the smallest binary size.
//! If the field set is not known until runtime, use a _dynamic_ field set: [`enums`]
//!
//! # What is a Field Set?
//!
//! A field set determines what datetime fields should be printed in the localized output.
//!
//! Examples of field sets include:
//!
//! 1. Year, month, and day ([`YMD`])
//! 2. Weekday and time ([`ET`])
//!
//! Field sets fit into four categories:
//!
//! 1. Date: fields that specify a particular day in time.
//! 2. Calendar period: fields that specify a span of time greater than a day.
//! 3. Time: fields that specify a time within a day.
//! 4. Zone: fields that specify a time zone or offset from UTC.
//!
//! Certain combinations of field sets are allowed, too. See [`Combo`].
//!
//! # Examples
//!
//! Two ways to configure the same field set:
//!
//! ```
//! use icu::datetime::fieldsets::YMDT;
//! use icu::datetime::options::{Alignment, TimePrecision, YearStyle};
//!
//! let field_set_1 = YMDT::long()
//!     .with_year_style(YearStyle::Full)
//!     .with_alignment(Alignment::Column)
//!     .with_time_precision(TimePrecision::MinuteExact);
//!
//! let mut field_set_2 = YMDT::long();
//! field_set_2.year_style = Some(YearStyle::Full);
//! field_set_2.alignment = Some(Alignment::Column);
//! field_set_2.time_precision = Some(TimePrecision::MinuteExact);
//!
//! assert_eq!(field_set_1, field_set_2);
//! ```

#[path = "dynamic.rs"]
pub mod enums;

pub use crate::combo::Combo;

use crate::{
    fields,
    options::*,
    provider::{neo::*, time_zones::tz, *},
    raw::neo::RawOptions,
    scaffold::*,
};
use enums::*;
use icu_calendar::{
    types::{
        DayOfMonth, IsoHour, IsoMinute, IsoSecond, IsoWeekday, MonthInfo, NanoSecond, YearInfo,
    },
    Date, Iso, Time,
};
use icu_provider::marker::NeverMarker;
use icu_timezone::{TimeZoneBcp47Id, UtcOffset, ZoneVariant};

/// 🚧 \[Experimental\] Types for dealing with serialization of semantic skeletons.
///
/// <div class="stab unstable">
/// 🚧 This code is experimental; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Use with caution.
/// <a href="https://github.com/unicode-org/icu4x/issues/5825">#5825</a>
/// </div>
#[cfg(all(feature = "experimental", feature = "serde"))]
pub mod serde {
    pub use crate::neo_serde::CompositeFieldSetSerde;
    pub use crate::neo_serde::CompositeFieldSetSerdeError;
}

#[cfg(doc)]
use icu_timezone::TimeZoneInfo;

/// Maps the token `yes` to the given ident
macro_rules! yes_to {
    ($any:expr, yes) => {
        $any
    };
    () => {
        unreachable!() // prevent bugs
    };
}

macro_rules! yes_or {
    ($fallback:expr, $actual:expr) => {
        $actual
    };
    ($fallback:expr,) => {
        $fallback
    };
}

macro_rules! ternary {
    ($present:expr, $missing:expr, yes) => {
        $present
    };
    ($present:expr, $missing:expr, $any:literal) => {
        $present
    };
    ($present:expr, $missing:expr,) => {
        $missing
    };
}

/// Generates the options argument passed into the docs test constructor
macro_rules! length_option_helper {
    ($type:ty, $length:ident) => {
        concat!(stringify!($type), "::", stringify!($length), "()")
    };
}

macro_rules! impl_composite {
    ($type:ident, $variant:ident, $enum:ident) => {
        impl $type {
            #[inline]
            pub(crate) fn to_enum(self) -> $enum {
                $enum::$type(self)
            }
        }
        impl GetField<CompositeFieldSet> for $type {
            #[inline]
            fn get_field(&self) -> CompositeFieldSet {
                CompositeFieldSet::$variant(self.to_enum())
            }
        }
    };
}

macro_rules! impl_marker_with_options {
    (
        $(#[$attr:meta])*
        $type:ident,
        $(sample_length: $sample_length:ident,)?
        $(alignment: $alignment_yes:ident,)?
        $(year_style: $yearstyle_yes:ident,)?
        $(time_precision: $timeprecision_yes:ident,)?
        $(enumerated: $enumerated_yes:ident,)?
    ) => {
        $(#[$attr])*
        #[derive(Debug, Copy, Clone, PartialEq, Eq)]
        #[non_exhaustive]
        pub struct $type {
            $(
                /// The desired length of the formatted string.
                ///
                /// See: [`Length`]
                pub length: datetime_marker_helper!(@option/length, $sample_length),
            )?
            $(
                /// Whether fields should be aligned for a column-like layout.
                ///
                /// See: [`Alignment`]
                pub alignment: datetime_marker_helper!(@option/alignment, $alignment_yes),
            )?
            $(
                /// When to display the era field in the formatted string.
                ///
                /// See: [`YearStyle`]
                pub year_style: datetime_marker_helper!(@option/yearstyle, $yearstyle_yes),
            )?
            $(
                /// How precisely to display the time of day
                ///
                /// See: [`TimePrecision`]
                pub time_precision: datetime_marker_helper!(@option/timeprecision, $timeprecision_yes),
            )?
        }
        impl $type {
            #[doc = concat!("Creates a ", stringify!($type), " skeleton with the given formatting length.")]
            pub const fn with_length(length: Length) -> Self {
                Self {
                    length,
                    $(
                        alignment: yes_to!(None, $alignment_yes),
                    )?
                    $(
                        year_style: yes_to!(None, $yearstyle_yes),
                    )?
                    $(
                        time_precision: yes_to!(None, $timeprecision_yes),
                    )?
                }
            }
            #[doc = concat!("Creates a ", stringify!($type), " skeleton with a long length.")]
            pub const fn long() -> Self {
                Self::with_length(Length::Long)
            }
            #[doc = concat!("Creates a ", stringify!($type), " skeleton with a medium length.")]
            pub const fn medium() -> Self {
                Self::with_length(Length::Medium)
            }
            #[doc = concat!("Creates a ", stringify!($type), " skeleton with a short length.")]
            pub const fn short() -> Self {
                Self::with_length(Length::Short)
            }
        }
        #[allow(dead_code)]
        impl $type {
            $(
                const _: () = yes_to!((), $enumerated_yes); // condition for this macro block
                #[warn(dead_code)]
            )?
            pub(crate) fn to_raw_options(self) -> RawOptions {
                RawOptions {
                    length: self.length,
                    alignment: ternary!(self.alignment, None, $($alignment_yes)?),
                    year_style: ternary!(self.year_style, None, $($yearstyle_yes)?),
                    time_precision: ternary!(self.time_precision, None, $($timeprecision_yes)?),
                }
            }
            $(
                const _: () = yes_to!((), $enumerated_yes); // condition for this macro block
                #[warn(dead_code)]
            )?
            pub(crate) fn from_raw_options(options: RawOptions) -> Self {
                Self {
                    length: options.length,
                    $(alignment: yes_to!(options.alignment, $alignment_yes),)?
                    $(year_style: yes_to!(options.year_style, $yearstyle_yes),)?
                    $(time_precision: yes_to!(options.time_precision, $timeprecision_yes),)?
                }
            }
        }
        $(
            impl $type {
                /// Sets the alignment option.
                pub const fn with_alignment(mut self, alignment: Alignment) -> Self {
                    self.alignment = Some(yes_to!(alignment, $alignment_yes));
                    self
                }
            }
        )?
        $(
            impl $type {
                /// Sets the year style option.
                pub const fn with_year_style(mut self, year_style: YearStyle) -> Self {
                    self.year_style = Some(yes_to!(year_style, $yearstyle_yes));
                    self
                }
            }
        )?
        $(
            impl $type {
                /// Sets the time precision option.
                pub const fn with_time_precision(mut self, time_precision: TimePrecision) -> Self {
                    self.time_precision = Some(yes_to!(time_precision, $timeprecision_yes));
                    self
                }
                /// Sets the time precision to [`TimePrecision::MinuteExact`]
                pub fn hm(mut self) -> Self {
                    self.time_precision = Some(TimePrecision::MinuteExact);
                    self
                }
            }
        )?
    };
}

macro_rules! impl_combo_get_field {
    ($type:ident, $composite:ident, $enum:ident, $wrap:ident, $in:ident, $out:ident) => {
        impl GetField<CompositeFieldSet> for Combo<$type, $in> {
            #[inline]
            fn get_field(&self) -> CompositeFieldSet {
                CompositeFieldSet::$composite(self.dt().to_enum(), ZoneStyle::$out)
            }
        }
    };
}

macro_rules! impl_combo_generic_fns {
    ($type:ident) => {
        impl<Z> Combo<$type, Z> {
            #[doc = concat!("Creates a ", stringify!($type), " skeleton with the given formatting length and a time zone.")]
            pub fn with_length(length: Length) -> Self {
                Self::new($type::with_length(length))
            }
            #[doc = concat!("Creates a ", stringify!($type), " skeleton with a long length and a time zone.")]
            pub const fn long() -> Self {
                Self::new($type::long())
            }
            #[doc = concat!("Creates a ", stringify!($type), " skeleton with a medium length and a time zone.")]
            pub const fn medium() -> Self {
                Self::new($type::medium())
            }
            #[doc = concat!("Creates a ", stringify!($type), " skeleton with a short length and a time zone.")]
            pub const fn short() -> Self {
                Self::new($type::short())
            }
        }
    };
}

macro_rules! impl_zone_combo_helpers {
    (
        $type:ident,
        $composite:ident,
        $enum:ident,
        $wrap:ident
    ) => {
        impl $type {
            /// Associates this field set with a specific non-location format time zone, as in
            /// “Pacific Daylight Time”.
            #[inline]
            pub fn zone_z(self) -> Combo<Self, Zs> {
                Combo::new(self)
            }
            /// Associates this field set with an offset format time zone, as in
            /// “GMT−8”.
            #[inline]
            pub fn zone_o(self) -> Combo<Self, O> {
                Combo::new(self)
            }
            /// Associates this field set with a generic non-location format time zone, as in
            /// “Pacific Time”.
            #[inline]
            pub fn zone_v(self) -> Combo<Self, Vs> {
                Combo::new(self)
            }
            /// Associates this field set with a location format time zone, as in
            /// “Los Angeles time”.
            #[inline]
            pub fn zone_l(self) -> Combo<Self, L> {
                Combo::new(self)
            }
        }
        impl_combo_get_field!($type, $composite, $enum, $wrap, Zs, Z);
        impl_combo_get_field!($type, $composite, $enum, $wrap, O, O);
        impl_combo_get_field!($type, $composite, $enum, $wrap, Vs, V);
        impl_combo_get_field!($type, $composite, $enum, $wrap, L, L);
    };
}

/// Internal helper macro used by [`impl_date_marker`] and [`impl_calendar_period_marker`]
macro_rules! impl_date_or_calendar_period_marker {
    (
        $(#[$attr:meta])*
        // The name of the type being created.
        $type:ident,
        // A plain language description of the field set for documentation.
        description = $description:literal,
        // Length of the sample string below.
        sample_length = $sample_length:ident,
        // A sample string. A docs test will be generated!
        sample = $sample:literal,
        // Whether years can occur.
        $(years = $years_yes:ident,)?
        // Whether months can occur.
        $(months = $months_yes:ident,)?
        // Whether weekdays can occur.
        $(weekdays = $weekdays_yes:ident,)?
        // Whether the input should contain years.
        $(input_year = $year_yes:ident,)?
        // Whether the input should contain months.
        $(input_month = $month_yes:ident,)?
        // Whether the input should contain the day of the month.
        $(input_day_of_month = $day_of_month_yes:ident,)?
        // Whether the input should contain the day of the week.
        $(input_day_of_week = $day_of_week_yes:ident,)?
        // Whether the input should contain the day of the year.
        $(input_day_of_year = $day_of_year_yes:ident,)?
        // Whether the input should declare its calendar kind.
        $(input_any_calendar_kind = $any_calendar_kind_yes:ident,)?
        // Whether the alignment option should be available.
        // According to UTS 35, it should be available with years, months, and days.
        $(option_alignment = $option_alignment_yes:ident,)?
    ) => {
        impl_marker_with_options!(
            #[doc = concat!("**“", $sample, "**” ⇒ ", $description)]
            ///
            /// This is a field set marker. For more information, see [`fieldsets`](crate::fieldsets).
            ///
            /// # Examples
            ///
            /// In [`DateTimeFormatter`](crate::neo::DateTimeFormatter):
            ///
            /// ```
            /// use icu::calendar::Date;
            /// use icu::datetime::DateTimeFormatter;
            #[doc = concat!("use icu::datetime::fieldsets::", stringify!($type), ";")]
            /// use icu::locale::locale;
            /// use writeable::assert_writeable_eq;
            #[doc = concat!("let fmt = DateTimeFormatter::<", stringify!($type), ">::try_new(")]
            ///     locale!("en").into(),
            #[doc = concat!("    ", length_option_helper!($type, $sample_length), ",")]
            /// )
            /// .unwrap();
            /// let dt = Date::try_new_iso(2024, 5, 17).unwrap();
            ///
            /// assert_writeable_eq!(
            ///     fmt.format_any_calendar(&dt),
            #[doc = concat!("    \"", $sample, "\"")]
            /// );
            /// ```
            ///
            /// In [`FixedCalendarDateTimeFormatter`](crate::neo::FixedCalendarDateTimeFormatter):
            ///
            /// ```
            /// use icu::calendar::Date;
            /// use icu::calendar::Gregorian;
            /// use icu::datetime::FixedCalendarDateTimeFormatter;
            #[doc = concat!("use icu::datetime::fieldsets::", stringify!($type), ";")]
            /// use icu::locale::locale;
            /// use writeable::assert_writeable_eq;
            ///
            #[doc = concat!("let fmt = FixedCalendarDateTimeFormatter::<Gregorian, ", stringify!($type), ">::try_new(")]
            ///     locale!("en").into(),
            #[doc = concat!("    ", length_option_helper!($type, $sample_length), ",")]
            /// )
            /// .unwrap();
            /// let dt = Date::try_new_gregorian(2024, 5, 17).unwrap();
            ///
            /// assert_writeable_eq!(
            ///     fmt.format(&dt),
            #[doc = concat!("    \"", $sample, "\"")]
            /// );
            /// ```
            $(#[$attr])*
            $type,
            sample_length: $sample_length,
            $(alignment: $option_alignment_yes,)?
            $(year_style: $year_yes,)?
        );
        impl UnstableSealed for $type {}
        impl DateTimeNamesMarker for $type {
            type YearNames = datetime_marker_helper!(@names/year, $($years_yes)?);
            type MonthNames = datetime_marker_helper!(@names/month, $($months_yes)?);
            type WeekdayNames = datetime_marker_helper!(@names/weekday, $($weekdays_yes)?);
            type DayPeriodNames = datetime_marker_helper!(@names/dayperiod,);
            type ZoneEssentials = datetime_marker_helper!(@names/zone/essentials,);
            type ZoneLocations = datetime_marker_helper!(@names/zone/locations,);
            type ZoneGenericLong = datetime_marker_helper!(@names/zone/generic_long,);
            type ZoneGenericShort = datetime_marker_helper!(@names/zone/generic_short,);
            type ZoneSpecificLong = datetime_marker_helper!(@names/zone/specific_long,);
            type ZoneSpecificShort = datetime_marker_helper!(@names/zone/specific_short,);
            type MetazoneLookup = datetime_marker_helper!(@names/zone/metazone_periods,);
        }
        impl DateInputMarkers for $type {
            type YearInput = datetime_marker_helper!(@input/year, $($year_yes)?);
            type MonthInput = datetime_marker_helper!(@input/month, $($month_yes)?);
            type DayOfMonthInput = datetime_marker_helper!(@input/day_of_month, $($day_of_month_yes)?);
            type DayOfYearInput = datetime_marker_helper!(@input/day_of_year, $($day_of_year_yes)?);
            type DayOfWeekInput = datetime_marker_helper!(@input/day_of_week, $($day_of_week_yes)?);
        }
        impl<C: CldrCalendar> TypedDateDataMarkers<C> for $type {
            type DateSkeletonPatternsV1Marker = datetime_marker_helper!(@dates/typed, yes);
            type YearNamesV1Marker = datetime_marker_helper!(@years/typed, $($years_yes)?);
            type MonthNamesV1Marker = datetime_marker_helper!(@months/typed, $($months_yes)?);
            type WeekdayNamesV1Marker = datetime_marker_helper!(@weekdays, $($weekdays_yes)?);
        }
        impl DateDataMarkers for $type {
            type Skel = datetime_marker_helper!(@calmarkers, yes);
            type Year = datetime_marker_helper!(@calmarkers, $($years_yes)?);
            type Month = datetime_marker_helper!(@calmarkers, $($months_yes)?);
            type WeekdayNamesV1Marker = datetime_marker_helper!(@weekdays, $($weekdays_yes)?);
        }
        impl DateTimeMarkers for $type {
            type D = Self;
            type T = ();
            type Z = ();
            type GluePatternV1Marker = datetime_marker_helper!(@glue,);
        }
    };
}

/// Implements a field set of date fields.
///
/// Several arguments to this macro are required, and the rest are optional.
/// The optional arguments should be written as `key = yes,` if that parameter
/// should be included.
///
/// See [`impl_date_marker`].
macro_rules! impl_date_marker {
    (
        $(#[$attr:meta])*
        $type:ident,
        $type_time:ident,
        description = $description:literal,
        sample_length = $sample_length:ident,
        sample = $sample:literal,
        sample_time = $sample_time:literal,
        $(years = $years_yes:ident,)?
        $(months = $months_yes:ident,)?
        $(dates = $dates_yes:ident,)?
        $(weekdays = $weekdays_yes:ident,)?
        $(input_year = $year_yes:ident,)?
        $(input_month = $month_yes:ident,)?
        $(input_day_of_month = $day_of_month_yes:ident,)?
        $(input_day_of_week = $day_of_week_yes:ident,)?
        $(input_day_of_year = $day_of_year_yes:ident,)?
        $(input_any_calendar_kind = $any_calendar_kind_yes:ident,)?
        $(option_alignment = $option_alignment_yes:ident,)?
    ) => {
        impl_date_or_calendar_period_marker!(
            $(#[$attr])*
            $type,
            description = $description,
            sample_length = $sample_length,
            sample = $sample,
            $(years = $years_yes,)?
            $(months = $months_yes,)?
            $(dates = $dates_yes,)?
            $(weekdays = $weekdays_yes,)?
            $(input_year = $year_yes,)?
            $(input_month = $month_yes,)?
            $(input_day_of_month = $day_of_month_yes,)?
            $(input_day_of_week = $day_of_week_yes,)?
            $(input_day_of_year = $day_of_year_yes,)?
            $(input_any_calendar_kind = $any_calendar_kind_yes,)?
            $(option_alignment = $option_alignment_yes,)?
        );
        impl_zone_combo_helpers!($type, DateZone, DateFieldSet, wrap);
        impl_combo_generic_fns!($type);
        impl_composite!($type, Date, DateFieldSet);
        impl_marker_with_options!(
            #[doc = concat!("**“", $sample_time, "**” ⇒ ", $description, " with time")]
            ///
            /// # Examples
            ///
            /// In [`DateTimeFormatter`](crate::neo::DateTimeFormatter):
            ///
            /// ```
            /// use icu::calendar::DateTime;
            /// use icu::datetime::DateTimeFormatter;
            #[doc = concat!("use icu::datetime::fieldsets::", stringify!($type_time), ";")]
            /// use icu::locale::locale;
            /// use writeable::assert_writeable_eq;
            ///
            #[doc = concat!("let fmt = DateTimeFormatter::try_new(")]
            ///     locale!("en").into(),
            #[doc = concat!("    ", length_option_helper!($type_time, $sample_length), ",")]
            /// )
            /// .unwrap();
            /// let dt = DateTime::try_new_iso(2024, 5, 17, 15, 47, 50).unwrap();
            ///
            /// assert_writeable_eq!(
            ///     fmt.format_any_calendar(&dt),
            #[doc = concat!("    \"", $sample_time, "\"")]
            /// );
            /// ```
            ///
            /// In [`FixedCalendarDateTimeFormatter`](crate::neo::FixedCalendarDateTimeFormatter):
            ///
            /// ```
            /// use icu::calendar::DateTime;
            /// use icu::calendar::Gregorian;
            /// use icu::datetime::FixedCalendarDateTimeFormatter;
            #[doc = concat!("use icu::datetime::fieldsets::", stringify!($type_time), ";")]
            /// use icu::locale::locale;
            /// use writeable::assert_writeable_eq;
            ///
            #[doc = concat!("let fmt = FixedCalendarDateTimeFormatter::try_new(")]
            ///     locale!("en").into(),
            #[doc = concat!("    ", length_option_helper!($type_time, $sample_length), ",")]
            /// )
            /// .unwrap();
            /// let dt = DateTime::try_new_gregorian(2024, 5, 17, 15, 47, 50).unwrap();
            ///
            /// assert_writeable_eq!(
            ///     fmt.format(&dt),
            #[doc = concat!("    \"", $sample_time, "\"")]
            /// );
            /// ```
            $(#[$attr])*
            $type_time,
            sample_length: $sample_length,
            alignment: yes,
            $(year_style: $year_yes,)?
            time_precision: yes,
        );
        impl_zone_combo_helpers!($type_time, DateTimeZone, DateAndTimeFieldSet, wrap);
        impl_combo_generic_fns!($type_time);
        impl UnstableSealed for $type_time {}
        impl DateTimeNamesMarker for $type_time {
            type YearNames = datetime_marker_helper!(@names/year, $($years_yes)?);
            type MonthNames = datetime_marker_helper!(@names/month, $($months_yes)?);
            type WeekdayNames = datetime_marker_helper!(@names/weekday, $($weekdays_yes)?);
            type DayPeriodNames = datetime_marker_helper!(@names/dayperiod, yes);
            type ZoneEssentials = datetime_marker_helper!(@names/zone/essentials,);
            type ZoneLocations = datetime_marker_helper!(@names/zone/locations,);
            type ZoneGenericLong = datetime_marker_helper!(@names/zone/generic_long,);
            type ZoneGenericShort = datetime_marker_helper!(@names/zone/generic_short,);
            type ZoneSpecificLong = datetime_marker_helper!(@names/zone/specific_long,);
            type ZoneSpecificShort = datetime_marker_helper!(@names/zone/specific_short,);
            type MetazoneLookup = datetime_marker_helper!(@names/zone/metazone_periods,);
        }
        impl DateInputMarkers for $type_time {
            type YearInput = datetime_marker_helper!(@input/year, $($year_yes)?);
            type MonthInput = datetime_marker_helper!(@input/month, $($month_yes)?);
            type DayOfMonthInput = datetime_marker_helper!(@input/day_of_month, $($day_of_month_yes)?);
            type DayOfYearInput = datetime_marker_helper!(@input/day_of_year, $($day_of_year_yes)?);
            type DayOfWeekInput = datetime_marker_helper!(@input/day_of_week, $($day_of_week_yes)?);
        }
        impl<C: CldrCalendar> TypedDateDataMarkers<C> for $type_time {
            type DateSkeletonPatternsV1Marker = datetime_marker_helper!(@dates/typed, yes);
            type YearNamesV1Marker = datetime_marker_helper!(@years/typed, $($years_yes)?);
            type MonthNamesV1Marker = datetime_marker_helper!(@months/typed, $($months_yes)?);
            type WeekdayNamesV1Marker = datetime_marker_helper!(@weekdays, $($weekdays_yes)?);
        }
        impl DateDataMarkers for $type_time {
            type Skel = datetime_marker_helper!(@calmarkers, yes);
            type Year = datetime_marker_helper!(@calmarkers, $($years_yes)?);
            type Month = datetime_marker_helper!(@calmarkers, $($months_yes)?);
            type WeekdayNamesV1Marker = datetime_marker_helper!(@weekdays, $($weekdays_yes)?);
        }
        impl TimeMarkers for $type_time {
            // TODO: Consider making dayperiods optional again
            type DayPeriodNamesV1Marker = datetime_marker_helper!(@dayperiods, yes);
            type TimeSkeletonPatternsV1Marker = datetime_marker_helper!(@times, yes);
            type HourInput = datetime_marker_helper!(@input/hour, yes);
            type MinuteInput = datetime_marker_helper!(@input/minute, yes);
            type SecondInput = datetime_marker_helper!(@input/second, yes);
            type NanoSecondInput = datetime_marker_helper!(@input/nanosecond, yes);
        }
        impl DateTimeMarkers for $type_time {
            type D = Self;
            type T = Self;
            type Z = ();
            type GluePatternV1Marker = datetime_marker_helper!(@glue, yes);
        }
        impl_composite!($type_time, DateTime, DateAndTimeFieldSet);
        impl $type_time {
            pub(crate) fn to_date_field_set(self) -> $type {
                $type {
                    length: self.length,
                    $(alignment: yes_to!(self.alignment, $option_alignment_yes),)?
                    $(year_style: yes_to!(self.year_style, $years_yes),)?
                }
            }
        }
    };
}

/// Implements a field set of calendar period fields.
///
/// Several arguments to this macro are required, and the rest are optional.
/// The optional arguments should be written as `key = yes,` if that parameter
/// should be included.
///
/// See [`impl_date_marker`].
macro_rules! impl_calendar_period_marker {
    (
        $(#[$attr:meta])*
        $type:ident,
        description = $description:literal,
        sample_length = $sample_length:ident,
        sample = $sample:literal,
        $(years = $years_yes:ident,)?
        $(months = $months_yes:ident,)?
        $(dates = $dates_yes:ident,)?
        $(input_year = $year_yes:ident,)?
        $(input_month = $month_yes:ident,)?
        $(input_any_calendar_kind = $any_calendar_kind_yes:ident,)?
        $(option_alignment = $option_alignment_yes:ident,)?
    ) => {
        impl_date_or_calendar_period_marker!(
            $(#[$attr])*
            $type,
            description = $description,
            sample_length = $sample_length,
            sample = $sample,
            $(years = $years_yes,)?
            $(months = $months_yes,)?
            $(dates = $dates_yes,)?
            $(input_year = $year_yes,)?
            $(input_month = $month_yes,)?
            $(input_any_calendar_kind = $any_calendar_kind_yes,)?
            $(option_alignment = $option_alignment_yes,)?
        );
        impl_composite!($type, CalendarPeriod, CalendarPeriodFieldSet);
    };
}

/// Implements a field set of time fields.
///
/// Several arguments to this macro are required, and the rest are optional.
/// The optional arguments should be written as `key = yes,` if that parameter
/// should be included.
///
/// Documentation for each option is shown inline below.
macro_rules! impl_time_marker {
    (
        $(#[$attr:meta])*
        // The name of the type being created.
        $type:ident,
        // A plain language description of the field set for documentation.
        description = $description:literal,
        // Length of the sample string below.
        sample_length = $sample_length:ident,
        // A sample string. A docs test will be generated!
        sample = $sample:literal,
        // Whether day periods can occur.
        $(dayperiods = $dayperiods_yes:ident,)?
        // Whether the input should include hours.
        $(input_hour = $hour_yes:ident,)?
        // Whether the input should contain minutes.
        $(input_minute = $minute_yes:ident,)?
        // Whether the input should contain seconds.
        $(input_second = $second_yes:ident,)?
        // Whether the input should contain fractional seconds.
        $(input_nanosecond = $nanosecond_yes:ident,)?
    ) => {
        impl_marker_with_options!(
            #[doc = concat!("**“", $sample, "**” ⇒ ", $description)]
            ///
            /// # Examples
            ///
            /// ```
            /// use icu::calendar::Time;
            /// use icu::datetime::TimeFormatter;
            #[doc = concat!("use icu::datetime::fieldsets::", stringify!($type), ";")]
            /// use icu::locale::locale;
            /// use writeable::assert_writeable_eq;
            ///
            #[doc = concat!("let fmt = TimeFormatter::try_new(")]
            ///     locale!("en").into(),
            #[doc = concat!("    ", length_option_helper!($type, $sample_length), ",")]
            /// )
            /// .unwrap();
            /// let time = Time::try_new(15, 47, 50, 0).unwrap();
            ///
            /// assert_writeable_eq!(
            ///     fmt.format(&time),
            #[doc = concat!("    \"", $sample, "\"")]
            /// );
            /// ```
            $(#[$attr])*
            $type,
            sample_length: $sample_length,
            alignment: yes,
            time_precision: yes,
        );
        impl_zone_combo_helpers!($type, TimeZone, TimeFieldSet, wrap);
        impl_combo_generic_fns!($type);
        impl UnstableSealed for $type {}
        impl DateTimeNamesMarker for $type {
            type YearNames = datetime_marker_helper!(@names/year,);
            type MonthNames = datetime_marker_helper!(@names/month,);
            type WeekdayNames = datetime_marker_helper!(@names/weekday,);
            type DayPeriodNames = datetime_marker_helper!(@names/dayperiod, $($dayperiods_yes)?);
            type ZoneEssentials = datetime_marker_helper!(@names/zone/essentials,);
            type ZoneLocations = datetime_marker_helper!(@names/zone/locations,);
            type ZoneGenericLong = datetime_marker_helper!(@names/zone/generic_long,);
            type ZoneGenericShort = datetime_marker_helper!(@names/zone/generic_short,);
            type ZoneSpecificLong = datetime_marker_helper!(@names/zone/specific_long,);
            type ZoneSpecificShort = datetime_marker_helper!(@names/zone/specific_short,);
            type MetazoneLookup = datetime_marker_helper!(@names/zone/metazone_periods,);
        }
        impl TimeMarkers for $type {
            type DayPeriodNamesV1Marker = datetime_marker_helper!(@dayperiods, $($dayperiods_yes)?);
            type TimeSkeletonPatternsV1Marker = datetime_marker_helper!(@times, yes);
            type HourInput = datetime_marker_helper!(@input/hour, $($hour_yes)?);
            type MinuteInput = datetime_marker_helper!(@input/minute, $($minute_yes)?);
            type SecondInput = datetime_marker_helper!(@input/second, $($second_yes)?);
            type NanoSecondInput = datetime_marker_helper!(@input/nanosecond, $($nanosecond_yes)?);
        }
        impl DateTimeMarkers for $type {
            type D = ();
            type T = Self;
            type Z = ();
            type GluePatternV1Marker = datetime_marker_helper!(@glue,);
        }
        impl_composite!($type, Time, TimeFieldSet);
    };
}

/// Implements a field set of time zone fields.
///
/// Several arguments to this macro are required, and the rest are optional.
/// The optional arguments should be written as `key = yes,` if that parameter
/// should be included.
///
/// Documentation for each option is shown inline below.
macro_rules! impl_zone_marker {
    (
        $(#[$attr:meta])*
        // The name of the type being created.
        $type:ident,
        // A plain language description of the field set for documentation.
        description = $description:literal,
        // Length of the sample string below.
        sample_length = $sample_length:ident,
        // A sample string. A docs test will be generated!
        sample = $sample:literal,
        // The field symbol and field length when the semantic length is short/medium.
        field_short = $field_short:expr,
        // The field symbol and field length when the semantic length is long.
        field_long = $field_long:expr,
        // The type in ZoneFieldSet for this field set
        resolved_type = $resolved_type:ident,
        // Whether to skip tests and render a message instead.
        $(skip_tests = $skip_tests:literal,)?
        // Whether zone-essentials should be loaded.
        $(zone_essentials = $zone_essentials_yes:ident,)?
        // Whether locations formats can occur.
        $(zone_locations = $zone_locations_yes:ident,)?
        // Whether generic long formats can occur.
        $(zone_generic_long = $zone_generic_long_yes:ident,)?
        // Whether generic short formats can occur.
        $(zone_generic_short = $zone_generic_short_yes:ident,)?
        // Whether specific long formats can occur.
        $(zone_specific_long = $zone_specific_long_yes:ident,)?
        // Whether specific short formats can occur.
        $(zone_specific_short = $zone_specific_short_yes:ident,)?
        // Whether metazone periods are needed
        $(metazone_periods = $metazone_periods_yes:ident,)?
        // Whether to require the TimeZoneBcp47Id
        $(input_tzid = $tzid_input_yes:ident,)?
        // Whether to require the ZoneVariant
        $(input_variant = $variant_input_yes:ident,)?
        // Whether to require the Local Time
        $(input_localtime = $localtime_input_yes:ident,)?
        // Whether this time zone style is enumerated in ZoneFieldSet
        $(enumerated = $enumerated_yes:ident,)?
    ) => {
        impl_marker_with_options!(
            #[doc = concat!("**“", $sample, "**” ⇒ ", $description)]
            ///
            #[doc = yes_or!("", $($skip_tests)?)]
            ///
            /// # Examples
            ///
            #[doc = concat!("```", ternary!("compile_fail", "", $($skip_tests)?))]
            /// use icu::calendar::{Date, Time};
            /// use icu::timezone::{TimeZoneBcp47Id, TimeZoneInfo, UtcOffset, ZoneVariant};
            /// use icu::datetime::TimeFormatter;
            #[doc = concat!("use icu::datetime::fieldsets::", stringify!($type), ";")]
            /// use icu::locale::locale;
            /// use tinystr::tinystr;
            /// use writeable::assert_writeable_eq;
            ///
            #[doc = concat!("let fmt = TimeFormatter::try_new(")]
            ///     locale!("en").into(),
            #[doc = concat!("    ", length_option_helper!($type, $sample_length), ",")]
            /// )
            /// .unwrap();
            ///
            /// // Time zone info for America/Chicago in the summer
            /// let zone = TimeZoneBcp47Id(tinystr!(8, "uschi"))
            ///     .with_offset("-05".parse().ok())
            ///     .at_time((Date::try_new_iso(2022, 8, 29).unwrap(), Time::midnight()))
            ///     .with_zone_variant(ZoneVariant::Daylight);
            ///
            /// assert_writeable_eq!(
            ///     fmt.format(&zone),
            #[doc = concat!("    \"", $sample, "\"")]
            /// );
            /// ```
            $(#[$attr])*
            $type,
            sample_length: $sample_length,
        );
        impl UnstableSealed for $type {}
        impl DateTimeNamesMarker for $type {
            type YearNames = datetime_marker_helper!(@names/year,);
            type MonthNames = datetime_marker_helper!(@names/month,);
            type WeekdayNames = datetime_marker_helper!(@names/weekday,);
            type DayPeriodNames = datetime_marker_helper!(@names/dayperiod,);
            type ZoneEssentials = datetime_marker_helper!(@names/zone/essentials, $($zone_essentials_yes)?);
            type ZoneLocations = datetime_marker_helper!(@names/zone/locations, $($zone_locations_yes)?);
            type ZoneGenericLong = datetime_marker_helper!(@names/zone/generic_long, $($zone_generic_long_yes)?);
            type ZoneGenericShort = datetime_marker_helper!(@names/zone/generic_short, $($zone_generic_short_yes)?);
            type ZoneSpecificLong = datetime_marker_helper!(@names/zone/specific_long, $($zone_specific_long_yes)?);
            type ZoneSpecificShort = datetime_marker_helper!(@names/zone/specific_short, $($zone_specific_short_yes)?);
            type MetazoneLookup = datetime_marker_helper!(@names/zone/metazone_periods, $($metazone_periods_yes)?);
        }
        impl ZoneMarkers for $type {
            type TimeZoneIdInput = datetime_marker_helper!(@input/timezone/id, $($tzid_input_yes)?);
            type TimeZoneOffsetInput = datetime_marker_helper!(@input/timezone/offset, yes);
            type TimeZoneVariantInput = datetime_marker_helper!(@input/timezone/variant, $($variant_input_yes)?);
            type TimeZoneLocalTimeInput = datetime_marker_helper!(@input/timezone/local_time, $($localtime_input_yes)?);
            type EssentialsV1Marker = datetime_marker_helper!(@data/zone/essentials, $($zone_essentials_yes)?);
            type LocationsV1Marker = datetime_marker_helper!(@data/zone/locations, $($zone_locations_yes)?);
            type GenericLongV1Marker = datetime_marker_helper!(@data/zone/generic_long, $($zone_generic_long_yes)?);
            type GenericShortV1Marker = datetime_marker_helper!(@data/zone/generic_short, $($zone_generic_short_yes)?);
            type SpecificLongV1Marker = datetime_marker_helper!(@data/zone/specific_long, $($zone_specific_long_yes)?);
            type SpecificShortV1Marker = datetime_marker_helper!(@data/zone/specific_short, $($zone_specific_short_yes)?);
            type MetazonePeriodV1Marker = datetime_marker_helper!(@data/zone/metazone_periods, $($metazone_periods_yes)?);
        }
        impl DateTimeMarkers for $type {
            type D = ();
            type T = ();
            type Z = Self;
            type GluePatternV1Marker = datetime_marker_helper!(@glue,);
        }
        $(
            const _: () = yes_to!((), $enumerated_yes); // condition for this macro block
            impl_composite!($type, Zone, ZoneFieldSet);
            impl $type {
                pub(crate) fn to_field(self) -> (fields::TimeZone, fields::FieldLength) {
                    match self.length {
                        Length::Short | Length::Medium => $field_short,
                        Length::Long => $field_long,
                    }
                }
            }
        )?
    };
}

macro_rules! impl_zoneddatetime_marker {
    (
        $type:ident,
        description = $description:literal,
        sample_length = $sample_length:ident,
        sample = $sample:literal,
        datetime = $datetime:path,
        zone = $zone:path,
    ) => {
        #[doc = concat!("**“", $sample, "**” ⇒ ", $description)]
        ///
        /// # Examples
        ///
        /// In [`DateTimeFormatter`](crate::neo::DateTimeFormatter):
        ///
        /// ```
        /// use icu::calendar::{Date, Time};
        /// use icu::timezone::{TimeZoneInfo, IxdtfParser};
        /// use icu::datetime::DateTimeFormatter;
        #[doc = concat!("use icu::datetime::fieldsets::", stringify!($type), ";")]
        /// use icu::locale::locale;
        /// use writeable::assert_writeable_eq;
        ///
        #[doc = concat!("let fmt = DateTimeFormatter::try_new(")]
        ///     locale!("en-GB").into(),
        #[doc = concat!("    ", length_option_helper!($type, $sample_length), ",")]
        /// )
        /// .unwrap();
        ///
        /// let mut dtz = IxdtfParser::new().try_from_str("2024-05-17T15:47:50+01:00[Europe/London]").unwrap();
        ///
        /// assert_writeable_eq!(
        ///     fmt.format_any_calendar(&dtz),
        #[doc = concat!("    \"", $sample, "\"")]
        /// );
        /// ```
        ///
        /// In [`FixedCalendarDateTimeFormatter`](crate::neo::FixedCalendarDateTimeFormatter):
        ///
        /// ```
        /// use icu::calendar::{Date, Time};
        /// use icu::timezone::{TimeZoneInfo, IxdtfParser};
        /// use icu::calendar::Gregorian;
        /// use icu::datetime::FixedCalendarDateTimeFormatter;
        #[doc = concat!("use icu::datetime::fieldsets::", stringify!($type), ";")]
        /// use icu::locale::locale;
        /// use writeable::assert_writeable_eq;
        ///
        #[doc = concat!("let fmt = FixedCalendarDateTimeFormatter::<Gregorian, ", stringify!($type), ">::try_new(")]
        ///     locale!("en-GB").into(),
        #[doc = concat!("    ", length_option_helper!($type, $sample_length), ",")]
        /// )
        /// .unwrap();
        ///
        /// let mut dtz = IxdtfParser::new().try_from_str("2024-05-17T15:47:50+01:00[Europe/London]")
        ///     .unwrap()
        ///     .to_calendar(Gregorian);
        ///
        /// assert_writeable_eq!(
        ///     fmt.format(&dtz),
        #[doc = concat!("    \"", $sample, "\"")]
        /// );
        /// ```
        pub type $type = Combo<$datetime, $zone>;
    }
}

impl_date_marker!(
    /// This format may use ordinal formatting, such as "the 17th",
    /// in the future. See CLDR-18040.
    D,
    DT,
    description = "day of month (standalone)",
    sample_length = short,
    sample = "17",
    sample_time = "17, 3:47:50 PM",
    input_day_of_month = yes,
    input_any_calendar_kind = yes,
    option_alignment = yes,
);

impl_date_marker!(
    E,
    ET,
    description = "weekday (standalone)",
    sample_length = long,
    sample = "Friday",
    sample_time = "Friday 3:47:50 PM",
    weekdays = yes,
    input_day_of_week = yes,
);

impl_date_marker!(
    /// This format may use ordinal formatting, such as "Friday the 17th",
    /// in the future. See CLDR-18040.
    DE,
    DET,
    description = "day of month and weekday",
    sample_length = long,
    sample = "17 Friday",
    sample_time = "17 Friday, 3:47:50 PM",
    weekdays = yes,
    input_day_of_month = yes,
    input_day_of_week = yes,
    option_alignment = yes,
);

impl_date_marker!(
    MD,
    MDT,
    description = "month and day",
    sample_length = medium,
    sample = "May 17",
    sample_time = "May 17, 3:47:50 PM",
    months = yes,
    input_month = yes,
    input_day_of_month = yes,
    input_any_calendar_kind = yes,
    option_alignment = yes,
);

impl_date_marker!(
    /// See CLDR-18040 for progress on improving this format.
    MDE,
    MDET,
    description = "month, day, and weekday",
    sample_length = medium,
    sample = "Fri, May 17",
    sample_time = "Fri, May 17, 3:47:50 PM",
    months = yes,
    weekdays = yes,
    input_month = yes,
    input_day_of_month = yes,
    input_day_of_week = yes,
    input_any_calendar_kind = yes,
    option_alignment = yes,
);

impl_date_marker!(
    YMD,
    YMDT,
    description = "year, month, and day",
    sample_length = short,
    sample = "5/17/24",
    sample_time = "5/17/24, 3:47:50 PM",
    years = yes,
    months = yes,
    input_year = yes,
    input_month = yes,
    input_day_of_month = yes,
    input_any_calendar_kind = yes,
    option_alignment = yes,
);

impl_date_marker!(
    YMDE,
    YMDET,
    description = "year, month, day, and weekday",
    sample_length = short,
    sample = "Fri, 5/17/24",
    sample_time = "Fri, 5/17/24, 3:47:50 PM",
    years = yes,
    months = yes,
    weekdays = yes,
    input_year = yes,
    input_month = yes,
    input_day_of_month = yes,
    input_day_of_week = yes,
    input_any_calendar_kind = yes,
    option_alignment = yes,
);

impl_calendar_period_marker!(
    Y,
    description = "year (standalone)",
    sample_length = medium,
    sample = "2024",
    years = yes,
    input_year = yes,
    input_any_calendar_kind = yes,
    option_alignment = yes,
);

impl_calendar_period_marker!(
    M,
    description = "month (standalone)",
    sample_length = long,
    sample = "May",
    months = yes,
    input_month = yes,
    input_any_calendar_kind = yes,
    option_alignment = yes,
);

impl_calendar_period_marker!(
    YM,
    description = "year and month",
    sample_length = medium,
    sample = "May 2024",
    years = yes,
    months = yes,
    input_year = yes,
    input_month = yes,
    input_any_calendar_kind = yes,
    option_alignment = yes,
);

impl_time_marker!(
    /// Hours can be switched between 12-hour and 24-hour time via the `u-hc` locale keyword
    /// or [`DateTimeFormatterPreferences`].
    ///
    /// ```
    /// use icu::calendar::Time;
    /// use icu::datetime::fieldsets::T;
    /// use icu::datetime::TimeFormatter;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// // By default, en-US uses 12-hour time and fr-FR uses 24-hour time,
    /// // but we can set overrides.
    ///
    /// let formatter = TimeFormatter::try_new(
    ///     locale!("en-US-u-hc-h12").into(),
    ///     T::short().hm(),
    /// )
    /// .unwrap();
    /// assert_writeable_eq!(
    ///     formatter.format(&Time::try_new(16, 12, 20, 0).unwrap()),
    ///     "4:12 PM"
    /// );
    ///
    /// let formatter = TimeFormatter::try_new(
    ///     locale!("en-US-u-hc-h23").into(),
    ///     T::short().hm(),
    /// )
    /// .unwrap();
    /// assert_writeable_eq!(
    ///     formatter.format(&Time::try_new(16, 12, 20, 0).unwrap()),
    ///     "16:12"
    /// );
    ///
    /// let formatter = TimeFormatter::try_new(
    ///     locale!("fr-FR-u-hc-h12").into(),
    ///     T::short().hm(),
    /// )
    /// .unwrap();
    /// assert_writeable_eq!(
    ///     formatter.format(&Time::try_new(16, 12, 20, 0).unwrap()),
    ///     "4:12 PM"
    /// );
    ///
    /// let formatter = TimeFormatter::try_new(
    ///     locale!("fr-FR-u-hc-h23").into(),
    ///     T::short().hm(),
    /// )
    /// .unwrap();
    /// assert_writeable_eq!(
    ///     formatter.format(&Time::try_new(16, 12, 20, 0).unwrap()),
    ///     "16:12"
    /// );
    /// ```
    ///
    /// Hour cycles `h11` and `h24` are supported, too:
    ///
    /// ```
    /// use icu::calendar::Time;
    /// use icu::datetime::fieldsets::T;
    /// use icu::datetime::TimeFormatter;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let formatter = TimeFormatter::try_new(
    ///     locale!("und-u-hc-h11").into(),
    ///     T::short().hm(),
    /// )
    /// .unwrap();
    ///
    /// assert_writeable_eq!(
    ///     formatter.format(&Time::try_new(0, 0, 0, 0).unwrap()),
    ///     "0:00 AM"
    /// );
    ///
    /// let formatter = TimeFormatter::try_new(
    ///     locale!("und-u-hc-h24").into(),
    ///     T::short().hm(),
    /// )
    /// .unwrap();
    ///
    /// assert_writeable_eq!(
    ///     formatter.format(&Time::try_new(0, 0, 0, 0).unwrap()),
    ///     "24:00"
    /// );
    /// ```
    ///
    /// [`DateTimeFormatterPreferences`]: crate::DateTimeFormatterPreferences
    T,
    description = "time (locale-dependent hour cycle)",
    sample_length = medium,
    sample = "3:47:50 PM",
    dayperiods = yes,
    input_hour = yes,
    input_minute = yes,
    input_second = yes,
    input_nanosecond = yes,
);

impl_zone_marker!(
    /// When a display name is unavailable, falls back to the localized offset format for short lengths, and
    /// to the location format for long lengths:
    ///
    /// ```
    /// use icu::calendar::{Date, Time};
    /// use icu::timezone::{IxdtfParser, TimeZoneBcp47Id, TimeZoneInfo, UtcOffset, ZoneVariant};
    /// use icu::calendar::Gregorian;
    /// use icu::datetime::FixedCalendarDateTimeFormatter;
    /// use icu::datetime::fieldsets::Z;
    /// use icu::locale::locale;
    /// use tinystr::tinystr;
    /// use writeable::assert_writeable_eq;
    ///
    /// // Time zone info for Europe/Istanbul in the winter
    /// let zone = TimeZoneBcp47Id(tinystr!(8, "trist"))
    ///     .with_offset("+02".parse().ok())
    ///     .at_time((Date::try_new_iso(2022, 1, 29).unwrap(), Time::midnight()))
    ///     .with_zone_variant(ZoneVariant::Standard);
    ///
    /// let fmt = FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(
    ///     locale!("en").into(),
    ///     Z::short(),
    /// )
    /// .unwrap();
    ///
    /// assert_writeable_eq!(
    ///     fmt.format(&zone),
    ///     "GMT+2"
    /// );
    ///
    /// let fmt = FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(
    ///     locale!("en").into(),
    ///     Z::long(),
    /// )
    /// .unwrap();
    ///
    /// assert_writeable_eq!(
    ///     fmt.format(&zone),
    ///     "Türkiye Standard Time"
    /// );
    /// ```
    ///
    /// This style requires a [`ZoneVariant`], so
    /// only a full time zone info can be formatted with this style.
    /// For example, [`TimeZoneInfo<AtTime>`] cannot be formatted.
    ///
    /// ```compile_fail,E0271
    /// use icu::calendar::{DateTime, Iso};
    /// use icu::datetime::FixedCalendarDateTimeFormatter;
    /// use icu::datetime::fieldsets::Z;
    /// use icu::timezone::{TimeZoneBcp47Id, UtcOffset, ZoneVariant};
    /// use tinystr::tinystr;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let datetime = DateTime::try_new_gregorian(2024, 10, 18, 0, 0, 0).unwrap();
    /// let time_zone_basic = TimeZoneBcp47Id(tinystr!(8, "uschi")).with_offset("-06".parse().ok());
    /// let time_zone_at_time = time_zone_basic.at_time((datetime.date.to_iso(), datetime.time));
    ///
    /// let formatter = FixedCalendarDateTimeFormatter::try_new(
    ///     locale!("en-US").into(),
    ///     Z::medium(),
    /// )
    /// .unwrap();
    ///
    /// // error[E0271]: type mismatch resolving `<AtTime as TimeZoneModel>::ZoneVariant == ZoneVariant`
    /// formatter.format(&time_zone_at_time);
    /// ```
    Z,
    description = "time zone in specific non-location format",
    sample_length = long,
    sample = "Central Daylight Time",
    field_short = (fields::TimeZone::SpecificNonLocation, fields::FieldLength::One),
    field_long = (fields::TimeZone::SpecificNonLocation, fields::FieldLength::Four),
    resolved_type = Z,
    zone_essentials = yes,
    zone_locations = yes,
    zone_specific_long = yes,
    zone_specific_short = yes,
    metazone_periods = yes,
    input_tzid = yes,
    input_variant = yes,
    input_localtime = yes,
    enumerated = yes,
);

impl_zone_marker!(
    /// This style requires a [`ZoneVariant`], so
    /// only a full time zone info can be formatted with this style.
    /// For example, [`TimeZoneInfo<AtTime>`] cannot be formatted.
    ///
    /// ```compile_fail,E0271
    /// use icu::calendar::{DateTime, Iso};
    /// use icu::datetime::FixedCalendarDateTimeFormatter;
    /// use icu::datetime::fieldsets::{Combo, T, Zs};
    /// use icu::timezone::{TimeZoneBcp47Id, UtcOffset, ZoneVariant};
    /// use tinystr::tinystr;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let datetime = DateTime::try_new_gregorian(2024, 10, 18, 0, 0, 0).unwrap();
    /// let time_zone_basic = TimeZoneBcp47Id(tinystr!(8, "uschi")).with_offset("-06".parse().ok());
    /// let time_zone_at_time = time_zone_basic.at_time((datetime.date.to_iso(), datetime.time));
    ///
    /// let formatter = FixedCalendarDateTimeFormatter::try_new(
    ///     locale!("en-US").into(),
    ///     Combo::<T, Zs>::medium(),
    /// )
    /// .unwrap();
    ///
    /// // error[E0271]: type mismatch resolving `<AtTime as TimeZoneModel>::ZoneVariant == ZoneVariant`
    /// // note: required by a bound in `FixedCalendarDateTimeFormatter::<C, FSet>::format`
    /// formatter.format(&time_zone_at_time);
    /// ```
    Zs,
    description = "time zone in specific non-location format (only short)",
    sample_length = short,
    sample = "CDT",
    field_short = (fields::TimeZone::SpecificNonLocation, fields::FieldLength::One),
    field_long = (fields::TimeZone::SpecificNonLocation, fields::FieldLength::One),
    resolved_type = Z,
    skip_tests = "This field set can be used only in combination with others.",
    zone_essentials = yes,
    zone_specific_short = yes,
    metazone_periods = yes,
    input_tzid = yes,
    input_variant = yes,
    input_localtime = yes,
);

impl_zone_marker!(
    /// All shapes of time zones can be formatted with this style.
    ///
    /// ```
    /// use icu::calendar::{Date, Time};
    /// use icu::datetime::TimeFormatter;
    /// use icu::datetime::fieldsets::O;
    /// use icu::timezone::{TimeZoneBcp47Id, UtcOffset, ZoneVariant};
    /// use tinystr::tinystr;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let utc_offset = "-06".parse().unwrap();
    /// let time_zone_basic = TimeZoneBcp47Id(tinystr!(8, "uschi")).with_offset(Some(utc_offset));
    ///
    /// let date = Date::try_new_iso(2024, 10, 18).unwrap();
    /// let time = Time::midnight();
    /// let time_zone_at_time = time_zone_basic.at_time((date, time));
    ///
    /// let time_zone_full = time_zone_at_time.with_zone_variant(ZoneVariant::Standard);
    ///
    /// let formatter = TimeFormatter::try_new(
    ///     locale!("en-US").into(),
    ///     O::medium(),
    /// )
    /// .unwrap();
    ///
    /// assert_writeable_eq!(
    ///     formatter.format(&utc_offset),
    ///     "GMT-6"
    /// );
    ///
    /// assert_writeable_eq!(
    ///     formatter.format(&time_zone_basic),
    ///     "GMT-6"
    /// );
    ///
    /// assert_writeable_eq!(
    ///     formatter.format(&time_zone_at_time),
    ///     "GMT-6"
    /// );
    ///
    /// assert_writeable_eq!(
    ///     formatter.format(&time_zone_full),
    ///     "GMT-6"
    /// );
    /// ```
    O,
    description = "UTC offset",
    sample_length = medium,
    sample = "GMT-5",
    field_short = (fields::TimeZone::LocalizedOffset, fields::FieldLength::One),
    field_long = (fields::TimeZone::LocalizedOffset, fields::FieldLength::Four),
    resolved_type = O,
    zone_essentials = yes,
    enumerated = yes,
);

impl_zone_marker!(
    /// When a display name is unavailable, falls back to the location format:
    ///
    /// ```
    /// use icu::calendar::{Date, Time};
    /// use icu::timezone::{IxdtfParser, TimeZoneBcp47Id, TimeZoneInfo, UtcOffset, ZoneVariant};
    /// use icu::calendar::Gregorian;
    /// use icu::datetime::FixedCalendarDateTimeFormatter;
    /// use icu::datetime::fieldsets::V;
    /// use icu::locale::locale;
    /// use tinystr::tinystr;
    /// use writeable::assert_writeable_eq;
    ///
    /// // Time zone info for Europe/Istanbul
    /// let zone = TimeZoneBcp47Id(tinystr!(8, "trist"))
    ///     .without_offset()
    ///     .at_time((Date::try_new_iso(2022, 1, 29).unwrap(), Time::midnight()));
    ///
    /// let fmt = FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(
    ///     locale!("en").into(),
    ///     V::short(),
    /// )
    /// .unwrap();
    ///
    /// assert_writeable_eq!(
    ///     fmt.format(&zone),
    ///     "Türkiye Time"
    /// );
    /// ```
    ///
    /// Can also fall back to the UTC offset:
    ///
    /// ```
    /// use icu::calendar::{Date, Time};
    /// use icu::timezone::{TimeZoneInfo, UtcOffset, TimeZoneIdMapper, TimeZoneBcp47Id};
    /// use icu::datetime::TimeFormatter;
    /// use icu::datetime::fieldsets::V;
    /// use icu::datetime::DateTimeWriteError;
    /// use icu::locale::locale;
    /// use tinystr::tinystr;
    /// use writeable::assert_writeable_eq;
    ///
    /// // Set up the formatter
    /// let mut tzf = TimeFormatter::try_new(
    ///     locale!("en").into(),
    ///     V::short(),
    /// )
    /// .unwrap();
    ///
    /// // "uschi" - has symbol data for short generic non-location
    /// let time_zone = TimeZoneIdMapper::new()
    ///     .iana_to_bcp47("America/Chicago")
    ///     .with_offset("-05".parse().ok())
    ///     .at_time((Date::try_new_iso(2022, 8, 29).unwrap(), Time::midnight()));
    /// assert_writeable_eq!(
    ///     tzf.format(&time_zone),
    ///     "CT"
    /// );
    ///
    /// // "ushnl" - has time zone override symbol data for short generic non-location
    /// let time_zone = TimeZoneIdMapper::new()
    ///     .iana_to_bcp47("Pacific/Honolulu")
    ///     .with_offset("-10".parse().ok())
    ///     .at_time((Date::try_new_iso(2022, 8, 29).unwrap(), Time::midnight()));
    /// assert_writeable_eq!(
    ///     tzf.format(&time_zone),
    ///     "HST"
    /// );
    ///
    /// // Mis-spelling of "America/Chicago" results in a fallback to offset format
    /// let time_zone = TimeZoneIdMapper::new()
    ///     .iana_to_bcp47("America/Chigagou")
    ///     .with_offset("-05".parse().ok())
    ///     .at_time((Date::try_new_iso(2022, 8, 29).unwrap(), Time::midnight()));
    /// assert_writeable_eq!(
    ///     tzf.format(&time_zone),
    ///     "GMT-5"
    /// );
    /// ```
    ///
    /// Since non-location names might change over time,
    /// this time zone style requires a reference time.
    ///
    /// ```compile_fail,E0271
    /// use icu::calendar::{DateTime, Iso};
    /// use icu::datetime::TimeFormatter;
    /// use icu::datetime::fieldsets::V;
    /// use icu::timezone::{TimeZoneBcp47Id, UtcOffset};
    /// use tinystr::tinystr;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let time_zone_basic = TimeZoneBcp47Id(tinystr!(8, "uschi")).without_offset();
    ///
    /// let formatter = TimeFormatter::try_new(
    ///     locale!("en-US").into(),
    ///     V::medium(),
    /// )
    /// .unwrap();
    ///
    /// // error[E0271]: type mismatch resolving `<Base as TimeZoneModel>::LocalTime == (Date<Iso>, Time)`
    /// // note: required by a bound in `TimeFormatter::<C, FSet>::format`
    /// formatter.format(&time_zone_basic);
    /// ```
    V,
    description = "time zone in generic non-location format",
    sample_length = long,
    sample = "Central Time",
    field_short = (fields::TimeZone::GenericNonLocation, fields::FieldLength::One),
    field_long = (fields::TimeZone::GenericNonLocation, fields::FieldLength::Four),
    resolved_type = V,
    zone_essentials = yes,
    zone_locations = yes,
    zone_generic_long = yes,
    zone_generic_short = yes,
    metazone_periods = yes,
    input_tzid = yes,
    input_localtime = yes,
    enumerated = yes,
);

impl_zone_marker!(
    /// Since non-location names might change over time,
    /// this time zone style requires a reference time.
    ///
    /// ```compile_fail,E0271
    /// use icu::calendar::{DateTime, Iso};
    /// use icu::datetime::FixedCalendarDateTimeFormatter;
    /// use icu::datetime::fieldsets::Vs;
    /// use icu::timezone::{TimeZoneBcp47Id, UtcOffset};
    /// use tinystr::tinystr;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let time_zone_basic = TimeZoneBcp47Id(tinystr!(8, "uschi")).with_offset("-06".parse().ok());1
    ///
    /// let formatter = FixedCalendarDateTimeFormatter::try_new(
    ///     locale!("en-US").into(),
    ///     Vs::medium(),
    /// )
    /// .unwrap();
    ///
    /// // error[E0271]: type mismatch resolving `<Base as TimeZoneModel>::LocalTime == (Date<Iso>, Time)`
    /// // note: required by a bound in `FixedCalendarDateTimeFormatter::<C, FSet>::format`
    /// formatter.format(&time_zone_basic);
    /// ```
    Vs,
    description = "time zone in generic non-location format (only short)",
    sample_length = short,
    sample = "CT",
    field_short = (fields::TimeZone::GenericNonLocation, fields::FieldLength::One),
    field_long = (fields::TimeZone::GenericNonLocation, fields::FieldLength::One),
    resolved_type = V,
    skip_tests = "This field set can be used only in combination with others.",
    zone_essentials = yes,
    zone_locations = yes,
    zone_generic_short = yes,
    metazone_periods = yes,
    input_tzid = yes,
    input_localtime = yes,
);

impl_zone_marker!(
    /// A time zone ID is required to format with this style.
    /// For example, a raw [`UtcOffset`] cannot be used here.
    ///
    /// ```compile_fail,E0277
    /// use icu::calendar::{DateTime, Iso};
    /// use icu::datetime::FixedCalendarDateTimeFormatter;
    /// use icu::datetime::fieldsets::L;
    /// use icu::timezone::UtcOffset;
    /// use tinystr::tinystr;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let utc_offset = UtcOffset::try_from_str("-06").unwrap();
    ///
    /// let formatter = FixedCalendarDateTimeFormatter::try_new(
    ///     locale!("en-US").into(),
    ///     L::medium(),
    /// )
    /// .unwrap();
    ///
    /// // error[E0277]: the trait bound `UtcOffset: AllInputMarkers<L>` is not satisfied
    /// // note: required by a bound in `FixedCalendarDateTimeFormatter::<C, FSet>::format`
    /// formatter.format(&utc_offset);
    /// ```
    L,
    description = "time zone in location format",
    sample_length = long,
    sample = "Chicago Time",
    field_short = (fields::TimeZone::Location, fields::FieldLength::Four),
    field_long = (fields::TimeZone::Location, fields::FieldLength::Four),
    resolved_type = L,
    zone_essentials = yes,
    zone_locations = yes,
    input_tzid = yes,
    enumerated = yes,
);

impl_zoneddatetime_marker!(
    YMDTV,
    description = "locale-dependent date and time fields with a time zone",
    sample_length = medium,
    sample = "17 May 2024, 15:47:50 GMT",
    datetime = YMDT,
    zone = Vs,
);

impl_zoneddatetime_marker!(
    YMDTZ,
    description = "locale-dependent date and time fields with a time zone",
    sample_length = medium,
    sample = "17 May 2024, 15:47:50 BST",
    datetime = YMDT,
    zone = Zs,
);

impl_zoneddatetime_marker!(
    YMDTO,
    description = "locale-dependent date and time fields with a time zone",
    sample_length = medium,
    sample = "17 May 2024, 15:47:50 GMT+1",
    datetime = YMDT,
    zone = O,
);

impl_zone_combo_helpers!(DateFieldSet, DateZone, UNREACHABLE, no_wrap);

impl_zone_combo_helpers!(TimeFieldSet, TimeZone, UNREACHABLE, no_wrap);

impl_zone_combo_helpers!(DateAndTimeFieldSet, DateTimeZone, UNREACHABLE, no_wrap);
