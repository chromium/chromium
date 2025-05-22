//! This module implements `YearMonth` and any directly related algorithms.

use alloc::string::String;
use core::{cmp::Ordering, str::FromStr};

use tinystr::TinyAsciiStr;

use crate::{
    iso::{year_month_within_limits, IsoDate, IsoDateTime, IsoTime},
    options::{
        ArithmeticOverflow, DifferenceOperation, DifferenceSettings, DisplayCalendar,
        ResolvedRoundingOptions, RoundingIncrement, Unit, UnitGroup,
    },
    parsers::{FormattableCalendar, FormattableDate, FormattableYearMonth},
    provider::NeverProvider,
    utils::pad_iso_year,
    Calendar, MonthCode, TemporalError, TemporalResult, TemporalUnwrap, TimeZone,
};

use super::{
    duration::normalized::NormalizedDurationRecord, Duration, PartialDate, PlainDate, PlainDateTime,
};

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
        op: DifferenceOperation,
        other: &Self,
        settings: DifferenceSettings,
    ) -> TemporalResult<Duration> {
        // 1. Set other to ? ToTemporalYearMonth(other).
        // 2. Let calendar be yearMonth.[[Calendar]].
        // 3. If CalendarEquals(calendar, other.[[Calendar]]) is false, throw a RangeError exception.
        if self.calendar().identifier() != other.calendar().identifier() {
            return Err(TemporalError::range()
                .with_message("Calendars for difference operation are not the same."));
        }

        // Check if weeks or days are disallowed in this operation
        if matches!(settings.largest_unit, Some(Unit::Week) | Some(Unit::Day))
            || matches!(settings.smallest_unit, Some(Unit::Week) | Some(Unit::Day))
        {
            return Err(TemporalError::range()
                .with_message("Weeks and days are not allowed in this operation."));
        }

        // 4. Let resolvedOptions be ? GetOptionsObject(options).
        // 5. Let settings be ? GetDifferenceSettings(operation, resolvedOptions, date, « week, day », month, year).
        let resolved = ResolvedRoundingOptions::from_diff_settings(
            settings,
            op,
            UnitGroup::Date,
            Unit::Year,
            Unit::Month,
        )?;

        // 6. If CompareISODate(yearMonth.[[ISODate]], other.[[ISODate]]) = 0, then
        if self.iso == other.iso {
            // a. Return ! CreateTemporalDuration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0).
            return Ok(Duration::default());
        }

        // 7. Let thisFields be ISODateToFields(calendar, yearMonth.[[ISODate]], year-month).
        // 8. Set thisFields.[[Day]] to 1.
        // 9. Let thisDate be ? CalendarDateFromFields(calendar, thisFields, constrain).
        // 10. Let otherFields be ISODateToFields(calendar, other.[[ISODate]], year-month).
        // 11. Set otherFields.[[Day]] to 1.
        // 12. Let otherDate be ? CalendarDateFromFields(calendar, otherFields, constrain).
        // 13. Let dateDifference be CalendarDateUntil(calendar, thisDate, otherDate, settings.[[LargestUnit]]).
        // 14. Let yearsMonthsDifference be ! AdjustDateDurationRecord(dateDifference, 0, 0).
        let result = self
            .calendar()
            .date_until(&self.iso, &other.iso, resolved.largest_unit)?;

        // 15. Let duration be CombineDateAndTimeDuration(yearsMonthsDifference, 0).
        let mut duration = NormalizedDurationRecord::from_date_duration(*result.date())?;

        // 16. If settings.[[SmallestUnit]] is not month or settings.[[RoundingIncrement]] ≠ 1, then
        if resolved.smallest_unit != Unit::Month || resolved.increment != RoundingIncrement::ONE {
            // a. Let isoDateTime be CombineISODateAndTimeRecord(thisDate, MidnightTimeRecord()).
            let iso_date_time = IsoDateTime::new_unchecked(self.iso, IsoTime::default());
            // b. Let isoDateTimeOther be CombineISODateAndTimeRecord(otherDate, MidnightTimeRecord()).
            let target_iso_date_time = IsoDateTime::new_unchecked(other.iso, IsoTime::default());
            // c. Let destEpochNs be GetUTCEpochNanoseconds(isoDateTimeOther).
            let dest_epoch_ns = target_iso_date_time.as_nanoseconds()?;
            // d. Set duration to ? RoundRelativeDuration(duration, destEpochNs, isoDateTime, unset, calendar, resolved.[[LargestUnit]], resolved.[[RoundingIncrement]], resolved.[[SmallestUnit]], resolved.[[RoundingMode]]).
            duration = duration.round_relative_duration(
                dest_epoch_ns.as_i128(),
                &PlainDateTime::new_unchecked(iso_date_time, self.calendar.clone()),
                Option::<(&TimeZone, &NeverProvider)>::None,
                resolved,
            )?;
        }

        // 17. Let result be ! TemporalDurationFromInternal(duration, day).
        let result = Duration::from_normalized(duration, Unit::Day)?;

        // 18. If operation is since, set result to CreateNegatedTemporalDuration(result).
        // 19. Return result.
        match op {
            DifferenceOperation::Since => Ok(result.negated()),
            DifferenceOperation::Until => Ok(result),
        }
    }
}

// ==== Public method implementations ====

impl PlainYearMonth {
    /// Creates a new `PlainYearMonth`, constraining any arguments that are invalid into a valid range.
    #[inline]
    pub fn new(
        year: i32,
        month: u8,
        reference_day: Option<u8>,
        calendar: Calendar,
    ) -> TemporalResult<Self> {
        Self::new_with_overflow(
            year,
            month,
            reference_day,
            calendar,
            ArithmeticOverflow::Constrain,
        )
    }

    /// Creates a new `PlainYearMonth`, rejecting any date that may be invalid.
    #[inline]
    pub fn try_new(
        year: i32,
        month: u8,
        reference_day: Option<u8>,
        calendar: Calendar,
    ) -> TemporalResult<Self> {
        Self::new_with_overflow(
            year,
            month,
            reference_day,
            calendar,
            ArithmeticOverflow::Reject,
        )
    }

    /// Creates a new `PlainYearMonth` with an ISO 8601 calendar, rejecting any date that may be invalid.
    #[inline]
    pub fn try_new_iso(year: i32, month: u8, reference_day: Option<u8>) -> TemporalResult<Self> {
        Self::try_new(year, month, reference_day, Calendar::default())
    }

    /// Creates a new `PlainYearMonth` with an ISO 8601 calendar, constraining any arguments
    /// that are invalid into a valid range.
    #[inline]
    pub fn new_iso(year: i32, month: u8, reference_day: Option<u8>) -> TemporalResult<Self> {
        Self::new(year, month, reference_day, Calendar::default())
    }

    /// Creates a new valid `YearMonth` with provided `ArithmeticOverflow` option.
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

    // Converts a UTF-8 encoded string into a `PlainYearMonth`.
    pub fn from_utf8(s: &[u8]) -> TemporalResult<Self> {
        let record = crate::parsers::parse_year_month(s)?;
        let calendar = record
            .calendar
            .map(Calendar::try_from_utf8)
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
        Self::from_utf8(s.as_bytes())
    }
}

#[cfg(test)]
mod tests {
    use core::str::FromStr;

    use super::PlainYearMonth;

    use tinystr::tinystr;

    use super::*;

    #[test]
    fn plain_year_month_since_until_diff_tests() {
        // Equal year-months
        {
            let earlier = PlainYearMonth::from_str("2024-03").unwrap();
            let later = PlainYearMonth::from_str("2024-03").unwrap();
            let settings = DifferenceSettings {
                smallest_unit: Some(Unit::Month),
                ..Default::default()
            };

            let until = earlier.until(&later, settings).unwrap();
            let since = earlier.since(&later, settings).unwrap();

            assert_eq!(until.days(), 0);
            assert_eq!(until.months(), 0);
            assert_eq!(until.years(), 0);

            assert_eq!(since.days(), 0);
            assert_eq!(since.months(), 0);
            assert_eq!(since.years(), 0);
        }

        // One month apart
        {
            let earlier = PlainYearMonth::from_str("2023-01").unwrap();
            let later = PlainYearMonth::from_str("2023-02").unwrap();
            let settings = DifferenceSettings {
                smallest_unit: Some(Unit::Month),
                ..Default::default()
            };

            let until = earlier.until(&later, settings).unwrap();
            let since = earlier.since(&later, settings).unwrap();

            assert_eq!(until.months(), 1);
            assert_eq!(until.years(), 0);

            assert_eq!(since.months(), -1);
            assert_eq!(since.years(), 0);
        }

        // Crossing year boundary
        {
            let earlier = PlainYearMonth::from_str("2022-11").unwrap();
            let later = PlainYearMonth::from_str("2023-02").unwrap();
            let settings = DifferenceSettings {
                smallest_unit: Some(Unit::Month),
                ..Default::default()
            };

            let until = earlier.until(&later, settings).unwrap();
            let since = earlier.since(&later, settings).unwrap();

            assert_eq!(until.months(), 3);
            assert_eq!(until.years(), 0);

            assert_eq!(since.months(), -3);
            assert_eq!(since.years(), 0);
        }

        // One year and one month
        {
            let earlier = PlainYearMonth::from_str("2002-05").unwrap();
            let later = PlainYearMonth::from_str("2003-06").unwrap();
            let settings = DifferenceSettings {
                smallest_unit: Some(Unit::Month),
                ..Default::default()
            };

            let until = earlier.until(&later, settings).unwrap();
            let since = earlier.since(&later, settings).unwrap();

            assert_eq!(until.years(), 1);
            assert_eq!(until.months(), 1);
            assert_eq!(until.days(), 0);

            assert_eq!(since.years(), -1);
            assert_eq!(since.months(), -1);
            assert_eq!(since.days(), 0);
        }

        // One year apart with unit = Year
        {
            let earlier = PlainYearMonth::from_str("2022-06").unwrap();
            let later = PlainYearMonth::from_str("2023-06").unwrap();
            let settings = DifferenceSettings {
                smallest_unit: Some(Unit::Year),
                ..Default::default()
            };

            let until = earlier.until(&later, settings).unwrap();
            let since = earlier.since(&later, settings).unwrap();

            assert_eq!(until.years(), 1);
            assert_eq!(until.months(), 0);

            assert_eq!(since.years(), -1);
            assert_eq!(since.months(), 0);
        }

        // Large year gap
        {
            let earlier = PlainYearMonth::from_str("1000-01").unwrap();
            let later = PlainYearMonth::from_str("2000-01").unwrap();
            let settings = DifferenceSettings {
                smallest_unit: Some(Unit::Year),
                ..Default::default()
            };

            let until = earlier.until(&later, settings).unwrap();
            let since = earlier.since(&later, settings).unwrap();

            assert_eq!(until.years(), 1000);
            assert_eq!(since.years(), -1000);
        }

        // Lower ISO limit
        {
            let earlier = PlainYearMonth::from_str("-271821-04").unwrap();
            let later = PlainYearMonth::from_str("-271820-04").unwrap();
            let settings = DifferenceSettings {
                smallest_unit: Some(Unit::Year),
                ..Default::default()
            };

            let until = earlier.until(&later, settings).unwrap();
            let since = earlier.since(&later, settings).unwrap();

            assert_eq!(until.years(), 1);
            assert_eq!(since.years(), -1);
        }
    }
    #[test]
    fn test_diff_with_different_calendars() {
        let ym1 = PlainYearMonth::new_with_overflow(
            2021,
            1,
            None,
            Calendar::from_str("islamic").unwrap(),
            ArithmeticOverflow::Reject,
        )
        .unwrap();

        let ym2 = PlainYearMonth::new_with_overflow(
            2021,
            1,
            None,
            Calendar::from_str("hebrew").unwrap(),
            ArithmeticOverflow::Reject,
        )
        .unwrap();

        let settings = DifferenceSettings {
            smallest_unit: Some(Unit::Month),
            ..Default::default()
        };

        let diff = ym1.until(&ym2, settings);
        assert!(
            diff.is_err(),
            "Expected an error when comparing dates from different calendars"
        );
    }
    #[test]
    fn test_diff_setting() {
        let ym1 = PlainYearMonth::from_str("2021-01").unwrap();
        let ym2 = PlainYearMonth::from_str("2023-02").unwrap();

        let settings = DifferenceSettings {
            smallest_unit: Some(Unit::Month),
            increment: Some(RoundingIncrement::ONE),
            ..Default::default()
        };

        let diff = ym1.until(&ym2, settings).unwrap();
        assert_eq!(diff.months(), 1);
        assert_eq!(diff.years(), 2);
    }
    #[test]
    fn test_diff_with_smallest_unit_year() {
        let ym1 = PlainYearMonth::from_str("2021-01").unwrap();
        let ym2 = PlainYearMonth::from_str("2023-02").unwrap();

        let settings = DifferenceSettings {
            smallest_unit: Some(Unit::Year),
            ..Default::default()
        };

        let diff = ym1.until(&ym2, settings).unwrap();
        assert_eq!(diff.years(), 2); // Rounded to the nearest year
        assert_eq!(diff.months(), 0); // Months are ignored
    }

    #[test]
    fn test_diff_with_smallest_unit_day() {
        let ym1 = PlainYearMonth::from_str("2021-01").unwrap();
        let ym2 = PlainYearMonth::from_str("2023-02").unwrap();

        let settings = DifferenceSettings {
            smallest_unit: Some(Unit::Day),
            ..Default::default()
        };

        let diff = ym1.until(&ym2, settings);
        assert!(
            diff.is_err(),
            "Expected an error when smallest_unit is set to Day"
        );
    }

    #[test]
    fn test_diff_with_smallest_unit_week() {
        let ym1 = PlainYearMonth::from_str("2021-01").unwrap();
        let ym2 = PlainYearMonth::from_str("2023-02").unwrap();

        let settings = DifferenceSettings {
            smallest_unit: Some(Unit::Week),
            ..Default::default()
        };

        let diff = ym1.until(&ym2, settings);
        assert!(
            diff.is_err(),
            "Expected an error when smallest_unit is set to Week"
        );
    }

    #[test]
    fn test_diff_with_no_rounding_increment() {
        let ym1 = PlainYearMonth::from_str("2021-01").unwrap();
        let ym2 = PlainYearMonth::from_str("2023-02").unwrap();

        let settings = DifferenceSettings {
            smallest_unit: Some(Unit::Month),
            increment: None, // No rounding increment
            ..Default::default()
        };

        let diff = ym1.until(&ym2, settings).unwrap();
        assert_eq!(diff.months(), 1); // Exact difference in months
        assert_eq!(diff.years(), 2); // Exact difference in years
    }

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
