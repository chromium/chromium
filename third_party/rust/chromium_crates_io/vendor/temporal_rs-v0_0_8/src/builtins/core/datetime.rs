//! This module implements `DateTime` any directly related algorithms.

use super::{
    duration::normalized::{NormalizedDurationRecord, NormalizedTimeDuration},
    Duration, PartialDate, PartialTime, PlainDate, PlainTime, ZonedDateTime,
};
use crate::{
    builtins::core::{calendar::Calendar, Instant},
    iso::{IsoDate, IsoDateTime, IsoTime},
    options::{
        ArithmeticOverflow, DifferenceOperation, DifferenceSettings, Disambiguation,
        DisplayCalendar, ResolvedRoundingOptions, RoundingOptions, ToStringRoundingOptions, Unit,
        UnitGroup,
    },
    parsers::{parse_date_time, IxdtfStringBuilder},
    primitive::FiniteF64,
    provider::{NeverProvider, TimeZoneProvider},
    temporal_assert, MonthCode, TemporalError, TemporalResult, TemporalUnwrap, TimeZone,
};
use alloc::string::String;
use core::{cmp::Ordering, str::FromStr};
use tinystr::TinyAsciiStr;

/// A partial PlainDateTime record
#[derive(Debug, Default, Clone)]
pub struct PartialDateTime {
    /// The `PartialDate` portion of a `PartialDateTime`
    pub date: PartialDate,
    /// The `PartialTime` portion of a `PartialDateTime`
    pub time: PartialTime,
}

impl PartialDateTime {
    pub fn is_empty(&self) -> bool {
        self.date.is_empty() && self.time.is_empty()
    }

    pub const fn new() -> Self {
        Self {
            date: PartialDate::new(),
            time: PartialTime::new(),
        }
    }

    pub const fn with_partial_date(mut self, partial_date: PartialDate) -> Self {
        self.date = partial_date;
        self
    }

    pub const fn with_partial_time(mut self, partial_time: PartialTime) -> Self {
        self.time = partial_time;
        self
    }
}

// TODO: Example doctest
/// The native Rust implementation of a Temporal `PlainDateTime`.
///
/// The `PlainDateTime` represents a date and time without a
/// time zone. The fundemental represenation of a `PlainDateTime`
/// is it's internal ISO date and time fields and a calendar.
///
/// ## Reference
///
/// For more information, see the [MDN documentation][mdn-datetime].
///
/// [mdn-datetime]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Temporal/PlainDateTime
#[non_exhaustive]
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct PlainDateTime {
    pub(crate) iso: IsoDateTime,
    calendar: Calendar,
}

impl core::fmt::Display for PlainDateTime {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        let ixdtf_str = self
            .to_ixdtf_string(ToStringRoundingOptions::default(), DisplayCalendar::Auto)
            .expect("ixdtf default configuration should not fail.");
        f.write_str(&ixdtf_str)
    }
}

// ==== Private PlainDateTime API ====

impl PlainDateTime {
    /// Creates a new unchecked `DateTime`.
    #[inline]
    #[must_use]
    pub(crate) fn new_unchecked(iso: IsoDateTime, calendar: Calendar) -> Self {
        Self { iso, calendar }
    }

    // TODO: Potentially deprecate and remove.
    /// Create a new `DateTime` from an `Instant`.
    #[allow(unused)]
    #[inline]
    pub(crate) fn from_instant(
        instant: &Instant,
        offset: i64,
        calendar: Calendar,
    ) -> TemporalResult<Self> {
        let iso = IsoDateTime::from_epoch_nanos(instant.epoch_nanoseconds(), offset)?;
        Ok(Self { iso, calendar })
    }

    // 5.5.14 AddDurationToOrSubtractDurationFromPlainDateTime ( operation, dateTime, temporalDurationLike, options )
    fn add_or_subtract_duration(
        &self,
        duration: &Duration,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        // SKIP: 1, 2, 3, 4
        // 1. If operation is subtract, let sign be -1. Otherwise, let sign be 1.
        // 2. Let duration be ? ToTemporalDurationRecord(temporalDurationLike).
        // 3. Set options to ? GetOptionsObject(options).
        // 4. Let calendarRec be ? CreateCalendarMethodsRecord(dateTime.[[Calendar]], « date-add »).

        // 5. Let norm be NormalizeTimeDuration(sign × duration.[[Hours]], sign × duration.[[Minutes]], sign × duration.[[Seconds]], sign × duration.[[Milliseconds]], sign × duration.[[Microseconds]], sign × duration.[[Nanoseconds]]).
        let norm = NormalizedTimeDuration::from_time_duration(duration.time());

        // TODO: validate Constrain is default with all the recent changes.
        // 6. Let result be ? AddDateTime(dateTime.[[ISOYear]], dateTime.[[ISOMonth]], dateTime.[[ISODay]], dateTime.[[ISOHour]], dateTime.[[ISOMinute]], dateTime.[[ISOSecond]], dateTime.[[ISOMillisecond]], dateTime.[[ISOMicrosecond]], dateTime.[[ISONanosecond]], calendarRec, sign × duration.[[Years]], sign × duration.[[Months]], sign × duration.[[Weeks]], sign × duration.[[Days]], norm, options).
        let result =
            self.iso
                .add_date_duration(self.calendar().clone(), duration.date(), norm, overflow)?;

        // 7. Assert: IsValidISODate(result.[[Year]], result.[[Month]], result.[[Day]]) is true.
        // 8. Assert: IsValidTime(result.[[Hour]], result.[[Minute]], result.[[Second]], result.[[Millisecond]],
        // result.[[Microsecond]], result.[[Nanosecond]]) is true.
        temporal_assert!(
            result.is_within_limits(),
            "Assertion failed: the below datetime is not within valid limits:\n{:?}",
            result
        );

        // 9. Return ? CreateTemporalDateTime(result.[[Year]], result.[[Month]], result.[[Day]], result.[[Hour]],
        // result.[[Minute]], result.[[Second]], result.[[Millisecond]], result.[[Microsecond]],
        // result.[[Nanosecond]], dateTime.[[Calendar]]).
        Ok(Self::new_unchecked(result, self.calendar.clone()))
    }

    /// Difference two `DateTime`s together.
    pub(crate) fn diff(
        &self,
        op: DifferenceOperation,
        other: &Self,
        settings: DifferenceSettings,
    ) -> TemporalResult<Duration> {
        // 3. If ? CalendarEquals(dateTime.[[Calendar]], other.[[Calendar]]) is false, throw a RangeError exception.
        if self.calendar != other.calendar {
            return Err(TemporalError::range()
                .with_message("Calendar must be the same when diffing two PlainDateTimes"));
        }

        // 5. Let settings be ? GetDifferenceSettings(operation, resolvedOptions, datetime, « », "nanosecond", "day").
        let options = ResolvedRoundingOptions::from_diff_settings(
            settings,
            op,
            UnitGroup::DateTime,
            Unit::Day,
            Unit::Nanosecond,
        )?;

        // Step 7-8 combined.
        if self.iso == other.iso {
            return Ok(Duration::default());
        }

        // Step 10-11.
        let norm_record = self.diff_dt_with_rounding(other, options)?;

        let result = Duration::from_normalized(norm_record, options.largest_unit)?;

        // Step 12
        match op {
            DifferenceOperation::Until => Ok(result),
            DifferenceOperation::Since => Ok(result.negated()),
        }
    }

    // TODO: Figure out whether to handle resolvedOptions
    // 5.5.12 DifferencePlainDateTimeWithRounding ( y1, mon1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, d2, h2, min2, s2, ms2,
    // mus2, ns2, calendarRec, largestUnit, roundingIncrement, smallestUnit, roundingMode, resolvedOptions )
    pub(crate) fn diff_dt_with_rounding(
        &self,
        other: &Self,
        options: ResolvedRoundingOptions,
    ) -> TemporalResult<NormalizedDurationRecord> {
        // 1. Assert: IsValidISODate(y1, mon1, d1) is true.
        // 2. Assert: IsValidISODate(y2, mon2, d2) is true.
        // 3. If CompareISODateTime(y1, mon1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, d2, h2, min2, s2, ms2, mus2, ns2) = 0, then
        if matches!(self.iso.cmp(&other.iso), Ordering::Equal) {
            // a. Let durationRecord be CreateDurationRecord(0, 0, 0, 0, 0, 0, 0, 0, 0, 0).
            // b. Return the Record { [[DurationRecord]]: durationRecord, [[Total]]: 0 }.
            return Ok(NormalizedDurationRecord::default());
        }
        // 3. Let diff be DifferenceISODateTime(isoDateTime1, isoDateTime2, calendar, largestUnit).
        let diff = self
            .iso
            .diff(&other.iso, &self.calendar, options.largest_unit)?;
        // 4. If smallestUnit is nanosecond and roundingIncrement = 1, return diff.
        if options.smallest_unit == Unit::Nanosecond && options.increment.get() == 1 {
            return Ok(diff);
        }

        // 5. Let destEpochNs be GetUTCEpochNanoseconds(isoDateTime2).
        let dest_epoch_ns = other.iso.as_nanoseconds()?;
        // 6. Return ? RoundRelativeDuration(diff, destEpochNs, isoDateTime1, unset, calendar, largestUnit, roundingIncrement, smallestUnit, roundingMode).
        diff.round_relative_duration(
            dest_epoch_ns.0,
            self,
            Option::<(&TimeZone, &NeverProvider)>::None,
            options,
        )
    }

    // 5.5.14 DifferencePlainDateTimeWithTotal ( isoDateTime1, isoDateTime2, calendar, unit )
    pub(crate) fn diff_dt_with_total(&self, other: &Self, unit: Unit) -> TemporalResult<FiniteF64> {
        // 1. If CompareISODateTime(isoDateTime1, isoDateTime2) = 0, then
        //    a. Return 0.
        if matches!(self.iso.cmp(&other.iso), Ordering::Equal) {
            return FiniteF64::try_from(0.0);
        }
        // 2. If ISODateTimeWithinLimits(isoDateTime1) is false or ISODateTimeWithinLimits(isoDateTime2) is false, throw a RangeError exception.
        if !self.iso.is_within_limits() || !other.iso.is_within_limits() {
            return Err(TemporalError::range().with_message("DateTime is not within valid limits."));
        }
        // 3. Let diff be DifferenceISODateTime(isoDateTime1, isoDateTime2, calendar, unit).
        let diff = self.iso.diff(&other.iso, &self.calendar, unit)?;
        // 4. If unit is nanosecond, return diff.[[Time]].
        if unit == Unit::Nanosecond {
            return FiniteF64::try_from(diff.normalized_time_duration().0);
        }
        // 5. Let destEpochNs be GetUTCEpochNanoseconds(isoDateTime2).
        let dest_epoch_ns = other.iso.as_nanoseconds()?;
        // 6. Return ? TotalRelativeDuration(diff, destEpochNs, isoDateTime1, unset, calendar, unit).
        diff.total_relative_duration(
            dest_epoch_ns.0,
            self,
            Option::<(&TimeZone, &NeverProvider)>::None,
            unit,
        )
    }
}

// ==== Public PlainDateTime API ====

impl PlainDateTime {
    /// Creates a new `DateTime`, constraining any arguments that are invalid
    /// into a valid range.
    #[inline]
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
        calendar: Calendar,
    ) -> TemporalResult<Self> {
        Self::new_with_overflow(
            year,
            month,
            day,
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            calendar,
            ArithmeticOverflow::Constrain,
        )
    }

    /// Creates a new `DateTime` with an ISO 8601 calendar, constraining any
    /// arguments that are invalid into a valid range.
    #[inline]
    #[allow(clippy::too_many_arguments)]
    pub fn new_iso(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
    ) -> TemporalResult<Self> {
        Self::new(
            year,
            month,
            day,
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            Calendar::default(),
        )
    }

    /// Creates a new `DateTime`, rejecting any arguments that are not in a valid range.
    #[inline]
    #[allow(clippy::too_many_arguments)]
    pub fn try_new(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
        calendar: Calendar,
    ) -> TemporalResult<Self> {
        Self::new_with_overflow(
            year,
            month,
            day,
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            calendar,
            ArithmeticOverflow::Reject,
        )
    }

    /// Creates a new `DateTime` with an ISO 8601 calendar, rejecting any arguments that are not in a valid range.
    #[inline]
    #[allow(clippy::too_many_arguments)]
    pub fn try_new_iso(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
    ) -> TemporalResult<Self> {
        Self::try_new(
            year,
            month,
            day,
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            Calendar::default(),
        )
    }

    /// Creates a new `DateTime` with the provided [`ArithmeticOverflow`] option.
    #[inline]
    #[allow(clippy::too_many_arguments)]
    pub fn new_with_overflow(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
        calendar: Calendar,
        overflow: ArithmeticOverflow,
    ) -> TemporalResult<Self> {
        let iso_date = IsoDate::new_with_overflow(year, month, day, overflow)?;
        let iso_time = IsoTime::new(
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            overflow,
        )?;
        Ok(Self::new_unchecked(
            IsoDateTime::new(iso_date, iso_time)?,
            calendar,
        ))
    }

    /// Create a `DateTime` from a `Date` and a `Time`.
    pub fn from_date_and_time(date: PlainDate, time: PlainTime) -> TemporalResult<Self> {
        Ok(Self::new_unchecked(
            IsoDateTime::new(date.iso, time.iso)?,
            date.calendar().clone(),
        ))
    }

    /// Creates a `DateTime` from a `PartialDateTime`.
    ///
    /// ```rust
    /// use temporal_rs::{PlainDateTime, partial::{PartialDateTime, PartialTime, PartialDate}};
    ///
    /// let date = PartialDate {
    ///     year: Some(2000),
    ///     month: Some(13),
    ///     day: Some(2),
    ///     ..Default::default()
    /// };
    ///
    /// let time = PartialTime {
    ///     hour: Some(4),
    ///     minute: Some(25),
    ///     ..Default::default()
    /// };
    ///
    /// let partial = PartialDateTime { date, time };
    ///
    /// let date = PlainDateTime::from_partial(partial, None).unwrap();
    ///
    /// assert_eq!(date.year(), 2000);
    /// assert_eq!(date.month(), 12);
    /// assert_eq!(date.day(), 2);
    /// assert_eq!(date.calendar().identifier(), "iso8601");
    /// assert_eq!(date.hour(), 4);
    /// assert_eq!(date.minute(), 25);
    /// assert_eq!(date.second(), 0);
    /// assert_eq!(date.millisecond(), 0);
    ///
    /// ```
    pub fn from_partial(
        partial: PartialDateTime,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        if partial.is_empty() {
            return Err(TemporalError::r#type().with_message("PartialDateTime cannot be empty."));
        }
        let date = PlainDate::from_partial(partial.date, overflow)?;
        let iso_time = IsoTime::default().with(partial.time, overflow.unwrap_or_default())?;
        Self::from_date_and_time(date, PlainTime::new_unchecked(iso_time))
    }

    // Converts a UTF-8 encoded string into a `PlainDateTime`.
    pub fn from_utf8(s: &[u8]) -> TemporalResult<Self> {
        let parse_record = parse_date_time(s)?;

        let calendar = parse_record
            .calendar
            .map(Calendar::try_from_utf8)
            .transpose()?
            .unwrap_or_default();

        let time = parse_record
            .time
            .map(IsoTime::from_time_record)
            .transpose()?
            .unwrap_or_default();

        let parsed_date = parse_record.date.temporal_unwrap()?;

        let date = IsoDate::new_with_overflow(
            parsed_date.year,
            parsed_date.month,
            parsed_date.day,
            ArithmeticOverflow::Reject,
        )?;

        Ok(Self::new_unchecked(IsoDateTime::new(date, time)?, calendar))
    }

    /// Creates a new `DateTime` with the fields of a `PartialDateTime`.
    ///
    /// ```rust
    /// use temporal_rs::{Calendar, PlainDateTime, partial::{PartialDateTime, PartialTime, PartialDate}};
    ///
    /// let initial = PlainDateTime::try_new(2000, 12, 2, 0,0,0,0,0,0, Calendar::default()).unwrap();
    ///
    /// let date = PartialDate {
    ///     month: Some(5),
    ///     ..Default::default()
    /// };
    ///
    /// let time = PartialTime {
    ///     hour: Some(4),
    ///     second: Some(30),
    ///     ..Default::default()
    /// };
    ///
    /// let partial = PartialDateTime { date, time };
    ///
    /// let date = initial.with(partial, None).unwrap();
    ///
    /// assert_eq!(date.year(), 2000);
    /// assert_eq!(date.month(), 5);
    /// assert_eq!(date.day(), 2);
    /// assert_eq!(date.calendar().identifier(), "iso8601");
    /// assert_eq!(date.hour(), 4);
    /// assert_eq!(date.minute(), 0);
    /// assert_eq!(date.second(), 30);
    /// assert_eq!(date.millisecond(), 0);
    ///
    /// ```
    #[inline]
    pub fn with(
        &self,
        partial_datetime: PartialDateTime,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        if partial_datetime.date.is_empty() && partial_datetime.time.is_empty() {
            return Err(
                TemporalError::r#type().with_message("A PartialDateTime must have a valid field.")
            );
        }

        let result_date = self.calendar.date_from_partial(
            &partial_datetime.date.with_fallback_datetime(self)?,
            overflow.unwrap_or(ArithmeticOverflow::Constrain),
        )?;

        // Determine the `Time` based off the partial values.
        let time = self.iso.time.with(
            partial_datetime.time,
            overflow.unwrap_or(ArithmeticOverflow::Constrain),
        )?;

        let iso_datetime = IsoDateTime::new(result_date.iso, time)?;

        Ok(Self::new_unchecked(iso_datetime, self.calendar().clone()))
    }

    /// Creates a new `DateTime` from the current `DateTime` and the provided `Time`.
    pub fn with_time(&self, time: PlainTime) -> TemporalResult<Self> {
        Self::try_new(
            self.iso_year(),
            self.iso_month(),
            self.iso_day(),
            time.hour(),
            time.minute(),
            time.second(),
            time.millisecond(),
            time.microsecond(),
            time.nanosecond(),
            self.calendar.clone(),
        )
    }

    /// Creates a new `DateTime` from the current `DateTime` and a provided `Calendar`.
    pub fn with_calendar(&self, calendar: Calendar) -> TemporalResult<Self> {
        Self::try_new(
            self.iso_year(),
            self.iso_month(),
            self.iso_day(),
            self.hour(),
            self.minute(),
            self.second(),
            self.millisecond(),
            self.microsecond(),
            self.nanosecond(),
            calendar,
        )
    }

    /// Returns this `Date`'s ISO year value.
    #[inline]
    #[must_use]
    pub const fn iso_year(&self) -> i32 {
        self.iso.date.year
    }

    /// Returns this `Date`'s ISO month value.
    #[inline]
    #[must_use]
    pub const fn iso_month(&self) -> u8 {
        self.iso.date.month
    }

    /// Returns this `Date`'s ISO day value.
    #[inline]
    #[must_use]
    pub const fn iso_day(&self) -> u8 {
        self.iso.date.day
    }

    /// Returns the hour value
    #[inline]
    #[must_use]
    pub fn hour(&self) -> u8 {
        self.iso.time.hour
    }

    /// Returns the minute value
    #[inline]
    #[must_use]
    pub fn minute(&self) -> u8 {
        self.iso.time.minute
    }

    /// Returns the second value
    #[inline]
    #[must_use]
    pub fn second(&self) -> u8 {
        self.iso.time.second
    }

    /// Returns the `millisecond` value
    #[inline]
    #[must_use]
    pub fn millisecond(&self) -> u16 {
        self.iso.time.millisecond
    }

    /// Returns the `microsecond` value
    #[inline]
    #[must_use]
    pub fn microsecond(&self) -> u16 {
        self.iso.time.microsecond
    }

    /// Returns the `nanosecond` value
    #[inline]
    #[must_use]
    pub fn nanosecond(&self) -> u16 {
        self.iso.time.nanosecond
    }

    /// Returns the Calendar value.
    #[inline]
    #[must_use]
    pub fn calendar(&self) -> &Calendar {
        &self.calendar
    }
}

// ==== Calendar-derived public API ====

impl PlainDateTime {
    /// Returns the calendar year value.
    pub fn year(&self) -> i32 {
        self.calendar.year(&self.iso.date)
    }

    /// Returns the calendar month value.
    pub fn month(&self) -> u8 {
        self.calendar.month(&self.iso.date)
    }

    /// Returns the calendar month code value.
    pub fn month_code(&self) -> MonthCode {
        self.calendar.month_code(&self.iso.date)
    }

    /// Returns the calendar day value.
    pub fn day(&self) -> u8 {
        self.calendar.day(&self.iso.date)
    }

    /// Returns the calendar day of week value.
    pub fn day_of_week(&self) -> TemporalResult<u16> {
        self.calendar.day_of_week(&self.iso.date)
    }

    /// Returns the calendar day of year value.
    pub fn day_of_year(&self) -> u16 {
        self.calendar.day_of_year(&self.iso.date)
    }

    /// Returns the calendar week of year value.
    pub fn week_of_year(&self) -> Option<u8> {
        self.calendar.week_of_year(&self.iso.date)
    }

    /// Returns the calendar year of week value.
    pub fn year_of_week(&self) -> Option<i32> {
        self.calendar.year_of_week(&self.iso.date)
    }

    /// Returns the calendar days in week value.
    pub fn days_in_week(&self) -> TemporalResult<u16> {
        self.calendar.days_in_week(&self.iso.date)
    }

    /// Returns the calendar days in month value.
    pub fn days_in_month(&self) -> u16 {
        self.calendar.days_in_month(&self.iso.date)
    }

    /// Returns the calendar days in year value.
    pub fn days_in_year(&self) -> u16 {
        self.calendar.days_in_year(&self.iso.date)
    }

    /// Returns the calendar months in year value.
    pub fn months_in_year(&self) -> u16 {
        self.calendar.months_in_year(&self.iso.date)
    }

    /// Returns returns whether the date in a leap year for the given calendar.
    pub fn in_leap_year(&self) -> bool {
        self.calendar.in_leap_year(&self.iso.date)
    }

    pub fn era(&self) -> Option<TinyAsciiStr<16>> {
        self.calendar.era(&self.iso.date)
    }

    pub fn era_year(&self) -> Option<i32> {
        self.calendar.era_year(&self.iso.date)
    }
}

impl PlainDateTime {
    /// Compares one `PlainDateTime` to another `PlainDateTime` using their
    /// `IsoDate` representation.
    ///
    /// # Note on Ordering.
    ///
    /// `temporal_rs` does not implement `PartialOrd`/`Ord` as `PlainDateTime` does
    /// not fulfill all the conditions required to implement the traits. However,
    /// it is possible to compare `PlainDate`'s as their `IsoDate` representation.
    #[inline]
    #[must_use]
    pub fn compare_iso(&self, other: &Self) -> Ordering {
        self.iso.cmp(&other.iso)
    }

    #[inline]
    /// Adds a `Duration` to the current `DateTime`.
    pub fn add(
        &self,
        duration: &Duration,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        self.add_or_subtract_duration(duration, overflow)
    }

    #[inline]
    /// Subtracts a `Duration` to the current `DateTime`.
    pub fn subtract(
        &self,
        duration: &Duration,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        self.add_or_subtract_duration(&duration.negated(), overflow)
    }

    #[inline]
    /// Returns a `Duration` representing the period of time from this `DateTime` until the other `DateTime`.
    pub fn until(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff(DifferenceOperation::Until, other, settings)
    }

    #[inline]
    /// Returns a `Duration` representing the period of time from this `DateTime` since the other `DateTime`.
    pub fn since(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff(DifferenceOperation::Since, other, settings)
    }

    /// Rounds the current datetime based on provided options.
    pub fn round(&self, options: RoundingOptions) -> TemporalResult<Self> {
        let resolved = ResolvedRoundingOptions::from_datetime_options(options)?;

        if resolved.is_noop() {
            return Ok(self.clone());
        }

        let result = self.iso.round(resolved)?;

        Ok(Self::new_unchecked(result, self.calendar.clone()))
    }

    pub fn to_zoned_date_time_with_provider(
        &self,
        time_zone: &TimeZone,
        disambiguation: Disambiguation,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<ZonedDateTime> {
        // 6. Let epochNs be ? GetEpochNanosecondsFor(timeZone, dateTime.[[ISODateTime]], disambiguation).
        let epoch_ns = time_zone.get_epoch_nanoseconds_for(self.iso, disambiguation, provider)?;
        // 7. Return ! CreateTemporalZonedDateTime(epochNs, timeZone, dateTime.[[Calendar]]).
        Ok(ZonedDateTime::new_unchecked(
            Instant::from(epoch_ns),
            self.calendar.clone(),
            time_zone.clone(),
        ))
    }

    pub fn to_plain_date(&self) -> TemporalResult<PlainDate> {
        // 3. Return ! CreateTemporalDate(dateTime.[[ISODateTime]].[[ISODate]], dateTime.[[Calendar]]).
        Ok(PlainDate::new_unchecked(
            self.iso.date,
            self.calendar.clone(),
        ))
    }

    pub fn to_plain_time(&self) -> TemporalResult<PlainTime> {
        // 3. Return ! CreateTemporalTime(dateTime.[[ISODateTime]].[[Time]]).
        Ok(PlainTime::new_unchecked(self.iso.time))
    }

    pub fn to_ixdtf_string(
        &self,
        options: ToStringRoundingOptions,
        display_calendar: DisplayCalendar,
    ) -> TemporalResult<String> {
        let resolved_options = options.resolve()?;
        let result = self
            .iso
            .round(ResolvedRoundingOptions::from_to_string_options(
                &resolved_options,
            ))?;
        if !result.is_within_limits() {
            return Err(TemporalError::range().with_message("DateTime is not within valid limits."));
        }
        let ixdtf_string = IxdtfStringBuilder::default()
            .with_date(result.date)
            .with_time(result.time, resolved_options.precision)
            .with_calendar(self.calendar.identifier(), display_calendar)
            .build();
        Ok(ixdtf_string)
    }
}

// ==== Trait impls ====

impl From<PlainDate> for PlainDateTime {
    fn from(value: PlainDate) -> Self {
        PlainDateTime::new_unchecked(
            IsoDateTime::new_unchecked(value.iso, IsoTime::default()),
            value.calendar().clone(),
        )
    }
}

impl FromStr for PlainDateTime {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::from_utf8(s.as_bytes())
    }
}

#[cfg(test)]
mod tests {
    use tinystr::{tinystr, TinyAsciiStr};

    use crate::{
        builtins::core::{
            calendar::Calendar, duration::DateDuration, Duration, PartialDate, PartialDateTime,
            PartialTime, PlainDateTime,
        },
        iso::{IsoDate, IsoDateTime, IsoTime},
        options::{
            DifferenceSettings, DisplayCalendar, RoundingIncrement, RoundingMode, RoundingOptions,
            ToStringRoundingOptions, Unit,
        },
        parsers::Precision,
        MonthCode, TemporalResult,
    };

    fn assert_datetime(
        dt: PlainDateTime,
        fields: (i32, u8, TinyAsciiStr<4>, u8, u8, u8, u8, u16, u16, u16),
    ) {
        assert_eq!(dt.year(), fields.0);
        assert_eq!(dt.month(), fields.1);
        assert_eq!(dt.month_code(), MonthCode(fields.2));
        assert_eq!(dt.day(), fields.3);
        assert_eq!(dt.hour(), fields.4);
        assert_eq!(dt.minute(), fields.5);
        assert_eq!(dt.second(), fields.6);
        assert_eq!(dt.millisecond(), fields.7);
        assert_eq!(dt.microsecond(), fields.8);
        assert_eq!(dt.nanosecond(), fields.9);
    }

    fn pdt_from_date(year: i32, month: u8, day: u8) -> TemporalResult<PlainDateTime> {
        PlainDateTime::try_new(year, month, day, 0, 0, 0, 0, 0, 0, Calendar::default())
    }

    #[test]
    #[allow(clippy::float_cmp)]
    fn plain_date_time_limits() {
        // This test is primarily to assert that the `expect` in the epoch methods is
        // valid, i.e., a valid instant is within the range of an f64.
        let negative_limit = pdt_from_date(-271_821, 4, 19);
        assert!(negative_limit.is_err());
        let positive_limit = pdt_from_date(275_760, 9, 14);
        assert!(positive_limit.is_err());
        let within_negative_limit = pdt_from_date(-271_821, 4, 20);
        assert_eq!(
            within_negative_limit,
            Ok(PlainDateTime {
                iso: IsoDateTime {
                    date: IsoDate {
                        year: -271_821,
                        month: 4,
                        day: 20,
                    },
                    time: IsoTime::default(),
                },
                calendar: Calendar::default(),
            })
        );

        let within_positive_limit = pdt_from_date(275_760, 9, 13);
        assert_eq!(
            within_positive_limit,
            Ok(PlainDateTime {
                iso: IsoDateTime {
                    date: IsoDate {
                        year: 275_760,
                        month: 9,
                        day: 13,
                    },
                    time: IsoTime::default(),
                },
                calendar: Calendar::default(),
            })
        );
    }

    #[test]
    fn basic_with_test() {
        let pdt =
            PlainDateTime::try_new(1976, 11, 18, 15, 23, 30, 123, 456, 789, Calendar::default())
                .unwrap();

        // Test year
        let partial = PartialDateTime {
            date: PartialDate {
                year: Some(2019),
                ..Default::default()
            },
            time: PartialTime::default(),
        };
        let result = pdt.with(partial, None).unwrap();
        assert_datetime(
            result,
            (2019, 11, tinystr!(4, "M11"), 18, 15, 23, 30, 123, 456, 789),
        );

        // Test month
        let partial = PartialDateTime {
            date: PartialDate {
                month: Some(5),
                ..Default::default()
            },
            time: PartialTime::default(),
        };
        let result = pdt.with(partial, None).unwrap();
        assert_datetime(
            result,
            (1976, 5, tinystr!(4, "M05"), 18, 15, 23, 30, 123, 456, 789),
        );

        // Test monthCode
        let partial = PartialDateTime {
            date: PartialDate {
                month_code: Some(MonthCode(tinystr!(4, "M05"))),
                ..Default::default()
            },
            time: PartialTime::default(),
        };
        let result = pdt.with(partial, None).unwrap();
        assert_datetime(
            result,
            (1976, 5, tinystr!(4, "M05"), 18, 15, 23, 30, 123, 456, 789),
        );

        // Test day
        let partial = PartialDateTime {
            date: PartialDate {
                day: Some(5),
                ..Default::default()
            },
            time: PartialTime::default(),
        };
        let result = pdt.with(partial, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 5, 15, 23, 30, 123, 456, 789),
        );

        // Test hour
        let partial = PartialDateTime {
            date: PartialDate::default(),
            time: PartialTime {
                hour: Some(5),
                ..Default::default()
            },
        };
        let result = pdt.with(partial, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 18, 5, 23, 30, 123, 456, 789),
        );

        // Test minute
        let partial = PartialDateTime {
            date: PartialDate::default(),
            time: PartialTime {
                minute: Some(5),
                ..Default::default()
            },
        };
        let result = pdt.with(partial, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 18, 15, 5, 30, 123, 456, 789),
        );

        // Test second
        let partial = PartialDateTime {
            date: PartialDate::default(),
            time: PartialTime {
                second: Some(5),
                ..Default::default()
            },
        };
        let result = pdt.with(partial, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 18, 15, 23, 5, 123, 456, 789),
        );

        // Test second
        let partial = PartialDateTime {
            date: PartialDate::default(),
            time: PartialTime {
                millisecond: Some(5),
                ..Default::default()
            },
        };
        let result = pdt.with(partial, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 18, 15, 23, 30, 5, 456, 789),
        );

        // Test second
        let partial = PartialDateTime {
            date: PartialDate::default(),
            time: PartialTime {
                microsecond: Some(5),
                ..Default::default()
            },
        };
        let result = pdt.with(partial, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 18, 15, 23, 30, 123, 5, 789),
        );

        // Test second
        let partial = PartialDateTime {
            date: PartialDate::default(),
            time: PartialTime {
                nanosecond: Some(5),
                ..Default::default()
            },
        };
        let result = pdt.with(partial, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 18, 15, 23, 30, 123, 456, 5),
        );
    }

    #[test]
    fn datetime_with_empty_partial() {
        let pdt =
            PlainDateTime::try_new(2020, 1, 31, 12, 34, 56, 987, 654, 321, Calendar::default())
                .unwrap();

        let err = pdt.with(PartialDateTime::default(), None);
        assert!(err.is_err());
    }

    // options-undefined.js
    #[test]
    fn datetime_add_test() {
        let pdt =
            PlainDateTime::try_new(2020, 1, 31, 12, 34, 56, 987, 654, 321, Calendar::default())
                .unwrap();

        let result = pdt
            .add(
                &Duration::from(DateDuration::new(0, 1, 0, 0).unwrap()),
                None,
            )
            .unwrap();

        assert_eq!(result.month(), 2);
        assert_eq!(result.day(), 29);
    }

    // options-undefined.js
    #[test]
    fn datetime_subtract_test() {
        let pdt =
            PlainDateTime::try_new(2000, 3, 31, 12, 34, 56, 987, 654, 321, Calendar::default())
                .unwrap();

        let result = pdt
            .subtract(
                &Duration::from(DateDuration::new(0, 1, 0, 0).unwrap()),
                None,
            )
            .unwrap();

        assert_eq!(result.month(), 2);
        assert_eq!(result.day(), 29);
    }

    // subtract/hour-overflow.js
    #[test]
    fn datetime_subtract_hour_overflows() {
        let dt =
            PlainDateTime::try_new(2019, 10, 29, 10, 46, 38, 271, 986, 102, Calendar::default())
                .unwrap();

        let result = dt.subtract(&Duration::hour(12), None).unwrap();
        assert_datetime(
            result,
            (2019, 10, tinystr!(4, "M10"), 28, 22, 46, 38, 271, 986, 102),
        );

        let result = dt.add(&Duration::hour(-12), None).unwrap();
        assert_datetime(
            result,
            (2019, 10, tinystr!(4, "M10"), 28, 22, 46, 38, 271, 986, 102),
        );
    }

    fn create_diff_setting(
        smallest: Unit,
        increment: u32,
        rounding_mode: RoundingMode,
    ) -> DifferenceSettings {
        DifferenceSettings {
            largest_unit: None,
            smallest_unit: Some(smallest),
            increment: Some(RoundingIncrement::try_new(increment).unwrap()),
            rounding_mode: Some(rounding_mode),
        }
    }

    #[test]
    fn dt_until_basic() {
        let earlier =
            PlainDateTime::try_new(2019, 1, 8, 8, 22, 36, 123, 456, 789, Calendar::default())
                .unwrap();
        let later =
            PlainDateTime::try_new(2021, 9, 7, 12, 39, 40, 987, 654, 321, Calendar::default())
                .unwrap();

        let settings = create_diff_setting(Unit::Hour, 3, RoundingMode::HalfExpand);
        let result = earlier.until(&later, settings).unwrap();

        assert_eq!(result.days(), 973);
        assert_eq!(result.hours(), 3);

        let settings = create_diff_setting(Unit::Minute, 30, RoundingMode::HalfExpand);
        let result = earlier.until(&later, settings).unwrap();

        assert_eq!(result.days(), 973);
        assert_eq!(result.hours(), 4);
        assert_eq!(result.minutes(), 30);
    }

    #[test]
    fn dt_since_basic() {
        let earlier =
            PlainDateTime::try_new(2019, 1, 8, 8, 22, 36, 123, 456, 789, Calendar::default())
                .unwrap();
        let later =
            PlainDateTime::try_new(2021, 9, 7, 12, 39, 40, 987, 654, 321, Calendar::default())
                .unwrap();

        let settings = create_diff_setting(Unit::Hour, 3, RoundingMode::HalfExpand);
        let result = later.since(&earlier, settings).unwrap();

        assert_eq!(result.days(), 973);
        assert_eq!(result.hours(), 3);

        let settings = create_diff_setting(Unit::Minute, 30, RoundingMode::HalfExpand);
        let result = later.since(&earlier, settings).unwrap();

        assert_eq!(result.days(), 973);
        assert_eq!(result.hours(), 4);
        assert_eq!(result.minutes(), 30);
    }

    #[test]
    fn dt_round_basic() {
        let assert_datetime =
            |dt: PlainDateTime, expected: (i32, u8, u8, u8, u8, u8, u16, u16, u16)| {
                assert_eq!(dt.iso_year(), expected.0);
                assert_eq!(dt.iso_month(), expected.1);
                assert_eq!(dt.iso_day(), expected.2);
                assert_eq!(dt.hour(), expected.3);
                assert_eq!(dt.minute(), expected.4);
                assert_eq!(dt.second(), expected.5);
                assert_eq!(dt.millisecond(), expected.6);
                assert_eq!(dt.microsecond(), expected.7);
                assert_eq!(dt.nanosecond(), expected.8);
            };

        let gen_rounding_options = |smallest: Unit, increment: u32| -> RoundingOptions {
            RoundingOptions {
                largest_unit: None,
                smallest_unit: Some(smallest),
                increment: Some(RoundingIncrement::try_new(increment).unwrap()),
                rounding_mode: None,
            }
        };
        let dt =
            PlainDateTime::try_new(1976, 11, 18, 14, 23, 30, 123, 456, 789, Calendar::default())
                .unwrap();

        let result = dt.round(gen_rounding_options(Unit::Hour, 4)).unwrap();
        assert_datetime(result, (1976, 11, 18, 16, 0, 0, 0, 0, 0));

        let result = dt.round(gen_rounding_options(Unit::Minute, 15)).unwrap();
        assert_datetime(result, (1976, 11, 18, 14, 30, 0, 0, 0, 0));

        let result = dt.round(gen_rounding_options(Unit::Second, 30)).unwrap();
        assert_datetime(result, (1976, 11, 18, 14, 23, 30, 0, 0, 0));

        let result = dt
            .round(gen_rounding_options(Unit::Millisecond, 10))
            .unwrap();
        assert_datetime(result, (1976, 11, 18, 14, 23, 30, 120, 0, 0));

        let result = dt
            .round(gen_rounding_options(Unit::Microsecond, 10))
            .unwrap();
        assert_datetime(result, (1976, 11, 18, 14, 23, 30, 123, 460, 0));

        let result = dt
            .round(gen_rounding_options(Unit::Nanosecond, 10))
            .unwrap();
        assert_datetime(result, (1976, 11, 18, 14, 23, 30, 123, 456, 790));
    }

    #[test]
    fn datetime_round_options() {
        let dt =
            PlainDateTime::try_new(1976, 11, 18, 14, 23, 30, 123, 456, 789, Calendar::default())
                .unwrap();

        let bad_options = RoundingOptions {
            largest_unit: None,
            smallest_unit: None,
            rounding_mode: Some(RoundingMode::Ceil),
            increment: Some(RoundingIncrement::ONE),
        };

        let err = dt.round(bad_options);
        assert!(err.is_err());

        let err = dt.round(RoundingOptions::default());
        assert!(err.is_err());
    }

    // Mapped from fractionaldigits-number.js
    #[test]
    fn to_string_precision_digits() {
        let few_seconds =
            PlainDateTime::try_new(1976, 2, 4, 5, 3, 1, 0, 0, 0, Calendar::default()).unwrap();
        let zero_seconds =
            PlainDateTime::try_new(1976, 11, 18, 15, 23, 0, 0, 0, 0, Calendar::default()).unwrap();
        let whole_seconds =
            PlainDateTime::try_new(1976, 11, 18, 15, 23, 30, 0, 0, 0, Calendar::default()).unwrap();
        let subseconds =
            PlainDateTime::try_new(1976, 11, 18, 15, 23, 30, 123, 400, 0, Calendar::default())
                .unwrap();

        let options = ToStringRoundingOptions {
            precision: Precision::Digit(0),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &few_seconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-02-04T05:03:01",
            "pads parts with 0"
        );

        let options = ToStringRoundingOptions {
            precision: Precision::Digit(0),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &subseconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30",
            "truncates 4 decimal places to 0"
        );

        let options = ToStringRoundingOptions {
            precision: Precision::Digit(2),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &zero_seconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:00.00",
            "pads zero seconds to 2 decimal places"
        );
        let options = ToStringRoundingOptions {
            precision: Precision::Digit(2),
            smallest_unit: None,
            rounding_mode: None,
        };

        assert_eq!(
            &whole_seconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.00",
            "pads whole seconds to 2 decimal places"
        );
        let options = ToStringRoundingOptions {
            precision: Precision::Digit(2),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &subseconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.12",
            "truncates 4 decimal places to 2"
        );

        let options = ToStringRoundingOptions {
            precision: Precision::Digit(3),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &subseconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.123",
            "truncates 4 decimal places to 3"
        );

        let options = ToStringRoundingOptions {
            precision: Precision::Digit(6),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &subseconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.123400",
            "pads 4 decimal places to 6"
        );
        let options = ToStringRoundingOptions {
            precision: Precision::Digit(7),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &zero_seconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:00.0000000",
            "pads zero seconds to 7 decimal places"
        );
        let options = ToStringRoundingOptions {
            precision: Precision::Digit(7),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &whole_seconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.0000000",
            "pads whole seconds to 7 decimal places"
        );
        let options = ToStringRoundingOptions {
            precision: Precision::Digit(7),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &subseconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.1234000",
            "pads 4 decimal places to 7"
        );
        let options = ToStringRoundingOptions {
            precision: Precision::Digit(9),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &subseconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.123400000",
            "pads 4 decimal places to 9"
        );
    }
}
