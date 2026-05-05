// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The `units` module provides definitions for common units.

use std::fmt;

use core::num::NonZero;

/// A `Timestamp` is an instant in time expressed in stream timebase units.
///
/// One timestamp "tick" is equal to the stream's `TimeBase` in seconds.
#[repr(transparent)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Timestamp(i64);

/// A `Duration` is a positive span of time expressed in stream timebase units.
///
/// One duration "tick" is equal to the stream's `TimeBase` in seconds.
#[repr(transparent)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Duration(u64);

/// A `Delta` is a signed difference between timestamps.
#[repr(transparent)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Delta(i64);

impl Timestamp {
    /// The minimum representable timestamp.
    pub const MIN: Self = Self(i64::MIN);
    /// The maximum representable timestamp.
    pub const MAX: Self = Self(i64::MAX);
    /// A timestamp with the value of 0.
    pub const ZERO: Self = Self(0);

    /// Create a timestamp.
    #[inline]
    pub const fn new(ts: i64) -> Self {
        Self(ts)
    }

    /// Get the underlying integer representing the timestamp.
    #[inline]
    pub const fn get(self) -> i64 {
        self.0
    }

    /// Returns `true` if `self` is 0.
    #[inline]
    pub const fn is_zero(self) -> bool {
        self.0 == 0
    }

    /// Returns `true` if `self` is negative and `false` if the timestamp is zero or positive.
    #[inline]
    pub const fn is_negative(self) -> bool {
        self.0.is_negative()
    }

    /// Returns `true` if `self` is positive and `false` if the timestamp is zero or negative.
    #[inline]
    pub const fn is_positive(self) -> bool {
        self.0.is_positive()
    }

    /// Add the provided duration to `self`, returning `None` if an overflow occurred.
    #[inline]
    pub const fn checked_add(self, dur: Duration) -> Option<Self> {
        match self.0.checked_add_unsigned(dur.0) {
            Some(value) => Some(Self(value)),
            _ => None,
        }
    }

    /// Subtract the provided duration from `self`, returning `None` if an overflow occurred.
    #[inline]
    pub const fn checked_sub(self, dur: Duration) -> Option<Self> {
        match self.0.checked_sub_unsigned(dur.0) {
            Some(value) => Some(Self(value)),
            _ => None,
        }
    }

    /// Add the provided duration to `self`, saturating at the numeric bounds instead of
    /// overflowing.
    #[inline]
    pub const fn saturating_add(self, dur: Duration) -> Self {
        Self(self.0.saturating_add_unsigned(dur.0))
    }

    /// Subtract the provided duration from `self`, saturating at the numeric bounds instead of
    /// overflowing.
    #[inline]
    pub const fn saturating_sub(self, dur: Duration) -> Self {
        Self(self.0.saturating_sub_unsigned(dur.0))
    }

    /// Computes the difference between `self` and `other`, returning `None` if an overflow
    /// occurred.
    #[inline]
    pub const fn checked_delta(self, other: Self) -> Option<Delta> {
        match self.0.checked_sub(other.0) {
            Some(delta) => Some(Delta(delta)),
            _ => None,
        }
    }

    /// Computes the difference between `self` and `other`, saturating at the numeric bounds instead
    /// of overflowing.
    #[inline]
    pub const fn saturating_delta(self, other: Self) -> Delta {
        Delta(self.0.saturating_sub(other.0))
    }

    /// Add the provided delta to `self`, returning `None` if an overflow occurred.
    #[inline]
    pub const fn checked_add_delta(self, delta: Delta) -> Option<Self> {
        match self.0.checked_add(delta.0) {
            Some(value) => Some(Self(value)),
            _ => None,
        }
    }

    /// Subtract the provided delta from `self`, returning `None` if an overflow occurred.
    #[inline]
    pub const fn checked_sub_delta(self, delta: Delta) -> Option<Self> {
        match self.0.checked_sub(delta.0) {
            Some(value) => Some(Self(value)),
            _ => None,
        }
    }

    /// Add the provided delta to `self`, saturating at the numeric bounds instead of
    /// overflowing.
    #[inline]
    pub const fn saturating_add_delta(self, delta: Delta) -> Self {
        Self(self.0.saturating_add(delta.0))
    }

    /// Subtract the provided delta from `self`, saturating at the numeric bounds instead of
    /// overflowing.
    #[inline]
    pub const fn saturating_sub_delta(self, delta: Delta) -> Self {
        Self(self.0.saturating_sub(delta.0))
    }

    /// Calculate the absolute difference in duration between `self` and `other`.
    #[inline]
    pub const fn abs_delta(self, other: Self) -> Duration {
        Duration(self.0.abs_diff(other.0))
    }

    /// Calculate the duration from `other` to `self`. If `other` is greater-than `self`, returns
    /// `None` since the duration from `other` would be negative.
    #[inline]
    pub const fn duration_from(self, other: Self) -> Option<Duration> {
        match self.0.checked_sub(other.0) {
            Some(diff) if !diff.is_negative() => Some(Duration(diff as u64)),
            _ => None,
        }
    }

    /// Calculate the duration from `self` to `other`. If `other` is less-than `self`, returns
    /// `None` since the duration to `other` would be negative.
    #[inline]
    pub const fn duration_to(self, other: Self) -> Option<Duration> {
        match other.0.checked_sub(self.0) {
            Some(diff) if !diff.is_negative() => Some(Duration(diff as u64)),
            _ => None,
        }
    }

    /// Align `self` to the nearest multiple of the provided duration that is closest to 0.
    ///
    /// Returns `None` if the duration is 0 or greater-than `i64::MAX`.
    #[inline]
    pub const fn align_towards_zero(self, align_to: Duration) -> Option<Self> {
        if align_to.is_zero() || align_to.0 > i64::MAX as u64 {
            return None;
        }

        Some(Self((self.0 / align_to.0 as i64) * align_to.0 as i64))
    }

    /// Align `self` to the nearest multiple of the provided duration that is closest to negative
    /// infinity.
    ///
    /// Returns `None` if the duration is 0 or greater-than `i64::MAX`.
    #[inline]
    pub const fn align_down(self, align_to: Duration) -> Option<Self> {
        if align_to.is_zero() || align_to.0 > i64::MAX as u64 {
            return None;
        }

        let dur_i64 = align_to.0 as i64;

        match self.0.div_euclid(dur_i64).checked_mul(dur_i64) {
            Some(snapped) => Some(Self(snapped)),
            _ => None,
        }
    }
}

impl From<i64> for Timestamp {
    fn from(value: i64) -> Self {
        Self(value)
    }
}

impl TryFrom<u64> for Timestamp {
    type Error = std::num::TryFromIntError;

    fn try_from(value: u64) -> Result<Self, Self::Error> {
        let ts: i64 = value.try_into()?;
        Ok(Self(ts))
    }
}

impl From<i32> for Timestamp {
    fn from(value: i32) -> Self {
        Self(i64::from(value))
    }
}

impl From<u32> for Timestamp {
    fn from(value: u32) -> Self {
        Self(i64::from(value))
    }
}

impl From<i16> for Timestamp {
    fn from(value: i16) -> Self {
        Self(i64::from(value))
    }
}

impl From<u16> for Timestamp {
    fn from(value: u16) -> Self {
        Self(i64::from(value))
    }
}

impl From<i8> for Timestamp {
    fn from(value: i8) -> Self {
        Self(i64::from(value))
    }
}

impl From<u8> for Timestamp {
    fn from(value: u8) -> Self {
        Self(i64::from(value))
    }
}

impl fmt::Display for Timestamp {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl Duration {
    pub const MIN: Self = Self(u64::MIN);
    pub const MAX: Self = Self(u64::MAX);
    pub const ZERO: Self = Self(0);

    /// Create a duration.
    #[inline]
    pub const fn new(dur: u64) -> Self {
        Self(dur)
    }

    /// Get the underlying unsigned integer representing the duration.
    #[inline]
    pub const fn get(self) -> u64 {
        self.0
    }

    /// Returns `true` if `self` is 0.
    #[inline]
    pub const fn is_zero(self) -> bool {
        self.0 == 0
    }

    /// Add the provided duration to `self`, returning `None` if an overflow occurred.
    #[inline]
    pub const fn checked_add(self, other: Self) -> Option<Self> {
        match self.0.checked_add(other.0) {
            Some(value) => Some(Self(value)),
            _ => None,
        }
    }

    /// Divide `self` by the provided value, returning `None` if `rhs == 0`.
    #[inline]
    pub const fn checked_div(self, rhs: u64) -> Option<Self> {
        match self.0.checked_div(rhs) {
            Some(value) => Some(Self(value)),
            _ => None,
        }
    }

    /// Calculate the quotient of Euclidean division of `self` by `rhs`, returning `None` if
    /// `rhs == 0`.
    #[inline]
    pub const fn checked_div_euclid(self, rhs: u64) -> Option<Self> {
        match self.0.checked_div_euclid(rhs) {
            Some(value) => Some(Self(value)),
            _ => None,
        }
    }

    /// Multiply `self` by the provided value, returning `None` if an overflow occurred.
    #[inline]
    pub const fn checked_mul(self, rhs: u64) -> Option<Self> {
        match self.0.checked_mul(rhs) {
            Some(value) => Some(Self(value)),
            _ => None,
        }
    }

    /// Subtract the provided duration from `self`, returning `None` if an overflow occurred.
    #[inline]
    pub const fn checked_sub(self, other: Self) -> Option<Self> {
        match self.0.checked_sub(other.0) {
            Some(value) => Some(Self(value)),
            _ => None,
        }
    }

    /// Add the provided duration to `self`, saturating at the numeric bounds instead of
    /// overflowing.
    #[inline]
    pub const fn saturating_add(self, other: Self) -> Self {
        Self(self.0.saturating_add(other.0))
    }

    /// Divide `self` by the provided value, saturating at the numeric bounds instead of
    /// overflowing.
    ///
    /// # Panics
    ///
    /// Panics if `rhs == 0`.
    #[inline]
    pub const fn saturating_div(self, rhs: u64) -> Self {
        Self(self.0.saturating_div(rhs))
    }

    /// Multiply `self` by the provided value, saturating at the numeric bounds instead of
    /// overflowing.
    #[inline]
    pub const fn saturating_mul(self, rhs: u64) -> Self {
        Self(self.0.saturating_mul(rhs))
    }

    /// Subtract the provided duration from `self`, saturating at the numeric bounds instead of
    /// overflowing.
    #[inline]
    pub const fn saturating_sub(self, other: Self) -> Self {
        Self(self.0.saturating_sub(other.0))
    }

    /// Align `self` to the nearest multiple of the provided duration that is closest to 0.
    ///
    /// Returns `None` if the provided duration is 0.
    #[inline]
    pub const fn align_towards_zero(self, align_to: Self) -> Option<Self> {
        if align_to.is_zero() {
            return None;
        }

        Some(Self((self.0 / align_to.0) * align_to.0))
    }

    /// Align `self` to the nearest multiple of the provided duration that is closest to negative
    /// infinity.
    ///
    /// Since a duration is unsigned, this function is the same as calling `align_towards_zero`.
    #[inline]
    pub const fn align_down(self, align_to: Self) -> Option<Self> {
        self.align_towards_zero(align_to)
    }

    /// Compute a `Timestamp` from the duration by adding `self` to `from`.
    #[inline]
    pub const fn timestamp_from(self, from: Timestamp) -> Option<Timestamp> {
        from.checked_add(self)
    }
}

impl From<u64> for Duration {
    fn from(value: u64) -> Self {
        Self(value)
    }
}

impl TryFrom<i64> for Duration {
    type Error = std::num::TryFromIntError;

    fn try_from(value: i64) -> Result<Self, Self::Error> {
        let dur: u64 = value.try_into()?;
        Ok(Self(dur))
    }
}

impl From<u32> for Duration {
    fn from(value: u32) -> Self {
        Self(u64::from(value))
    }
}

impl TryFrom<i32> for Duration {
    type Error = std::num::TryFromIntError;

    fn try_from(value: i32) -> Result<Self, Self::Error> {
        let dur: u64 = value.try_into()?;
        Ok(Self(dur))
    }
}

impl From<u16> for Duration {
    fn from(value: u16) -> Self {
        Self(u64::from(value))
    }
}

impl TryFrom<i16> for Duration {
    type Error = std::num::TryFromIntError;

    fn try_from(value: i16) -> Result<Self, Self::Error> {
        let dur: u64 = value.try_into()?;
        Ok(Self(dur))
    }
}

impl From<u8> for Duration {
    fn from(value: u8) -> Self {
        Self(u64::from(value))
    }
}

impl TryFrom<i8> for Duration {
    type Error = std::num::TryFromIntError;

    fn try_from(value: i8) -> Result<Self, Self::Error> {
        let dur: u64 = value.try_into()?;
        Ok(Self(dur))
    }
}

impl fmt::Display for Duration {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl Delta {
    /// The minimum representable delta.
    pub const MIN: Self = Self(i64::MIN);
    /// The maximum representable delta.
    pub const MAX: Self = Self(i64::MAX);
    /// A delta with the value of 0.
    pub const ZERO: Self = Self(0);

    /// Get the underlying unsigned integer representing the duration.
    #[inline]
    pub const fn get(self) -> i64 {
        self.0
    }

    /// Returns `true` if `self` is 0.
    #[inline]
    pub const fn is_zero(self) -> bool {
        self.0 == 0
    }

    /// Returns `true` if `self` is negative and `false` if the delta is zero or positive.
    #[inline]
    pub const fn is_negative(self) -> bool {
        self.0.is_negative()
    }

    /// Returns `true` if `self` is positive and `false` if the delta is zero or negative.
    #[inline]
    pub const fn is_positive(self) -> bool {
        self.0.is_positive()
    }

    /// Compute the absolute value, returning `None` if it is not representable.
    #[inline]
    pub const fn checked_abs(self) -> Option<Self> {
        match self.0.checked_abs() {
            Some(abs) => Some(Self(abs)),
            None => None,
        }
    }

    /// Get the delta as an unsigned absolute value.
    #[inline]
    pub const fn unsigned_abs(self) -> u64 {
        self.0.unsigned_abs()
    }
}

impl fmt::Display for Delta {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

/// `Time` represents an instant in time from an arbitrary epoch with nanosecond precision.
///
/// The duration of time from the epoch is stored as a signed-integer number of seconds and an
/// unsigned number of nanoseconds. The exact duration from the epoch is the sum of both.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
pub struct Time {
    seconds: i64,
    nanos: u32,
}

impl Time {
    /// The maximum representable time.
    pub const MAX: Self = Time { seconds: i64::MAX, nanos: 999_999_999 };
    /// The minimum representable time.
    pub const MIN: Self = Time { seconds: i64::MIN, nanos: 0 };
    /// A time of 0.
    pub const ZERO: Self = Time { seconds: 0, nanos: 0 };

    /// Milliseconds per second.
    const MS_PER_SEC: i64 = 1_000;
    /// Microseconds per second.
    const US_PER_SEC: i64 = 1_000_000;
    /// Nanoseconds per second.
    const NS_PER_SEC: i64 = 1_000_000_000;
    /// Nonseconds per second as i128.
    const NS_PER_SEC_128: i128 = 1_000_000_000;

    /// Seconds per hour.
    const SECS_PER_HR: i64 = 60 * 60;
    /// Seconds per minute.
    const SECS_PER_MIN: i64 = 60;

    /// Try to create from a count of seconds and nanoseconds representing the fractional portion of
    /// of a second.
    ///
    /// Returns `None` if nanoseconds exceeds `999_999_999`.
    pub const fn try_new(s: i64, ns: u32) -> Option<Self> {
        match ns {
            0..1_000_000_000 => Some(Time { seconds: s, nanos: ns }),
            _ => None,
        }
    }

    /// Try to create from a 128-bit count of total nanoseconds.
    ///
    /// Returns `None` if an overflow occurs.
    pub fn try_from_nanos_i128(total_ns: i128) -> Option<Time> {
        let seconds = total_ns.div_euclid(Self::NS_PER_SEC_128);
        let nanos = total_ns.rem_euclid(Self::NS_PER_SEC_128);
        seconds.try_into().ok().map(|seconds| Time { seconds, nanos: nanos as u32 })
    }

    /// Try to create from an unsigned 128-bit count of total nanoseconds.
    ///
    /// Returns `None` if an overflow occurs.
    pub fn try_from_nanos_u128(total_ns: u128) -> Option<Time> {
        let seconds = total_ns.div_euclid(Self::NS_PER_SEC_128 as u128);
        let nanos = total_ns.rem_euclid(Self::NS_PER_SEC_128 as u128);
        seconds.try_into().ok().map(|seconds| Time { seconds, nanos: nanos as u32 })
    }

    /// Try to instantiate from a floating-point count of total seconds.
    ///
    /// Returns `None` when the conversion fails. For the conversion to succeed the value provided
    /// must be a finite number with the integer/whole part not exceeding `i64::MIN` or `i64::MAX`.
    pub fn try_from_secs_f64(total_secs: f64) -> Option<Time> {
        // Cannot represent non-finite values.
        if !total_secs.is_finite() {
            return None;
        }

        let seconds = total_secs.floor();

        // The integer number of seconds exceeds the limits of an i64.
        if seconds < i64::MIN as f64 || seconds > i64::MAX as f64 {
            return None;
        }

        let nanos = ((total_secs - seconds) * 1_000_000_000.0).round();

        // Rounding may cause nanos exceed its bounds.
        Some(if nanos >= 1_000_000_000.0 {
            // Note: In practice, this addition will never saturate because `seconds` is less-than
            // `i64::MAX as f64 as i64` which is much smaller than `i64::MAX`.
            Time { seconds: (seconds as i64).saturating_add(1), nanos: 0 }
        }
        else {
            Time { seconds: seconds as i64, nanos: nanos as u32 }
        })
    }

    /// Create from a count of total nanoseconds.
    pub const fn from_nanos(total_ns: i64) -> Time {
        let seconds = total_ns.div_euclid(Self::NS_PER_SEC);
        let nanos = total_ns.rem_euclid(Self::NS_PER_SEC);
        Time { seconds, nanos: nanos as u32 }
    }

    /// Create from an unsigned count of total nanoseconds.
    pub const fn from_nanos_u64(total_ns: u64) -> Time {
        let seconds = total_ns.div_euclid(Self::NS_PER_SEC as u64);
        let nanos = total_ns.rem_euclid(Self::NS_PER_SEC as u64);
        Time { seconds: seconds as i64, nanos: nanos as u32 }
    }

    /// Create from a count of total microseconds.
    pub const fn from_micros(total_us: i64) -> Time {
        let seconds = total_us.div_euclid(Self::US_PER_SEC);
        let micros = total_us.rem_euclid(Self::US_PER_SEC);
        Time { seconds, nanos: 1_000 * micros as u32 }
    }

    /// Create from an unsigned count of total microseconds.
    pub const fn from_micros_u64(total_us: u64) -> Time {
        let seconds = total_us.div_euclid(Self::US_PER_SEC as u64);
        let micros = total_us.rem_euclid(Self::US_PER_SEC as u64);
        Time { seconds: seconds as i64, nanos: 1_000 * micros as u32 }
    }

    /// Create from a count of total milliseconds.
    pub const fn from_millis(total_ms: i64) -> Time {
        let seconds = total_ms.div_euclid(Self::MS_PER_SEC);
        let millis = total_ms.rem_euclid(Self::MS_PER_SEC);
        Time { seconds, nanos: 1_000_000 * millis as u32 }
    }

    /// Create from an unsigned count of total milliseconds.
    pub const fn from_millis_u64(total_ms: u64) -> Time {
        let seconds = total_ms.div_euclid(Self::MS_PER_SEC as u64);
        let millis = total_ms.rem_euclid(Self::MS_PER_SEC as u64);
        Time { seconds: seconds as i64, nanos: 1_000_000 * millis as u32 }
    }

    /// Try to create from a clock time consisting of seconds and nanoseconds (i.e.,
    /// ss.nnnnnnnnn).
    ///
    /// Returns `None` if seconds exceeds `59`, or nanoseconds exceeds `999_999_999`.
    pub fn from_ss(s: u8, ns: u32) -> Option<Time> {
        if s > 59 || ns >= Time::NS_PER_SEC as u32 {
            return None;
        }

        // Guaranteed to never exceed 2^31.
        let seconds = i64::from(s);

        Some(Time { seconds, nanos: ns })
    }

    /// Try to create from a clock time consisting of minute, seconds, and nanoseconds (i.e.,
    /// mm:ss.nnnnnnnnn).
    ///
    /// Returns `None` if minutes or seconds exceed `59`, or nanoseconds exceeds `999_999_999`.
    pub fn from_mmss(m: u8, s: u8, ns: u32) -> Option<Time> {
        if m > 59 || s > 59 || ns >= Time::NS_PER_SEC as u32 {
            return None;
        }

        // Guaranteed to never exceed 2^31.
        let seconds = (Time::SECS_PER_MIN * i64::from(m)) + i64::from(s);

        Some(Time { seconds, nanos: ns })
    }

    /// Try to create from a clock time consisting of hours, minute, seconds, and nanoseconds (i.e.,
    /// hh:mm:ss.nnnnnnnnn).
    ///
    /// Returns `None` if minutes or seconds exceed `59`, or nanoseconds exceeds `999_999_999`.
    pub fn from_hhmmss(h: u32, m: u8, s: u8, ns: u32) -> Option<Time> {
        if m > 59 || s > 59 || ns >= Time::NS_PER_SEC as u32 {
            return None;
        }

        // Guaranteed to never exceed i64::MAX.
        let seconds =
            (Time::SECS_PER_HR * i64::from(h)) + (Time::SECS_PER_MIN * i64::from(m)) + i64::from(s);

        Some(Time { seconds, nanos: ns })
    }

    /// Returns true if zero.
    #[inline]
    pub const fn is_zero(&self) -> bool {
        self.seconds == 0 && self.nanos == 0
    }

    /// Returns `true` if self is positive and `false` if the number is zero or negative.
    #[inline]
    pub const fn is_positive(&self) -> bool {
        self.seconds.is_positive()
    }

    /// Returns `true` if self is positive and `false` if the number is zero or negative.
    #[inline]
    pub const fn is_negative(&self) -> bool {
        self.seconds.is_negative()
    }

    /// Get the time in nanoseconds.
    #[inline]
    pub fn as_nanos(&self) -> i128 {
        1_000_000_000 * i128::from(self.seconds) + i128::from(self.nanos)
    }

    /// Get the time in whole microseconds.
    #[inline]
    pub fn as_micros(&self) -> i128 {
        self.as_nanos() / 1_000
    }

    /// Get the time in whole milliseconds.
    #[inline]
    pub fn as_millis(&self) -> i128 {
        self.as_nanos() / 1_000_000
    }

    /// Get the time in whole seconds.
    #[inline]
    pub fn as_secs(&self) -> i64 {
        if self.seconds >= 0 || self.nanos == 0 { self.seconds } else { self.seconds + 1 }
    }

    /// Get the time in whole minutes.
    #[inline]
    pub fn as_mins(&self) -> i64 {
        self.as_secs() / 60
    }

    /// Get the time in whole hours.
    #[inline]
    pub fn as_hours(&self) -> i64 {
        self.as_secs() / (60 * 60)
    }

    /// Get the time in floating-point seconds.
    #[inline]
    pub fn as_secs_f64(&self) -> f64 {
        self.seconds as f64 + self.nanos as f64 * 1e-9
    }

    /// Decomposes the time into two parts: total whole seconds and a fractional sub-second
    /// expressed in nanoseconds.
    ///
    /// The sub-second part is always positive while total whole seconds may be negative. The total
    /// time in decimal seconds is the concatenation (not addition) of the two parts.
    ///
    /// For example:
    ///  - `( 1, 750_000_000)` is `1.75` seconds
    ///  - `( 0, 100_000_000)` is `0.1` seconds
    ///  - `(-1, 250_000_000)` is `-1.25` seconds
    ///
    pub fn parts(&self) -> (i64, u32) {
        if self.seconds >= 0 {
            (self.seconds, self.nanos)
        }
        else if self.nanos == 0 {
            (self.seconds, 0)
        }
        else {
            (self.seconds + 1, 1_000_000_000 - self.nanos)
        }
    }
}

impl From<u8> for Time {
    fn from(seconds: u8) -> Self {
        // UNWRAP: Nanoseconds is < 1000000000.
        Time::try_new(i64::from(seconds), 0).unwrap()
    }
}

impl From<u16> for Time {
    fn from(seconds: u16) -> Self {
        // UNWRAP: Nanoseconds is < 1000000000.
        Time::try_new(i64::from(seconds), 0).unwrap()
    }
}

impl From<u32> for Time {
    fn from(seconds: u32) -> Self {
        // UNWRAP: Nanoseconds is < 1000000000.
        Time::try_new(i64::from(seconds), 0).unwrap()
    }
}

/// A `TimeBase` is the conversion factor between time, expressed in seconds, and a `TimeStamp` or
/// `Duration`.
///
/// In other words, a `TimeBase` is the length in seconds of one tick of a `TimeStamp` or
/// `Duration`.
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct TimeBase {
    /// The numerator.
    pub numer: NonZero<u32>,
    /// The denominator.
    pub denom: NonZero<u32>,
}

impl Default for TimeBase {
    fn default() -> Self {
        // UNWRAP: Never panics because 1 is non-zero.
        Self { numer: NonZero::new(1).unwrap(), denom: NonZero::new(1).unwrap() }
    }
}

impl TimeBase {
    /// Create a new timebase.
    pub const fn new(numer: NonZero<u32>, denom: NonZero<u32>) -> Self {
        TimeBase { numer, denom }
    }

    /// Try to create a new timebase. Returns `None` if either the numerator or denominator are 0.
    pub fn try_new(numer: u32, denom: u32) -> Option<Self> {
        let numer = NonZero::new(numer)?;
        let denom = NonZero::new(denom)?;
        Some(TimeBase { numer, denom })
    }

    /// Create a timebase from the reciprocal of the provided rate.
    pub const fn from_recip(rate: NonZero<u32>) -> Self {
        // UNWRAP: Never panics because 1 is non-zero.
        TimeBase { numer: NonZero::new(1).unwrap(), denom: rate }
    }

    /// Try to create a timebase from the reciprocal of the provided rate. Returns `None` if the
    /// rate is 0.
    pub fn try_from_recip(rate: u32) -> Option<Self> {
        let denom = NonZero::new(rate)?;
        // UNWRAP: Never panics because 1 is non-zero.
        Some(TimeBase { numer: NonZero::new(1).unwrap(), denom })
    }

    /// Calculate a `Time` upto nanosecond precision from the provided `TimeStamp` using `self` as
    /// the conversion factor. Returns `None` if an overflow occurs.
    pub fn calc_time(&self, ts: Timestamp) -> Option<Time> {
        const NS_PER_SEC_64: i64 = 1_000_000_000;
        const NS_PER_SEC_128: i128 = 1_000_000_000;

        let numer = i64::from(self.numer.get());
        let denom = i64::from(self.denom.get());

        if let Some(product) =
            ts.get().checked_mul(numer).and_then(|x| x.checked_mul(NS_PER_SEC_64))
        {
            let total_nanos = product / denom;
            Some(Time::from_nanos(total_nanos))
        }
        else {
            let product = i128::from(ts.get()) * i128::from(numer) * NS_PER_SEC_128;
            let total_nanos = product / i128::from(denom);
            Time::try_from_nanos_i128(total_nanos)
        }
    }

    pub fn calc_time_saturating(&self, ts: Timestamp) -> Time {
        self.calc_time(ts).unwrap_or_else(|| if ts.is_negative() { Time::MIN } else { Time::MAX })
    }

    /// Calculate a `TimeStamp` from the provided `Time` using `self` as the conversion factor.
    /// Returns `None` if an overflow occurs.
    pub fn calc_timestamp(&self, time: Time) -> Option<Timestamp> {
        const NS_PER_SEC: i64 = 1_000_000_000;

        let numer = i64::from(self.numer.get());
        let denom = i64::from(self.denom.get());

        // Always positive, can never exceeds i64::MAX at any point.
        let frac = (i64::from(time.nanos) * denom) / NS_PER_SEC;

        if let Some(ts) = time
            .seconds
            .checked_mul(denom)
            .and_then(|whole| whole.checked_add(frac))
            .map(|x| x / numer)
        {
            // Common case: Calculation can be done entirely in an i64.
            Some(Timestamp(ts))
        }
        else {
            // Fallback case: Calculation must be done in an i128.
            let whole = i128::from(time.seconds) * i128::from(denom);
            let ts = (whole + i128::from(frac)) / i128::from(numer);
            ts.try_into().ok().map(Timestamp)
        }
    }
}

impl From<TimeBase> for f64 {
    fn from(timebase: TimeBase) -> Self {
        f64::from(timebase.numer.get()) / f64::from(timebase.denom.get())
    }
}

impl fmt::Display for TimeBase {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}/{}", self.numer, self.denom)
    }
}

#[cfg(test)]
mod tests {
    use super::{Time, TimeBase, Timestamp};

    #[test]
    fn verify_time() {
        // Zero.
        assert_eq!(Time::try_new(0, 0).unwrap(), Time::ZERO);
        assert_eq!(Time::try_from_secs_f64(0.0).unwrap(), Time::ZERO);
        assert_eq!(Time::try_from_nanos_i128(0).unwrap(), Time::ZERO);
        assert_eq!(Time::from_nanos(0), Time::ZERO);
        assert_eq!(Time::from_micros(0), Time::ZERO);
        assert_eq!(Time::from_millis(0), Time::ZERO);

        // Do not allow > 999_999_999 nanoseconds.
        assert!(Time::try_new(0, 999_999_999).is_some());
        assert!(Time::try_new(0, 1_000_000_000).is_none());

        // Clock-time exceeds 999_999_999 nanoseconds.
        assert!(Time::from_ss(0, 1_000_000_000).is_none());
        assert!(Time::from_mmss(0, 0, 1_000_000_000).is_none());
        assert!(Time::from_hhmmss(0, 0, 0, 1_000_000_000).is_none());
        // Clock-time exceeds 59 seconds.
        assert!(Time::from_ss(60, 0).is_none());
        assert!(Time::from_mmss(0, 60, 0).is_none());
        assert!(Time::from_hhmmss(0, 0, 60, 0).is_none());
        // Clock-time exceeds 59 minutes.
        assert!(Time::from_mmss(60, 0, 0).is_none());
        assert!(Time::from_hhmmss(0, 60, 0, 0).is_none());

        // Equality.
        assert!(Time::try_new(5, 1) == Time::try_new(5, 1));
        assert!(Time::try_new(-5, 1) == Time::try_new(-5, 1));

        // Ordering.
        assert!(Time::ZERO < Time::try_new(0, 1).unwrap()); // 0 < 0.000_000_001
        assert!(Time::ZERO > Time::try_new(-1, 999_999_999).unwrap()); // 0 > -0.000_000_001
        assert!(Time::try_new(5, 1) < Time::try_new(5, 2)); // 5.000_000_001 < 5.000_000_002
        assert!(Time::try_new(-5, 1) < Time::try_new(-5, 2)); // -4.999_999_999 < -4.999_999_998

        // Decomposition
        assert_eq!(Time::try_new(1, 750_000_000).unwrap().parts(), (1, 750_000_000)); // 1.75 s
        assert_eq!(Time::try_new(0, 100_000_000).unwrap().parts(), (0, 100_000_000)); // 0.1 s
        assert_eq!(Time::try_new(-2, 750_000_000).unwrap().parts(), (-1, 250_000_000)); // -1.25 s

        // As nanoseconds.
        assert_eq!(Time::try_new(100, 250_999_999).unwrap().as_nanos(), 100_250_999_999);
        assert_eq!(Time::try_new(0, 100_999_999).unwrap().as_nanos(), 100_999_999);
        assert_eq!(Time::try_new(-100, 250_999_999).unwrap().as_nanos(), -99_749_000_001);

        // As whole microseconds.
        assert_eq!(Time::try_new(100, 250_999_999).unwrap().as_micros(), 100_250_999);
        assert_eq!(Time::try_new(0, 100_999_999).unwrap().as_micros(), 100_999);
        assert_eq!(Time::try_new(-100, 250_999_999).unwrap().as_micros(), -99_749_000);

        // As whole milliseconds.
        assert_eq!(Time::try_new(100, 250_999_999).unwrap().as_millis(), 100_250);
        assert_eq!(Time::try_new(0, 100_999_999).unwrap().as_millis(), 100);
        assert_eq!(Time::try_new(-100, 250_999_999).unwrap().as_millis(), -99_749);

        // As whole seconds.
        assert_eq!(Time::try_new(100, 250_000_000).unwrap().as_secs(), 100); // 100.25 s
        assert_eq!(Time::try_new(-100, 250_000_000).unwrap().as_secs(), -99); // -99.75 s
        assert_eq!(Time::try_new(-100, 0).unwrap().as_secs(), -100); // -100 s

        // As whole minutes.
        assert_eq!(Time::try_new(60, 0).unwrap().as_mins(), 1); // 60 s
        assert_eq!(Time::try_new(-60, 0).unwrap().as_mins(), -1); // -59 s
        assert_eq!(Time::try_new(-60, 1).unwrap().as_mins(), 0); // -59.999999999 s

        // As whole hours.
        assert_eq!(Time::try_new(3600, 0).unwrap().as_hours(), 1); // 3600 s
        assert_eq!(Time::try_new(-3600, 0).unwrap().as_hours(), -1); // -3600 s
        assert_eq!(Time::try_new(-3600, 1).unwrap().as_hours(), 0); // -3599.999999999 s

        // A floating-point seconds.
        assert_eq!(Time::try_new(100, 250_000_000).unwrap().as_secs_f64(), 100.25);
        assert_eq!(Time::try_new(0, 100_000_000).unwrap().as_secs_f64(), 0.1);
        assert_eq!(Time::try_new(-100, 250_000_000).unwrap().as_secs_f64(), -99.75);
    }

    #[test]
    fn verify_timebase() {
        let tb1 = TimeBase::try_new(1, 320).unwrap(); // 1 tick = 3.125 ms
        let tb2 = TimeBase::try_new(1_000_000, 320_000_000).unwrap(); // 1 tick = 3.125 ms

        assert_eq!(tb1.calc_time(Timestamp::from(0)).unwrap(), Time::try_new(0, 0).unwrap());
        assert_eq!(
            tb1.calc_time(Timestamp::from(i64::MAX)).unwrap(),
            Time::try_new(28823037615171174, 396875000).unwrap()
        );
        assert_eq!(
            tb1.calc_time(Timestamp::from(i64::MIN)).unwrap(),
            Time::try_new(-28823037615171175, 600000000).unwrap()
        );
        assert_eq!(
            tb1.calc_time(Timestamp::from(12345)).unwrap(),
            Time::try_new(38, 578125000).unwrap()
        );

        assert_eq!(tb2.calc_time(Timestamp::from(0)).unwrap(), Time::try_new(0, 0).unwrap());
        assert_eq!(
            tb2.calc_time(Timestamp::from(i64::MAX)).unwrap(),
            Time::try_new(28823037615171174, 396875000).unwrap()
        );
        assert_eq!(
            tb2.calc_time(Timestamp::from(i64::MIN)).unwrap(),
            Time::try_new(-28823037615171175, 600000000).unwrap()
        );
        assert_eq!(
            tb2.calc_time(Timestamp::from(12345)).unwrap(),
            Time::try_new(38, 578125000).unwrap()
        );

        assert_eq!(Time::try_new(0, 0).unwrap(), tb1.calc_time(Timestamp::from(0)).unwrap());
        assert_eq!(
            Time::try_new(28823037615171174, 396875000).unwrap(),
            tb1.calc_time(Timestamp::from(i64::MAX)).unwrap()
        );
        assert_eq!(
            Time::try_new(-28823037615171175, 600000000).unwrap(),
            tb1.calc_time(Timestamp::from(i64::MIN)).unwrap()
        );
        assert_eq!(
            Time::try_new(38, 578125000).unwrap(),
            tb1.calc_time(Timestamp::from(12345)).unwrap()
        );

        assert_eq!(Time::try_new(0, 0).unwrap(), tb2.calc_time(Timestamp::from(0)).unwrap());
        assert_eq!(
            Time::try_new(28823037615171174, 396875000).unwrap(),
            tb2.calc_time(Timestamp::from(i64::MAX)).unwrap()
        );
        assert_eq!(
            Time::try_new(-28823037615171175, 600000000).unwrap(),
            tb2.calc_time(Timestamp::from(i64::MIN)).unwrap()
        );
        assert_eq!(
            Time::try_new(38, 578125000).unwrap(),
            tb2.calc_time(Timestamp::from(12345)).unwrap()
        );

        // Test overflow.
        let tb3 = TimeBase::try_new(u32::MAX, 1).unwrap(); // 1 tick = u32::MAX seconds
        let tb4 = TimeBase::try_new(1, u32::MAX).unwrap(); // 1 tick = 1 / u32::MAX seconds

        assert!(tb3.calc_time(Timestamp::from(i64::MIN)).is_none());
        assert!(tb3.calc_time(Timestamp::from(i64::MAX)).is_none());

        assert!(tb4.calc_timestamp(Time::MIN).is_none());
        assert!(tb4.calc_timestamp(Time::MAX).is_none());
    }

    #[test]
    fn verify_ts_roundtrip_with_time() {
        let bases = [
            TimeBase::try_new(1, 2).unwrap(),
            TimeBase::try_new(1, 25).unwrap(),
            TimeBase::try_new(1001, 30_000).unwrap(),
            TimeBase::try_new(1, 48_000).unwrap(),
            TimeBase::try_new(1, 1).unwrap(),
            TimeBase::try_new(1, 2).unwrap(),
            TimeBase::try_new(2, 3).unwrap(),
            TimeBase::try_new(3, 7).unwrap(),
            TimeBase::try_new(1, 1_000_000_000).unwrap(),
            TimeBase::try_new(u32::MAX, u32::MAX).unwrap(),
        ];

        let ticks = [
            i64::MIN,
            i64::MIN + 1,
            -1_000_000,
            -12_345,
            -1,
            0,
            1,
            12_345,
            1_000_000,
            i64::MAX - 1,
            i64::MAX,
        ];

        fn test_roundtrip(ts: i64, tb: TimeBase) {
            let time = tb.calc_time(Timestamp::from(ts)).expect("time should not overflow");
            let rtts = tb.calc_timestamp(time).expect("ticks should not overflow");
            let diff = rtts.get() - ts;
            // NOTE: `Time` only has nanosecond precision which may result in some remainder
            // being dropped. Therefore, the round-trip may result in 1 less tick.
            assert!((-1..=0).contains(&diff));
        }

        for tb in bases {
            for &ts in &ticks {
                test_roundtrip(ts, tb);
            }
        }

        for tb in bases {
            for ts in -10_000..=10_000 {
                test_roundtrip(ts, tb);
            }
        }
    }
}
