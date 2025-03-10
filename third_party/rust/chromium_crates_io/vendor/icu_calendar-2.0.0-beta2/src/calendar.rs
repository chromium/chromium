// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::any_calendar::AnyCalendarKind;
use crate::error::DateError;
use crate::{types, Date, DateDuration, DateDurationUnit, Iso};
use core::fmt;

/// A calendar implementation
///
/// Only implementors of [`Calendar`] should care about these methods, in general users of
/// these calendars should use the methods on [`Date`] instead.
///
/// Individual [`Calendar`] implementations may have inherent utility methods
/// allowing for direct construction, etc.
///
/// For ICU4X 1.0, implementing this trait or calling methods directly is considered
/// unstable and prone to change, especially for `offset_date()` and `until()`.
pub trait Calendar {
    /// The internal type used to represent dates
    type DateInner: Eq + Copy + fmt::Debug;
    /// Construct a date from era/month codes and fields
    ///
    /// The year is extended_year if no era is provided
    fn date_from_codes(
        &self,
        era: Option<types::Era>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError>;
    /// Construct the date from an ISO date
    fn date_from_iso(&self, iso: Date<Iso>) -> Self::DateInner;
    /// Obtain an ISO date from this date
    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso>;
    // fn validate_date(&self, e: Era, y: Year, m: MonthCode, d: Day) -> bool;
    // // similar validators for YearMonth, etc

    // fn is_leap<A: AsCalendar<Calendar = Self>>(&self, date: &Date<A>) -> bool;
    /// Count the number of months in a given year, specified by providing a date
    /// from that year
    fn months_in_year(&self, date: &Self::DateInner) -> u8;
    /// Count the number of days in a given year, specified by providing a date
    /// from that year
    fn days_in_year(&self, date: &Self::DateInner) -> u16;
    /// Count the number of days in a given month, specified by providing a date
    /// from that year/month
    fn days_in_month(&self, date: &Self::DateInner) -> u8;
    /// Calculate the day of the week and return it
    fn day_of_week(&self, date: &Self::DateInner) -> types::Weekday {
        self.date_to_iso(date).day_of_week()
    }
    // fn week_of_year(&self, date: &Self::DateInner) -> u8;

    #[doc(hidden)] // unstable
    /// Add `offset` to `date`
    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>);

    #[doc(hidden)] // unstable
    /// Calculate `date2 - date` as a duration
    ///
    /// `calendar2` is the calendar object associated with `date2`. In case the specific calendar objects
    /// differ on data, the data for the first calendar is used, and `date2` may be converted if necessary.
    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        calendar2: &Self,
        largest_unit: DateDurationUnit,
        smallest_unit: DateDurationUnit,
    ) -> DateDuration<Self>;

    /// Obtain a name for the calendar for debug printing
    fn debug_name(&self) -> &'static str;
    // fn since(&self, from: &Date<Self>, to: &Date<Self>) -> Duration<Self>, Error;

    /// Information about the year
    fn year(&self, date: &Self::DateInner) -> types::YearInfo;

    /// Calculate if a date is in a leap year
    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool;

    /// The calendar-specific month represented by `date`
    fn month(&self, date: &Self::DateInner) -> types::MonthInfo;

    /// The calendar-specific day-of-month represented by `date`
    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth;

    /// Information of the day of the year
    fn day_of_year_info(&self, date: &Self::DateInner) -> types::DayOfYearInfo;

    /// The [`AnyCalendarKind`] corresponding to this calendar,
    /// if one exists. Implementors outside of `icu::calendar` should return `None`
    fn any_calendar_kind(&self) -> Option<AnyCalendarKind> {
        None
    }
}
