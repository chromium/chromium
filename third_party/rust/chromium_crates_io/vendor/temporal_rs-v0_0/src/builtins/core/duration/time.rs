//! An implementation of `TimeDuration` and it's methods.

use crate::{
    options::TemporalUnit, primitive::FiniteF64, temporal_assert, Sign, TemporalError,
    TemporalResult,
};

use super::{duration_sign, is_valid_duration, normalized::NormalizedTimeDuration};

use alloc::vec::Vec;
use num_traits::Euclid;

/// `TimeDuration` represents the [Time Duration record][spec] of the `Duration.`
///
/// These fields are laid out in the [Temporal Proposal][field spec] as 64-bit floating point numbers.
///
/// [spec]: https://tc39.es/proposal-temporal/#sec-temporal-time-duration-records
/// [field spec]: https://tc39.es/proposal-temporal/#sec-properties-of-temporal-duration-instances
#[non_exhaustive]
#[derive(Debug, Default, Clone, Copy, PartialEq, PartialOrd)]
pub struct TimeDuration {
    /// `TimeDuration`'s internal hour value.
    pub hours: FiniteF64,
    /// `TimeDuration`'s internal minute value.
    pub minutes: FiniteF64,
    /// `TimeDuration`'s internal second value.
    pub seconds: FiniteF64,
    /// `TimeDuration`'s internal millisecond value.
    pub milliseconds: FiniteF64,
    /// `TimeDuration`'s internal microsecond value.
    pub microseconds: FiniteF64,
    /// `TimeDuration`'s internal nanosecond value.
    pub nanoseconds: FiniteF64,
}
// ==== TimeDuration Private API ====

impl TimeDuration {
    /// Creates a new `TimeDuration`.
    #[must_use]
    pub(crate) const fn new_unchecked(
        hours: FiniteF64,
        minutes: FiniteF64,
        seconds: FiniteF64,
        milliseconds: FiniteF64,
        microseconds: FiniteF64,
        nanoseconds: FiniteF64,
    ) -> Self {
        Self {
            hours,
            minutes,
            seconds,
            milliseconds,
            microseconds,
            nanoseconds,
        }
    }

    /// Balances and creates `TimeDuration` from a `NormalizedTimeDuration`. This method will return
    /// a tuple (f64, TimeDuration) where f64 is the overflow day value from balancing.
    ///
    /// Equivalent: `BalanceTimeDuration`
    ///
    /// # Errors:
    ///   - Will error if provided duration is invalid
    pub(crate) fn from_normalized(
        norm: NormalizedTimeDuration,
        largest_unit: TemporalUnit,
    ) -> TemporalResult<(FiniteF64, Self)> {
        // 1. Let days, hours, minutes, seconds, milliseconds, and microseconds be 0.
        let mut days = 0;
        let mut hours = 0;
        let mut minutes = 0;
        let mut seconds = 0;
        let mut milliseconds = 0;
        let mut microseconds = 0;

        // 2. Let sign be NormalizedTimeDurationSign(norm).
        let sign = i32::from(norm.sign() as i8);
        // 3. Let nanoseconds be NormalizedTimeDurationAbs(norm).[[TotalNanoseconds]].
        let mut nanoseconds = norm.0.abs();

        match largest_unit {
            // 4. If largestUnit is "year", "month", "week", or "day", then
            TemporalUnit::Year | TemporalUnit::Month | TemporalUnit::Week | TemporalUnit::Day => {
                // a. Set microseconds to floor(nanoseconds / 1000).
                // b. Set nanoseconds to nanoseconds modulo 1000.
                (microseconds, nanoseconds) = nanoseconds.div_rem_euclid(&1_000);

                // c. Set milliseconds to floor(microseconds / 1000).
                // d. Set microseconds to microseconds modulo 1000.
                (milliseconds, microseconds) = microseconds.div_rem_euclid(&1_000);

                // e. Set seconds to floor(milliseconds / 1000).
                // f. Set milliseconds to milliseconds modulo 1000.
                (seconds, milliseconds) = milliseconds.div_rem_euclid(&1_000);

                // g. Set minutes to floor(seconds / 60).
                // h. Set seconds to seconds modulo 60.
                (minutes, seconds) = seconds.div_rem_euclid(&60);

                // i. Set hours to floor(minutes / 60).
                // j. Set minutes to minutes modulo 60.
                (hours, minutes) = minutes.div_rem_euclid(&60);

                // k. Set days to floor(hours / 24).
                // l. Set hours to hours modulo 24.
                (days, hours) = hours.div_rem_euclid(&24);
            }
            // 5. Else if largestUnit is "hour", then
            TemporalUnit::Hour => {
                // a. Set microseconds to floor(nanoseconds / 1000).
                // b. Set nanoseconds to nanoseconds modulo 1000.
                (microseconds, nanoseconds) = nanoseconds.div_rem_euclid(&1_000);

                // c. Set milliseconds to floor(microseconds / 1000).
                // d. Set microseconds to microseconds modulo 1000.
                (milliseconds, microseconds) = microseconds.div_rem_euclid(&1_000);

                // e. Set seconds to floor(milliseconds / 1000).
                // f. Set milliseconds to milliseconds modulo 1000.
                (seconds, milliseconds) = milliseconds.div_rem_euclid(&1_000);

                // g. Set minutes to floor(seconds / 60).
                // h. Set seconds to seconds modulo 60.
                (minutes, seconds) = seconds.div_rem_euclid(&60);

                // i. Set hours to floor(minutes / 60).
                // j. Set minutes to minutes modulo 60.
                (hours, minutes) = minutes.div_rem_euclid(&60);
            }
            // 6. Else if largestUnit is "minute", then
            TemporalUnit::Minute => {
                // a. Set microseconds to floor(nanoseconds / 1000).
                // b. Set nanoseconds to nanoseconds modulo 1000.
                (microseconds, nanoseconds) = nanoseconds.div_rem_euclid(&1_000);

                // c. Set milliseconds to floor(microseconds / 1000).
                // d. Set microseconds to microseconds modulo 1000.
                (milliseconds, microseconds) = microseconds.div_rem_euclid(&1_000);

                // e. Set seconds to floor(milliseconds / 1000).
                // f. Set milliseconds to milliseconds modulo 1000.
                (seconds, milliseconds) = milliseconds.div_rem_euclid(&1_000);

                // g. Set minutes to floor(seconds / 60).
                // h. Set seconds to seconds modulo 60.
                (minutes, seconds) = seconds.div_rem_euclid(&60);
            }
            // 7. Else if largestUnit is "second", then
            TemporalUnit::Second => {
                // a. Set microseconds to floor(nanoseconds / 1000).
                // b. Set nanoseconds to nanoseconds modulo 1000.
                (microseconds, nanoseconds) = nanoseconds.div_rem_euclid(&1_000);

                // c. Set milliseconds to floor(microseconds / 1000).
                // d. Set microseconds to microseconds modulo 1000.
                (milliseconds, microseconds) = microseconds.div_rem_euclid(&1_000);

                // e. Set seconds to floor(milliseconds / 1000).
                // f. Set milliseconds to milliseconds modulo 1000.
                (seconds, milliseconds) = milliseconds.div_rem_euclid(&1_000);
            }
            // 8. Else if largestUnit is "millisecond", then
            TemporalUnit::Millisecond => {
                // a. Set microseconds to floor(nanoseconds / 1000).
                // b. Set nanoseconds to nanoseconds modulo 1000.
                (microseconds, nanoseconds) = nanoseconds.div_rem_euclid(&1_000);

                // c. Set milliseconds to floor(microseconds / 1000).
                // d. Set microseconds to microseconds modulo 1000.
                (milliseconds, microseconds) = microseconds.div_rem_euclid(&1_000);
            }
            // 9. Else if largestUnit is "microsecond", then
            TemporalUnit::Microsecond => {
                // a. Set microseconds to floor(nanoseconds / 1000).
                // b. Set nanoseconds to nanoseconds modulo 1000.
                (microseconds, nanoseconds) = nanoseconds.div_rem_euclid(&1_000);
            }
            // 10. Else,
            // a. Assert: largestUnit is "nanosecond".
            _ => temporal_assert!(largest_unit == TemporalUnit::Nanosecond),
        }

        // NOTE(nekevss): `mul_add` is essentially the Rust's implementation of `std::fma()`, so that's handy, but
        // this should be tested much further.
        // 11. NOTE: When largestUnit is "millisecond", "microsecond", or "nanosecond", milliseconds, microseconds, or
        // nanoseconds may be an unsafe integer. In this case, care must be taken when implementing the calculation
        // using floating point arithmetic. It can be implemented in C++ using std::fma(). String manipulation will also
        // give an exact result, since the multiplication is by a power of 10.

        // NOTE: days may have the potentially to exceed i64
        // 12. Return ! CreateTimeDurationRecord(days × sign, hours × sign, minutes × sign, seconds × sign, milliseconds × sign, microseconds × sign, nanoseconds × sign).
        let days = FiniteF64::try_from(days as f64)?.copysign(sign.into());
        let result = Self::new_unchecked(
            FiniteF64::try_from(hours)?.copysign(f64::from(sign)),
            FiniteF64::try_from(minutes)?.copysign(f64::from(sign)),
            FiniteF64::try_from(seconds)?.copysign(f64::from(sign)),
            FiniteF64::try_from(milliseconds)?.copysign(f64::from(sign)),
            FiniteF64::try_from(microseconds)?.copysign(f64::from(sign)),
            FiniteF64::try_from(nanoseconds)?.copysign(f64::from(sign)),
        );

        if !is_valid_duration(
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            days,
            result.hours,
            result.minutes,
            result.seconds,
            result.milliseconds,
            result.microseconds,
            result.nanoseconds,
        ) {
            return Err(TemporalError::range().with_message("Invalid balance TimeDuration."));
        }

        // TODO: Remove cast below.
        Ok((days, result))
    }

    /// Returns this `TimeDuration` as a `NormalizedTimeDuration`.
    #[inline]
    pub(crate) fn to_normalized(self) -> NormalizedTimeDuration {
        NormalizedTimeDuration::from_time_duration(&self)
    }

    /// Returns the value of `TimeDuration`'s fields.
    #[inline]
    #[must_use]
    pub(crate) fn fields(&self) -> Vec<FiniteF64> {
        Vec::from(&[
            self.hours,
            self.minutes,
            self.seconds,
            self.milliseconds,
            self.microseconds,
            self.nanoseconds,
        ])
    }
}

// ==== TimeDuration's public API ====

impl TimeDuration {
    /// Creates a new validated `TimeDuration`.
    pub fn new(
        hours: FiniteF64,
        minutes: FiniteF64,
        seconds: FiniteF64,
        milliseconds: FiniteF64,
        microseconds: FiniteF64,
        nanoseconds: FiniteF64,
    ) -> TemporalResult<Self> {
        let result = Self::new_unchecked(
            hours,
            minutes,
            seconds,
            milliseconds,
            microseconds,
            nanoseconds,
        );
        if !is_valid_duration(
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            hours,
            minutes,
            seconds,
            milliseconds,
            microseconds,
            nanoseconds,
        ) {
            return Err(
                TemporalError::range().with_message("Attempted to create an invalid TimeDuration.")
            );
        }
        Ok(result)
    }

    /// Returns a new `TimeDuration` representing the absolute value of the current.
    #[inline]
    #[must_use]
    pub fn abs(&self) -> Self {
        Self {
            hours: self.hours.abs(),
            minutes: self.minutes.abs(),
            seconds: self.seconds.abs(),
            milliseconds: self.milliseconds.abs(),
            microseconds: self.microseconds.abs(),
            nanoseconds: self.nanoseconds.abs(),
        }
    }

    /// Returns a negated `TimeDuration`.
    #[inline]
    #[must_use]
    pub fn negated(&self) -> Self {
        Self {
            hours: self.hours.negate(),
            minutes: self.minutes.negate(),
            seconds: self.seconds.negate(),
            milliseconds: self.milliseconds.negate(),
            microseconds: self.microseconds.negate(),
            nanoseconds: self.nanoseconds.negate(),
        }
    }

    /// Utility function for returning if values in a valid range.
    #[inline]
    #[must_use]
    pub fn is_within_range(&self) -> bool {
        self.hours.abs() < 24f64
            && self.minutes.abs() < 60f64
            && self.seconds.abs() < 60f64
            && self.milliseconds.abs() < 1000f64
            && self.milliseconds.abs() < 1000f64
            && self.milliseconds.abs() < 1000f64
    }

    #[inline]
    pub fn sign(&self) -> Sign {
        duration_sign(&self.fields())
    }
}
