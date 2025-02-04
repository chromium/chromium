// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains various types used by `icu_calendar` and `icu::datetime`

use crate::error::RangeError;
use core::convert::TryFrom;
use core::convert::TryInto;
use core::fmt;
use core::num::NonZeroU8;
use tinystr::TinyAsciiStr;
use tinystr::{TinyStr16, TinyStr4};
use zerovec::maps::ZeroMapKV;
use zerovec::ule::AsULE;

/// The era of a particular date
///
/// Different calendars use different era codes, see their documentation
/// for details.
///
/// Era codes are shared with Temporal, [see Temporal proposal][era-proposal].
///
/// [era-proposal]: https://tc39.es/proposal-intl-era-monthcode/
#[derive(Copy, Clone, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
pub struct Era(pub TinyStr16);

impl From<TinyStr16> for Era {
    fn from(o: TinyStr16) -> Self {
        Self(o)
    }
}

/// General information about a year
#[derive(Copy, Clone, Debug, PartialEq)]
#[non_exhaustive]
pub struct YearInfo {
    /// The "extended year", typically anchored with year 1 as the year 1 of either the most modern or
    /// otherwise some "major" era for the calendar
    pub extended_year: i32,
    /// The rest of the details about the year
    pub kind: YearKind,
}

/// The type of year: Calendars like Chinese don't have an era and instead format with cyclic years.
#[derive(Copy, Clone, Debug, PartialEq)]
#[non_exhaustive]
pub enum YearKind {
    /// An era and a year in that era
    Era(EraYear),
    /// A cyclic year, and the related ISO year
    ///
    /// Knowing the cyclic year is typically not enough to pinpoint a date, however cyclic calendars
    /// don't typically use eras, so disambiguation can be done by saying things like "Year 甲辰 (2024)"
    Cyclic(CyclicYear),
}

impl YearInfo {
    /// Construct a new Year given an era and number
    pub(crate) fn new(extended_year: i32, era: EraYear) -> Self {
        Self {
            extended_year,
            kind: YearKind::Era(era),
        }
    }
    /// Construct a new cyclic Year given a cycle and a related_iso
    pub(crate) fn new_cyclic(extended_year: i32, cycle: NonZeroU8, related_iso: i32) -> Self {
        Self {
            extended_year,
            kind: YearKind::Cyclic(CyclicYear {
                year: cycle,
                related_iso,
            }),
        }
    }
    /// Get the year in the era if this is a non-cyclic calendar
    ///
    /// Gets the eraYear for era dates, otherwise falls back to Extended Year
    pub fn era_year(self) -> Option<i32> {
        match self.kind {
            YearKind::Era(e) => Some(e.era_year),
            YearKind::Cyclic(..) => None,
        }
    }

    /// Get the year ambiguity.
    pub fn year_ambiguity(self) -> YearAmbiguity {
        match self.kind {
            YearKind::Cyclic(_) => YearAmbiguity::EraRequired,
            YearKind::Era(e) => e.ambiguity,
        }
    }

    /// Get *some* year number that can be displayed
    ///
    /// Gets the eraYear for era dates, otherwise falls back to Extended Year
    pub fn era_year_or_extended(self) -> i32 {
        self.era_year().unwrap_or(self.extended_year)
    }

    /// Get the era, if available
    pub fn formatting_era(self) -> Option<FormattingEra> {
        match self.kind {
            YearKind::Era(e) => Some(e.formatting_era),
            YearKind::Cyclic(..) => None,
        }
    }

    /// Get the era, if available
    pub fn standard_era(self) -> Option<Era> {
        match self.kind {
            YearKind::Era(e) => Some(e.standard_era),
            YearKind::Cyclic(..) => None,
        }
    }

    /// Return the cyclic year, if any
    pub fn cyclic(self) -> Option<NonZeroU8> {
        match self.kind {
            YearKind::Era(..) => None,
            YearKind::Cyclic(cy) => Some(cy.year),
        }
    }
    /// Return the Related ISO year, if any
    pub fn related_iso(self) -> Option<i32> {
        match self.kind {
            YearKind::Era(..) => None,
            YearKind::Cyclic(cy) => Some(cy.related_iso),
        }
    }
}

/// Defines whether the era or century is required to interpret the year.
///
/// For example 2024 AD can be formatted as `2024`, or even `24`, but 1931 AD
/// should not be formatted as `31`, and 2024 BC should not be formatted as `2024`.
#[derive(Copy, Clone, Debug, PartialEq)]
#[allow(clippy::exhaustive_enums)] // logically complete
pub enum YearAmbiguity {
    /// The year is unambiguous without a century or era.
    Unambiguous,
    /// The century is required, the era may be included.
    CenturyRequired,
    /// The era is required, the century may be included.
    EraRequired,
    /// The century and era are required.
    EraAndCenturyRequired,
}

/// Information about the era as usable for formatting
///
/// This is optimized for storing datetime formatting data.
#[derive(Copy, Clone, Debug, PartialEq)]
#[non_exhaustive]
pub enum FormattingEra {
    /// An Era Index, for calendars with a small, fixed set of eras. The eras are indexed chronologically.
    ///
    /// In this context, chronological ordering of eras is obtained by ordering by their start date (or in the case of
    /// negative eras, their end date) first, and for eras sharing a date, put the negative one first. For example,
    /// bce < ce, and mundi < pre-incar < incar for Ethiopian.
    ///
    /// The TInyStr16 is a fallback string for the era when a display name is not available. It need not be an era code, it should
    /// be something sensible (or empty).
    Index(u8, TinyStr16),
    /// An era code, for calendars with a large set of era codes (Japanese)
    ///
    /// This code may not be the canonical era code, but will typically be a valid era alias
    Code(Era),
}

impl FormattingEra {
    /// Get a fallback era name suitable for display to the user when the real era name is not availabe
    pub fn fallback_name(self) -> TinyStr16 {
        match self {
            Self::Index(_idx, fallback) => fallback,
            Self::Code(era) => era.0,
        }
    }
}

/// Year information for a year that is specified with an era
#[derive(Copy, Clone, Debug, PartialEq)]
#[non_exhaustive]
pub struct EraYear {
    /// The era code as used in formatting. This era code is not necessarily unique for the calendar, and
    /// is whatever ICU4X datetime datagen uses for this era.
    ///
    /// It will typically be a valid era alias.
    ///
    /// <https://tc39.es/proposal-intl-era-monthcode/#table-eras>
    pub formatting_era: FormattingEra,
    /// The era code as expected by Temporal/CLDR. This era code is unique for the calendar
    /// and follows a particular scheme.
    ///
    /// <https://tc39.es/proposal-intl-era-monthcode/#table-eras>
    pub standard_era: Era,
    /// The numeric year in that era
    pub era_year: i32,
    /// The ambiguity when formatting this year
    pub ambiguity: YearAmbiguity,
}

/// Year information for a year that is specified as a cyclic year
#[derive(Copy, Clone, Debug, PartialEq)]
#[non_exhaustive]
pub struct CyclicYear {
    /// The year in the cycle.
    pub year: NonZeroU8,
    /// The ISO year corresponding to this year
    pub related_iso: i32,
}

/// Representation of a month in a year
///
/// Month codes typically look like `M01`, `M02`, etc, but can handle leap months
/// (`M03L`) in lunar calendars. Solar calendars will have codes between `M01` and `M12`
/// potentially with an `M13` for epagomenal months. Check the docs for a particular calendar
/// for details on what its month codes are.
///
/// Month codes are shared with Temporal, [see Temporal proposal][era-proposal].
///
/// [era-proposal]: https://tc39.es/proposal-intl-era-monthcode/
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_calendar::types))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct MonthCode(pub TinyStr4);

impl MonthCode {
    /// Returns an option which is `Some` containing the non-month version of a leap month
    /// if the [`MonthCode`] this method is called upon is a leap month, and `None` otherwise.
    /// This method assumes the [`MonthCode`] is valid.
    pub fn get_normal_if_leap(self) -> Option<MonthCode> {
        let bytes = self.0.all_bytes();
        if bytes[3] == b'L' {
            Some(MonthCode(TinyAsciiStr::try_from_utf8(&bytes[0..3]).ok()?))
        } else {
            None
        }
    }
    /// Get the month number and whether or not it is leap from the month code
    pub fn parsed(self) -> Option<(u8, bool)> {
        // Match statements on tinystrs are annoying so instead
        // we calculate it from the bytes directly

        let bytes = self.0.all_bytes();
        let is_leap = bytes[3] == b'L';
        if bytes[0] != b'M' {
            return None;
        }
        if bytes[1] == b'0' {
            if bytes[2] >= b'1' && bytes[2] <= b'9' {
                return Some((bytes[2] - b'0', is_leap));
            }
        } else if bytes[1] == b'1' && bytes[2] >= b'0' && bytes[2] <= b'3' {
            return Some((10 + bytes[2] - b'0', is_leap));
        }
        None
    }

    /// Construct a "normal" month code given a number ("Mxx").
    ///
    /// Returns an error for months greater than 99
    #[cfg(test)] // Only used in tests for now. Could be made public if people need it.
    pub(crate) fn new_normal(number: u8) -> Option<Self> {
        let tens = number / 10;
        let ones = number % 10;
        if tens > 9 {
            return None;
        }

        let bytes = [b'M', b'0' + tens, b'0' + ones, 0];
        Some(MonthCode(TinyAsciiStr::try_from_raw(bytes).ok()?))
    }
}

#[test]
fn test_get_normal_month_code_if_leap() {
    let mc1 = MonthCode(tinystr::tinystr!(4, "M01L"));
    let result1 = mc1.get_normal_if_leap();
    assert_eq!(result1, Some(MonthCode(tinystr::tinystr!(4, "M01"))));

    let mc2 = MonthCode(tinystr::tinystr!(4, "M11L"));
    let result2 = mc2.get_normal_if_leap();
    assert_eq!(result2, Some(MonthCode(tinystr::tinystr!(4, "M11"))));

    let mc_invalid = MonthCode(tinystr::tinystr!(4, "M10"));
    let result_invalid = mc_invalid.get_normal_if_leap();
    assert_eq!(result_invalid, None);
}

impl AsULE for MonthCode {
    type ULE = TinyStr4;
    fn to_unaligned(self) -> TinyStr4 {
        self.0
    }
    fn from_unaligned(u: TinyStr4) -> Self {
        Self(u)
    }
}

impl<'a> ZeroMapKV<'a> for MonthCode {
    type Container = zerovec::ZeroVec<'a, MonthCode>;
    type Slice = zerovec::ZeroSlice<MonthCode>;
    type GetType = <MonthCode as AsULE>::ULE;
    type OwnedType = MonthCode;
}

impl fmt::Display for MonthCode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// Representation of a formattable month.
#[derive(Copy, Clone, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct MonthInfo {
    /// The month number in this given year. For calendars with leap months, all months after
    /// the leap month will end up with an incremented number.
    ///
    /// In general, prefer using the month code in generic code.
    pub ordinal: u8,

    /// The month code, used to distinguish months during leap years.
    ///
    /// This follows [Temporal's specification](https://tc39.es/proposal-intl-era-monthcode/#table-additional-month-codes).
    /// Months considered the "same" have the same code: This means that the Hebrew months "Adar" and "Adar II" ("Adar, but during a leap year")
    /// are considered the same month and have the code M05
    pub standard_code: MonthCode,
    /// A month code, useable for formatting
    ///
    /// This may not necessarily be the canonical month code for a month in cases where a month has different
    /// formatting in a leap year, for example Adar/Adar II in the Hebrew calendar in a leap year has
    /// the standard code M06, but for formatting specifically the Hebrew calendar will return M06L since it is formatted
    /// differently.
    pub formatting_code: MonthCode,
}

impl MonthInfo {
    /// Gets the month number. A month number N is not necessarily the Nth month in the year
    /// if there are leap months in the year, rather it is associated with the Nth month of a "regular"
    /// year. There may be multiple month Ns in a year
    pub fn month_number(self) -> u8 {
        self.standard_code
            .parsed()
            .map(|(i, _)| i)
            .unwrap_or(self.ordinal)
    }

    /// Get whether the month is a leap month
    pub fn is_leap(self) -> bool {
        self.standard_code.parsed().map(|(_, l)| l).unwrap_or(false)
    }
}
/// A struct containing various details about the position of the day within a year. It is returned
// by the [`day_of_year_info()`](trait.DateInput.html#tymethod.day_of_year_info) method of the
// [`DateInput`] trait.
#[derive(Copy, Clone, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct DayOfYearInfo {
    /// The current day of the year, 1-based.
    pub day_of_year: u16,
    /// The number of days in a year.
    pub days_in_year: u16,
    /// The previous year.
    pub prev_year: YearInfo,
    /// The number of days in the previous year.
    pub days_in_prev_year: u16,
    /// The next year.
    pub next_year: YearInfo,
}

/// A day number in a month. Usually 1-based.
#[allow(clippy::exhaustive_structs)] // this is a newtype
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct DayOfMonth(pub u8);

/// A week number in a month. Usually 1-based.
#[derive(Clone, Copy, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
pub struct WeekOfMonth(pub u8);

/// A week number in a year. Usually 1-based.
#[derive(Clone, Copy, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
pub struct WeekOfYear(pub u8);

/// A day of week in month. 1-based.
#[derive(Clone, Copy, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
pub struct DayOfWeekInMonth(pub u8);

impl From<DayOfMonth> for DayOfWeekInMonth {
    fn from(day_of_month: DayOfMonth) -> Self {
        DayOfWeekInMonth(1 + ((day_of_month.0 - 1) / 7))
    }
}

#[test]
fn test_day_of_week_in_month() {
    assert_eq!(DayOfWeekInMonth::from(DayOfMonth(1)).0, 1);
    assert_eq!(DayOfWeekInMonth::from(DayOfMonth(7)).0, 1);
    assert_eq!(DayOfWeekInMonth::from(DayOfMonth(8)).0, 2);
}

/// This macro defines a struct for 0-based date fields: hours, minutes, seconds
/// and fractional seconds. Each unit is bounded by a range. The traits implemented
/// here will return a Result on whether or not the unit is in range from the given
/// input.
macro_rules! dt_unit {
    ($name:ident, $storage:ident, $value:expr, $docs:expr) => {
        #[doc=$docs]
        #[derive(Debug, Default, Clone, Copy, PartialEq, Eq, Ord, PartialOrd, Hash)]
        pub struct $name($storage);

        impl $name {
            /// Gets the numeric value for this component.
            pub const fn number(self) -> $storage {
                self.0
            }

            /// Creates a new value at 0.
            pub const fn zero() -> $name {
                Self(0)
            }
        }

        impl TryFrom<$storage> for $name {
            type Error = RangeError;

            fn try_from(input: $storage) -> Result<Self, Self::Error> {
                if input > $value {
                    Err(RangeError {
                        field: "$name",
                        min: 0,
                        max: $value,
                        value: input as i32,
                    })
                } else {
                    Ok(Self(input))
                }
            }
        }

        impl TryFrom<usize> for $name {
            type Error = RangeError;

            fn try_from(input: usize) -> Result<Self, Self::Error> {
                if input > $value {
                    Err(RangeError {
                        field: "$name",
                        min: 0,
                        max: $value,
                        value: input as i32,
                    })
                } else {
                    Ok(Self(input as $storage))
                }
            }
        }

        impl From<$name> for $storage {
            fn from(input: $name) -> Self {
                input.0
            }
        }

        impl From<$name> for usize {
            fn from(input: $name) -> Self {
                input.0 as Self
            }
        }

        impl $name {
            /// Attempts to add two values.
            /// Returns `Some` if the sum is within bounds.
            /// Returns `None` if the sum is out of bounds.
            pub fn try_add(self, other: $storage) -> Option<Self> {
                let sum = self.0.saturating_add(other);
                if sum > $value {
                    None
                } else {
                    Some(Self(sum))
                }
            }

            /// Attempts to subtract two values.
            /// Returns `Some` if the difference is within bounds.
            /// Returns `None` if the difference is out of bounds.
            pub fn try_sub(self, other: $storage) -> Option<Self> {
                self.0.checked_sub(other).map(Self)
            }

            /// Returns whether the value is zero.
            #[inline]
            pub fn is_zero(self) -> bool {
                self.0 == 0
            }
        }
    };
}

dt_unit!(
    IsoHour,
    u8,
    24,
    "An ISO-8601 hour component, for use with ISO calendars.

Must be within inclusive bounds `[0, 24]`. The value could be equal to 24 to
denote the end of a day, with the writing 24:00:00. It corresponds to the same
time as the next day at 00:00:00."
);

dt_unit!(
    IsoMinute,
    u8,
    60,
    "An ISO-8601 minute component, for use with ISO calendars.

Must be within inclusive bounds `[0, 60]`. The value could be equal to 60 to
denote the end of an hour, with the writing 12:60:00. This example corresponds
to the same time as 13:00:00. This is an extension to ISO 8601."
);

dt_unit!(
    IsoSecond,
    u8,
    61,
    "An ISO-8601 second component, for use with ISO calendars.

Must be within inclusive bounds `[0, 61]`. `60` accommodates for leap seconds.

The value could also be equal to 60 or 61, to indicate the end of a leap second,
with the writing `23:59:61.000000000Z` or `23:59:60.000000000Z`. These examples,
if used with this goal, would correspond to the same time as the next day, at
time `00:00:00.000000000Z`. This is an extension to ISO 8601."
);

dt_unit!(
    NanoSecond,
    u32,
    999_999_999,
    "A fractional second component, stored as nanoseconds.

Must be within inclusive bounds `[0, 999_999_999]`."
);

#[test]
fn test_iso_hour_arithmetic() {
    const HOUR_MAX: u8 = 24;
    const HOUR_VALUE: u8 = 5;
    let hour = IsoHour(HOUR_VALUE);

    // middle of bounds
    assert_eq!(
        hour.try_add(HOUR_VALUE - 1),
        Some(IsoHour(HOUR_VALUE + (HOUR_VALUE - 1)))
    );
    assert_eq!(
        hour.try_sub(HOUR_VALUE - 1),
        Some(IsoHour(HOUR_VALUE - (HOUR_VALUE - 1)))
    );

    // edge of bounds
    assert_eq!(hour.try_add(HOUR_MAX - HOUR_VALUE), Some(IsoHour(HOUR_MAX)));
    assert_eq!(hour.try_sub(HOUR_VALUE), Some(IsoHour(0)));

    // out of bounds
    assert_eq!(hour.try_add(1 + HOUR_MAX - HOUR_VALUE), None);
    assert_eq!(hour.try_sub(1 + HOUR_VALUE), None);
}

#[test]
fn test_iso_minute_arithmetic() {
    const MINUTE_MAX: u8 = 60;
    const MINUTE_VALUE: u8 = 5;
    let minute = IsoMinute(MINUTE_VALUE);

    // middle of bounds
    assert_eq!(
        minute.try_add(MINUTE_VALUE - 1),
        Some(IsoMinute(MINUTE_VALUE + (MINUTE_VALUE - 1)))
    );
    assert_eq!(
        minute.try_sub(MINUTE_VALUE - 1),
        Some(IsoMinute(MINUTE_VALUE - (MINUTE_VALUE - 1)))
    );

    // edge of bounds
    assert_eq!(
        minute.try_add(MINUTE_MAX - MINUTE_VALUE),
        Some(IsoMinute(MINUTE_MAX))
    );
    assert_eq!(minute.try_sub(MINUTE_VALUE), Some(IsoMinute(0)));

    // out of bounds
    assert_eq!(minute.try_add(1 + MINUTE_MAX - MINUTE_VALUE), None);
    assert_eq!(minute.try_sub(1 + MINUTE_VALUE), None);
}

#[test]
fn test_iso_second_arithmetic() {
    const SECOND_MAX: u8 = 61;
    const SECOND_VALUE: u8 = 5;
    let second = IsoSecond(SECOND_VALUE);

    // middle of bounds
    assert_eq!(
        second.try_add(SECOND_VALUE - 1),
        Some(IsoSecond(SECOND_VALUE + (SECOND_VALUE - 1)))
    );
    assert_eq!(
        second.try_sub(SECOND_VALUE - 1),
        Some(IsoSecond(SECOND_VALUE - (SECOND_VALUE - 1)))
    );

    // edge of bounds
    assert_eq!(
        second.try_add(SECOND_MAX - SECOND_VALUE),
        Some(IsoSecond(SECOND_MAX))
    );
    assert_eq!(second.try_sub(SECOND_VALUE), Some(IsoSecond(0)));

    // out of bounds
    assert_eq!(second.try_add(1 + SECOND_MAX - SECOND_VALUE), None);
    assert_eq!(second.try_sub(1 + SECOND_VALUE), None);
}

#[test]
fn test_iso_nano_second_arithmetic() {
    const NANO_SECOND_MAX: u32 = 999_999_999;
    const NANO_SECOND_VALUE: u32 = 5;
    let nano_second = NanoSecond(NANO_SECOND_VALUE);

    // middle of bounds
    assert_eq!(
        nano_second.try_add(NANO_SECOND_VALUE - 1),
        Some(NanoSecond(NANO_SECOND_VALUE + (NANO_SECOND_VALUE - 1)))
    );
    assert_eq!(
        nano_second.try_sub(NANO_SECOND_VALUE - 1),
        Some(NanoSecond(NANO_SECOND_VALUE - (NANO_SECOND_VALUE - 1)))
    );

    // edge of bounds
    assert_eq!(
        nano_second.try_add(NANO_SECOND_MAX - NANO_SECOND_VALUE),
        Some(NanoSecond(NANO_SECOND_MAX))
    );
    assert_eq!(nano_second.try_sub(NANO_SECOND_VALUE), Some(NanoSecond(0)));

    // out of bounds
    assert_eq!(
        nano_second.try_add(1 + NANO_SECOND_MAX - NANO_SECOND_VALUE),
        None
    );
    assert_eq!(nano_second.try_sub(1 + NANO_SECOND_VALUE), None);
}

/// A representation of a time in hours, minutes, seconds, and nanoseconds
#[derive(Debug, Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Time {
    /// 0-based hour.
    pub hour: IsoHour,

    /// 0-based minute.
    pub minute: IsoMinute,

    /// 0-based second.
    pub second: IsoSecond,

    /// Fractional second
    pub nanosecond: NanoSecond,
}

impl Time {
    /// Construct a new [`Time`], without validating that all components are in range
    pub const fn new(
        hour: IsoHour,
        minute: IsoMinute,
        second: IsoSecond,
        nanosecond: NanoSecond,
    ) -> Self {
        Self {
            hour,
            minute,
            second,
            nanosecond,
        }
    }

    /// Construct a new [`Time`] representing midnight (00:00.000)
    pub const fn midnight() -> Self {
        Self {
            hour: IsoHour::zero(),
            minute: IsoMinute::zero(),
            second: IsoSecond::zero(),
            nanosecond: NanoSecond::zero(),
        }
    }

    /// Construct a new [`Time`], whilst validating that all components are in range
    pub fn try_new(hour: u8, minute: u8, second: u8, nanosecond: u32) -> Result<Self, RangeError> {
        Ok(Self {
            hour: hour.try_into()?,
            minute: minute.try_into()?,
            second: second.try_into()?,
            nanosecond: nanosecond.try_into()?,
        })
    }
}

/// A weekday in a 7-day week, according to ISO-8601.
///
/// The discriminant values correspond to ISO-8601 weekday numbers (Monday = 1, Sunday = 7).
///
/// # Examples
///
/// ```
/// use icu::calendar::types::IsoWeekday;
///
/// assert_eq!(1, IsoWeekday::Monday as usize);
/// assert_eq!(7, IsoWeekday::Sunday as usize);
/// ```
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[allow(missing_docs)] // The weekday variants should be self-obvious.
#[repr(i8)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_calendar::types))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[allow(clippy::exhaustive_enums)] // This is stable
pub enum IsoWeekday {
    Monday = 1,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday,
}

impl From<usize> for IsoWeekday {
    /// Convert from an ISO-8601 weekday number to an [`IsoWeekday`] enum. 0 is automatically converted
    /// to 7 (Sunday). If the number is out of range, it is interpreted modulo 7.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::types::IsoWeekday;
    ///
    /// assert_eq!(IsoWeekday::Sunday, IsoWeekday::from(0));
    /// assert_eq!(IsoWeekday::Monday, IsoWeekday::from(1));
    /// assert_eq!(IsoWeekday::Sunday, IsoWeekday::from(7));
    /// assert_eq!(IsoWeekday::Monday, IsoWeekday::from(8));
    /// ```
    fn from(input: usize) -> Self {
        use IsoWeekday::*;
        match input % 7 {
            0 => Sunday,
            1 => Monday,
            2 => Tuesday,
            3 => Wednesday,
            4 => Thursday,
            5 => Friday,
            6 => Saturday,
            _ => unreachable!(),
        }
    }
}

impl IsoWeekday {
    /// Returns the day after the current day.
    pub(crate) fn next_day(self) -> IsoWeekday {
        use IsoWeekday::*;
        match self {
            Monday => Tuesday,
            Tuesday => Wednesday,
            Wednesday => Thursday,
            Thursday => Friday,
            Friday => Saturday,
            Saturday => Sunday,
            Sunday => Monday,
        }
    }
}
