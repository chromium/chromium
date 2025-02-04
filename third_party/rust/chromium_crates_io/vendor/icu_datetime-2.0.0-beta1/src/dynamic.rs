// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Enumerations over [field sets](crate::fieldsets).
//!
//! These enumerations can be used when the field set is not known at compile time. However,
//! they may contribute negatively to the binary size of the formatters.
//!
//! The most general type is [`CompositeFieldSet`], which supports all field
//! sets in a single enumeration. [`CompositeDateTimeFieldSet`] is a good
//! choice when you don't need to format time zones.
//!
//! Summary of all the types:
//!
//! | Type | Supported Field Sets |
//! |---|---|
//! | [`DateFieldSet`] | Date |
//! | [`CalendarPeriodFieldSet`] | Calendar Period |
//! | [`TimeFieldSet`] | Time |
//! | [`ZoneFieldSet`] | Zone |
//! | [`DateAndTimeFieldSet`] | Date + Time |
//! | [`CompositeDateTimeFieldSet`] | Date, Calendar Period, Time, Date + Time |
//! | [`CompositeFieldSet`] | All |
//!
//! # Examples
//!
//! Format with the time display depending on a runtime boolean:
//!
//! ```
//! use icu::calendar::DateTime;
//! use icu::datetime::fieldsets;
//! use icu::datetime::fieldsets::enums::CompositeDateTimeFieldSet;
//! use icu::datetime::DateTimeFormatter;
//! use icu::locale::locale;
//! use writeable::Writeable;
//!
//! fn get_field_set(should_display_time: bool) -> CompositeDateTimeFieldSet {
//!     if should_display_time {
//!         let field_set = fieldsets::MDT::medium().hm();
//!         CompositeDateTimeFieldSet::DateTime(
//!             fieldsets::enums::DateAndTimeFieldSet::MDT(field_set),
//!         )
//!     } else {
//!         let field_set = fieldsets::MD::medium();
//!         CompositeDateTimeFieldSet::Date(fieldsets::enums::DateFieldSet::MD(
//!             field_set,
//!         ))
//!     }
//! }
//!
//! let datetime = DateTime::try_new_iso(2025, 1, 15, 16, 0, 0).unwrap();
//!
//! let results = [true, false]
//!     .map(get_field_set)
//!     .map(|field_set| {
//!         DateTimeFormatter::try_new(locale!("en-US").into(), field_set)
//!             .unwrap()
//!     })
//!     .map(|formatter| formatter.format_any_calendar(&datetime).to_string());
//!
//! assert_eq!(results, ["Jan 15, 4:00 PM", "Jan 15"])
//! ```

use crate::raw::neo::RawOptions;
use crate::scaffold::GetField;
use crate::{fields, fieldsets, Length};
use icu_provider::prelude::*;

/// An enumeration over all possible date field sets.
///
/// This is a dynamic field set. For more information, see [`enums`](crate::fieldsets::enums).
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum DateFieldSet {
    /// The day of the month, as in
    /// “on the 1st”.
    D(fieldsets::D),
    /// The month and day of the month, as in
    /// “January 1st”.
    MD(fieldsets::MD),
    /// The year, month, and day of the month, as in
    /// “January 1st, 2000”.
    YMD(fieldsets::YMD),
    /// The day of the month and day of the week, as in
    /// “Saturday 1st”.
    DE(fieldsets::DE),
    /// The month, day of the month, and day of the week, as in
    /// “Saturday, January 1st”.
    MDE(fieldsets::MDE),
    /// The year, month, day of the month, and day of the week, as in
    /// “Saturday, January 1st, 2000”.
    YMDE(fieldsets::YMDE),
    /// The day of the week alone, as in
    /// “Saturday”.
    E(fieldsets::E),
}

/// An enumeration over all possible calendar period field sets.
///
/// This is a dynamic field set. For more information, see [`enums`](crate::fieldsets::enums).
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum CalendarPeriodFieldSet {
    /// A standalone month, as in
    /// “January”.
    M(fieldsets::M),
    /// A month and year, as in
    /// “January 2000”.
    YM(fieldsets::YM),
    /// A year, as in
    /// “2000”.
    Y(fieldsets::Y),
    // TODO: Add support for week-of-year
    // /// The year and week of the year, as in
    // /// “52nd week of 1999”.
    // YW(fieldsets::YW),
    // TODO(#501): Consider adding support for Quarter and YearQuarter.
}

/// An enumeration over all possible time field sets.
///
/// This is a dynamic field set. For more information, see [`enums`](crate::fieldsets::enums).
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum TimeFieldSet {
    /// A time of day.
    T(fieldsets::T),
}

/// An enumeration over all possible zone field sets.
///
/// This is a dynamic field set. For more information, see [`enums`](crate::fieldsets::enums).
///
/// Note: [`fieldsets::Zs`] and [`fieldsets::Vs`] are not included in this enum
/// because they are data size optimizations only.
///
/// # Time Zone Data Size
///
/// Time zone names contribute a lot of data size. For resource-constrained
/// environments, the following formats require the least amount of data:
///
/// - [`fieldsets::Zs`]
/// - [`fieldsets::O`]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum ZoneFieldSet {
    /// The specific non-location format, as in
    /// “Pacific Daylight Time”.
    Z(fieldsets::Z),
    /// The offset format, as in
    /// “GMT−8”.
    O(fieldsets::O),
    /// The generic non-location format, as in
    /// “Pacific Time”.
    V(fieldsets::V),
    /// The location format, as in
    /// “Los Angeles time”.
    L(fieldsets::L),
}

/// An enumeration over all possible zone styles.
///
/// This is similar to [`ZoneFieldSet`], except the fields are not
/// self-contained semantic skeletons: they do not contain the length.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum ZoneStyle {
    /// The specific non-location format, as in
    /// “Pacific Daylight Time”.
    Z,
    /// The offset format, as in
    /// “GMT−8”.
    O,
    /// The generic non-location format, as in
    /// “Pacific Time”.
    V,
    /// The location format, as in
    /// “Los Angeles time”.
    L,
}

/// An enumeration over all possible date+time composite field sets.
///
/// This is a dynamic field set. For more information, see [`enums`](crate::fieldsets::enums).
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum DateAndTimeFieldSet {
    /// The day of the month with time of day, as in
    /// “on the 1st at 10:31 AM”.
    DT(fieldsets::DT),
    /// The month and day of the month with time of day, as in
    /// “January 1st at 10:31 AM”.
    MDT(fieldsets::MDT),
    /// The year, month, and day of the month with time of day, as in
    /// “January 1st, 2000 at 10:31 AM”.
    YMDT(fieldsets::YMDT),
    /// The day of the month and day of the week with time of day, as in
    /// “Saturday 1st at 10:31 AM”.
    DET(fieldsets::DET),
    /// The month, day of the month, and day of the week with time of day, as in
    /// “Saturday, January 1st at 10:31 AM”.
    MDET(fieldsets::MDET),
    /// The year, month, day of the month, and day of the week with time of day, as in
    /// “Saturday, January 1st, 2000 at 10:31 AM”.
    YMDET(fieldsets::YMDET),
    /// The day of the week alone with time of day, as in
    /// “Saturday at 10:31 AM”.
    ET(fieldsets::ET),
}

/// An enum supporting date, calendar period, time, and date+time field sets
/// and options.
///
/// Time zones are not supported with this enum.
///
/// This enum is useful when formatting a type that does not contain a
/// time zone or to avoid storing time zone data.
///
/// This is a dynamic field set. For more information, see [`enums`](crate::fieldsets::enums).
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum CompositeDateTimeFieldSet {
    /// Field set for a date.
    Date(DateFieldSet),
    /// Field set for a calendar period.
    CalendarPeriod(CalendarPeriodFieldSet),
    /// Field set for a time.
    Time(TimeFieldSet),
    /// Field set for a date and a time together.
    DateTime(DateAndTimeFieldSet),
}

impl CompositeDateTimeFieldSet {
    /// If the [`CompositeFieldSet`] does not contain a time zone,
    /// returns the corresponding [`CompositeDateTimeFieldSet`].
    pub fn try_from_composite_field_set(field_set: CompositeFieldSet) -> Option<Self> {
        match field_set {
            CompositeFieldSet::Date(v) => Some(Self::Date(v)),
            CompositeFieldSet::CalendarPeriod(v) => Some(Self::CalendarPeriod(v)),
            CompositeFieldSet::Time(v) => Some(Self::Time(v)),
            CompositeFieldSet::Zone(_) => None,
            CompositeFieldSet::DateTime(v) => Some(Self::DateTime(v)),
            CompositeFieldSet::DateZone(_, _) => None,
            CompositeFieldSet::TimeZone(_, _) => None,
            CompositeFieldSet::DateTimeZone(_, _) => None,
        }
    }

    /// Returns the [`CompositeFieldSet`] corresponding to this
    /// [`CompositeDateTimeFieldSet`].
    pub fn to_composite_field_set(self) -> CompositeFieldSet {
        match self {
            Self::Date(v) => CompositeFieldSet::Date(v),
            Self::CalendarPeriod(v) => CompositeFieldSet::CalendarPeriod(v),
            Self::Time(v) => CompositeFieldSet::Time(v),
            Self::DateTime(v) => CompositeFieldSet::DateTime(v),
        }
    }
}

impl GetField<CompositeFieldSet> for CompositeDateTimeFieldSet {
    fn get_field(&self) -> CompositeFieldSet {
        self.to_composite_field_set()
    }
}

/// An enum supporting all possible field sets and options.
///
/// This is a dynamic field set. For more information, see [`enums`](crate::fieldsets::enums).
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum CompositeFieldSet {
    /// Field set for a date.
    Date(DateFieldSet),
    /// Field set for a calendar period.
    CalendarPeriod(CalendarPeriodFieldSet),
    /// Field set for a time.
    Time(TimeFieldSet),
    /// Field set for a time zone.
    Zone(ZoneFieldSet),
    /// Field set for a date and a time together.
    DateTime(DateAndTimeFieldSet),
    /// Field set for a date and a time zone together.
    DateZone(DateFieldSet, ZoneStyle),
    /// Field set for a time and a time zone together.
    TimeZone(TimeFieldSet, ZoneStyle),
    /// Field set for a date, a time, and a time zone together.
    DateTimeZone(DateAndTimeFieldSet, ZoneStyle),
}

macro_rules! first {
    ($first:literal, $($remainder:literal,)*) => {
        $first
    };
}

macro_rules! impl_attrs {
    (@attrs, $type:path, [$(($attr_var:ident, $str_var:ident, $value:literal),)+]) => {
        impl $type {
            $(
                const $attr_var: &'static DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic($value);
            )+
            /// All attributes associated with this enum.
            ///
            /// # Encoding Details
            ///
            /// The string is based roughly on the UTS 35 symbol table with the following exceptions:
            ///
            /// 1. Lowercase letters are chosen where there is no ambiguity: `E` becomes `e`
            /// 2. Capitals are replaced with their lowercase and a number 0: `M` becomes `m0`
            /// 3. A single symbol is included for each component: length doesn't matter
            /// 4. Time fields are encoded with their hour field only: `j`, `h`, or `h0`
            ///
            /// # Examples
            ///
            /// ```
            #[doc = concat!("use icu::datetime::fieldsets::enums::", stringify!($type), " as FS;")]
            /// use icu_provider::DataMarkerAttributes;
            ///
            /// assert!(FS::ALL_DATA_MARKER_ATTRIBUTES.contains(
            #[doc = concat!("    &DataMarkerAttributes::from_str_or_panic(\"", first!($($value,)*), "\")")]
            /// ));
            /// ```
            pub const ALL_DATA_MARKER_ATTRIBUTES: &'static [&'static DataMarkerAttributes] = &[
                $(
                    Self::$attr_var,
                )+
            ];
        }
    };
    (@id_str, $type:path, [$(($variant:ident, $attr_var:ident)),+,]) => {
        impl $type {
            /// Returns a stable string identifying this set of fields.
            pub(crate) const fn id_str(self) -> &'static DataMarkerAttributes {
                match self {
                    $(
                        Self::$variant(_) => Self::$attr_var,
                    )+
                }
            }
        }
    };
    (@to_raw_options, $type:path, [$($variant:ident),+,]) => {
        impl $type {
            pub(crate) fn to_raw_options(self) -> RawOptions {
                match self {
                    $(
                        Self::$variant(variant) => variant.to_raw_options(),
                    )+
                }
            }
        }
    };
    (@composite, $type:path, $variant:ident) => {
        impl $type {
            #[inline]
            pub(crate) fn to_enum(self) -> $type {
                self
            }
        }
        impl GetField<CompositeFieldSet> for $type {
            #[inline]
            fn get_field(&self) -> CompositeFieldSet {
                CompositeFieldSet::$variant(self.to_enum())
            }
        }
    };
    (@date, $type:path, [$(($variant:ident, $attr_var:ident, $str_var:ident, $value:literal)),+,]) => {
        impl_attrs! { @attrs, $type, [$(($attr_var, $str_var, $value)),+,] }
        impl_attrs! { @id_str, $type, [$(($variant, $attr_var)),+,] }
        impl_attrs! { @to_raw_options, $type, [$($variant),+,] }
        impl_attrs! { @composite, $type, Date }
    };
    (@calendar_period, $type:path, [$(($variant:ident, $attr_var:ident, $str_var:ident, $value:literal)),+,]) => {
        impl_attrs! { @attrs, $type, [$(($attr_var, $str_var, $value)),+,] }
        impl_attrs! { @to_raw_options, $type, [$($variant),+,] }
        impl_attrs! { @composite, $type, CalendarPeriod }
        impl_attrs! { @id_str, $type, [$(($variant, $attr_var)),+,] }
    };
    (@time, $type:path, [$(($attr_var:ident, $str_var:ident, $value:literal)),+,]) => {
        impl_attrs! { @attrs, $type, [$(($attr_var, $str_var, $value)),+,] }
        impl_attrs! { @to_raw_options, $type, [T,] }
        impl_attrs! { @composite, $type, Time }
    };
    (@zone, $type:path, [$($variant:ident),+,]) => {
        impl_attrs! { @composite, $type, Zone }
        impl $type {
            pub(crate) fn to_field(self) -> (fields::TimeZone, fields::FieldLength) {
                match self {
                    $(
                        Self::$variant(variant) => variant.to_field(),
                    )+
                }
            }
        }
    };
    (@datetime, $type:path, [$(($d_variant:ident, $variant:ident)),+,]) => {
        impl_attrs! { @to_raw_options, $type, [$($variant),+,] }
        impl_attrs! { @composite, $type, DateTime }
        impl $type {
            pub(crate) fn to_date_field_set(self) -> DateFieldSet {
                match self {
                    $(
                        Self::$variant(variant) => DateFieldSet::$d_variant(variant.to_date_field_set()),
                    )+
                }
            }
            pub(crate) fn to_time_field_set(self) -> TimeFieldSet {
                let (length, time_precision, alignment) = match self {
                    $(
                        Self::$variant(variant) => (variant.length, variant.time_precision, variant.alignment),
                    )+
                };
                TimeFieldSet::T(fieldsets::T {
                    length,
                    time_precision,
                    alignment,
                })
            }
            #[cfg(all(feature = "serde", feature = "experimental"))]
            pub(crate) fn from_date_field_set_with_raw_options(date_field_set: DateFieldSet, options: RawOptions) -> Self {
                match date_field_set {
                    $(
                        DateFieldSet::$d_variant(_) => Self::$variant(fieldsets::$variant::from_raw_options(options)),
                    )+
                }
            }
        }
    };
}

impl_attrs! {
    @date,
    DateFieldSet,
    [
        (D, ATTR_D, STR_D, "d"),
        (MD, ATTR_MD, STR_MD, "m0d"),
        (YMD, ATTR_YMD, STR_YMD, "ym0d"),
        (DE, ATTR_DE, STR_DE, "de"),
        (MDE, ATTR_MDE, STR_MDE, "m0de"),
        (YMDE, ATTR_YMDE, STR_YMDE, "ym0de"),
        (E, ATTR_E, STR_E, "e"),
    ]
}

impl_attrs! {
    @calendar_period,
    CalendarPeriodFieldSet,
    [
        (M, ATTR_M, STR_M, "m0"),
        (YM, ATTR_YM, STR_YM, "ym0"),
        (Y, ATTR_Y, STR_Y, "y"),
    ]
}

impl_attrs! {
    @time,
    TimeFieldSet,
    [
        (ATTR_T, STR_T, "j"),
        (ATTR_T12, STR_T12, "h"),
        (ATTR_T24, STR_T24, "h0"),
    ]
}

impl TimeFieldSet {
    pub(crate) const fn id_str_for_hour_cycle(
        self,
        hour_cycle: Option<fields::Hour>,
    ) -> &'static DataMarkerAttributes {
        use fields::Hour::*;
        match hour_cycle {
            None => Self::ATTR_T,
            Some(H11 | H12) => Self::ATTR_T12,
            Some(H23 | H24) => Self::ATTR_T24,
        }
    }
}

impl_attrs! {
    @zone,
    ZoneFieldSet,
    [
        Z,
        O,
        V,
        L,
    ]
}

impl ZoneFieldSet {
    pub(crate) fn from_time_zone_style_and_length(style: ZoneStyle, length: Length) -> Self {
        match style {
            ZoneStyle::Z => Self::Z(fieldsets::Z::with_length(length)),
            ZoneStyle::O => Self::O(fieldsets::O::with_length(length)),
            ZoneStyle::V => Self::V(fieldsets::V::with_length(length)),
            ZoneStyle::L => Self::L(fieldsets::L::with_length(length)),
        }
    }
}

impl_attrs! {
    @attrs,
    DateAndTimeFieldSet,
    [
        (ATTR_ET, STR_ET, "ej"),
    ]
}

impl_attrs! {
    @datetime,
    DateAndTimeFieldSet,
    [
        (D, DT),
        (MD, MDT),
        (YMD, YMDT),
        (DE, DET),
        (MDE, MDET),
        (YMDE, YMDET),
        (E, ET),
    ]
}

impl DateAndTimeFieldSet {
    pub(crate) const fn id_str(self) -> Option<&'static DataMarkerAttributes> {
        match self {
            DateAndTimeFieldSet::ET(_) => Some(Self::ATTR_ET),
            _ => None,
        }
    }
}
