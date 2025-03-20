//! This module implements `Time` and any directly related algorithms.

use crate::{
    builtins::core::{duration::TimeDuration, Duration},
    iso::IsoTime,
    options::{
        ArithmeticOverflow, DifferenceOperation, DifferenceSettings, ResolvedRoundingOptions,
        RoundingIncrement, TemporalRoundingMode, TemporalUnit, ToStringRoundingOptions, UnitGroup,
    },
    parsers::{parse_time, IxdtfStringBuilder},
    primitive::FiniteF64,
    TemporalError, TemporalResult,
};
use alloc::string::String;
use core::str::FromStr;
use num_traits::AsPrimitive;

use super::{duration::normalized::NormalizedTimeDuration, PlainDateTime};

/// A `PartialTime` represents partially filled `Time` fields.
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct PartialTime {
    // A potentially set `hour` field.
    pub hour: Option<u8>,
    // A potentially set `minute` field.
    pub minute: Option<u8>,
    // A potentially set `second` field.
    pub second: Option<u8>,
    // A potentially set `millisecond` field.
    pub millisecond: Option<u16>,
    // A potentially set `microsecond` field.
    pub microsecond: Option<u16>,
    // A potentially set `nanosecond` field.
    pub nanosecond: Option<u16>,
}

impl PartialTime {
    pub fn is_empty(&self) -> bool {
        *self == Self::default()
    }
}

/// Convenience methods for building a `PartialTime`
impl PartialTime {
    pub const fn new() -> Self {
        Self {
            hour: None,
            minute: None,
            second: None,
            millisecond: None,
            microsecond: None,
            nanosecond: None,
        }
    }

    pub const fn with_hour(mut self, hour: Option<u8>) -> Self {
        self.hour = hour;
        self
    }

    pub const fn with_minute(mut self, minute: Option<u8>) -> Self {
        self.minute = minute;
        self
    }

    pub const fn with_second(mut self, second: Option<u8>) -> Self {
        self.second = second;
        self
    }

    pub const fn with_millisecond(mut self, millisecond: Option<u16>) -> Self {
        self.millisecond = millisecond;
        self
    }

    pub const fn with_microsecond(mut self, microsecond: Option<u16>) -> Self {
        self.microsecond = microsecond;
        self
    }

    pub const fn with_nanosecond(mut self, nanosecond: Option<u16>) -> Self {
        self.nanosecond = nanosecond;
        self
    }
}

/// The native Rust implementation of `Temporal.PlainTime`.
#[non_exhaustive]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct PlainTime {
    pub(crate) iso: IsoTime,
}

// ==== Private API ====

impl PlainTime {
    #[inline]
    #[must_use]
    /// Creates a new unvalidated `Time`.
    pub(crate) fn new_unchecked(iso: IsoTime) -> Self {
        Self { iso }
    }

    /// Returns true if a valid `Time`.
    #[allow(dead_code)]
    pub(crate) fn is_valid(&self) -> bool {
        self.iso.is_valid()
    }

    /// Specification equivalent to `AddTime`
    pub(crate) fn add_normalized_time_duration(&self, norm: NormalizedTimeDuration) -> (i32, Self) {
        // 1. Set second to second + NormalizedTimeDurationSeconds(norm).
        let second = i64::from(self.second()) + norm.seconds();
        // 2. Set nanosecond to nanosecond + NormalizedTimeDurationSubseconds(norm).
        let nanosecond = i32::from(self.nanosecond()) + norm.subseconds();
        // 3. Return BalanceTime(hour, minute, second, millisecond, microsecond, nanosecond).
        let (day, balance_result) = IsoTime::balance(
            self.hour().into(),
            self.minute().into(),
            second,
            self.millisecond().into(),
            self.microsecond().into(),
            nanosecond.into(),
        );

        (day, Self::new_unchecked(balance_result))
    }

    /// Adds a `TimeDuration` to the current `Time`.
    ///
    /// Spec Equivalent: `AddDurationToOrSubtractDurationFromPlainTime`.
    pub(crate) fn add_to_time(&self, duration: &TimeDuration) -> TemporalResult<Self> {
        let (_, result) = IsoTime::balance(
            FiniteF64::from(self.hour())
                .checked_add(&duration.hours)?
                .as_(),
            FiniteF64::from(self.minute())
                .checked_add(&duration.minutes)?
                .as_(),
            FiniteF64::from(self.second())
                .checked_add(&duration.seconds)?
                .as_(),
            FiniteF64::from(self.millisecond())
                .checked_add(&duration.milliseconds)?
                .as_(),
            FiniteF64::from(self.microsecond())
                .checked_add(&duration.microseconds)?
                .as_(),
            FiniteF64::from(self.nanosecond())
                .checked_add(&duration.nanoseconds)?
                .as_(),
        );

        // NOTE (nekevss): IsoTime::balance should never return an invalid `IsoTime`

        Ok(Self::new_unchecked(result))
    }

    // TODO: Migrate to
    /// Performs a desired difference op between two `Time`'s, returning the resulting `Duration`.
    pub(crate) fn diff_time(
        &self,
        op: DifferenceOperation,
        other: &PlainTime,
        settings: DifferenceSettings,
    ) -> TemporalResult<Duration> {
        // 1. If operation is SINCE, let sign be -1. Otherwise, let sign be 1.
        // 2. Set other to ? ToTemporalTime(other).
        // 3. Let resolvedOptions be ? SnapshotOwnProperties(? GetOptionsObject(options), null).
        // 4. Let settings be ? GetDifferenceSettings(operation, resolvedOptions, TIME, « », "nanosecond", "hour").
        let resolved = ResolvedRoundingOptions::from_diff_settings(
            settings,
            op,
            UnitGroup::Time,
            TemporalUnit::Hour,
            TemporalUnit::Nanosecond,
        )?;

        // 5. Let norm be ! DifferenceTime(temporalTime.[[ISOHour]], temporalTime.[[ISOMinute]],
        // temporalTime.[[ISOSecond]], temporalTime.[[ISOMillisecond]], temporalTime.[[ISOMicrosecond]],
        // temporalTime.[[ISONanosecond]], other.[[ISOHour]], other.[[ISOMinute]], other.[[ISOSecond]],
        // other.[[ISOMillisecond]], other.[[ISOMicrosecond]], other.[[ISONanosecond]]).
        let mut normalized_time = self.iso.diff(&other.iso).to_normalized();

        // 6. If settings.[[SmallestUnit]] is not "nanosecond" or settings.[[RoundingIncrement]] ≠ 1, then
        if resolved.smallest_unit != TemporalUnit::Nanosecond
            || resolved.increment != RoundingIncrement::ONE
        {
            // a. Let roundRecord be ! RoundDuration(0, 0, 0, 0, norm, settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
            let (round_record, _) = normalized_time.round(FiniteF64::default(), resolved)?;
            // b. Set norm to roundRecord.[[NormalizedDuration]].[[NormalizedTime]].
            normalized_time = round_record.normalized_time_duration()
        };

        // 7. Let result be BalanceTimeDuration(norm, settings.[[LargestUnit]]).
        let result = TimeDuration::from_normalized(normalized_time, resolved.largest_unit)?.1;

        // 8. Return ! CreateTemporalDuration(0, 0, 0, 0, sign × result.[[Hours]], sign × result.[[Minutes]], sign × result.[[Seconds]], sign × result.[[Milliseconds]], sign × result.[[Microseconds]], sign × result.[[Nanoseconds]]).
        match op {
            DifferenceOperation::Until => Ok(Duration::from(result)),
            DifferenceOperation::Since => Ok(Duration::from(result.negated())),
        }
    }
}

// ==== Public API ====

impl PlainTime {
    /// Creates a new `PlainTime`, constraining any field into a valid range.
    ///
    /// ```rust
    /// use temporal_rs::PlainTime;
    ///
    /// let time = PlainTime::new(23, 59, 59, 999, 999, 999).unwrap();
    ///
    /// let constrained_time = PlainTime::new(24, 59, 59, 999, 999, 999).unwrap();
    /// assert_eq!(time, constrained_time);
    /// ```
    pub fn new(
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
    ) -> TemporalResult<Self> {
        Self::new_with_overflow(
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            ArithmeticOverflow::Constrain,
        )
    }

    /// Creates a new `PlainTime`, rejecting any field that is not in a valid range.
    ///
    /// ```rust
    /// use temporal_rs::PlainTime;
    ///
    /// let time = PlainTime::try_new(23, 59, 59, 999, 999, 999).unwrap();
    ///
    /// let invalid_time = PlainTime::try_new(24, 59, 59, 999, 999, 999);
    /// assert!(invalid_time.is_err());
    /// ```
    pub fn try_new(
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
    ) -> TemporalResult<Self> {
        Self::new_with_overflow(
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            ArithmeticOverflow::Reject,
        )
    }

    /// Creates a new `PlainTime` with the provided [`ArithmeticOverflow`] option.
    #[inline]
    pub fn new_with_overflow(
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
        overflow: ArithmeticOverflow,
    ) -> TemporalResult<Self> {
        let time = IsoTime::new(
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            overflow,
        )?;
        Ok(Self::new_unchecked(time))
    }

    /// Creates a new `PlainTime` from a `PartialTime`.
    ///
    /// ```rust
    /// use temporal_rs::{partial::PartialTime, PlainTime};
    ///
    /// let partial_time = PartialTime {
    ///     hour: Some(22),
    ///     ..Default::default()
    /// };
    ///
    /// let time = PlainTime::from_partial(partial_time, None).unwrap();
    ///
    /// assert_eq!(time.hour(), 22);
    /// assert_eq!(time.minute(), 0);
    /// assert_eq!(time.second(), 0);
    /// assert_eq!(time.millisecond(), 0);
    /// assert_eq!(time.microsecond(), 0);
    /// assert_eq!(time.nanosecond(), 0);
    ///
    /// ```
    pub fn from_partial(
        partial: PartialTime,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        // NOTE: 4.5.12 ToTemporalTimeRecord requires one field to be set.
        if partial.is_empty() {
            return Err(TemporalError::r#type().with_message("PartialTime cannot be empty."));
        }

        let overflow = overflow.unwrap_or_default();
        let iso = IsoTime::default().with(partial, overflow)?;
        Ok(Self::new_unchecked(iso))
    }

    /// Creates a new `PlainTime` using the current `PlainTime` fields as a fallback.
    ///
    /// ```rust
    /// use temporal_rs::{partial::PartialTime, PlainTime};
    ///
    /// let partial_time = PartialTime {
    ///     hour: Some(22),
    ///     ..Default::default()
    /// };
    ///
    /// let initial = PlainTime::try_new(15, 30, 12, 123, 456, 789).unwrap();
    ///
    /// let time = initial.with(partial_time, None).unwrap();
    ///
    /// assert_eq!(time.hour(), 22);
    /// assert_eq!(time.minute(), 30);
    /// assert_eq!(time.second(), 12);
    /// assert_eq!(time.millisecond(), 123);
    /// assert_eq!(time.microsecond(), 456);
    /// assert_eq!(time.nanosecond(), 789);
    ///
    /// ```
    pub fn with(
        &self,
        partial: PartialTime,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        // NOTE: 4.5.12 ToTemporalTimeRecord requires one field to be set.
        if partial.is_empty() {
            return Err(TemporalError::r#type().with_message("PartialTime cannot be empty."));
        }

        let iso = self
            .iso
            .with(partial, overflow.unwrap_or(ArithmeticOverflow::Constrain))?;
        Ok(Self::new_unchecked(iso))
    }

    /// Returns the internal `hour` field.
    #[inline]
    #[must_use]
    pub const fn hour(&self) -> u8 {
        self.iso.hour
    }

    /// Returns the internal `minute` field.
    #[inline]
    #[must_use]
    pub const fn minute(&self) -> u8 {
        self.iso.minute
    }

    /// Returns the internal `second` field.
    #[inline]
    #[must_use]
    pub const fn second(&self) -> u8 {
        self.iso.second
    }

    /// Returns the internal `millisecond` field.
    #[inline]
    #[must_use]
    pub const fn millisecond(&self) -> u16 {
        self.iso.millisecond
    }

    /// Returns the internal `microsecond` field.
    #[inline]
    #[must_use]
    pub const fn microsecond(&self) -> u16 {
        self.iso.microsecond
    }

    /// Returns the internal `nanosecond` field.
    #[inline]
    #[must_use]
    pub const fn nanosecond(&self) -> u16 {
        self.iso.nanosecond
    }

    /// Add a `Duration` to the current `Time`.
    pub fn add(&self, duration: &Duration) -> TemporalResult<Self> {
        if !duration.is_time_duration() {
            return Err(TemporalError::range()
                .with_message("DateDuration values cannot be added to `Time`."));
        }
        self.add_time_duration(duration.time())
    }

    /// Adds a `TimeDuration` to the current `Time`.
    #[inline]
    pub fn add_time_duration(&self, duration: &TimeDuration) -> TemporalResult<Self> {
        self.add_to_time(duration)
    }

    /// Subtract a `Duration` to the current `Time`.
    pub fn subtract(&self, duration: &Duration) -> TemporalResult<Self> {
        if !duration.is_time_duration() {
            return Err(TemporalError::range()
                .with_message("DateDuration values cannot be added to `Time` component."));
        }
        self.subtract_time_duration(duration.time())
    }

    /// Adds a `TimeDuration` to the current `Time`.
    #[inline]
    pub fn subtract_time_duration(&self, duration: &TimeDuration) -> TemporalResult<Self> {
        self.add_to_time(&duration.negated())
    }

    #[inline]
    /// Returns the `Duration` until the provided `Time` from the current `Time`.
    ///
    /// NOTE: `until` assumes the provided other time will occur in the future relative to the current.
    pub fn until(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff_time(DifferenceOperation::Until, other, settings)
    }

    #[inline]
    /// Returns the `Duration` since the provided `Time` from the current `Time`.
    ///
    /// NOTE: `since` assumes the provided other time is in the past relative to the current.
    pub fn since(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff_time(DifferenceOperation::Since, other, settings)
    }

    // TODO (nekevss): optimize and test rounding_increment type (f64 vs. u64).
    /// Rounds the current `Time` according to provided options.
    pub fn round(
        &self,
        smallest_unit: TemporalUnit,
        rounding_increment: Option<f64>,
        rounding_mode: Option<TemporalRoundingMode>,
    ) -> TemporalResult<Self> {
        let increment = RoundingIncrement::try_from(rounding_increment.unwrap_or(1.0))?;
        let rounding_mode = rounding_mode.unwrap_or(TemporalRoundingMode::HalfExpand);

        let max = smallest_unit
            .to_maximum_rounding_increment()
            .ok_or_else(|| {
                TemporalError::range().with_message("smallestUnit must be a time value.")
            })?;

        // Safety (nekevss): to_rounding_increment returns a value in the range of a u32.
        increment.validate(u64::from(max), false)?;

        let resolved = ResolvedRoundingOptions {
            largest_unit: TemporalUnit::Auto,
            increment,
            smallest_unit,
            rounding_mode,
        };

        let (_, result) = self.iso.round(resolved)?;

        Ok(Self::new_unchecked(result))
    }

    pub fn to_ixdtf_string(&self, options: ToStringRoundingOptions) -> TemporalResult<String> {
        let resolved = options.resolve()?;
        let (_, result) = self
            .iso
            .round(ResolvedRoundingOptions::from_to_string_options(&resolved))?;
        let ixdtf_string = IxdtfStringBuilder::default()
            .with_time(result, resolved.precision)
            .build();
        Ok(ixdtf_string)
    }
}

impl From<PlainDateTime> for PlainTime {
    fn from(value: PlainDateTime) -> Self {
        PlainTime::new_unchecked(value.iso.time)
    }
}

impl FromStr for PlainTime {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let result = parse_time(s)?;
        let iso = IsoTime::from_time_record(result)?;
        Ok(Self::new_unchecked(iso))
    }
}

// ==== Test land ====

#[cfg(test)]
mod tests {
    use core::str::FromStr;

    use crate::{
        builtins::core::Duration,
        iso::IsoTime,
        options::{ArithmeticOverflow, DifferenceSettings, RoundingIncrement, TemporalUnit},
    };
    use num_traits::FromPrimitive;

    use super::PlainTime;

    fn assert_time(result: PlainTime, values: (u8, u8, u8, u16, u16, u16)) {
        assert_eq!(
            result,
            PlainTime {
                iso: IsoTime {
                    hour: values.0,
                    minute: values.1,
                    second: values.2,
                    millisecond: values.3,
                    microsecond: values.4,
                    nanosecond: values.5,
                }
            }
        );
    }

    fn assert_duration(
        result: Duration,
        values: (i32, i32, i32, i32, i32, i32, i32, i32, i32, i32),
    ) {
        let fields = result
            .fields()
            .iter()
            .map(|v| v.as_date_value().unwrap())
            .collect::<alloc::vec::Vec<i32>>();
        assert_eq!(
            fields,
            &[
                values.0, values.1, values.2, values.3, values.4, values.5, values.6, values.7,
                values.8, values.9
            ]
        )
    }

    #[test]
    fn from_str_cast_sanity_test() {
        let max = u32::MAX;
        let (millisecond, rem) = (max / 1_000_000, max % 1_000_000);
        let (microsecond, nanosecond) = (rem / 1_000, rem % 1_000);

        assert!(i32::from_u32(millisecond).is_some());
        assert!(i32::from_u32(microsecond).is_some());
        assert!(i32::from_u32(nanosecond).is_some());
    }

    #[test]
    fn basic_parse_time() {
        let result = "T12:05:24-05:00[u-ca=iso8601]".parse::<PlainTime>();
        assert_time(result.unwrap(), (12, 5, 24, 0, 0, 0));

        let result = "T12:05:24.123456789-05:00[u-ca=iso8601]".parse::<PlainTime>();
        assert_time(result.unwrap(), (12, 5, 24, 123, 456, 789));

        let result = "2024-05-04 12:05:24.123456789-05:00[u-ca=iso8601]".parse::<PlainTime>();
        assert_time(result.unwrap(), (12, 5, 24, 123, 456, 789));

        let result = "2024-05-04 12:05:24.123456789-05:00[u-ca=iso8601]".parse::<PlainTime>();
        assert_time(result.unwrap(), (12, 5, 24, 123, 456, 789));
    }

    #[test]
    fn time_round_millisecond() {
        let base = PlainTime::new_unchecked(IsoTime::new_unchecked(3, 34, 56, 987, 654, 321));

        let result_1 = base
            .round(TemporalUnit::Millisecond, Some(1.0), None)
            .unwrap();
        assert_time(result_1, (3, 34, 56, 988, 0, 0));

        let result_2 = base
            .round(TemporalUnit::Millisecond, Some(2.0), None)
            .unwrap();
        assert_time(result_2, (3, 34, 56, 988, 0, 0));

        let result_3 = base
            .round(TemporalUnit::Millisecond, Some(4.0), None)
            .unwrap();
        assert_time(result_3, (3, 34, 56, 988, 0, 0));

        let result_4 = base
            .round(TemporalUnit::Millisecond, Some(5.0), None)
            .unwrap();
        assert_time(result_4, (3, 34, 56, 990, 0, 0));
    }

    #[test]
    fn time_round_microsecond() {
        let base = PlainTime::new_unchecked(IsoTime::new_unchecked(3, 34, 56, 987, 654, 321));

        let result_1 = base
            .round(TemporalUnit::Microsecond, Some(1.0), None)
            .unwrap();
        assert_time(result_1, (3, 34, 56, 987, 654, 0));

        let result_2 = base
            .round(TemporalUnit::Microsecond, Some(2.0), None)
            .unwrap();
        assert_time(result_2, (3, 34, 56, 987, 654, 0));

        let result_3 = base
            .round(TemporalUnit::Microsecond, Some(4.0), None)
            .unwrap();
        assert_time(result_3, (3, 34, 56, 987, 656, 0));

        let result_4 = base
            .round(TemporalUnit::Microsecond, Some(5.0), None)
            .unwrap();
        assert_time(result_4, (3, 34, 56, 987, 655, 0));
    }

    #[test]
    fn time_round_nanoseconds() {
        let base = PlainTime::new_unchecked(IsoTime::new_unchecked(3, 34, 56, 987, 654, 321));

        let result_1 = base
            .round(TemporalUnit::Nanosecond, Some(1.0), None)
            .unwrap();
        assert_time(result_1, (3, 34, 56, 987, 654, 321));

        let result_2 = base
            .round(TemporalUnit::Nanosecond, Some(2.0), None)
            .unwrap();
        assert_time(result_2, (3, 34, 56, 987, 654, 322));

        let result_3 = base
            .round(TemporalUnit::Nanosecond, Some(4.0), None)
            .unwrap();
        assert_time(result_3, (3, 34, 56, 987, 654, 320));

        let result_4 = base
            .round(TemporalUnit::Nanosecond, Some(5.0), None)
            .unwrap();
        assert_time(result_4, (3, 34, 56, 987, 654, 320));
    }

    #[test]
    fn add_duration_basic() {
        let base = PlainTime::new_unchecked(IsoTime::new_unchecked(15, 23, 30, 123, 456, 789));
        let result = base.add(&"PT16H".parse::<Duration>().unwrap()).unwrap();

        assert_time(result, (7, 23, 30, 123, 456, 789));
    }

    #[test]
    fn since_basic() {
        let one =
            PlainTime::new_with_overflow(15, 23, 30, 123, 456, 789, ArithmeticOverflow::Constrain)
                .unwrap();
        let two =
            PlainTime::new_with_overflow(14, 23, 30, 123, 456, 789, ArithmeticOverflow::Constrain)
                .unwrap();
        let three =
            PlainTime::new_with_overflow(13, 30, 30, 123, 456, 789, ArithmeticOverflow::Constrain)
                .unwrap();

        let result = one.since(&two, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), 1.0);

        let result = two.since(&one, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), -1.0);

        let result = one.since(&three, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), 1.0);
        assert_eq!(result.minutes(), 53.0);

        let result = three.since(&one, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), -1.0);
        assert_eq!(result.minutes(), -53.0);
    }

    #[test]
    fn until_basic() {
        let one =
            PlainTime::new_with_overflow(15, 23, 30, 123, 456, 789, ArithmeticOverflow::Constrain)
                .unwrap();
        let two =
            PlainTime::new_with_overflow(16, 23, 30, 123, 456, 789, ArithmeticOverflow::Constrain)
                .unwrap();
        let three =
            PlainTime::new_with_overflow(17, 0, 30, 123, 456, 789, ArithmeticOverflow::Constrain)
                .unwrap();

        let result = one.until(&two, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), 1.0);

        let result = two.until(&one, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), -1.0);

        let result = one.until(&three, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), 1.0);
        assert_eq!(result.minutes(), 37.0);

        let result = three.until(&one, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), -1.0);
        assert_eq!(result.minutes(), -37.0);
    }

    #[test]
    fn since_rounding() {
        let earlier = PlainTime::new(3, 12, 34, 123, 456, 789).unwrap();
        let later = PlainTime::new(13, 47, 57, 988, 655, 322).unwrap();

        let settings = DifferenceSettings {
            smallest_unit: Some(TemporalUnit::Second),
            increment: Some(RoundingIncrement::try_new(1).unwrap()),
            ..Default::default()
        };
        assert_duration(
            later.since(&earlier, settings).unwrap(),
            (0, 0, 0, 0, 10, 35, 23, 0, 0, 0),
        );

        let settings = DifferenceSettings {
            smallest_unit: Some(TemporalUnit::Second),
            increment: Some(RoundingIncrement::try_new(4).unwrap()),
            ..Default::default()
        };
        assert_duration(
            later.since(&earlier, settings).unwrap(),
            (0, 0, 0, 0, 10, 35, 20, 0, 0, 0),
        );
    }

    #[test]
    // test262/test/built-ins/Temporal/PlainTime/prototype/round/roundingincrement-nanoseconds.js
    fn rounding_increment_nanos() {
        let time =
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 321, ArithmeticOverflow::Constrain)
                .unwrap();

        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(1.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 321, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(2.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 322, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(4.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 320, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(5.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 320, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(8.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 320, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(10.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 320, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(20.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 320, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(25.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 325, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(40.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 320, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(50.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 300, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(100.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 300, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(125.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 375, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(200.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 400, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(250.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 250, ArithmeticOverflow::Constrain)
                .unwrap()
        );
        assert_eq!(
            time.round(TemporalUnit::Nanosecond, Some(500.0), None)
                .unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 500, ArithmeticOverflow::Constrain)
                .unwrap()
        );
    }

    #[test]
    fn invalid_time_from_strs() {
        // UTC designator case
        let invalid_cases = [
            "2019-10-01T09:00:00Z",
            "2019-10-01T09:00:00Z[UTC]",
            "09:00:00Z[UTC]",
            "09:00:00Z",
        ];
        for invalid_str in invalid_cases {
            let err = PlainTime::from_str(invalid_str);
            assert!(err.is_err());
        }
    }
}
