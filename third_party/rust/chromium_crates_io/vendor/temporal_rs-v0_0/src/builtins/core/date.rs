//! This module implements `Date` and any directly related algorithms.

use crate::{
    builtins::core::{
        calendar::Calendar, duration::DateDuration, Duration, PlainDateTime, PlainTime,
        ZonedDateTime,
    },
    iso::{IsoDate, IsoDateTime, IsoTime},
    options::{
        ArithmeticOverflow, DifferenceOperation, DifferenceSettings, Disambiguation,
        DisplayCalendar, ResolvedRoundingOptions, Unit, UnitGroup,
    },
    parsers::{parse_date_time, IxdtfStringBuilder},
    provider::{NeverProvider, TimeZoneProvider},
    MonthCode, TemporalError, TemporalResult, TemporalUnwrap, TimeZone,
};
use alloc::{format, string::String};
use core::{cmp::Ordering, str::FromStr};
use icu_calendar::AnyCalendarKind;

use super::{
    calendar::month_to_month_code,
    duration::{normalized::NormalizedDurationRecord, TimeDuration},
    PlainMonthDay, PlainYearMonth,
};
use tinystr::TinyAsciiStr;

// TODO (potentially): Bump era up to TinyAsciiStr<18> to accomodate
// "ethiopic-amete-alem". TODO: PrepareTemporalFields expects a type
// error to be thrown when all partial fields are None/undefined.
/// A partial PlainDate that may or may not be complete.
#[derive(Debug, Default, Clone, PartialEq)]
pub struct PartialDate {
    // A potentially set `year` field.
    pub year: Option<i32>,
    // A potentially set `month` field.
    pub month: Option<u8>,
    // A potentially set `month_code` field.
    pub month_code: Option<MonthCode>,
    // A potentially set `day` field.
    pub day: Option<u8>,
    // A potentially set `era` field.
    pub era: Option<TinyAsciiStr<19>>,
    // A potentially set `era_year` field.
    pub era_year: Option<i32>,
    /// The calendar field
    pub calendar: Calendar,
}

impl PartialDate {
    /// Returns a boolean for if the current `PartialDate` is empty.
    pub(crate) fn is_empty(&self) -> bool {
        *self == Self::default()
    }

    pub(crate) fn try_from_year_month(year_month: &PlainYearMonth) -> TemporalResult<Self> {
        let (year, era, era_year) = if year_month.era().is_some() {
            (
                None,
                year_month
                    .era()
                    .map(|t| TinyAsciiStr::<19>::try_from_utf8(t.as_bytes()))
                    .transpose()
                    .map_err(|e| TemporalError::general(format!("{e}")))?,
                year_month.era_year(),
            )
        } else {
            (Some(year_month.year()), None, None)
        };
        Ok(Self {
            year,
            month: Some(year_month.month()),
            month_code: Some(year_month.month_code()),
            day: Some(1),
            era,
            era_year,
            calendar: year_month.calendar().clone(),
        })
    }

    crate::impl_with_fallback_method!(with_fallback_year_month, () PlainYearMonth); // excludes day
    crate::impl_with_fallback_method!(with_fallback_date, (with_day: day) PlainDate);
    crate::impl_with_fallback_method!(with_fallback_datetime, (with_day:day) PlainDateTime);
}

// Use macro to impl fallback methods to avoid having a trait method.
#[doc(hidden)]
#[macro_export]
macro_rules! impl_with_fallback_method {
    ($method_name:ident, ( $(with_day: $day:ident)? ) $component_type:ty) => {
        pub(crate) fn $method_name(&self, fallback: &$component_type) -> TemporalResult<Self> {
            let era = if let Some(era) = self.era {
                Some(era)
            } else {
                let era = fallback.era();
                era.map(|e| {
                    TinyAsciiStr::<19>::try_from_utf8(e.as_bytes())
                        .map_err(|e| TemporalError::general(format!("{e}")))
                })
                .transpose()?
            };
            let era_year = self
                .era_year
                .map_or_else(|| fallback.era_year(), |ey| Some(ey));

            let (month, month_code) = match (self.month, self.month_code) {
                (Some(month), Some(mc)) => (Some(month), Some(mc)),
                (Some(month), None) => (Some(month), Some(month_to_month_code(month)?)),
                (None, Some(mc)) => (Some(mc.to_month_integer()).map(Into::into), Some(mc)),
                (None, None) => (
                    Some(fallback.month()).map(Into::into),
                    Some(fallback.month_code()),
                ),
            };
            #[allow(clippy::needless_update)] {
                Ok(Self {
                    year: Some(self.year.unwrap_or(fallback.year())),
                    month,
                    month_code,
                    $($day: Some(self.day.unwrap_or(fallback.day().into())),)?
                    era,
                    era_year,
                    calendar: fallback.calendar().clone(),
                    ..Default::default()
                })
            }
        }
    };
}

/// Convenience methods for building a `PartialDate`
impl PartialDate {
    pub const fn new() -> Self {
        Self {
            year: None,
            month: None,
            month_code: None,
            day: None,
            era: None,
            era_year: None,
            calendar: Calendar::new(AnyCalendarKind::Iso),
        }
    }

    pub const fn with_era(mut self, era: Option<TinyAsciiStr<19>>) -> Self {
        self.era = era;
        self
    }

    pub const fn with_era_year(mut self, era_year: Option<i32>) -> Self {
        self.era_year = era_year;
        self
    }

    pub const fn with_year(mut self, year: Option<i32>) -> Self {
        self.year = year;
        self
    }

    pub const fn with_month(mut self, month: Option<u8>) -> Self {
        self.month = month;
        self
    }

    pub const fn with_month_code(mut self, month_code: Option<MonthCode>) -> Self {
        self.month_code = month_code;
        self
    }

    pub const fn with_day(mut self, day: Option<u8>) -> Self {
        self.day = day;
        self
    }

    pub const fn with_calendar(mut self, calendar: Calendar) -> Self {
        self.calendar = calendar;
        self
    }
}

/// The native Rust implementation of `Temporal.PlainDate`.
#[non_exhaustive]
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct PlainDate {
    pub(crate) iso: IsoDate,
    calendar: Calendar,
}

impl core::fmt::Display for PlainDate {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str(&self.to_ixdtf_string(DisplayCalendar::Auto))
    }
}

// ==== Private API ====

impl PlainDate {
    /// Create a new `Date` with the date values and calendar slot.
    #[inline]
    #[must_use]
    pub(crate) fn new_unchecked(iso: IsoDate, calendar: Calendar) -> Self {
        Self { iso, calendar }
    }

    /// Returns the date after adding the given duration to date.
    ///
    /// Temporal Equivalent: 3.5.13 `AddDate ( calendar, plainDate, duration [ , options [ , dateAdd ] ] )`
    #[inline]
    pub(crate) fn add_date(
        &self,
        duration: &Duration,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        // 2. If options is not present, set options to undefined.
        let overflow = overflow.unwrap_or(ArithmeticOverflow::Constrain);
        // 3. If duration.[[Years]] ≠ 0, or duration.[[Months]] ≠ 0, or duration.[[Weeks]] ≠ 0, then
        if duration.date().years != 0 || duration.date().months != 0 || duration.date().weeks != 0 {
            // a. If dateAdd is not present, then
            // i. Set dateAdd to unused.
            // ii. If calendar is an Object, set dateAdd to ? GetMethod(calendar, "dateAdd").
            // b. Return ? CalendarDateAdd(calendar, plainDate, duration, options, dateAdd).
            return self.calendar().date_add(&self.iso, duration, overflow);
        }

        // 4. Let overflow be ? ToTemporalOverflow(options).
        // 5. Let norm be NormalizeTimeDuration(duration.[[Hours]],
        //    duration.[[Minutes]], duration.[[Seconds]],
        //    duration.[[Milliseconds]], duration.[[Microseconds]],
        //    duration.[[Nanoseconds]]).
        // 6. Let days be duration.[[Days]] + BalanceTimeDuration(norm,
        //    "day").[[Days]].
        let days = duration
            .days()
            .checked_add(
                TimeDuration::from_normalized(duration.time().to_normalized(), Unit::Day)?.0,
            )
            .ok_or(TemporalError::range())?;

        // 7. Let result be ? AddISODate(plainDate.[[ISOYear]], plainDate.[[ISOMonth]], plainDate.[[ISODay]], 0, 0, 0, days, overflow).
        let result = self
            .iso
            .add_date_duration(&DateDuration::new(0, 0, 0, days)?, overflow)?;

        Self::try_new(
            result.year,
            result.month,
            result.day,
            self.calendar().clone(),
        )
    }

    /// Returns a duration representing the difference between the dates one and two.
    ///
    /// Temporal Equivalent: 3.5.6 `DifferenceDate ( calendar, one, two, options )`
    #[inline]
    pub(crate) fn internal_diff_date(
        &self,
        other: &Self,
        largest_unit: Unit,
    ) -> TemporalResult<Duration> {
        if self.iso.year == other.iso.year
            && self.iso.month == other.iso.month
            && self.iso.day == other.iso.day
        {
            return Ok(Duration::default());
        }

        if largest_unit == Unit::Day {
            let days = self.days_until(other);
            return Ok(Duration::from(DateDuration::new(0, 0, 0, days.into())?));
        }

        self.calendar()
            .date_until(&self.iso, &other.iso, largest_unit)
    }

    /// Equivalent: DifferenceTemporalPlainDate
    pub(crate) fn diff_date(
        &self,
        op: DifferenceOperation,
        other: &Self,
        settings: DifferenceSettings,
    ) -> TemporalResult<Duration> {
        // 1. If operation is SINCE, let sign be -1. Otherwise, let sign be 1.
        // 2. Set other to ? ToTemporalDate(other).

        // 3. If ? CalendarEquals(temporalDate.[[Calendar]], other.[[Calendar]]) is false, throw a RangeError exception.
        if self.calendar().identifier() != other.calendar().identifier() {
            return Err(TemporalError::range()
                .with_message("Calendars are for difference operation are not the same."));
        }

        // 4. Let resolvedOptions be ? SnapshotOwnProperties(? GetOptionsObject(options), null).
        // 5. Let settings be ? GetDifferenceSettings(operation, resolvedOptions, DATE, « », "day", "day").
        let resolved = ResolvedRoundingOptions::from_diff_settings(
            settings,
            op,
            UnitGroup::Date,
            Unit::Day,
            Unit::Day,
        )?;

        // 6. If temporalDate.[[ISOYear]] = other.[[ISOYear]], and temporalDate.[[ISOMonth]] = other.[[ISOMonth]],
        // and temporalDate.[[ISODay]] = other.[[ISODay]], then
        if self.iso == other.iso {
            // a. Return ! CreateTemporalDuration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0).
            return Ok(Duration::default());
        }

        // 7. Let calendarRec be ? CreateCalendarMethodsRecord(temporalDate.[[Calendar]], « DATE-ADD, DATE-UNTIL »).
        // 8. Perform ! CreateDataPropertyOrThrow(resolvedOptions, "largestUnit", settings.[[LargestUnit]]).
        // 9. Let result be ? DifferenceDate(calendarRec, temporalDate, other, resolvedOptions).
        let result = self.internal_diff_date(other, resolved.largest_unit)?;

        // 10. Let duration be ! CreateNormalizedDurationRecord(result.[[Years]], result.[[Months]], result.[[Weeks]], result.[[Days]], ZeroTimeDuration()).
        let mut duration = NormalizedDurationRecord::from_date_duration(*result.date())?;
        // 11. If settings.[[SmallestUnit]] is "day" and settings.[[RoundingIncrement]] = 1, let roundingGranularityIsNoop be true; else let roundingGranularityIsNoop be false.
        let rounding_granularity_is_noop =
            resolved.smallest_unit == Unit::Day && resolved.increment.get() == 1;
        // 12. If roundingGranularityIsNoop is false, then
        if !rounding_granularity_is_noop {
            // a. Let destEpochNs be GetUTCEpochNanoseconds(other.[[ISOYear]], other.[[ISOMonth]], other.[[ISODay]], 0, 0, 0, 0, 0, 0).
            let dest_epoch_ns = other.iso.as_nanoseconds()?;
            // b. Let dateTime be ISO Date-Time Record { [[Year]]: temporalDate.[[ISOYear]], [[Month]]: temporalDate.[[ISOMonth]], [[Day]]: temporalDate.[[ISODay]], [[Hour]]: 0, [[Minute]]: 0, [[Second]]: 0, [[Millisecond]]: 0, [[Microsecond]]: 0, [[Nanosecond]]: 0 }.
            let dt = PlainDateTime::new_unchecked(
                IsoDateTime::new_unchecked(self.iso, IsoTime::default()),
                self.calendar.clone(),
            );
            // c. Set duration to ? RoundRelativeDuration(duration, destEpochNs, dateTime, calendarRec, unset, settings.[[LargestUnit]], settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
            duration = duration.round_relative_duration(
                dest_epoch_ns.0,
                &dt,
                Option::<(&TimeZone, &NeverProvider)>::None,
                resolved,
            )?
        }
        let result = Duration::from_normalized(duration, Unit::Day)?;
        // 13. Return ! CreateTemporalDuration(sign × duration.[[Years]], sign × duration.[[Months]], sign × duration.[[Weeks]], sign × duration.[[Days]], 0, 0, 0, 0, 0, 0).
        match op {
            DifferenceOperation::Until => Ok(result),
            DifferenceOperation::Since => Ok(result.negated()),
        }
    }

    ///  Abstract operation `DaysUntil`
    ///
    /// Calculates the epoch days between two `Date`s
    #[inline]
    #[must_use]
    fn days_until(&self, other: &Self) -> i32 {
        other.iso.to_epoch_days() - self.iso.to_epoch_days()
    }
}

// ==== Public API ====

impl PlainDate {
    /// Creates a new `PlainDate` automatically constraining any values that may be invalid.
    #[inline]
    pub fn new(year: i32, month: u8, day: u8, calendar: Calendar) -> TemporalResult<Self> {
        Self::new_with_overflow(year, month, day, calendar, ArithmeticOverflow::Constrain)
    }

    /// Creates a new `PlainDate` with an ISO 8601 calendar automatically constraining any
    /// values that may be invalid into a valid range.
    #[inline]
    pub fn new_iso(year: i32, month: u8, day: u8) -> TemporalResult<Self> {
        Self::new(year, month, day, Calendar::default())
    }

    /// Creates a new `PlainDate` rejecting any date that may be invalid.
    #[inline]
    pub fn try_new(year: i32, month: u8, day: u8, calendar: Calendar) -> TemporalResult<Self> {
        Self::new_with_overflow(year, month, day, calendar, ArithmeticOverflow::Reject)
    }

    /// Creates a new `PlainDate` with an ISO 8601 calendar rejecting any date that may be invalid.
    #[inline]
    pub fn try_new_iso(year: i32, month: u8, day: u8) -> TemporalResult<Self> {
        Self::try_new(year, month, day, Calendar::default())
    }

    /// Creates a new `PlainDate` with the specified overflow.
    ///
    /// This operation is the public facing API to Temporal's `RegulateIsoDate`
    #[inline]
    pub fn new_with_overflow(
        year: i32,
        month: u8,
        day: u8,
        calendar: Calendar,
        overflow: ArithmeticOverflow,
    ) -> TemporalResult<Self> {
        let iso = IsoDate::new_with_overflow(year, month, day, overflow)?;
        Ok(Self::new_unchecked(iso, calendar))
    }

    /// Create a `PlainDate` from a `PartialDate`
    ///
    /// ```rust
    /// use temporal_rs::{PlainDate, partial::PartialDate};
    ///
    /// let partial = PartialDate {
    ///     year: Some(2000),
    ///     month: Some(13),
    ///     day: Some(2),
    ///     ..Default::default()
    /// };
    ///
    /// let date = PlainDate::from_partial(partial, None).unwrap();
    ///
    /// assert_eq!(date.year(), 2000);
    /// assert_eq!(date.month(), 12);
    /// assert_eq!(date.day(), 2);
    /// assert_eq!(date.calendar().identifier(), "iso8601");
    ///
    /// ```
    #[inline]
    pub fn from_partial(
        partial: PartialDate,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        let year_check =
            partial.year.is_some() || (partial.era.is_some() && partial.era_year.is_some());
        let month_check = partial.month.is_some() || partial.month_code.is_some();
        if !year_check || !month_check || partial.day.is_none() {
            return Err(TemporalError::r#type().with_message("Invalid PlainDate fields provided."));
        }

        let overflow = overflow.unwrap_or_default();
        partial.calendar.date_from_partial(&partial, overflow)
    }

    // Converts a UTF-8 encoded string into a `PlainDate`.
    pub fn from_utf8(s: &[u8]) -> TemporalResult<Self> {
        let parse_record = parse_date_time(s)?;

        let calendar = parse_record
            .calendar
            .map(Calendar::try_from_utf8)
            .transpose()?
            .unwrap_or_default();

        // Assertion: PlainDate must exist on a DateTime parse.
        let date = parse_record.date.temporal_unwrap()?;

        Self::try_new(date.year, date.month, date.day, calendar)
    }

    /// Creates a date time with values from a `PartialDate`.
    pub fn with(
        &self,
        partial: PartialDate,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        if partial.is_empty() {
            return Err(TemporalError::r#type().with_message("A PartialDate must have a field."));
        }
        // 6. Let fieldsResult be ? PrepareCalendarFieldsAndFieldNames(calendarRec, temporalDate, « "day", "month", "monthCode", "year" »).
        // 7. Let partialDate be ? PrepareTemporalFields(temporalDateLike, fieldsResult.[[FieldNames]], partial).
        // 8. Let fields be ? CalendarMergeFields(calendarRec, fieldsResult.[[Fields]], partialDate).
        // 9. Set fields to ? PrepareTemporalFields(fields, fieldsResult.[[FieldNames]], «»).
        // 10. Return ? CalendarDateFromFields(calendarRec, fields, resolvedOptions).
        self.calendar.date_from_partial(
            &partial.with_fallback_date(self)?,
            overflow.unwrap_or(ArithmeticOverflow::Constrain),
        )
    }

    /// Creates a new `Date` from the current `Date` and the provided calendar.
    pub fn with_calendar(&self, calendar: Calendar) -> TemporalResult<Self> {
        Self::try_new(self.iso_year(), self.iso_month(), self.iso_day(), calendar)
    }

    #[inline]
    #[must_use]
    /// Returns this `Date`'s ISO year value.
    pub const fn iso_year(&self) -> i32 {
        self.iso.year
    }

    #[inline]
    #[must_use]
    /// Returns this `Date`'s ISO month value.
    pub const fn iso_month(&self) -> u8 {
        self.iso.month
    }

    #[inline]
    #[must_use]
    /// Returns this `Date`'s ISO day value.
    pub const fn iso_day(&self) -> u8 {
        self.iso.day
    }

    #[inline]
    #[must_use]
    /// Returns a reference to this `Date`'s calendar slot.
    pub fn calendar(&self) -> &Calendar {
        &self.calendar
    }

    /// 3.5.7 `IsValidISODate`
    ///
    /// Checks if the current date is a valid `ISODate`.
    #[must_use]
    pub fn is_valid(&self) -> bool {
        self.iso.is_valid()
    }

    /// Compares one `PlainDate` to another `PlainDate` using their
    /// `IsoDate` representation.
    ///
    /// # Note on Ordering.
    ///
    /// `temporal_rs` does not implement `PartialOrd`/`Ord` as `PlainDate` does
    /// not fulfill all the conditions required to implement the traits. However,
    /// it is possible to compare `PlainDate`'s as their `IsoDate` representation.
    #[inline]
    #[must_use]
    pub fn compare_iso(&self, other: &Self) -> Ordering {
        self.iso.cmp(&other.iso)
    }

    #[inline]
    /// Adds a `Duration` to the current `Date`
    pub fn add(
        &self,
        duration: &Duration,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        self.add_date(duration, overflow)
    }

    #[inline]
    /// Subtracts a `Duration` to the current `Date`
    pub fn subtract(
        &self,
        duration: &Duration,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        self.add_date(&duration.negated(), overflow)
    }

    #[inline]
    /// Returns a `Duration` representing the time from this `Date` until the other `Date`.
    pub fn until(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff_date(DifferenceOperation::Until, other, settings)
    }

    #[inline]
    /// Returns a `Duration` representing the time passed from this `Date` since the other `Date`.
    pub fn since(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff_date(DifferenceOperation::Since, other, settings)
    }
}

// ==== Calendar-derived Public API ====

impl PlainDate {
    /// Returns the calendar year value.
    pub fn year(&self) -> i32 {
        self.calendar.year(&self.iso)
    }

    /// Returns the calendar month value.
    pub fn month(&self) -> u8 {
        self.calendar.month(&self.iso)
    }

    /// Returns the calendar month code value.
    pub fn month_code(&self) -> MonthCode {
        self.calendar.month_code(&self.iso)
    }

    /// Returns the calendar day value.
    pub fn day(&self) -> u8 {
        self.calendar.day(&self.iso)
    }

    /// Returns the calendar day of week value.
    pub fn day_of_week(&self) -> TemporalResult<u16> {
        self.calendar.day_of_week(&self.iso)
    }

    /// Returns the calendar day of year value.
    pub fn day_of_year(&self) -> u16 {
        self.calendar.day_of_year(&self.iso)
    }

    /// Returns the calendar week of year value.
    pub fn week_of_year(&self) -> Option<u8> {
        self.calendar.week_of_year(&self.iso)
    }

    /// Returns the calendar year of week value.
    pub fn year_of_week(&self) -> Option<i32> {
        self.calendar.year_of_week(&self.iso)
    }

    /// Returns the calendar days in week value.
    pub fn days_in_week(&self) -> TemporalResult<u16> {
        self.calendar.days_in_week(&self.iso)
    }

    /// Returns the calendar days in month value.
    pub fn days_in_month(&self) -> u16 {
        self.calendar.days_in_month(&self.iso)
    }

    /// Returns the calendar days in year value.
    pub fn days_in_year(&self) -> u16 {
        self.calendar.days_in_year(&self.iso)
    }

    /// Returns the calendar months in year value.
    pub fn months_in_year(&self) -> u16 {
        self.calendar.months_in_year(&self.iso)
    }

    /// Returns returns whether the date in a leap year for the given calendar.
    pub fn in_leap_year(&self) -> bool {
        self.calendar.in_leap_year(&self.iso)
    }

    pub fn era(&self) -> Option<TinyAsciiStr<16>> {
        self.calendar.era(&self.iso)
    }

    pub fn era_year(&self) -> Option<i32> {
        self.calendar.era_year(&self.iso)
    }
}

// ==== ToX Methods ====

impl PlainDate {
    /// Converts the current `Date` into a `DateTime`
    ///
    /// # Notes
    ///
    /// If no time is provided, then the time will default to midnight.
    #[inline]
    pub fn to_plain_date_time(&self, time: Option<PlainTime>) -> TemporalResult<PlainDateTime> {
        let time = time.unwrap_or_default();
        let iso = IsoDateTime::new(self.iso, time.iso)?;
        Ok(PlainDateTime::new_unchecked(iso, self.calendar().clone()))
    }

    /// Converts the current `Date` into a `PlainYearMonth`
    #[inline]
    pub fn to_plain_year_month(&self) -> TemporalResult<PlainYearMonth> {
        // TODO: Migrate to `PartialYearMonth`
        let era = self
            .era()
            .map(|e| {
                TinyAsciiStr::<19>::try_from_utf8(e.as_bytes())
                    .map_err(|e| TemporalError::general(format!("{e}")))
            })
            .transpose()?;
        let partial = PartialDate::new()
            .with_year(Some(self.year()))
            .with_era(era)
            .with_era_year(self.era_year())
            .with_month(Some(self.month()))
            .with_month_code(Some(self.month_code()));
        self.calendar()
            .year_month_from_partial(&partial, ArithmeticOverflow::Constrain)
    }

    /// Converts the current `Date` into a `PlainMonthDay`
    #[inline]
    pub fn to_plain_month_day(&self) -> TemporalResult<PlainMonthDay> {
        self.calendar().month_day_from_partial(
            &PartialDate::default().with_fallback_date(self)?,
            ArithmeticOverflow::Constrain,
        )
    }

    #[inline]
    pub fn to_ixdtf_string(&self, display_calendar: DisplayCalendar) -> String {
        IxdtfStringBuilder::default()
            .with_date(self.iso)
            .with_calendar(self.calendar.identifier(), display_calendar)
            .build()
    }

    #[inline]
    pub fn to_zoned_date_time_with_provider(
        &self,
        tz: TimeZone,
        plain_time: Option<PlainTime>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<ZonedDateTime> {
        // 1. Let temporalDate be the this value.
        // 2. Perform ? RequireInternalSlot(temporalDate, [[InitializedTemporalDate]]).
        // 3. If item is an Object, then
        //      a. Let timeZoneLike be ? Get(item, "timeZone").
        //      b. If timeZoneLike is undefined, then
        //          i. Let timeZone be ? ToTemporalTimeZoneIdentifier(item).
        //          ii. Let temporalTime be undefined.
        //      c. Else,
        //          i. Let timeZone be ? ToTemporalTimeZoneIdentifier(timeZoneLike).
        //          ii. Let temporalTime be ? Get(item, "plainTime").
        // 4. Else,
        //     a. Let timeZone be ? ToTemporalTimeZoneIdentifier(item).
        //     b. Let temporalTime be undefined.

        //  5. If temporalTime is undefined, then
        //     a. Let epochNs be ? GetStartOfDay(timeZone, temporalDate.[[ISODate]]).
        //  6. Else,
        //     a. Set temporalTime to ? ToTemporalTime(temporalTime).
        //     b. Let isoDateTime be CombineISODateAndTimeRecord(temporalDate.[[ISODate]], temporalTime.[[Time]]).
        //     c. If ISODateTimeWithinLimits(isoDateTime) is false, throw a RangeError exception.
        //     d. Let epochNs be ? GetEpochNanosecondsFor(timeZone, isoDateTime, compatible).
        let epoch_ns = if let Some(time) = plain_time {
            let result_iso = IsoDateTime::new(self.iso, time.iso);

            tz.get_epoch_nanoseconds_for(
                result_iso.unwrap_or_default(),
                Disambiguation::Compatible,
                provider,
            )?
        } else {
            tz.get_start_of_day(&self.iso, provider)?
        };
        //  7. Return ! CreateTemporalZonedDateTime(epochNs, timeZone, temporalDate.[[Calendar]]).
        ZonedDateTime::try_new(epoch_ns.0, self.calendar.clone(), tz)
    }
}
// ==== Trait impls ====

impl From<PlainDateTime> for PlainDate {
    fn from(value: PlainDateTime) -> Self {
        PlainDate::new_unchecked(value.iso.date, value.calendar().clone())
    }
}

// TODO: impl From<ZonedDateTime> for PlainDate

impl FromStr for PlainDate {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::from_utf8(s.as_bytes())
    }
}

#[cfg(test)]
mod tests {
    use tinystr::tinystr;

    use super::*;

    #[test]
    fn new_date_limits() {
        let err = PlainDate::try_new(-271_821, 4, 18, Calendar::default());
        assert!(err.is_err());
        let err = PlainDate::try_new(275_760, 9, 14, Calendar::default());
        assert!(err.is_err());
        let ok = PlainDate::try_new(-271_821, 4, 19, Calendar::default());
        assert_eq!(
            ok,
            Ok(PlainDate {
                iso: IsoDate {
                    year: -271_821,
                    month: 4,
                    day: 19,
                },
                calendar: Calendar::default(),
            })
        );

        let ok = PlainDate::try_new(275_760, 9, 13, Calendar::default());
        assert_eq!(
            ok,
            Ok(PlainDate {
                iso: IsoDate {
                    year: 275760,
                    month: 9,
                    day: 13,
                },
                calendar: Calendar::default(),
            })
        );
    }

    #[test]
    fn simple_date_add() {
        let base = PlainDate::from_str("1976-11-18").unwrap();

        // Test 1
        let result = base
            .add(&Duration::from_str("P43Y").unwrap(), None)
            .unwrap();
        assert_eq!(
            result.iso,
            IsoDate {
                year: 2019,
                month: 11,
                day: 18,
            }
        );

        // Test 2
        let result = base.add(&Duration::from_str("P3M").unwrap(), None).unwrap();
        assert_eq!(
            result.iso,
            IsoDate {
                year: 1977,
                month: 2,
                day: 18,
            }
        );

        // Test 3
        let result = base
            .add(&Duration::from_str("P20D").unwrap(), None)
            .unwrap();
        assert_eq!(
            result.iso,
            IsoDate {
                year: 1976,
                month: 12,
                day: 8,
            }
        )
    }

    #[test]
    fn date_add_limits() {
        let max = PlainDate::try_new(275_760, 9, 13, Calendar::default()).unwrap();
        let result = max.add(&Duration::from_str("P1D").unwrap(), None);
        assert!(result.is_err());

        let max = PlainDate::try_new(275_760, 9, 12, Calendar::default()).unwrap();
        let result = max.add(&Duration::from_str("P1D").unwrap(), None);
        assert_eq!(
            result,
            Ok(PlainDate {
                iso: IsoDate {
                    year: 275760,
                    month: 9,
                    day: 13
                },
                calendar: Calendar::default(),
            })
        );

        let min = PlainDate::try_new(-271_821, 4, 19, Calendar::default()).unwrap();
        let result = min.add(&Duration::from_str("-P1D").unwrap(), None);
        assert!(result.is_err());

        let min = PlainDate::try_new(-271_821, 4, 20, Calendar::default()).unwrap();
        let result = min.add(&Duration::from_str("-P1D").unwrap(), None);
        assert_eq!(
            result,
            Ok(PlainDate {
                iso: IsoDate {
                    year: -271_821,
                    month: 4,
                    day: 19
                },
                calendar: Calendar::default(),
            })
        );
    }

    #[test]
    fn simple_date_subtract() {
        let base = PlainDate::from_str("2019-11-18").unwrap();

        // Test 1
        let result = base
            .subtract(&Duration::from_str("P43Y").unwrap(), None)
            .unwrap();
        assert_eq!(
            result.iso,
            IsoDate {
                year: 1976,
                month: 11,
                day: 18,
            }
        );

        // Test 2
        let result = base
            .subtract(&Duration::from_str("P11M").unwrap(), None)
            .unwrap();
        assert_eq!(
            result.iso,
            IsoDate {
                year: 2018,
                month: 12,
                day: 18,
            }
        );

        // Test 3
        let result = base
            .subtract(&Duration::from_str("P20D").unwrap(), None)
            .unwrap();
        assert_eq!(
            result.iso,
            IsoDate {
                year: 2019,
                month: 10,
                day: 29,
            }
        )
    }

    #[test]
    fn simple_date_until() {
        let earlier = PlainDate::from_str("1969-07-24").unwrap();
        let later = PlainDate::from_str("1969-10-05").unwrap();
        let result = earlier
            .until(&later, DifferenceSettings::default())
            .unwrap();
        assert_eq!(result.days(), 73,);

        let later = PlainDate::from_str("1996-03-03").unwrap();
        let result = earlier
            .until(&later, DifferenceSettings::default())
            .unwrap();
        assert_eq!(result.days(), 9719,);
    }

    #[test]
    fn simple_date_since() {
        let earlier = PlainDate::from_str("1969-07-24").unwrap();
        let later = PlainDate::from_str("1969-10-05").unwrap();
        let result = later
            .since(&earlier, DifferenceSettings::default())
            .unwrap();
        assert_eq!(result.days(), 73,);

        let later = PlainDate::from_str("1996-03-03").unwrap();
        let result = later
            .since(&earlier, DifferenceSettings::default())
            .unwrap();
        assert_eq!(result.days(), 9719,);
    }

    #[test]
    fn date_with_empty_error() {
        let base = PlainDate::new(1976, 11, 18, Calendar::default()).unwrap();

        let err = base.with(PartialDate::default(), None);
        assert!(err.is_err());
    }

    #[test]
    fn basic_date_with() {
        let base = PlainDate::new(1976, 11, 18, Calendar::default()).unwrap();

        // Year
        let partial = PartialDate {
            year: Some(2019),
            ..Default::default()
        };
        let with_year = base.with(partial, None).unwrap();
        assert_eq!(with_year.year(), 2019);
        assert_eq!(with_year.month(), 11);
        assert_eq!(with_year.month_code(), MonthCode::from_str("M11").unwrap());
        assert_eq!(with_year.day(), 18);

        // Month
        let partial = PartialDate {
            month: Some(5),
            ..Default::default()
        };
        let with_month = base.with(partial, None).unwrap();
        assert_eq!(with_month.year(), 1976);
        assert_eq!(with_month.month(), 5);
        assert_eq!(with_month.month_code(), MonthCode::from_str("M05").unwrap());
        assert_eq!(with_month.day(), 18);

        // Month Code
        let partial = PartialDate {
            month_code: Some(MonthCode(tinystr!(4, "M05"))),
            ..Default::default()
        };
        let with_mc = base.with(partial, None).unwrap();
        assert_eq!(with_mc.year(), 1976);
        assert_eq!(with_mc.month(), 5);
        assert_eq!(with_mc.month_code(), MonthCode::from_str("M05").unwrap());
        assert_eq!(with_mc.day(), 18);

        // Day
        let partial = PartialDate {
            day: Some(17),
            ..Default::default()
        };
        let with_day = base.with(partial, None).unwrap();
        assert_eq!(with_day.year(), 1976);
        assert_eq!(with_day.month(), 11);
        assert_eq!(with_day.month_code(), MonthCode::from_str("M11").unwrap());
        assert_eq!(with_day.day(), 17);
    }

    // test toZonedDateTime
    #[cfg(feature = "tzdb")]
    #[test]
    fn to_zoned_date_time() {
        use crate::tzdb::FsTzdbProvider;
        let date = PlainDate::from_str("2020-01-01").unwrap();
        let tz = TimeZone::try_from_str("UTC").unwrap();
        let provider = &FsTzdbProvider::default();
        let zdt = date
            .to_zoned_date_time_with_provider(tz, None, provider)
            .unwrap();
        assert_eq!(zdt.year_with_provider(provider).unwrap(), 2020);
        assert_eq!(zdt.month_with_provider(provider).unwrap(), 1);
        assert_eq!(zdt.day_with_provider(provider).unwrap(), 1);
        assert_eq!(zdt.hour_with_provider(provider).unwrap(), 0);
        assert_eq!(zdt.minute_with_provider(provider).unwrap(), 0);
        assert_eq!(zdt.second_with_provider(provider).unwrap(), 0);
        assert_eq!(zdt.millisecond_with_provider(provider).unwrap(), 0);
        assert_eq!(zdt.microsecond_with_provider(provider).unwrap(), 0);
        assert_eq!(zdt.nanosecond_with_provider(provider).unwrap(), 0);
    }

    // test262/test/built-ins/Temporal/Calendar/prototype/month/argument-string-invalid.js
    #[test]
    fn invalid_strings() {
        const INVALID_STRINGS: [&str; 35] = [
            // invalid ISO strings:
            "",
            "invalid iso8601",
            "2020-01-00",
            "2020-01-32",
            "2020-02-30",
            "2021-02-29",
            "2020-00-01",
            "2020-13-01",
            "2020-01-01T",
            "2020-01-01T25:00:00",
            "2020-01-01T01:60:00",
            "2020-01-01T01:60:61",
            "2020-01-01junk",
            "2020-01-01T00:00:00junk",
            "2020-01-01T00:00:00+00:00junk",
            "2020-01-01T00:00:00+00:00[UTC]junk",
            "2020-01-01T00:00:00+00:00[UTC][u-ca=iso8601]junk",
            "02020-01-01",
            "2020-001-01",
            "2020-01-001",
            "2020-01-01T001",
            "2020-01-01T01:001",
            "2020-01-01T01:01:001",
            // valid, but forms not supported in Temporal:
            "2020-W01-1",
            "2020-001",
            "+0002020-01-01",
            // valid, but this calendar must not exist:
            "2020-01-01[u-ca=notexist]",
            // may be valid in other contexts, but insufficient information for PlainDate:
            "2020-01",
            "+002020-01",
            "01-01",
            "2020-W01",
            "P1Y",
            "-P12Y",
            // valid, but outside the supported range:
            "-999999-01-01",
            "+999999-01-01",
        ];
        for s in INVALID_STRINGS {
            assert!(PlainDate::from_str(s).is_err())
        }
    }

    // test262/test/built-ins/Temporal/Calendar/prototype/day/argument-string-critical-unknown-annotation.js
    #[test]
    fn argument_string_critical_unknown_annotation() {
        const INVALID_STRINGS: [&str; 6] = [
            "1970-01-01[!foo=bar]",
            "1970-01-01T00:00[!foo=bar]",
            "1970-01-01T00:00[UTC][!foo=bar]",
            "1970-01-01T00:00[u-ca=iso8601][!foo=bar]",
            "1970-01-01T00:00[UTC][!foo=bar][u-ca=iso8601]",
            "1970-01-01T00:00[foo=bar][!_foo-bar0=Dont-Ignore-This-99999999999]",
        ];
        for s in INVALID_STRINGS {
            assert!(PlainDate::from_str(s).is_err())
        }
    }
}
