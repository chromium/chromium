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
    temporal_assert, Sign, TemporalError, TemporalResult, NS_PER_DAY,
};
use alloc::format;
use alloc::string::String;
use core::{cmp::Ordering, str::FromStr};
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
/// Represents a span of time such as "2 hours and 30 minutes" or "3 years, 2 months".
/// Unlike `Instant` which represents a specific moment in time, Duration represents
/// the amount of time between two moments.
///
/// A Duration consists of two categories of components:
/// - Date components: years, months, weeks, and days
/// - Time components: hours, minutes, seconds, and subsecond units (nanosecond precision)
///
/// Note that date arithmetic can be complex. For example, adding "1 month" to January 31st
/// could result in February 28th (non-leap year), February 29th (leap year), or March 3rd
/// (if you overflow February), depending on the calendar system and overflow handling.
///
/// ## Examples
///
/// ### Creating durations
///
/// ```rust
/// use temporal_rs::Duration;
///
/// // Create a duration with specific components
/// let vacation_duration = Duration::new(
///     0, 0, 2, 3,    // 2 weeks and 3 days
///     0, 0, 0,       // no time components
///     0, 0, 0        // no subsecond components
/// ).unwrap();
///
/// assert_eq!(vacation_duration.weeks(), 2);
/// assert_eq!(vacation_duration.days(), 3);
/// ```
///
/// ### Parsing ISO 8601 duration strings
///
/// ```rust
/// use temporal_rs::Duration;
/// use core::str::FromStr;
///
/// // Complex duration with multiple components
/// let complex = Duration::from_str("P1Y2M3DT4H5M6.789S").unwrap();
/// assert_eq!(complex.years(), 1);
/// assert_eq!(complex.months(), 2);
/// assert_eq!(complex.days(), 3);
/// assert_eq!(complex.hours(), 4);
/// assert_eq!(complex.minutes(), 5);
/// assert_eq!(complex.seconds(), 6);
///
/// // Time-only duration
/// let movie_length = Duration::from_str("PT2H30M").unwrap();
/// assert_eq!(movie_length.hours(), 2);
/// assert_eq!(movie_length.minutes(), 30);
///
/// // Negative durations
/// let negative = Duration::from_str("-P1D").unwrap();
/// assert_eq!(negative.days(), -1);
/// ```
///
/// ### Duration arithmetic
///
/// ```rust
/// use temporal_rs::Duration;
/// use core::str::FromStr;
///
/// let commute_time = Duration::from_str("PT45M").unwrap();
/// let lunch_break = Duration::from_str("PT1H").unwrap();
///
/// // Add durations together
/// let total_time = commute_time.add(&lunch_break).unwrap();
/// assert_eq!(total_time.hours(), 1);    // Results in 1 hour 45 minutes
/// assert_eq!(total_time.minutes(), 45);
///
/// // Subtract duration components
/// let shortened = lunch_break.subtract(&Duration::from_str("PT15M").unwrap()).unwrap();
/// assert_eq!(shortened.minutes(), 45);
/// ```
///
/// ### Date arithmetic with durations
///
/// ```rust
/// use temporal_rs::{PlainDate, Duration, options::ArithmeticOverflow};
/// use core::str::FromStr;
///
/// // January 31st in different years
/// let jan_31_2023 = PlainDate::try_new_iso(2023, 1, 31).unwrap();
/// let jan_31_2024 = PlainDate::try_new_iso(2024, 1, 31).unwrap(); // leap year
///
/// let one_month = Duration::from_str("P1M").unwrap();
///
/// // Adding 1 month to Jan 31st gives different results:
/// let feb_2023 = jan_31_2023.add(&one_month, Some(ArithmeticOverflow::Constrain)).unwrap();
/// let feb_2024 = jan_31_2024.add(&one_month, Some(ArithmeticOverflow::Constrain)).unwrap();
///
/// // 2023: Jan 31 + 1 month = Feb 28 (no Feb 31st exists)
/// assert_eq!(feb_2023.day(), 28);
/// // 2024: Jan 31 + 1 month = Feb 29 (leap year)  
/// assert_eq!(feb_2024.day(), 29);
/// ```
///
/// ### Working with partial durations
///
/// ```rust
/// use temporal_rs::{Duration, partial::PartialDuration};
///
/// let partial = PartialDuration {
///     hours: Some(3),
///     minutes: Some(45),
///     ..Default::default()
/// };
///
/// let duration = Duration::from_partial_duration(partial).unwrap();
/// assert_eq!(duration.hours(), 3);
/// assert_eq!(duration.minutes(), 45);
/// assert_eq!(duration.days(), 0); // other fields default to 0
/// ```
///
/// ### Duration properties
///
/// ```rust
/// use temporal_rs::Duration;
/// use core::str::FromStr;
///
/// let duration = Duration::from_str("P1Y2M3D").unwrap();
///
/// // Check if duration is zero
/// assert!(!duration.is_zero());
///
/// // Get the sign of the duration
/// let sign = duration.sign();
/// // Note: This particular duration has mixed signs which affects the overall sign
///
/// // Get absolute value
/// let abs_duration = duration.abs();
/// assert_eq!(abs_duration.years(), 1);
/// assert_eq!(abs_duration.months(), 2);
/// assert_eq!(abs_duration.days(), 3);
///
/// // Negate duration
/// let negated = duration.negated();
/// assert_eq!(negated.years(), -1);
/// assert_eq!(negated.months(), -2);
/// assert_eq!(negated.days(), -3);
/// ```
///
/// ## Reference
///
/// For more information, see the [MDN documentation][mdn-duration].
///
/// [mdn-duration]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Temporal/Duration
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

    /// `17.3.20 Temporal.Duration.prototype.round ( roundTo )`
    ///
    /// Spec: <https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.round>
    ///
    // Spec last accessed: 2025-05-16, <https://github.com/tc39/proposal-temporal/tree/c150e7135c56afc9114032e93b53ac49f980d254>
    #[inline]
    pub fn round_with_provider(
        &self,
        options: RoundingOptions,
        relative_to: Option<RelativeTo>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        // NOTE(HalidOdat): Steps 1-12 are handled before calling the function.
        //
        // SKIP: 1. Let duration be the this value.
        // SKIP: 2. Perform ? RequireInternalSlot(duration, [[InitializedTemporalDuration]]).
        // SKIP: 3. If roundTo is undefined, then
        // SKIP:    a. Throw a TypeError exception.
        // SKIP: 4. If roundTo is a String, then
        // SKIP:    a. Let paramString be roundTo.
        // SKIP:    b. Set roundTo to OrdinaryObjectCreate(null).
        // SKIP:    c. Perform ! CreateDataPropertyOrThrow(roundTo, "smallestUnit", paramString).
        // SKIP: 5. Else,
        // SKIP:    a. Set roundTo to ? GetOptionsObject(roundTo).
        // SKIP: 6. Let smallestUnitPresent be true.
        // SKIP: 7. Let largestUnitPresent be true.
        // SKIP: 8. NOTE: The following steps read options and perform independent validation in alphabetical order
        //          (GetTemporalRelativeToOption reads "relativeTo",
        //           GetRoundingIncrementOption reads "roundingIncrement"
        //           and GetRoundingModeOption reads "roundingMode").
        // SKIP: 9. Let largestUnit be ? GetTemporalUnitValuedOption(roundTo, "largestUnit", datetime, unset, « auto »).
        // SKIP: 10. Let relativeToRecord be ? GetTemporalRelativeToOption(roundTo).
        // SKIP: 11. Let zonedRelativeTo be relativeToRecord.[[ZonedRelativeTo]].
        // SKIP: 12. Let plainRelativeTo be relativeToRecord.[[PlainRelativeTo]].

        // 13. Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).
        let rounding_increment = options.increment.unwrap_or_default();

        // 14. Let roundingMode be ? GetRoundingModeOption(roundTo, half-expand).
        let rounding_mode = options.rounding_mode.unwrap_or_default();

        // 15. Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo, "smallestUnit", datetime, unset).
        let smallest_unit = options.smallest_unit;

        // 16. If smallestUnit is unset, then
        //     a. Set smallestUnitPresent to false.
        //     b. Set smallestUnit to nanosecond.
        let smallest_unit = smallest_unit.unwrap_or(Unit::Nanosecond);

        // 17. Let existingLargestUnit be DefaultTemporalLargestUnit(duration).
        let existing_largest_unit = self.default_largest_unit();

        // 18. Let defaultLargestUnit be LargerOfTwoTemporalUnits(existingLargestUnit, smallestUnit).
        let default_largest_unit = Unit::larger(existing_largest_unit, smallest_unit)?;

        let largest_unit = match options.largest_unit {
            // 19. If largestUnit is unset, then
            //     a. Set largestUnitPresent to false.
            //     b. Set largestUnit to defaultLargestUnit.
            // 20. Else if largestUnit is auto, then
            //     a. Set largestUnit to defaultLargestUnit.
            Some(Unit::Auto) | None => default_largest_unit,
            Some(unit) => unit,
        };

        // 21. If smallestUnitPresent is false and largestUnitPresent is false, then
        if options.largest_unit.is_none() && options.smallest_unit.is_none() {
            // a. Throw a RangeError exception.
            return Err(TemporalError::range()
                .with_message("smallestUnit and largestUnit cannot both be None."));
        }

        // 22. If LargerOfTwoTemporalUnits(largestUnit, smallestUnit) is not largestUnit, throw a RangeError exception.
        if Unit::larger(largest_unit, smallest_unit)? != largest_unit {
            return Err(
                TemporalError::range().with_message("smallestUnit is larger than largestUnit.")
            );
        }

        // 23. Let maximum be MaximumTemporalDurationRoundingIncrement(smallestUnit).
        let maximum = smallest_unit.to_maximum_rounding_increment();

        // 24. If maximum is not unset, perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, false).
        if let Some(maximum) = maximum {
            rounding_increment.validate(maximum.into(), false)?;
        }

        // 25. If roundingIncrement > 1, and largestUnit is not smallestUnit, and TemporalUnitCategory(smallestUnit) is date, throw a RangeError exception.
        if rounding_increment > RoundingIncrement::ONE
            && largest_unit != smallest_unit
            && smallest_unit.is_calendar_unit()
        {
            return Err(TemporalError::range().with_message(
                "roundingIncrement > 1 and largest_unit is not smallest_unit and smallest_unit is date",
            ));
        }

        let resolved_options = ResolvedRoundingOptions {
            largest_unit,
            smallest_unit,
            increment: rounding_increment,
            rounding_mode,
        };

        match relative_to {
            // 26. If zonedRelativeTo is not undefined, then
            Some(RelativeTo::ZonedDateTime(zoned_relative_to)) => {
                // a. Let internalDuration be ToInternalDurationRecord(duration).

                // b. Let timeZone be zonedRelativeTo.[[TimeZone]].
                let time_zone = zoned_relative_to.timezone().clone();

                // c. Let calendar be zonedRelativeTo.[[Calendar]].
                let calendar = zoned_relative_to.calendar().clone();

                // d. Let relativeEpochNs be zonedRelativeTo.[[EpochNanoseconds]].
                // let relative_epoch_ns = zoned_relative_to.epoch_nanoseconds();

                // e. Let targetEpochNs be ? AddZonedDateTime(relativeEpochNs, timeZone, calendar, internalDuration, constrain).
                let target_epoch_ns = zoned_relative_to.add_as_instant(
                    self,
                    ArithmeticOverflow::Constrain,
                    provider,
                )?;

                // f. Set internalDuration to ? DifferenceZonedDateTimeWithRounding(relativeEpochNs, targetEpochNs, timeZone, calendar, largestUnit, roundingIncrement, smallestUnit, roundingMode).
                let internal = zoned_relative_to.diff_with_rounding(
                    &ZonedDateTime::new_unchecked(target_epoch_ns, calendar, time_zone),
                    resolved_options,
                    provider,
                )?;

                // g. If TemporalUnitCategory(largestUnit) is date, set largestUnit to hour.
                let mut largest_unit = resolved_options.largest_unit;
                if resolved_options.largest_unit.is_date_unit() {
                    largest_unit = Unit::Hour;
                }

                // h. Return ? TemporalDurationFromInternal(internalDuration, largestUnit).
                return Duration::from_normalized(internal, largest_unit);
            }

            // 27. If plainRelativeTo is not undefined, then
            Some(RelativeTo::PlainDate(plain_relative_to)) => {
                // a. Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
                let internal_duration =
                    NormalizedDurationRecord::from_duration_with_24_hour_days(self)?;

                // b. Let targetTime be AddTime(MidnightTimeRecord(), internalDuration.[[Time]]).
                let (target_time_days, target_time) = PlainTime::default()
                    .add_normalized_time_duration(internal_duration.normalized_time_duration());

                // c. Let calendar be plainRelativeTo.[[Calendar]].
                let calendar = plain_relative_to.calendar();

                // d. Let dateDuration be ! AdjustDateDurationRecord(internalDuration.[[Date]], targetTime.[[Days]]).
                let date_duration =
                    internal_duration
                        .date()
                        .adjust(target_time_days, None, None)?;

                // e. Let targetDate be ? CalendarDateAdd(calendar, plainRelativeTo.[[ISODate]], dateDuration, constrain).
                let target_date = calendar.date_add(
                    &plain_relative_to.iso,
                    &Duration::from(date_duration),
                    ArithmeticOverflow::Constrain,
                )?;

                // f. Let isoDateTime be CombineISODateAndTimeRecord(plainRelativeTo.[[ISODate]], MidnightTimeRecord()).
                let iso_date_time =
                    IsoDateTime::new_unchecked(plain_relative_to.iso, IsoTime::default());

                // g. Let targetDateTime be CombineISODateAndTimeRecord(targetDate, targetTime).
                let target_date_time = IsoDateTime::new_unchecked(target_date.iso, target_time.iso);

                // h. Set internalDuration to ? DifferencePlainDateTimeWithRounding(isoDateTime, targetDateTime, calendar, largestUnit, roundingIncrement, smallestUnit, roundingMode).
                let internal_duration =
                    PlainDateTime::new_unchecked(iso_date_time, calendar.clone())
                        .diff_dt_with_rounding(
                            &PlainDateTime::new_unchecked(target_date_time, calendar.clone()),
                            resolved_options,
                        )?;

                // i. Return ? TemporalDurationFromInternal(internalDuration, largestUnit).
                return Duration::from_normalized(internal_duration, resolved_options.largest_unit);
            }
            None => {}
        }

        // 28. If IsCalendarUnit(existingLargestUnit) is true, or IsCalendarUnit(largestUnit) is true, throw a RangeError exception.
        if existing_largest_unit.is_calendar_unit()
            || resolved_options.largest_unit.is_calendar_unit()
        {
            return Err(TemporalError::range().with_message(
                "largestUnit when rounding Duration was not the largest provided unit",
            ));
        }

        // 29. Assert: IsCalendarUnit(smallestUnit) is false.
        temporal_assert!(!resolved_options.smallest_unit.is_calendar_unit());

        // 30. Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
        let internal_duration = NormalizedDurationRecord::from_duration_with_24_hour_days(self)?;

        // 31. If smallestUnit is day, then
        let internal_duration = if resolved_options.smallest_unit == Unit::Day {
            // a. Let fractionalDays be TotalTimeDuration(internalDuration.[[Time]], day).
            // b. Let days be RoundNumberToIncrement(fractionalDays, roundingIncrement, roundingMode).
            let days = internal_duration
                .normalized_time_duration()
                .round_to_fractional_days(
                    resolved_options.increment,
                    resolved_options.rounding_mode,
                )?;

            // c. Let dateDuration be ? CreateDateDurationRecord(0, 0, 0, days).
            let date = DateDuration::new(0, 0, 0, days)?;

            // d. Set internalDuration to CombineDateAndTimeDuration(dateDuration, 0).
            NormalizedDurationRecord::new(date, NormalizedTimeDuration::default())?
        } else {
            // 32. Else,
            // a. Let timeDuration be ? RoundTimeDuration(internalDuration.[[Time]], roundingIncrement, smallestUnit, roundingMode).
            let time_duration = internal_duration
                .normalized_time_duration()
                .round(resolved_options)?;

            // b. Set internalDuration to CombineDateAndTimeDuration(ZeroDateDuration(), timeDuration).
            NormalizedDurationRecord::new(DateDuration::default(), time_duration)?
        };

        // 33. Return ? TemporalDurationFromInternal(internalDuration, largestUnit).
        Duration::from_normalized(internal_duration, resolved_options.largest_unit)
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
