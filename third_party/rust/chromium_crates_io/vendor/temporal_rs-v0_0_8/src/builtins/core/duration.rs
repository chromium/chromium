//! This module implements `Duration` along with it's methods and components.

use crate::{
    builtins::core::{PlainDateTime, PlainTime, ZonedDateTime},
    iso::{IsoDateTime, IsoTime},
    options::{
        ArithmeticOverflow, RelativeTo, ResolvedRoundingOptions, RoundingIncrement,
        RoundingOptions, ToStringRoundingOptions, Unit,
    },
    parsers::{FormattableDateDuration, FormattableDuration, FormattableTimeDuration, Precision},
    primitive::FiniteF64,
    provider::TimeZoneProvider,
    temporal_assert, Sign, TemporalError, TemporalResult, TemporalUnwrap, NS_PER_DAY,
};
use alloc::format;
use alloc::string::String;
use core::{cmp::Ordering, num::NonZeroU128, str::FromStr};
use ixdtf::parsers::{records::TimeDurationRecord, IsoDurationParser};
use normalized::NormalizedDurationRecord;

use self::normalized::NormalizedTimeDuration;

mod date;
pub(crate) mod normalized;
mod time;

#[cfg(test)]
mod tests;

#[doc(inline)]
pub use date::DateDuration;
#[doc(inline)]
pub use time::TimeDuration;

/// A `PartialDuration` is a Duration that may have fields not set.
#[derive(Debug, Default, Clone, Copy, PartialEq, PartialOrd)]
pub struct PartialDuration {
    /// A potentially existent `years` field.
    pub years: Option<i64>,
    /// A potentially existent `months` field.
    pub months: Option<i64>,
    /// A potentially existent `weeks` field.
    pub weeks: Option<i64>,
    /// A potentially existent `days` field.
    pub days: Option<i64>,
    /// A potentially existent `hours` field.
    pub hours: Option<i64>,
    /// A potentially existent `minutes` field.
    pub minutes: Option<i64>,
    /// A potentially existent `seconds` field.
    pub seconds: Option<i64>,
    /// A potentially existent `milliseconds` field.
    pub milliseconds: Option<i64>,
    /// A potentially existent `microseconds` field.
    pub microseconds: Option<i128>,
    /// A potentially existent `nanoseconds` field.
    pub nanoseconds: Option<i128>,
}

impl PartialDuration {
    /// Returns whether the `PartialDuration` is empty.
    #[inline]
    #[must_use]
    pub fn is_empty(&self) -> bool {
        self == &Self::default()
    }
}

/// The native Rust implementation of `Temporal.Duration`.
///
/// `Duration` is made up of a `DateDuration` and `TimeDuration` as primarily
/// defined by Abtract Operation 7.5.1-5.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, Default)]
pub struct Duration {
    date: DateDuration,
    time: TimeDuration,
}

impl core::fmt::Display for Duration {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str(
            &self
                .as_temporal_string(ToStringRoundingOptions::default())
                .expect("Duration must return a valid string with default options."),
        )
    }
}

// NOTE(nekevss): Structure of the below is going to be a little convoluted,
// but intended to section everything based on the below
//
// Notation - [section](sub-section(s)).
//
// Sections:
//   - Creation (private/public)
//   - Getters/Setters
//   - Methods (private/public/feature)
//

#[cfg(test)]
impl Duration {
    pub(crate) fn hour(value: i64) -> Self {
        Self::new_unchecked(
            DateDuration::default(),
            TimeDuration::new_unchecked(value, 0, 0, 0, 0, 0),
        )
    }
}

// ==== Private Creation methods ====

impl Duration {
    /// Creates a new `Duration` from a `DateDuration` and `TimeDuration`.
    #[inline]
    pub(crate) const fn new_unchecked(date: DateDuration, time: TimeDuration) -> Self {
        Self { date, time }
    }

    #[inline]
    pub(crate) fn from_normalized(
        duration_record: NormalizedDurationRecord,
        largest_unit: Unit,
    ) -> TemporalResult<Self> {
        let (overflow_day, time) = TimeDuration::from_normalized(
            duration_record.normalized_time_duration(),
            largest_unit,
        )?;
        Self::new(
            duration_record.date().years,
            duration_record.date().months,
            duration_record.date().weeks,
            duration_record
                .date()
                .days
                .checked_add(overflow_day)
                .ok_or(TemporalError::range())?,
            time.hours,
            time.minutes,
            time.seconds,
            time.milliseconds,
            time.microseconds,
            time.nanoseconds,
        )
    }

    /// Returns the a `Vec` of the fields values.
    #[inline]
    #[must_use]
    pub(crate) fn fields_signum(&self) -> [i64; 10] {
        [
            self.years().signum(),
            self.months().signum(),
            self.weeks().signum(),
            self.days().signum(),
            self.hours().signum(),
            self.minutes().signum(),
            self.seconds().signum(),
            self.milliseconds().signum(),
            self.microseconds().signum() as i64,
            self.nanoseconds().signum() as i64,
        ]
    }

    /// Returns whether `Duration`'s `DateDuration` is empty and is therefore a `TimeDuration`.
    #[inline]
    #[must_use]
    pub(crate) fn is_time_duration(&self) -> bool {
        self.date().fields().iter().all(|x| x == &0)
    }

    /// Returns the `Unit` corresponding to the largest non-zero field.
    #[inline]
    pub(crate) fn default_largest_unit(&self) -> Unit {
        self.fields_signum()
            .iter()
            .enumerate()
            .find(|x| x.1 != &0)
            .map(|x| Unit::from(10 - x.0))
            .unwrap_or(Unit::Nanosecond)
    }
}

// ==== Public Duration API ====

impl Duration {
    /// Creates a new validated `Duration`.
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        years: i64,
        months: i64,
        weeks: i64,
        days: i64,
        hours: i64,
        minutes: i64,
        seconds: i64,
        milliseconds: i64,
        microseconds: i128,
        nanoseconds: i128,
    ) -> TemporalResult<Self> {
        let duration = Self::new_unchecked(
            DateDuration::new_unchecked(years, months, weeks, days),
            TimeDuration::new_unchecked(
                hours,
                minutes,
                seconds,
                milliseconds,
                microseconds,
                nanoseconds,
            ),
        );
        if !is_valid_duration(
            years,
            months,
            weeks,
            days,
            hours,
            minutes,
            seconds,
            milliseconds,
            microseconds,
            nanoseconds,
        ) {
            return Err(TemporalError::range().with_message("Duration was not valid."));
        }
        Ok(duration)
    }

    /// Creates a `Duration` from a provided a day and a `TimeDuration`.
    ///
    /// Note: `TimeDuration` records can store a day value to deal with overflow.
    #[must_use]
    pub fn from_day_and_time(day: i64, time: &TimeDuration) -> Self {
        Self {
            date: DateDuration::new_unchecked(0, 0, 0, day),
            time: *time,
        }
    }

    /// Creates a `Duration` from a provided `PartialDuration`.
    pub fn from_partial_duration(partial: PartialDuration) -> TemporalResult<Self> {
        if partial == PartialDuration::default() {
            return Err(TemporalError::r#type()
                .with_message("PartialDuration cannot have all empty fields."));
        }
        Self::new(
            partial.years.unwrap_or_default(),
            partial.months.unwrap_or_default(),
            partial.weeks.unwrap_or_default(),
            partial.days.unwrap_or_default(),
            partial.hours.unwrap_or_default(),
            partial.minutes.unwrap_or_default(),
            partial.seconds.unwrap_or_default(),
            partial.milliseconds.unwrap_or_default(),
            partial.microseconds.unwrap_or_default(),
            partial.nanoseconds.unwrap_or_default(),
        )
    }

    // Converts a UTF-8 encoded string into a `Duration`.
    pub fn from_utf8(s: &[u8]) -> TemporalResult<Self> {
        let parse_record = IsoDurationParser::from_utf8(s)
            .parse()
            .map_err(|e| TemporalError::range().with_message(format!("{e}")))?;

        let (hours, minutes, seconds, millis, micros, nanos) = match parse_record.time {
            Some(TimeDurationRecord::Hours { hours, fraction }) => {
                let unadjusted_fraction =
                    fraction.and_then(|x| x.to_nanoseconds()).unwrap_or(0) as u64;
                let fractional_hours_ns = unadjusted_fraction * 3600;
                let minutes = fractional_hours_ns.div_euclid(60 * 1_000_000_000);
                let fractional_minutes_ns = fractional_hours_ns.rem_euclid(60 * 1_000_000_000);

                let seconds = fractional_minutes_ns.div_euclid(1_000_000_000);
                let fractional_seconds = fractional_minutes_ns.rem_euclid(1_000_000_000);

                let milliseconds = fractional_seconds.div_euclid(1_000_000);
                let rem = fractional_seconds.rem_euclid(1_000_000);

                let microseconds = rem.div_euclid(1_000);
                let nanoseconds = rem.rem_euclid(1_000);

                (
                    hours,
                    minutes,
                    seconds,
                    milliseconds,
                    microseconds,
                    nanoseconds,
                )
            }
            // Minutes variant is defined as { hours: u32, minutes: u32, fraction: u64 }
            Some(TimeDurationRecord::Minutes {
                hours,
                minutes,
                fraction,
            }) => {
                let unadjusted_fraction =
                    fraction.and_then(|x| x.to_nanoseconds()).unwrap_or(0) as u64;
                let fractional_minutes_ns = unadjusted_fraction * 60;
                let seconds = fractional_minutes_ns.div_euclid(1_000_000_000);
                let fractional_seconds = fractional_minutes_ns.rem_euclid(1_000_000_000);

                let milliseconds = fractional_seconds.div_euclid(1_000_000);
                let rem = fractional_seconds.rem_euclid(1_000_000);

                let microseconds = rem.div_euclid(1_000);
                let nanoseconds = rem.rem_euclid(1_000);

                (
                    hours,
                    minutes,
                    seconds,
                    milliseconds,
                    microseconds,
                    nanoseconds,
                )
            }
            // Seconds variant is defined as { hours: u32, minutes: u32, seconds: u32, fraction: u32 }
            Some(TimeDurationRecord::Seconds {
                hours,
                minutes,
                seconds,
                fraction,
            }) => {
                let ns = fraction.and_then(|x| x.to_nanoseconds()).unwrap_or(0);
                let milliseconds = ns.div_euclid(1_000_000);
                let rem = ns.rem_euclid(1_000_000);

                let microseconds = rem.div_euclid(1_000);
                let nanoseconds = rem.rem_euclid(1_000);

                (
                    hours,
                    minutes,
                    seconds,
                    milliseconds as u64,
                    microseconds as u64,
                    nanoseconds as u64,
                )
            }
            None => (0, 0, 0, 0, 0, 0),
        };

        let (years, months, weeks, days) = if let Some(date) = parse_record.date {
            (date.years, date.months, date.weeks, date.days)
        } else {
            (0, 0, 0, 0)
        };

        let sign = parse_record.sign as i64;

        Self::new(
            years as i64 * sign,
            months as i64 * sign,
            weeks as i64 * sign,
            days as i64 * sign,
            hours as i64 * sign,
            minutes as i64 * sign,
            seconds as i64 * sign,
            millis as i64 * sign,
            micros as i128 * sign as i128,
            nanos as i128 * sign as i128,
        )
    }

    /// Return if the Durations values are within their valid ranges.
    #[inline]
    #[must_use]
    pub fn is_time_within_range(&self) -> bool {
        self.time.is_within_range()
    }

    #[inline]
    pub fn compare_with_provider(
        &self,
        other: &Duration,
        relative_to: Option<RelativeTo>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Ordering> {
        if self.date == other.date && self.time == other.time {
            return Ok(Ordering::Equal);
        }
        // 8. Let largestUnit1 be DefaultTemporalLargestUnit(one).
        // 9. Let largestUnit2 be DefaultTemporalLargestUnit(two).
        let largest_unit_1 = self.default_largest_unit();
        let largest_unit_2 = other.default_largest_unit();
        // 10. Let duration1 be ToInternalDurationRecord(one).
        // 11. Let duration2 be ToInternalDurationRecord(two).
        // 12. If zonedRelativeTo is not undefined, and either UnitCategory(largestUnit1) or UnitCategory(largestUnit2) is date, then
        if let Some(RelativeTo::ZonedDateTime(zdt)) = relative_to.as_ref() {
            if largest_unit_1.is_date_unit() || largest_unit_2.is_date_unit() {
                // a. Let timeZone be zonedRelativeTo.[[TimeZone]].
                // b. Let calendar be zonedRelativeTo.[[Calendar]].
                // c. Let after1 be ? AddZonedDateTime(zonedRelativeTo.[[EpochNanoseconds]], timeZone, calendar, duration1, constrain).
                // d. Let after2 be ? AddZonedDateTime(zonedRelativeTo.[[EpochNanoseconds]], timeZone, calendar, duration2, constrain).
                let after1 = zdt.add_as_instant(self, ArithmeticOverflow::Constrain, provider)?;
                let after2 = zdt.add_as_instant(other, ArithmeticOverflow::Constrain, provider)?;
                // e. If after1 > after2, return 1𝔽.
                // f. If after1 < after2, return -1𝔽.
                // g. Return +0𝔽.
                return Ok(after1.cmp(&after2));
            }
        }
        // 13. If IsCalendarUnit(largestUnit1) is true or IsCalendarUnit(largestUnit2) is true, then
        let (days1, days2) =
            if largest_unit_1.is_calendar_unit() || largest_unit_2.is_calendar_unit() {
                // a. If plainRelativeTo is undefined, throw a RangeError exception.
                // b. Let days1 be ? DateDurationDays(duration1.[[Date]], plainRelativeTo).
                // c. Let days2 be ? DateDurationDays(duration2.[[Date]], plainRelativeTo).
                let Some(RelativeTo::PlainDate(pdt)) = relative_to.as_ref() else {
                    return Err(TemporalError::range());
                };
                let days1 = self.date.days(pdt)?;
                let days2 = other.date.days(pdt)?;
                (days1, days2)
            } else {
                (self.date.days, other.date.days)
            };
        // 15. Let timeDuration1 be ? Add24HourDaysToTimeDuration(duration1.[[Time]], days1).
        let time_duration_1 = self.time.to_normalized().add_days(days1)?;
        // 16. Let timeDuration2 be ? Add24HourDaysToTimeDuration(duration2.[[Time]], days2).
        let time_duration_2 = other.time.to_normalized().add_days(days2)?;
        // 17. Return 𝔽(CompareTimeDuration(timeDuration1, timeDuration2)).
        Ok(time_duration_1.cmp(&time_duration_2))
    }
}

// ==== Public `Duration` Getters/Setters ====

impl Duration {
    /// Returns a reference to the inner `TimeDuration`
    #[inline]
    #[must_use]
    pub fn time(&self) -> &TimeDuration {
        &self.time
    }

    /// Returns a reference to the inner `DateDuration`
    #[inline]
    #[must_use]
    pub fn date(&self) -> &DateDuration {
        &self.date
    }

    /// Returns the `years` field of duration.
    #[inline]
    #[must_use]
    pub const fn years(&self) -> i64 {
        self.date.years
    }

    /// Returns the `months` field of duration.
    #[inline]
    #[must_use]
    pub const fn months(&self) -> i64 {
        self.date.months
    }

    /// Returns the `weeks` field of duration.
    #[inline]
    #[must_use]
    pub const fn weeks(&self) -> i64 {
        self.date.weeks
    }

    /// Returns the `days` field of duration.
    #[inline]
    #[must_use]
    pub const fn days(&self) -> i64 {
        self.date.days
    }

    /// Returns the `hours` field of duration.
    #[inline]
    #[must_use]
    pub const fn hours(&self) -> i64 {
        self.time.hours
    }

    /// Returns the `hours` field of duration.
    #[inline]
    #[must_use]
    pub const fn minutes(&self) -> i64 {
        self.time.minutes
    }

    /// Returns the `seconds` field of duration.
    #[inline]
    #[must_use]
    pub const fn seconds(&self) -> i64 {
        self.time.seconds
    }

    /// Returns the `hours` field of duration.
    #[inline]
    #[must_use]
    pub const fn milliseconds(&self) -> i64 {
        self.time.milliseconds
    }

    /// Returns the `microseconds` field of duration.
    #[inline]
    #[must_use]
    pub const fn microseconds(&self) -> i128 {
        self.time.microseconds
    }

    /// Returns the `nanoseconds` field of duration.
    #[inline]
    #[must_use]
    pub const fn nanoseconds(&self) -> i128 {
        self.time.nanoseconds
    }
}

// ==== Public Duration methods ====

impl Duration {
    /// Determines the sign for the current self.
    #[inline]
    #[must_use]
    pub fn sign(&self) -> Sign {
        duration_sign(&self.fields_signum())
    }

    /// Returns whether the current `Duration` is zero.
    ///
    /// Equivalant to `Temporal.Duration.blank()`.
    #[inline]
    #[must_use]
    pub fn is_zero(&self) -> bool {
        self.sign() == Sign::Zero
    }

    /// Returns a negated `Duration`
    #[inline]
    #[must_use]
    pub fn negated(&self) -> Self {
        Self {
            date: self.date().negated(),
            time: self.time().negated(),
        }
    }

    /// Returns the absolute value of `Duration`.
    #[inline]
    #[must_use]
    pub fn abs(&self) -> Self {
        Self {
            date: self.date().abs(),
            time: self.time().abs(),
        }
    }

    /// Returns the result of adding a `Duration` to the current `Duration`
    #[inline]
    pub fn add(&self, other: &Self) -> TemporalResult<Self> {
        // NOTE: Implemented from AddDurations
        // Steps 1-22 are functionally useless in this context.

        // 23. Let largestUnit1 be DefaultTemporalLargestUnit(y1, mon1, w1, d1, h1, min1, s1, ms1, mus1).
        let largest_one = self.default_largest_unit();
        // 24. Let largestUnit2 be DefaultTemporalLargestUnit(y2, mon2, w2, d2, h2, min2, s2, ms2, mus2).
        let largest_two = other.default_largest_unit();
        // 25. Let largestUnit be LargerOfTwoUnits(largestUnit1, largestUnit2).
        let largest_unit = largest_one.max(largest_two);
        // 26. Let norm1 be NormalizeTimeDuration(h1, min1, s1, ms1, mus1, ns1).
        let norm_one = NormalizedTimeDuration::from_time_duration(self.time());
        // 27. Let norm2 be NormalizeTimeDuration(h2, min2, s2, ms2, mus2, ns2).
        let norm_two = NormalizedTimeDuration::from_time_duration(other.time());

        // 28. If IsCalendarUnit(largestUnit), throw a RangeError exception.
        if largest_unit.is_calendar_unit() {
            return Err(TemporalError::range().with_message(
                "Largest unit cannot be a calendar unit when adding two durations.",
            ));
        }

        // NOTE: for lines 488-489
        //
        // Maximum amount of days in a valid duration: 104_249_991_374 * 2 < i64::MAX
        // 29. Let normResult be ? AddNormalizedTimeDuration(norm1, norm2).
        // 30. Set normResult to ? Add24HourDaysToNormalizedTimeDuration(normResult, d1 + d2).
        let result = (norm_one + norm_two)?.add_days(
            self.days()
                .checked_add(other.days())
                .ok_or(TemporalError::range())?,
        )?;

        // 31. Let result be ? BalanceTimeDuration(normResult, largestUnit).
        let (result_days, result_time) = TimeDuration::from_normalized(result, largest_unit)?;

        // 32. Return ! CreateTemporalDuration(0, 0, 0, result.[[Days]], result.[[Hours]], result.[[Minutes]],
        // result.[[Seconds]], result.[[Milliseconds]], result.[[Microseconds]], result.[[Nanoseconds]]).
        Ok(Duration::from_day_and_time(result_days, &result_time))
    }

    /// Returns the result of subtracting a `Duration` from the current `Duration`
    #[inline]
    pub fn subtract(&self, other: &Self) -> TemporalResult<Self> {
        self.add(&other.negated())
    }

    #[inline]
    pub fn round_with_provider(
        &self,
        options: RoundingOptions,
        relative_to: Option<RelativeTo>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        // NOTE: Steps 1-14 seem to be implementation specific steps.
        // 14. Let roundingIncrement be ? ToTemporalRoundingIncrement(roundTo).
        // 15. Let roundingMode be ? ToRoundingMode(roundTo, "halfExpand").
        // 16. Let smallestUnit be ? GetUnit(roundTo, "smallestUnit", DATETIME, undefined).
        // 17. If smallestUnit is undefined, then
        // a. Set smallestUnitPresent to false.
        // b. Set smallestUnit to "nanosecond".
        // 18. Let existingLargestUnit be ! DefaultTemporalLargestUnit(duration.[[Years]],
        // duration.[[Months]], duration.[[Weeks]], duration.[[Days]], duration.[[Hours]],
        // duration.[[Minutes]], duration.[[Seconds]], duration.[[Milliseconds]],
        // duration.[[Microseconds]]).
        // 19. Let defaultLargestUnit be LargerOfTwoUnits(existingLargestUnit, smallestUnit).
        // 20. If largestUnit is undefined, then
        // a. Set largestUnitPresent to false.
        // b. Set largestUnit to defaultLargestUnit.
        // 21. Else if largestUnit is "auto", then
        // a. Set largestUnit to defaultLargestUnit.
        // 23. If LargerOfTwoUnits(largestUnit, smallestUnit) is not largestUnit, throw a RangeError exception.
        // 24. Let maximum be MaximumTemporalDurationRoundingIncrement(smallestUnit).
        // 25. If maximum is not undefined, perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, false).
        let existing_largest_unit = self.default_largest_unit();
        let resolved_options =
            ResolvedRoundingOptions::from_duration_options(options, existing_largest_unit)?;

        let is_zoned_datetime = matches!(relative_to, Some(RelativeTo::ZonedDateTime(_)));

        // 26. Let hoursToDaysConversionMayOccur be false.
        // 27. If duration.[[Days]] ≠ 0 and zonedRelativeTo is not undefined, set hoursToDaysConversionMayOccur to true.
        // 28. Else if abs(duration.[[Hours]]) ≥ 24, set hoursToDaysConversionMayOccur to true.
        let hours_to_days_may_occur =
            (self.days() != 0 && is_zoned_datetime) || self.hours().abs() >= 24;

        // 29. If smallestUnit is "nanosecond" and roundingIncrement = 1, let roundingGranularityIsNoop
        // be true; else let roundingGranularityIsNoop be false.
        // 30. If duration.[[Years]] = 0 and duration.[[Months]] = 0 and duration.[[Weeks]] = 0,
        // let calendarUnitsPresent be false; else let calendarUnitsPresent be true.
        let calendar_units_present =
            !(self.years() == 0 && self.months() == 0 && self.weeks() == 0);

        let is_noop = resolved_options.is_noop();

        // 31. If roundingGranularityIsNoop is true, and largestUnit is existingLargestUnit, and calendarUnitsPresent is false,
        // and hoursToDaysConversionMayOccur is false, and abs(duration.[[Minutes]]) < 60, and abs(duration.[[Seconds]]) < 60,
        // and abs(duration.[[Milliseconds]]) < 1000, and abs(duration.[[Microseconds]]) < 1000, and abs(duration.[[Nanoseconds]]) < 1000, then
        if is_noop
            && resolved_options.largest_unit == existing_largest_unit
            && !calendar_units_present
            && !hours_to_days_may_occur
            && self.minutes().abs() < 60
            && self.seconds().abs() < 60
            && self.milliseconds() < 1000
            && self.microseconds() < 1000
            && self.nanoseconds() < 1000
        {
            // a. NOTE: The above conditions mean that the operation will have no effect: the
            // smallest unit and rounding increment will leave the total duration unchanged,
            // and it can be determined without calling a calendar or time zone method that
            // no balancing will take place.
            // b. Return ! CreateTemporalDuration(duration.[[Years]], duration.[[Months]],
            // duration.[[Weeks]], duration.[[Days]], duration.[[Hours]], duration.[[Minutes]],
            // duration.[[Seconds]], duration.[[Milliseconds]], duration.[[Microseconds]],
            // duration.[[Nanoseconds]]).
            return Ok(*self);
        }

        // 32. Let precalculatedPlainDateTime be undefined.
        // 33. If roundingGranularityIsNoop is false, or IsCalendarUnit(largestUnit) is true, or largestUnit is "day",
        // or calendarUnitsPresent is true, or duration.[[Days]] ≠ 0, let plainDateTimeOrRelativeToWillBeUsed be true;
        // else let plainDateTimeOrRelativeToWillBeUsed be false.
        // 34. If zonedRelativeTo is not undefined and plainDateTimeOrRelativeToWillBeUsed is true, then
        // 35. Let calendarRec be ? CreateCalendarMethodsRecordFromRelativeTo(plainRelativeTo, zonedRelativeTo, « DATE-ADD, DATE-UNTIL »).

        // 36. Let norm be NormalizeTimeDuration(duration.[[Hours]], duration.[[Minutes]], duration.[[Seconds]], duration.[[Milliseconds]],
        // duration.[[Microseconds]], duration.[[Nanoseconds]]).
        let norm = NormalizedTimeDuration::from_time_duration(self.time());
        // 37. Let emptyOptions be OrdinaryObjectCreate(null).

        match relative_to {
            // 38. If zonedRelativeTo is not undefined, then
            Some(RelativeTo::ZonedDateTime(zoned_datetime)) => {
                // a. Let relativeEpochNs be zonedRelativeTo.[[Nanoseconds]].
                // b. Let relativeInstant be ! CreateTemporalInstant(relativeEpochNs).
                // c. Let targetEpochNs be ? AddZonedDateTime(relativeInstant, timeZoneRec, calendarRec, duration.[[Years]], duration.[[Months]], duration.[[Weeks]], duration.[[Days]], norm, precalculatedPlainDateTime).
                let target_epoch_ns =
                    zoned_datetime.add_as_instant(self, ArithmeticOverflow::Constrain, provider)?;
                // d. Let roundRecord be ? DifferenceZonedDateTimeWithRounding(relativeEpochNs, targetEpochNs, calendarRec, timeZoneRec, precalculatedPlainDateTime, emptyOptions, largestUnit, roundingIncrement, smallestUnit, roundingMode).
                // e. Let roundResult be roundRecord.[[DurationRecord]].
                let internal = zoned_datetime.diff_with_rounding(
                    &ZonedDateTime::new_unchecked(
                        target_epoch_ns,
                        zoned_datetime.calendar().clone(),
                        zoned_datetime.timezone().clone(),
                    ),
                    resolved_options,
                    provider,
                )?;
                Duration::from_normalized(internal, resolved_options.largest_unit)
            }
            // 39. Else if plainRelativeTo is not undefined, then
            Some(RelativeTo::PlainDate(plain_date)) => {
                // a. Let targetTime be AddTime(0, 0, 0, 0, 0, 0, norm).
                let (balanced_days, time) = PlainTime::default().add_normalized_time_duration(norm);
                // b. Let dateDuration be ? CreateTemporalDuration(duration.[[Years]], duration.[[Months]], duration.[[Weeks]],
                // duration.[[Days]] + targetTime.[[Days]], 0, 0, 0, 0, 0, 0).
                let date_duration = DateDuration::new(
                    self.years(),
                    self.months(),
                    self.weeks(),
                    self.days()
                        .checked_add(balanced_days)
                        .ok_or(TemporalError::range())?,
                )?;
                // NOTE (remove): values are fine to this point.
                // TODO: Should this be using AdjustDateDurationRecord?

                // c. Let targetDate be ? AddDate(calendarRec, plainRelativeTo, dateDuration).
                let target_date = plain_date.add_date(&Duration::from(date_duration), None)?;

                let plain_dt = PlainDateTime::new_unchecked(
                    IsoDateTime::new(plain_date.iso, IsoTime::default())?,
                    plain_date.calendar().clone(),
                );

                let target_dt = PlainDateTime::new_unchecked(
                    IsoDateTime::new(target_date.iso, time.iso)?,
                    target_date.calendar().clone(),
                );

                // d. Let roundRecord be ? DifferencePlainDateTimeWithRounding(plainRelativeTo.[[ISOYear]], plainRelativeTo.[[ISOMonth]],
                // plainRelativeTo.[[ISODay]], 0, 0, 0, 0, 0, 0, targetDate.[[ISOYear]], targetDate.[[ISOMonth]], targetDate.[[ISODay]],
                // targetTime.[[Hours]], targetTime.[[Minutes]], targetTime.[[Seconds]], targetTime.[[Milliseconds]],
                // targetTime.[[Microseconds]], targetTime.[[Nanoseconds]], calendarRec, largestUnit, roundingIncrement,
                // smallestUnit, roundingMode, emptyOptions).
                let round_record = plain_dt.diff_dt_with_rounding(&target_dt, resolved_options)?;

                // e. Let roundResult be roundRecord.[[DurationRecord]].
                Duration::from_normalized(round_record, resolved_options.largest_unit)
            }
            // TODO (nekevss): Align the above steps with the updates ones from below.
            None => {
                // 28. If calendarUnitsPresent is true, or IsCalendarUnit(largestUnit) is true, throw a RangeError exception.
                if calendar_units_present || resolved_options.largest_unit.is_calendar_unit() {
                    return Err(TemporalError::range().with_message(
                        "Calendar units cannot be present without a relative point.",
                    ));
                }
                // 29. Assert: IsCalendarUnit(smallestUnit) is false.
                temporal_assert!(
                    !resolved_options.smallest_unit.is_calendar_unit(),
                    "Assertion failed: resolvedOptions contains a calendar unit\n{:?}",
                    resolved_options
                );
                // 30. Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
                let internal = NormalizedDurationRecord::from_duration_with_24_hour_days(self)?;
                // 31. If smallestUnit is day, then
                let internal = if resolved_options.smallest_unit == Unit::Day {
                    // a. Let fractionalDays be TotalTimeDuration(internalDuration.[[Time]], day).
                    // b. Let days be RoundNumberToIncrement(fractionalDays, roundingIncrement, roundingMode).
                    let days = internal
                        .normalized_time_duration()
                        .round_to_fractional_days(
                            resolved_options.increment,
                            resolved_options.rounding_mode,
                        )?;
                    // c. Let dateDuration be ? CreateDateDurationRecord(0, 0, 0, days).
                    let date = DateDuration::new(0, 0, 0, days)?;
                    // d. Set internalDuration to CombineDateAndTimeDuration(dateDuration, 0).
                    NormalizedDurationRecord::new(date, NormalizedTimeDuration::default())?
                // 32. Else,
                } else {
                    // TODO: update round / round_inner methods
                    // a. Let timeDuration be ? RoundTimeDuration(internalDuration.[[Time]], roundingIncrement, smallestUnit, roundingMode).
                    let divisor = resolved_options
                        .smallest_unit
                        .as_nanoseconds()
                        .temporal_unwrap()?;
                    let increment = resolved_options
                        .increment
                        .as_extended_increment()
                        .checked_mul(NonZeroU128::new(divisor.into()).expect("cannot fail"))
                        .temporal_unwrap()?;
                    let normalized_time = internal
                        .normalized_time_duration()
                        .round_inner(increment, resolved_options.rounding_mode)?;
                    // b. Set internalDuration to CombineDateAndTimeDuration(ZeroDateDuration(), timeDuration).
                    NormalizedDurationRecord::new(DateDuration::default(), normalized_time)?
                };
                // 33. Return ? TemporalDurationFromInternal(internalDuration, largestUnit).
                Duration::from_normalized(internal, resolved_options.largest_unit)
            }
        }
    }

    /// Returns the total of the `Duration`
    pub fn total_with_provider(
        &self,
        unit: Unit,
        relative_to: Option<RelativeTo>,
        provider: &impl TimeZoneProvider,
        // Review question what is the return type of duration.prototye.total?
    ) -> TemporalResult<FiniteF64> {
        match relative_to {
            // 11. If zonedRelativeTo is not undefined, then
            Some(RelativeTo::ZonedDateTime(zoned_datetime)) => {
                // a. Let internalDuration be ToInternalDurationRecord(duration).
                // b. Let timeZone be zonedRelativeTo.[[TimeZone]].
                // c. Let calendar be zonedRelativeTo.[[Calendar]].
                // d. Let relativeEpochNs be zonedRelativeTo.[[EpochNanoseconds]].
                // e. Let targetEpochNs be ? AddZonedDateTime(relativeEpochNs, timeZone, calendar, internalDuration, constrain).
                let target_epcoh_ns =
                    zoned_datetime.add_as_instant(self, ArithmeticOverflow::Constrain, provider)?;
                // f. Let total be ? DifferenceZonedDateTimeWithTotal(relativeEpochNs, targetEpochNs, timeZone, calendar, unit).
                let total = zoned_datetime.diff_with_total(
                    &ZonedDateTime::new_unchecked(
                        target_epcoh_ns,
                        zoned_datetime.calendar().clone(),
                        zoned_datetime.timezone().clone(),
                    ),
                    unit,
                    provider,
                )?;
                Ok(total)
            }
            // 12. Else if plainRelativeTo is not undefined, then
            Some(RelativeTo::PlainDate(plain_date)) => {
                // a. Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
                // b. Let targetTime be AddTime(MidnightTimeRecord(), internalDuration.[[Time]]).
                let (balanced_days, time) =
                    PlainTime::default().add_normalized_time_duration(self.time.to_normalized());
                // c. Let calendar be plainRelativeTo.[[Calendar]].
                // d. Let dateDuration be ! AdjustDateDurationRecord(internalDuration.[[Date]], targetTime.[[Days]]).
                let date_duration = DateDuration::new(
                    self.years(),
                    self.months(),
                    self.weeks(),
                    self.days()
                        .checked_add(balanced_days)
                        .ok_or(TemporalError::range())?,
                )?;
                // e. Let targetDate be ? CalendarDateAdd(calendar, plainRelativeTo.[[ISODate]], dateDuration, constrain).
                let target_date = plain_date.calendar().date_add(
                    &plain_date.iso,
                    &Duration::from(date_duration),
                    ArithmeticOverflow::Constrain,
                )?;
                // f. Let isoDateTime be CombineISODateAndTimeRecord(plainRelativeTo.[[ISODate]], MidnightTimeRecord()).
                let iso_date_time = IsoDateTime::new_unchecked(plain_date.iso, IsoTime::default());
                // g. Let targetDateTime be CombineISODateAndTimeRecord(targetDate, targetTime).
                let target_date_time = IsoDateTime::new_unchecked(target_date.iso, time.iso);
                // h. Let total be ? DifferencePlainDateTimeWithTotal(isoDateTime, targetDateTime, calendar, unit).
                let plain_dt =
                    PlainDateTime::new_unchecked(iso_date_time, plain_date.calendar().clone());
                let total = plain_dt.diff_dt_with_total(
                    &PlainDateTime::new_unchecked(target_date_time, plain_date.calendar().clone()),
                    unit,
                )?;
                Ok(total)
            }
            None => {
                // a. Let largestUnit be DefaultTemporalLargestUnit(duration).
                let largest_unit = self.default_largest_unit();
                // b. If IsCalendarUnit(largestUnit) is true, or IsCalendarUnit(unit) is true, throw a RangeError exception.
                if largest_unit.is_calendar_unit() || unit.is_calendar_unit() {
                    return Err(TemporalError::range());
                }
                // c. Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
                let internal = NormalizedDurationRecord::from_duration_with_24_hour_days(self)?;
                // d. Let total be TotalTimeDuration(internalDuration.[[Time]], unit).
                let total = internal.normalized_time_duration().total(unit)?;
                Ok(total)
            }
        }
    }

    /// Returns the `Duration` as a formatted string
    pub fn as_temporal_string(&self, options: ToStringRoundingOptions) -> TemporalResult<String> {
        if options.smallest_unit == Some(Unit::Hour) || options.smallest_unit == Some(Unit::Minute)
        {
            return Err(TemporalError::range().with_message(
                "string rounding options cannot have hour or minute smallest unit.",
            ));
        }

        let resolved_options = options.resolve()?;
        if resolved_options.smallest_unit == Unit::Nanosecond
            && resolved_options.increment == RoundingIncrement::ONE
        {
            let duration = duration_to_formattable(self, resolved_options.precision)?;
            return Ok(duration.to_string());
        }

        let rounding_options = ResolvedRoundingOptions::from_to_string_options(&resolved_options);

        // 11. Let largestUnit be DefaultTemporalLargestUnit(duration).
        let largest = self.default_largest_unit();
        // 12. Let internalDuration be ToInternalDurationRecord(duration).
        let norm = NormalizedDurationRecord::new(
            self.date,
            NormalizedTimeDuration::from_time_duration(&self.time),
        )?;
        // 13. Let timeDuration be ? RoundTimeDuration(internalDuration.[[Time]], precision.[[Increment]], precision.[[Unit]], roundingMode).
        let time = norm.normalized_time_duration().round(rounding_options)?;
        // 14. Set internalDuration to CombineDateAndTimeDuration(internalDuration.[[Date]], timeDuration).
        let norm = NormalizedDurationRecord::new(norm.date(), time)?;
        // 15. Let roundedLargestUnit be LargerOfTwoUnits(largestUnit, second).
        let rounded_largest = largest.max(Unit::Second);
        // 16. Let roundedDuration be ? TemporalDurationFromInternal(internalDuration, roundedLargestUnit).
        let rounded = Self::from_normalized(norm, rounded_largest)?;

        // 17. Return TemporalDurationToString(roundedDuration, precision.[[Precision]]).
        Ok(duration_to_formattable(&rounded, resolved_options.precision)?.to_string())
    }
}

pub fn duration_to_formattable(
    duration: &Duration,
    precision: Precision,
) -> TemporalResult<FormattableDuration> {
    let sign = duration.sign();
    let duration = duration.abs();
    let date = duration.years() + duration.months() + duration.weeks() + duration.days();
    let date = if date != 0 {
        Some(FormattableDateDuration {
            years: duration.years() as u32,
            months: duration.months() as u32,
            weeks: duration.weeks() as u32,
            days: duration.days() as u64,
        })
    } else {
        None
    };

    let hours = duration.hours().abs();
    let minutes = duration.minutes().abs();

    let time = NormalizedTimeDuration::from_time_duration(&TimeDuration::new_unchecked(
        0,
        0,
        duration.seconds(),
        duration.milliseconds(),
        duration.microseconds(),
        duration.nanoseconds(),
    ));

    let seconds = time.seconds().unsigned_abs();
    let subseconds = time.subseconds().unsigned_abs();

    let time = Some(FormattableTimeDuration::Seconds(
        hours as u64,
        minutes as u64,
        seconds,
        Some(subseconds),
    ));

    Ok(FormattableDuration {
        precision,
        sign,
        date,
        time,
    })
}

// TODO: Update, optimize, and fix the below. is_valid_duration should probably be generic over a T.

const TWO_POWER_FIFTY_THREE: i128 = 9_007_199_254_740_992;
const MAX_SAFE_NS_PRECISION: i128 = TWO_POWER_FIFTY_THREE * 1_000_000_000;

// NOTE: Can FiniteF64 optimize the duration_validation
/// Utility function to check whether the `Duration` fields are valid.
#[inline]
#[must_use]
#[allow(clippy::too_many_arguments)]
pub(crate) fn is_valid_duration(
    years: i64,
    months: i64,
    weeks: i64,
    days: i64,
    hours: i64,
    minutes: i64,
    seconds: i64,
    milliseconds: i64,
    microseconds: i128,
    nanoseconds: i128,
) -> bool {
    // 1. Let sign be ! DurationSign(years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds).
    let set = [
        years,
        months,
        weeks,
        days,
        hours,
        minutes,
        seconds,
        milliseconds,
        microseconds.signum() as i64,
        nanoseconds.signum() as i64,
    ];
    let sign = duration_sign(&set);
    // 2. For each value v of « years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds », do
    for v in set {
        // FiniteF64 must always be finite.
        // a. If 𝔽(v) is not finite, return false.
        // b. If v < 0 and sign > 0, return false.
        if v < 0 && sign == Sign::Positive {
            return false;
        }
        // c. If v > 0 and sign < 0, return false.
        if v > 0 && sign == Sign::Negative {
            return false;
        }
    }
    // 3. If abs(years) ≥ 2**32, return false.
    if years.abs() >= u32::MAX as i64 {
        return false;
    };
    // 4. If abs(months) ≥ 2**32, return false.
    if months.abs() >= u32::MAX as i64 {
        return false;
    };
    // 5. If abs(weeks) ≥ 2**32, return false.
    if weeks.abs() >= u32::MAX as i64 {
        return false;
    };

    // 6. Let normalizedSeconds be days × 86,400 + hours × 3600 + minutes × 60 + seconds
    // + ℝ(𝔽(milliseconds)) × 10**-3 + ℝ(𝔽(microseconds)) × 10**-6 + ℝ(𝔽(nanoseconds)) × 10**-9.
    // 7. NOTE: The above step cannot be implemented directly using floating-point arithmetic.
    // Multiplying by 10**-3, 10**-6, and 10**-9 respectively may be imprecise when milliseconds,
    // microseconds, or nanoseconds is an unsafe integer. This multiplication can be implemented
    // in C++ with an implementation of core::remquo() with sufficient bits in the quotient.
    // String manipulation will also give an exact result, since the multiplication is by a power of 10.
    // Seconds part
    // TODO: Fix the below parts after clarification around behavior.
    let normalized_nanoseconds = (days as i128 * NS_PER_DAY as i128)
        + (hours as i128) * 3_600_000_000_000
        + minutes as i128 * 60_000_000_000
        + seconds as i128 * 1_000_000_000;
    // Subseconds part
    let normalized_subseconds_parts =
        (milliseconds as i128 * 1_000_000) + (microseconds * 1_000) + nanoseconds;

    let total_normalized_seconds = normalized_nanoseconds + normalized_subseconds_parts;
    // 8. If abs(normalizedSeconds) ≥ 2**53, return false.
    if total_normalized_seconds.abs() >= MAX_SAFE_NS_PRECISION {
        return false;
    }

    // 9. Return true.
    true
}

/// Utility function for determining the sign for the current set of `Duration` fields.
///
/// Equivalent: 7.5.10 `DurationSign ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds )`
#[inline]
#[must_use]
fn duration_sign(set: &[i64]) -> Sign {
    // 1. For each value v of « years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds », do
    for v in set {
        // a. If v < 0, return -1.
        // b. If v > 0, return 1.
        match (*v).cmp(&0) {
            Ordering::Less => return Sign::Negative,
            Ordering::Greater => return Sign::Positive,
            _ => {}
        }
    }
    // 2. Return 0.
    Sign::Zero
}

impl From<TimeDuration> for Duration {
    fn from(value: TimeDuration) -> Self {
        Self {
            time: value,
            date: DateDuration::default(),
        }
    }
}

impl From<DateDuration> for Duration {
    fn from(value: DateDuration) -> Self {
        Self {
            date: value,
            time: TimeDuration::default(),
        }
    }
}

// ==== FromStr trait impl ====

impl FromStr for Duration {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let parse_record = IsoDurationParser::from_str(s)
            .parse()
            .map_err(|e| TemporalError::range().with_message(format!("{e}")))?;

        let (hours, minutes, seconds, millis, micros, nanos) = match parse_record.time {
            Some(TimeDurationRecord::Hours { hours, fraction }) => {
                let unadjusted_fraction =
                    fraction.and_then(|x| x.to_nanoseconds()).unwrap_or(0) as u64;
                let fractional_hours_ns = unadjusted_fraction * 3600;
                let minutes = fractional_hours_ns.div_euclid(60 * 1_000_000_000);
                let fractional_minutes_ns = fractional_hours_ns.rem_euclid(60 * 1_000_000_000);

                let seconds = fractional_minutes_ns.div_euclid(1_000_000_000);
                let fractional_seconds = fractional_minutes_ns.rem_euclid(1_000_000_000);

                let milliseconds = fractional_seconds.div_euclid(1_000_000);
                let rem = fractional_seconds.rem_euclid(1_000_000);

                let microseconds = rem.div_euclid(1_000);
                let nanoseconds = rem.rem_euclid(1_000);

                (
                    hours,
                    minutes,
                    seconds,
                    milliseconds,
                    microseconds,
                    nanoseconds,
                )
            }
            // Minutes variant is defined as { hours: u32, minutes: u32, fraction: u64 }
            Some(TimeDurationRecord::Minutes {
                hours,
                minutes,
                fraction,
            }) => {
                let unadjusted_fraction =
                    fraction.and_then(|x| x.to_nanoseconds()).unwrap_or(0) as u64;
                let fractional_minutes_ns = unadjusted_fraction * 60;
                let seconds = fractional_minutes_ns.div_euclid(1_000_000_000);
                let fractional_seconds = fractional_minutes_ns.rem_euclid(1_000_000_000);

                let milliseconds = fractional_seconds.div_euclid(1_000_000);
                let rem = fractional_seconds.rem_euclid(1_000_000);

                let microseconds = rem.div_euclid(1_000);
                let nanoseconds = rem.rem_euclid(1_000);

                (
                    hours,
                    minutes,
                    seconds,
                    milliseconds,
                    microseconds,
                    nanoseconds,
                )
            }
            // Seconds variant is defined as { hours: u32, minutes: u32, seconds: u32, fraction: u32 }
            Some(TimeDurationRecord::Seconds {
                hours,
                minutes,
                seconds,
                fraction,
            }) => {
                let ns = fraction.and_then(|x| x.to_nanoseconds()).unwrap_or(0);
                let milliseconds = ns.div_euclid(1_000_000);
                let rem = ns.rem_euclid(1_000_000);

                let microseconds = rem.div_euclid(1_000);
                let nanoseconds = rem.rem_euclid(1_000);

                (
                    hours,
                    minutes,
                    seconds,
                    milliseconds as u64,
                    microseconds as u64,
                    nanoseconds as u64,
                )
            }
            None => (0, 0, 0, 0, 0, 0),
        };

        let (years, months, weeks, days) = if let Some(date) = parse_record.date {
            (date.years, date.months, date.weeks, date.days)
        } else {
            (0, 0, 0, 0)
        };

        let sign = parse_record.sign as i64;

        Self::new(
            years as i64 * sign,
            months as i64 * sign,
            weeks as i64 * sign,
            days as i64 * sign,
            hours as i64 * sign,
            minutes as i64 * sign,
            seconds as i64 * sign,
            millis as i64 * sign,
            micros as i128 * sign as i128,
            nanos as i128 * sign as i128,
        )
    }
}
