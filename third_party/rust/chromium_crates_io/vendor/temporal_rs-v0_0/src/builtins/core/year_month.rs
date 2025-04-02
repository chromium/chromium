//! This module implements `YearMonth` and any directly related algorithms.

use alloc::string::String;
use core::{cmp::Ordering, str::FromStr};

use tinystr::TinyAsciiStr;

use crate::{
    iso::{year_month_within_limits, IsoDate},
    options::{ArithmeticOverflow, DifferenceOperation, DifferenceSettings, DisplayCalendar},
    parsers::{FormattableCalendar, FormattableDate, FormattableYearMonth},
    utils::pad_iso_year,
    Calendar, MonthCode, TemporalError, TemporalResult, TemporalUnwrap,
};

use super::{Duration, PartialDate, PlainDate};

/// The native Rust implementation of `Temporal.YearMonth`.
#[non_exhaustive]
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct PlainYearMonth {
    pub(crate) iso: IsoDate,
    calendar: Calendar,
}

impl core::fmt::Display for PlainYearMonth {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str(&self.to_ixdtf_string(DisplayCalendar::Auto))
    }
}

impl PlainYearMonth {
    /// Creates an unvalidated `YearMonth`.
    #[inline]
    #[must_use]
    pub(crate) fn new_unchecked(iso: IsoDate, calendar: Calendar) -> Self {
        Self { iso, calendar }
    }

    /// Internal addition method for adding `Duration` to a `PlainYearMonth`
    pub(crate) fn add_or_subtract_duration(
        &self,
        duration: &Duration,
        overflow: ArithmeticOverflow,
    ) -> TemporalResult<Self> {
        // Potential TODO: update to current Temporal specification
        let partial = PartialDate::try_from_year_month(self)?;

        let mut intermediate_date = self.calendar().date_from_partial(&partial, overflow)?;

        intermediate_date = intermediate_date.add_date(duration, Some(overflow))?;

        let result_fields = PartialDate::default().with_fallback_date(&intermediate_date)?;

        self.calendar()
            .year_month_from_partial(&result_fields, overflow)
    }

    /// The internal difference operation of `PlainYearMonth`.
    pub(crate) fn diff(
        &self,
        _op: DifferenceOperation,
        _other: &Self,
        _settings: DifferenceSettings,
    ) -> TemporalResult<Duration> {
        // TODO: implement
        Err(TemporalError::general("Not yet implemented"))
    }
}

// ==== Public method implementations ====

impl PlainYearMonth {
    /// Creates a new valid `YearMonth`.
    #[inline]
    pub fn new_with_overflow(
        year: i32,
        month: u8,
        reference_day: Option<u8>,
        calendar: Calendar,
        overflow: ArithmeticOverflow,
    ) -> TemporalResult<Self> {
        let day = reference_day.unwrap_or(1);
        let iso = IsoDate::regulate(year, month, day, overflow)?;
        if !year_month_within_limits(iso.year, iso.month) {
            return Err(TemporalError::range().with_message("Exceeded valid range."));
        }
        Ok(Self::new_unchecked(iso, calendar))
    }

    /// Create a `PlainYearMonth` from a `PartialDate`
    pub fn from_partial(
        partial: PartialDate,
        overflow: ArithmeticOverflow,
    ) -> TemporalResult<Self> {
        partial.calendar.year_month_from_partial(&partial, overflow)
    }

    /// Returns the iso year value for this `YearMonth`.
    #[inline]
    #[must_use]
    pub fn iso_year(&self) -> i32 {
        self.iso.year
    }

    /// Returns the padded ISO year string
    #[inline]
    #[must_use]
    pub fn padded_iso_year_string(&self) -> String {
        pad_iso_year(self.iso.year)
    }

    /// Returns the iso month value for this `YearMonth`.
    #[inline]
    #[must_use]
    pub fn iso_month(&self) -> u8 {
        self.iso.month
    }

    /// Returns the calendar era of the current `PlainYearMonth`
    pub fn era(&self) -> Option<TinyAsciiStr<16>> {
        self.calendar().era(&self.iso)
    }

    /// Returns the calendar era year of the current `PlainYearMonth`
    pub fn era_year(&self) -> Option<i32> {
        self.calendar().era_year(&self.iso)
    }

    /// Returns the calendar year of the current `PlainYearMonth`
    pub fn year(&self) -> i32 {
        self.calendar().year(&self.iso)
    }

    /// Returns the calendar month of the current `PlainYearMonth`
    pub fn month(&self) -> u8 {
        self.calendar().month(&self.iso)
    }

    /// Returns the calendar month code of the current `PlainYearMonth`
    pub fn month_code(&self) -> MonthCode {
        self.calendar().month_code(&self.iso)
    }

    /// Returns the days in the calendar year of the current `PlainYearMonth`.
    pub fn days_in_year(&self) -> u16 {
        self.calendar().days_in_year(&self.iso)
    }

    /// Returns the days in the calendar month of the current `PlainYearMonth`.
    pub fn days_in_month(&self) -> u16 {
        self.calendar().days_in_month(&self.iso)
    }

    /// Returns the months in the calendar year of the current `PlainYearMonth`.
    pub fn months_in_year(&self) -> u16 {
        self.calendar().months_in_year(&self.iso)
    }

    #[inline]
    #[must_use]
    /// Returns a boolean representing whether the current `PlainYearMonth` is in a leap year.
    pub fn in_leap_year(&self) -> bool {
        self.calendar().in_leap_year(&self.iso)
    }
}

impl PlainYearMonth {
    /// Returns the Calendar value.
    #[inline]
    #[must_use]
    pub fn calendar(&self) -> &Calendar {
        &self.calendar
    }

    /// Returns the string identifier for the current calendar used.
    #[inline]
    #[must_use]
    pub fn calendar_id(&self) -> &'static str {
        self.calendar.identifier()
    }

    /// Creates a `PlainYearMonth` using the fields provided from a [`PartialDate`]
    pub fn with(
        &self,
        partial: PartialDate,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        // 1. Let yearMonth be the this value.
        // 2. Perform ? RequireInternalSlot(yearMonth, [[InitializedTemporalYearMonth]]).
        // 3. If ? IsPartialTemporalObject(temporalYearMonthLike) is false, throw a TypeError exception.
        // 4. Let calendar be yearMonth.[[Calendar]].
        // 5. Let fields be ISODateToFields(calendar, yearMonth.[[ISODate]], year-month).
        // 6. Let partialYearMonth be ? PrepareCalendarFields(calendar, temporalYearMonthLike, « year, month, month-code », « », partial).
        // 7. Set fields to CalendarMergeFields(calendar, fields, partialYearMonth).
        // 8. Let resolvedOptions be ? GetOptionsObject(options).
        // 9. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
        // 10. Let isoDate be ? CalendarYearMonthFromFields(calendar, fields, overflow).
        // 11. Return ! CreateTemporalYearMonth(isoDate, calendar).
        self.calendar.year_month_from_partial(
            &partial.with_fallback_year_month(self)?,
            overflow.unwrap_or(ArithmeticOverflow::Constrain),
        )
    }

    /// Compares one `PlainYearMonth` to another `PlainYearMonth` using their
    /// `IsoDate` representation.
    ///
    /// # Note on Ordering.
    ///
    /// `temporal_rs` does not implement `PartialOrd`/`Ord` as `PlainYearMonth` does
    /// not fulfill all the conditions required to implement the traits. However,
    /// it is possible to compare `PlainDate`'s as their `IsoDate` representation.
    #[inline]
    #[must_use]
    pub fn compare_iso(&self, other: &Self) -> Ordering {
        self.iso.cmp(&other.iso)
    }

    /// Adds a [`Duration`] from the current `PlainYearMonth`.
    #[inline]
    pub fn add(&self, duration: &Duration, overflow: ArithmeticOverflow) -> TemporalResult<Self> {
        self.add_or_subtract_duration(duration, overflow)
    }

    /// Subtracts a [`Duration`] from the current `PlainYearMonth`.
    #[inline]
    pub fn subtract(
        &self,
        duration: &Duration,
        overflow: ArithmeticOverflow,
    ) -> TemporalResult<Self> {
        self.add_or_subtract_duration(&duration.negated(), overflow)
    }

    /// Returns a `Duration` representing the period of time from this `PlainYearMonth` until the other `PlainYearMonth`.
    #[inline]
    pub fn until(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff(DifferenceOperation::Until, other, settings)
    }

    /// Returns a `Duration` representing the period of time from this `PlainYearMonth` since the other `PlainYearMonth`.
    #[inline]
    pub fn since(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff(DifferenceOperation::Since, other, settings)
    }

    pub fn to_plain_date(&self) -> TemporalResult<PlainDate> {
        Err(TemporalError::general("Not yet iimplemented."))
    }

    /// Returns a RFC9557 IXDTF string for the current `PlainYearMonth`
    #[inline]
    pub fn to_ixdtf_string(&self, display_calendar: DisplayCalendar) -> String {
        let ixdtf = FormattableYearMonth {
            date: FormattableDate(self.iso_year(), self.iso_month(), self.iso.day),
            calendar: FormattableCalendar {
                show: display_calendar,
                calendar: self.calendar().identifier(),
            },
        };
        ixdtf.to_string()
    }
}

impl FromStr for PlainYearMonth {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let record = crate::parsers::parse_year_month(s)?;
        let calendar = record
            .calendar
            .map(Calendar::from_utf8)
            .transpose()?
            .unwrap_or_default();

        // ParseISODateTime
        // Step 4.a.ii.3
        // If goal is TemporalMonthDayString or TemporalYearMonthString, calendar is
        // not empty, and the ASCII-lowercase of calendar is not "iso8601", throw a
        // RangeError exception.
        if !calendar.is_iso() {
            return Err(TemporalError::range().with_message("non-ISO calendar not supported."));
        }

        let date = record.date.temporal_unwrap()?;

        // The below steps are from `ToTemporalYearMonth`
        // 10. Let isoDate be CreateISODateRecord(result.[[Year]], result.[[Month]], result.[[Day]]).
        let iso = IsoDate::new_unchecked(date.year, date.month, date.day);

        // 11. If ISOYearMonthWithinLimits(isoDate) is false, throw a RangeError exception.
        if !year_month_within_limits(iso.year, iso.month) {
            return Err(TemporalError::range().with_message("Exceeded valid range."));
        }

        let intermediate = Self::new_unchecked(iso, calendar);
        // 12. Set result to ISODateToFields(calendar, isoDate, year-month).
        let partial = PartialDate::try_from_year_month(&intermediate)?;
        // 13. NOTE: The following operation is called with constrain regardless of the
        // value of overflow, in order for the calendar to store a canonical value in the
        // [[Day]] field of the [[ISODate]] internal slot of the result.
        // 14. Set isoDate to ? CalendarYearMonthFromFields(calendar, result, constrain).
        // 15. Return ! CreateTemporalYearMonth(isoDate, calendar).
        PlainYearMonth::from_partial(partial, ArithmeticOverflow::Constrain)
    }
}

#[cfg(test)]
mod tests {
    use core::str::FromStr;

    use super::PlainYearMonth;

    use tinystr::tinystr;

    use super::*;

    #[test]
    fn test_plain_year_month_with() {
        let base = PlainYearMonth::new_with_overflow(
            2025,
            3,
            None,
            Calendar::default(),
            ArithmeticOverflow::Reject,
        )
        .unwrap();

        // Year
        let partial = PartialDate {
            year: Some(2001),
            ..Default::default()
        };

        let with_year = base.with(partial, None).unwrap();
        assert_eq!(with_year.iso_year(), 2001); // year is changed
        assert_eq!(with_year.iso_month(), 3); // month is not changed
        assert_eq!(with_year.month_code(), MonthCode::from_str("M03").unwrap()); // assert month code has been initialized correctly

        // Month
        let partial = PartialDate {
            month: Some(2),
            ..Default::default()
        };
        let with_month = base.with(partial, None).unwrap();
        assert_eq!(with_month.iso_year(), 2025); // year is not changed
        assert_eq!(with_month.iso_month(), 2); // month is changed
        assert_eq!(with_month.month_code(), MonthCode::from_str("M02").unwrap()); // assert month code has changed as well as month

        // Month Code
        let partial = PartialDate {
            month_code: Some(MonthCode(tinystr!(4, "M05"))), // change month to May (5)
            ..Default::default()
        };
        let with_month_code = base.with(partial, None).unwrap();
        assert_eq!(with_month_code.iso_year(), 2025); // year is not changed
        assert_eq!(
            with_month_code.month_code(),
            MonthCode::from_str("M05").unwrap()
        ); // assert month code has changed
        assert_eq!(with_month_code.iso_month(), 5); // month is changed as well

        // Day
        let partial = PartialDate {
            day: Some(15),
            ..Default::default()
        };
        let with_day = base.with(partial, None).unwrap();
        assert_eq!(with_day.iso_year(), 2025); // year is not changed
        assert_eq!(with_day.iso_month(), 3); // month is not changed
        assert_eq!(with_day.iso.day, 1); // day is ignored

        // All
        let partial = PartialDate {
            year: Some(2001),
            month: Some(2),
            day: Some(15),
            ..Default::default()
        };
        let with_all = base.with(partial, None).unwrap();
        assert_eq!(with_all.iso_year(), 2001); // year is changed
        assert_eq!(with_all.iso_month(), 2); // month is changed
        assert_eq!(with_all.iso.day, 1); // day is ignored
    }

    #[test]
    fn basic_from_str() {
        let valid_strings = [
            "-271821-04",
            "-271821-04-01",
            "-271821-04-01T00:00",
            "+275760-09",
            "+275760-09-30",
            "+275760-09-30T23:59:59.999999999",
        ];

        for valid_case in valid_strings {
            let ym = PlainYearMonth::from_str(valid_case);
            assert!(ym.is_ok());
        }
    }

    #[test]
    fn invalid_from_str() {
        let invalid_strings = [
            "-271821-03-31",
            "-271821-03-31T23:59:59.999999999",
            "+275760-10",
            "+275760-10-01",
            "+275760-10-01T00:00",
            "1976-11[u-ca=hebrew]",
        ];

        for invalid_case in invalid_strings {
            let err = PlainYearMonth::from_str(invalid_case);
            assert!(err.is_err());
        }

        let invalid_strings = ["2019-10-01T09:00:00Z", "2019-10-01T09:00:00Z[UTC]"];

        for invalid_case in invalid_strings {
            let err = PlainYearMonth::from_str(invalid_case);
            assert!(err.is_err());
        }
    }
}
