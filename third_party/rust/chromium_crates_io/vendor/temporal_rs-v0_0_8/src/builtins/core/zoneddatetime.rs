//! This module contains the core implementation of the `ZonedDateTime`
//! builtin type.

use alloc::string::String;
use core::{cmp::Ordering, num::NonZeroU128};
use ixdtf::parsers::records::UtcOffsetRecordOrZ;
use tinystr::TinyAsciiStr;

use crate::{
    builtins::core::{
        calendar::Calendar,
        duration::normalized::{NormalizedDurationRecord, NormalizedTimeDuration},
        timezone::{TimeZone, UtcOffset},
        Duration, Instant, PlainDate, PlainDateTime, PlainTime,
    },
    iso::{IsoDate, IsoDateTime, IsoTime},
    options::{
        ArithmeticOverflow, DifferenceOperation, DifferenceSettings, Disambiguation,
        DisplayCalendar, DisplayOffset, DisplayTimeZone, OffsetDisambiguation,
        ResolvedRoundingOptions, RoundingIncrement, RoundingMode, RoundingOptions,
        ToStringRoundingOptions, Unit, UnitGroup,
    },
    parsers::{self, FormattableOffset, FormattableTime, IxdtfStringBuilder, Precision},
    partial::{PartialDate, PartialTime},
    primitive::FiniteF64,
    provider::{TimeZoneProvider, TransitionDirection},
    rounding::{IncrementRounder, Round},
    temporal_assert,
    unix_time::EpochNanoseconds,
    MonthCode, Sign, TemporalError, TemporalResult, TemporalUnwrap,
};

/// A struct representing a partial `ZonedDateTime`.
#[derive(Debug, Default, Clone, PartialEq)]
pub struct PartialZonedDateTime {
    /// The `PartialDate` portion of a `PartialZonedDateTime`
    pub date: PartialDate,
    /// The `PartialTime` portion of a `PartialZonedDateTime`
    pub time: PartialTime,
    /// An optional offset string
    pub offset: Option<UtcOffset>,
    /// The time zone value of a partial time zone.
    pub timezone: Option<TimeZone>,
}

impl PartialZonedDateTime {
    pub fn is_empty(&self) -> bool {
        self.date.is_empty()
            && self.time.is_empty()
            && self.offset.is_none()
            && self.timezone.is_none()
    }

    pub const fn new() -> Self {
        Self {
            date: PartialDate::new(),
            time: PartialTime::new(),
            offset: None,
            timezone: None,
        }
    }

    pub const fn with_date(mut self, partial_date: PartialDate) -> Self {
        self.date = partial_date;
        self
    }

    pub const fn with_time(mut self, partial_time: PartialTime) -> Self {
        self.time = partial_time;
        self
    }

    pub const fn with_offset(mut self, offset: Option<UtcOffset>) -> Self {
        self.offset = offset;
        self
    }

    pub fn with_timezone(mut self, timezone: Option<TimeZone>) -> Self {
        self.timezone = timezone;
        self
    }
}

/// The native Rust implementation of a Temporal `ZonedDateTime`.
///
/// A `ZonedDateTime` represents a date and time in a specific time
/// zone and calendar. A `ZonedDateTime` is represented as an instant
/// of unix epoch nanoseconds with a calendar, and a time zone.
///
/// ## Time zone provider` API
///
/// The core implementation of `ZonedDateTime` are primarily time zone
/// provider APIs denoted by a `*_with_provider` suffix. This means a
/// provider that implements the `TimeZoneProvider` trait must be provided.
///
/// A default file system time zone provider, `FsTzdbProvider`, can be
/// enabled with the `tzdb` feature flag.
///
/// The non time zone provider API, which is a default implementation of the
/// methods using `FsTzdbProvider` can be enabled with the `compiled_data`
/// feature flag.
///
/// ## Example
///
/// ```rust
/// use temporal_rs::{Calendar, Instant, TimeZone, ZonedDateTime};
///
/// let zoned_date_time = ZonedDateTime::try_new(
///     0,
///     Calendar::default(),
///     TimeZone::default(),
/// ).unwrap();
///
/// assert_eq!(zoned_date_time.epoch_milliseconds(), 0);
/// assert_eq!(zoned_date_time.epoch_nanoseconds().as_i128(), 0);
/// assert_eq!(zoned_date_time.timezone().identifier().unwrap(), "UTC");
/// assert_eq!(zoned_date_time.calendar().identifier(), "iso8601");
/// ```
///
/// ## Reference
///
/// For more information, see the [MDN documentation][mdn-zoneddatetime].
///
/// [mdn-zoneddatetime]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Temporal/ZonedDateTime
#[non_exhaustive]
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ZonedDateTime {
    instant: Instant,
    calendar: Calendar,
    tz: TimeZone,
}

// ==== Private API ====

impl ZonedDateTime {
    /// Creates a `ZonedDateTime` without validating the input.
    #[inline]
    #[must_use]
    pub(crate) fn new_unchecked(instant: Instant, calendar: Calendar, tz: TimeZone) -> Self {
        Self {
            instant,
            calendar,
            tz,
        }
    }

    pub(crate) fn add_as_instant(
        &self,
        duration: &Duration,
        overflow: ArithmeticOverflow,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Instant> {
        // 1. If DateDurationSign(duration.[[Date]]) = 0, then
        if duration.date().sign() == Sign::Zero {
            // a. Return ? AddInstant(epochNanoseconds, duration.[[Time]]).
            return self.instant.add_to_instant(duration.time());
        }
        // 2. Let isoDateTime be GetISODateTimeFor(timeZone, epochNanoseconds).
        let iso_datetime = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        // 3. Let addedDate be ? CalendarDateAdd(calendar, isoDateTime.[[ISODate]], duration.[[Date]], overflow).
        let added_date = self
            .calendar()
            .date_add(&iso_datetime.date, duration, overflow)?;
        // 4. Let intermediateDateTime be CombineISODateAndTimeRecord(addedDate, isoDateTime.[[Time]]).
        let intermediate = IsoDateTime::new_unchecked(added_date.iso, iso_datetime.time);
        // 5. If ISODateTimeWithinLimits(intermediateDateTime) is false, throw a RangeError exception.
        if !intermediate.is_within_limits() {
            return Err(TemporalError::range()
                .with_message("Intermediate ISO datetime was not within a valid range."));
        }
        // 6. Let intermediateNs be ! GetEpochNanosecondsFor(timeZone, intermediateDateTime, compatible).
        let intermediate_ns = self.timezone().get_epoch_nanoseconds_for(
            intermediate,
            Disambiguation::Compatible,
            provider,
        )?;

        // 7. Return ? AddInstant(intermediateNs, duration.[[Time]]).
        Instant::from(intermediate_ns).add_to_instant(duration.time())
    }

    /// Adds a duration to the current `ZonedDateTime`, returning the resulting `ZonedDateTime`.
    ///
    /// Aligns with Abstract Operation 6.5.10
    #[inline]
    pub(crate) fn add_internal(
        &self,
        duration: &Duration,
        overflow: ArithmeticOverflow,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        // 1. Let duration be ? ToTemporalDuration(temporalDurationLike).
        // 2. If operation is subtract, set duration to CreateNegatedTemporalDuration(duration).
        // 3. Let resolvedOptions be ? GetOptionsObject(options).
        // 4. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
        // 5. Let calendar be zonedDateTime.[[Calendar]].
        // 6. Let timeZone be zonedDateTime.[[TimeZone]].
        // 7. Let internalDuration be ToInternalDurationRecord(duration).
        // 8. Let epochNanoseconds be ? AddZonedDateTime(zonedDateTime.[[EpochNanoseconds]], timeZone, calendar, internalDuration, overflow).
        let epoch_ns = self.add_as_instant(duration, overflow, provider)?;
        // 9. Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
        Ok(Self::new_unchecked(
            epoch_ns,
            self.calendar().clone(),
            self.timezone().clone(),
        ))
    }

    /// Internal representation of Abstract Op 6.5.7
    pub(crate) fn diff_with_rounding(
        &self,
        other: &Self,
        resolved_options: ResolvedRoundingOptions,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<NormalizedDurationRecord> {
        // 1. If UnitCategory(largestUnit) is time, then
        if resolved_options.largest_unit.is_time_unit() {
            // a. Return DifferenceInstant(ns1, ns2, roundingIncrement, smallestUnit, roundingMode).
            return self
                .instant
                .diff_instant_internal(&other.instant, resolved_options);
        }
        // 2. let difference be ? differencezoneddatetime(ns1, ns2, timezone, calendar, largestunit).
        let diff = self.diff_zoned_datetime(other, resolved_options.largest_unit, provider)?;
        // 3. if smallestunit is nanosecond and roundingincrement = 1, return difference.
        if resolved_options.smallest_unit == Unit::Nanosecond
            && resolved_options.increment == RoundingIncrement::ONE
        {
            return Ok(diff);
        }
        // 4. let datetime be getisodatetimefor(timezone, ns1).
        let iso = self
            .timezone()
            .get_iso_datetime_for(&self.instant, provider)?;
        // 5. Return ? RoundRelativeDuration(difference, ns2, dateTime, timeZone, calendar, largestUnit, roundingIncrement, smallestUnit, roundingMode).
        diff.round_relative_duration(
            other.epoch_nanoseconds().as_i128(),
            &PlainDateTime::new_unchecked(iso, self.calendar().clone()),
            Some((self.timezone(), provider)),
            resolved_options,
        )
    }

    /// Internal representation of Abstract Op 6.5.8
    pub(crate) fn diff_with_total(
        &self,
        other: &Self,
        unit: Unit,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<FiniteF64> {
        // 1. If UnitCategory(unit) is time, then
        if unit.is_time_unit() {
            // a. Let difference be TimeDurationFromEpochNanosecondsDifference(ns2, ns1).
            let diff = NormalizedTimeDuration::from_nanosecond_difference(
                other.epoch_nanoseconds().as_i128(),
                self.epoch_nanoseconds().as_i128(),
            )?;
            // b. Return TotalTimeDuration(difference, unit).
            return Ok(diff.total(unit))?;
        }

        // 2. Let difference be ? DifferenceZonedDateTime(ns1, ns2, timeZone, calendar, unit).
        let diff = self.diff_zoned_datetime(other, unit, provider)?;
        // 3. Let dateTime be GetISODateTimeFor(timeZone, ns1).
        let iso = self
            .timezone()
            .get_iso_datetime_for(&self.instant, provider)?;
        // 4. Return ? TotalRelativeDuration(difference, ns2, dateTime, timeZone, calendar, unit).
        diff.total_relative_duration(
            other.epoch_nanoseconds().as_i128(),
            &PlainDateTime::new_unchecked(iso, self.calendar().clone()),
            Some((self.timezone(), provider)),
            unit,
        )
    }

    pub(crate) fn diff_zoned_datetime(
        &self,
        other: &Self,
        largest_unit: Unit,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<NormalizedDurationRecord> {
        // 1. If ns1 = ns2, return CombineDateAndTimeDuration(ZeroDateDuration(), 0).
        if self.epoch_nanoseconds() == other.epoch_nanoseconds() {
            return Ok(NormalizedDurationRecord::default());
        }
        // 2. Let startDateTime be GetISODateTimeFor(timeZone, ns1).
        let start = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        // 3. Let endDateTime be GetISODateTimeFor(timeZone, ns2).
        let end = self.tz.get_iso_datetime_for(&other.instant, provider)?;
        // 4. If ns2 - ns1 < 0, let sign be -1; else let sign be 1.
        let sign = if other.epoch_nanoseconds().as_i128() - self.epoch_nanoseconds().as_i128() < 0 {
            Sign::Negative
        } else {
            Sign::Positive
        };
        // 5. If sign = 1, let maxDayCorrection be 2; else let maxDayCorrection be 1.
        let max_correction = if sign == Sign::Positive { 2 } else { 1 };
        // 6. Let dayCorrection be 0.
        // 7. Let timeDuration be DifferenceTime(startDateTime.[[Time]], endDateTime.[[Time]]).
        let time = start.time.diff(&end.time);
        // 8. If TimeDurationSign(timeDuration) = -sign, set dayCorrection to dayCorrection + 1.
        let mut day_correction = if time.sign() as i8 == -(sign as i8) {
            1
        } else {
            0
        };

        // 9. Let success be false.
        let mut intermediate_dt = IsoDateTime::default();
        let mut time_duration = NormalizedTimeDuration::default();
        let mut is_success = false;
        // 10. Repeat, while dayCorrection ≤ maxDayCorrection and success is false,
        while day_correction <= max_correction && !is_success {
            // a. Let intermediateDate be BalanceISODate(endDateTime.[[ISODate]].[[Year]],
            // endDateTime.[[ISODate]].[[Month]], endDateTime.[[ISODate]].[[Day]] - dayCorrection × sign).
            let intermediate = IsoDate::balance(
                end.date.year,
                end.date.month.into(),
                i32::from(end.date.day) - i32::from(day_correction * sign as i8),
            );
            // b. Let intermediateDateTime be CombineISODateAndTimeRecord(intermediateDate, startDateTime.[[Time]]).
            intermediate_dt = IsoDateTime::new_unchecked(intermediate, start.time);
            // c. Let intermediateNs be ? GetEpochNanosecondsFor(timeZone, intermediateDateTime, compatible).
            let intermediate_ns = self.tz.get_epoch_nanoseconds_for(
                intermediate_dt,
                Disambiguation::Compatible,
                provider,
            )?;
            // d. Set timeDuration to TimeDurationFromEpochNanosecondsDifference(ns2, intermediateNs).
            time_duration = NormalizedTimeDuration::from_nanosecond_difference(
                other.epoch_nanoseconds().as_i128(),
                intermediate_ns.0,
            )?;
            // e. Let timeSign be TimeDurationSign(timeDuration).
            let time_sign = time_duration.sign() as i8;
            // f. If sign ≠ -timeSign, then
            if sign as i8 != -time_sign {
                // i. Set success to true.
                is_success = true;
            }
            // g. Set dayCorrection to dayCorrection + 1.
            day_correction += 1;
        }
        // 11. Assert: success is true.
        // 12. Let dateLargestUnit be LargerOfTwoUnits(largestUnit, day).
        let date_largest = largest_unit.max(Unit::Day);
        // 13. Let dateDifference be CalendarDateUntil(calendar, startDateTime.[[ISODate]], intermediateDateTime.[[ISODate]], dateLargestUnit).
        // 14. Return CombineDateAndTimeDuration(dateDifference, timeDuration).
        let date_diff =
            self.calendar()
                .date_until(&start.date, &intermediate_dt.date, date_largest)?;
        NormalizedDurationRecord::new(*date_diff.date(), time_duration)
    }

    /// `temporal_rs` equivalent to `DifferenceTemporalZonedDateTime`.
    pub(crate) fn diff_internal_with_provider(
        &self,
        op: DifferenceOperation,
        other: &Self,
        options: DifferenceSettings,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Duration> {
        // NOTE: for order of operations, this should be asserted prior to this point
        // by any engine implementors, but asserting out of caution.
        if self.calendar != other.calendar {
            return Err(TemporalError::range()
                .with_message("Calendar must be the same when diffing two ZonedDateTimes"));
        }

        // 4. Set settings be ? GetDifferenceSettings(operation, resolvedOptions, datetime, « », nanosecond, hour).
        let resolved_options = ResolvedRoundingOptions::from_diff_settings(
            options,
            op,
            UnitGroup::DateTime,
            Unit::Hour,
            Unit::Nanosecond,
        )?;

        // 5. If UnitCategory(settings.[[LargestUnit]]) is time, then
        if resolved_options.largest_unit.is_time_unit() {
            // a. Let internalDuration be DifferenceInstant(zonedDateTime.[[EpochNanoseconds]], other.[[EpochNanoseconds]], settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
            let internal = self
                .instant
                .diff_instant_internal(&other.instant, resolved_options)?;
            // b. Let result be ! TemporalDurationFromInternal(internalDuration, settings.[[LargestUnit]]).
            let result = Duration::from_normalized(internal, resolved_options.largest_unit)?;
            // c. If operation is since, set result to CreateNegatedTemporalDuration(result).
            // d. Return result.
            match op {
                DifferenceOperation::Since => return Ok(result.negated()),
                DifferenceOperation::Until => return Ok(result),
            }
        }

        // 6. NOTE: To calculate differences in two different time zones,
        // settings.[[LargestUnit]] must be a time unit, because day lengths
        // can vary between time zones due to DST and other UTC offset shifts.
        // 7. If TimeZoneEquals(zonedDateTime.[[TimeZone]], other.[[TimeZone]]) is false, then
        if self.tz != other.tz {
            // a. Throw a RangeError exception.
            return Err(TemporalError::range()
                .with_message("Time zones cannot be different if unit is a date unit."));
        }

        // 8. If zonedDateTime.[[EpochNanoseconds]] = other.[[EpochNanoseconds]], then
        if self.instant == other.instant {
            // a. Return ! CreateTemporalDuration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0).
            return Ok(Duration::default());
        }

        // 9. Let internalDuration be ? DifferenceZonedDateTimeWithRounding(zonedDateTime.[[EpochNanoseconds]], other.[[EpochNanoseconds]], zonedDateTime.[[TimeZone]], zonedDateTime.[[Calendar]], settings.[[LargestUnit]], settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
        let internal = self.diff_with_rounding(other, resolved_options, provider)?;
        // 10. Let result be ! TemporalDurationFromInternal(internalDuration, hour).
        let result = Duration::from_normalized(internal, Unit::Hour)?;
        // 11. If operation is since, set result to CreateNegatedTemporalDuration(result).
        // 12. Return result.
        match op {
            DifferenceOperation::Since => Ok(result.negated()),
            DifferenceOperation::Until => Ok(result),
        }
    }
}

// ==== Public API ====

impl ZonedDateTime {
    /// Creates a new valid `ZonedDateTime`.
    #[inline]
    pub fn try_new(nanos: i128, calendar: Calendar, time_zone: TimeZone) -> TemporalResult<Self> {
        let instant = Instant::try_new(nanos)?;
        Ok(Self::new_unchecked(instant, calendar, time_zone))
    }

    /// Creates a new valid `ZonedDateTime` with an ISO 8601 calendar.
    #[inline]
    pub fn try_new_iso(nanos: i128, time_zone: TimeZone) -> TemporalResult<Self> {
        let instant = Instant::try_new(nanos)?;
        Ok(Self::new_unchecked(instant, Calendar::default(), time_zone))
    }

    /// Returns `ZonedDateTime`'s Calendar.
    #[inline]
    #[must_use]
    pub fn calendar(&self) -> &Calendar {
        &self.calendar
    }

    /// Returns `ZonedDateTime`'s `TimeZone` slot.
    #[inline]
    #[must_use]
    pub fn timezone(&self) -> &TimeZone {
        &self.tz
    }

    /// Create a `ZonedDateTime` from a `PartialZonedDateTime`.
    #[inline]
    pub fn from_partial_with_provider(
        partial: PartialZonedDateTime,
        overflow: Option<ArithmeticOverflow>,
        disambiguation: Option<Disambiguation>,
        offset_option: Option<OffsetDisambiguation>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let overflow = overflow.unwrap_or(ArithmeticOverflow::Constrain);
        let disambiguation = disambiguation.unwrap_or(Disambiguation::Compatible);
        let offset_option = offset_option.unwrap_or(OffsetDisambiguation::Reject);

        let date = partial
            .date
            .calendar
            .date_from_partial(&partial.date, overflow)?
            .iso;
        let time = if !partial.time.is_empty() {
            Some(IsoTime::default().with(partial.time, overflow)?)
        } else {
            None
        };

        // Handle time zones
        let offset_nanos = partial
            .offset
            .map(|offset| i64::from(offset.0) * 60_000_000_000);

        let timezone = partial.timezone.unwrap_or_default();

        let epoch_nanos = interpret_isodatetime_offset(
            date,
            time,
            false,
            offset_nanos,
            &timezone,
            disambiguation,
            offset_option,
            true,
            provider,
        )?;

        Ok(Self::new_unchecked(
            Instant::from(epoch_nanos),
            partial.date.calendar.clone(),
            timezone,
        ))
    }

    /// Returns the `epochMilliseconds` value of this `ZonedDateTime`.
    #[must_use]
    pub fn epoch_milliseconds(&self) -> i64 {
        self.instant.epoch_milliseconds()
    }

    /// Returns the `epochNanoseconds` value of this `ZonedDateTime`.
    #[must_use]
    pub fn epoch_nanoseconds(&self) -> &EpochNanoseconds {
        self.instant.epoch_nanoseconds()
    }

    /// Returns the current `ZonedDateTime` as an [`Instant`].
    #[must_use]
    pub fn to_instant(&self) -> Instant {
        self.instant
    }

    pub fn with(
        &self,
        partial: PartialZonedDateTime,
        disambiguation: Option<Disambiguation>,
        offset_option: Option<OffsetDisambiguation>,
        overflow: Option<ArithmeticOverflow>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let overflow = overflow.unwrap_or_default();
        let disambiguation = disambiguation.unwrap_or_default();
        let offset_option = offset_option.unwrap_or(OffsetDisambiguation::Reject);

        let iso_date_time = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let plain_date_time = PlainDateTime::new_unchecked(iso_date_time, self.calendar.clone());

        // 23. Let dateTimeResult be ? InterpretTemporalDateTimeFields(calendar, fields, overflow).
        let result_date = self.calendar.date_from_partial(
            &partial.date.with_fallback_datetime(&plain_date_time)?,
            overflow,
        )?;

        let time = iso_date_time.time.with(partial.time, overflow)?;

        // 24. Let newOffsetNanoseconds be ! ParseDateTimeUTCOffset(fields.[[OffsetString]]).
        let original_offset = self.offset_nanoseconds_with_provider(provider)?;
        let new_offset_nanos = partial
            .offset
            .map(|offset| i64::from(offset.0) * 60_000_000_000)
            .or(Some(original_offset));

        // 25. Let epochNanoseconds be ? InterpretISODateTimeOffset(dateTimeResult.[[ISODate]], dateTimeResult.[[Time]], option, newOffsetNanoseconds, timeZone, disambiguation, offset, match-exactly).
        let epoch_nanos = interpret_isodatetime_offset(
            result_date.iso,
            Some(time),
            false,
            new_offset_nanos,
            &self.tz,
            disambiguation,
            offset_option,
            true,
            provider,
        )?;

        // 26. Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
        Ok(Self::new_unchecked(
            Instant::from(epoch_nanos),
            self.calendar.clone(),
            self.tz.clone(),
        ))
    }

    /// Creates a new `ZonedDateTime` from the current `ZonedDateTime`
    /// combined with the provided `TimeZone`.
    pub fn with_timezone(&self, timezone: TimeZone) -> TemporalResult<Self> {
        Self::try_new(
            self.epoch_nanoseconds().as_i128(),
            self.calendar.clone(),
            timezone,
        )
    }

    /// Creates a new `ZonedDateTime` from the current `ZonedDateTime`
    /// combined with the provided `Calendar`.
    pub fn with_calendar(&self, calendar: Calendar) -> TemporalResult<Self> {
        Self::try_new(
            self.epoch_nanoseconds().as_i128(),
            calendar,
            self.tz.clone(),
        )
    }

    /// Compares one `ZonedDateTime` to another `ZonedDateTime` using their
    /// `Instant` representation.
    ///
    /// # Note on Ordering.
    ///
    /// `temporal_rs` does not implement `PartialOrd`/`Ord` as `ZonedDateTime` does
    /// not fulfill all the conditions required to implement the traits. However,
    /// it is possible to compare `PlainDate`'s as their `IsoDate` representation.
    #[inline]
    #[must_use]
    pub fn compare_instant(&self, other: &Self) -> Ordering {
        self.instant.cmp(&other.instant)
    }
}

// ==== HoursInDay accessor method implementation ====

impl ZonedDateTime {
    // TODO: implement and stabalize
    pub fn get_time_zone_transition_with_provider(
        &self,
        direction: TransitionDirection,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Option<Self>> {
        // 8. If IsOffsetTimeZoneIdentifier(timeZone) is true, return null.
        let TimeZone::IanaIdentifier(identifier) = &self.tz else {
            return Ok(None);
        };
        // 9. If direction is next, then
        // a. Let transition be GetNamedTimeZoneNextTransition(timeZone, zonedDateTime.[[EpochNanoseconds]]).
        // 10. Else,
        // a. Assert: direction is previous.
        // b. Let transition be GetNamedTimeZonePreviousTransition(timeZone, zonedDateTime.[[EpochNanoseconds]]).
        let transition = provider.get_named_tz_transition(
            identifier,
            self.epoch_nanoseconds().as_i128(),
            direction,
        )?;

        // 11. If transition is null, return null.
        // 12. Return ! CreateTemporalZonedDateTime(transition, timeZone, zonedDateTime.[[Calendar]]).
        let result = transition
            .map(|t| ZonedDateTime::try_new(t.0, self.calendar().clone(), self.tz.clone()))
            .transpose()?;

        Ok(result)
    }

    pub fn hours_in_day_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<u8> {
        // 1-3. Is engine specific steps
        // 4. Let isoDateTime be GetISODateTimeFor(timeZone, zonedDateTime.[[EpochNanoseconds]]).
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        // 5. Let today be isoDateTime.[[ISODate]].
        let today = iso.date;
        // 6. Let tomorrow be BalanceISODate(today.[[Year]], today.[[Month]], today.[[Day]] + 1).
        let tomorrow = IsoDate::balance(today.year, today.month.into(), i32::from(today.day + 1));
        // 7. Let todayNs be ? GetStartOfDay(timeZone, today).
        let today_ns = self.tz.get_start_of_day(&today, provider)?;
        // 8. Let tomorrowNs be ? GetStartOfDay(timeZone, tomorrow).
        let tomorrow_ns = self.tz.get_start_of_day(&tomorrow, provider)?;
        // 9. Let diff be TimeDurationFromEpochNanosecondsDifference(tomorrowNs, todayNs).
        let diff = NormalizedTimeDuration::from_nanosecond_difference(tomorrow_ns.0, today_ns.0)?;
        // NOTE: The below should be safe as today_ns and tomorrow_ns should be at most 25 hours.
        // TODO: Tests for the below cast.
        // 10. Return 𝔽(TotalTimeDuration(diff, hour)).
        Ok(diff.divide(60_000_000_000) as u8)
    }
}

// ==== Core accessor methods ====

impl ZonedDateTime {
    /// Returns the `year` value for this `ZonedDateTime`.
    #[inline]
    pub fn year_with_provider(&self, provider: &impl TimeZoneProvider) -> TemporalResult<i32> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let dt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        Ok(self.calendar.year(&dt.iso.date))
    }

    /// Returns the `month` value for this `ZonedDateTime`.
    pub fn month_with_provider(&self, provider: &impl TimeZoneProvider) -> TemporalResult<u8> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let dt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        Ok(self.calendar.month(&dt.iso.date))
    }

    /// Returns the `monthCode` value for this `ZonedDateTime`.
    pub fn month_code_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<MonthCode> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let dt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        Ok(self.calendar.month_code(&dt.iso.date))
    }

    /// Returns the `day` value for this `ZonedDateTime`.
    pub fn day_with_provider(&self, provider: &impl TimeZoneProvider) -> TemporalResult<u8> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let dt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        Ok(self.calendar.day(&dt.iso.date))
    }

    /// Returns the `hour` value for this `ZonedDateTime`.
    pub fn hour_with_provider(&self, provider: &impl TimeZoneProvider) -> TemporalResult<u8> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        Ok(iso.time.hour)
    }

    /// Returns the `minute` value for this `ZonedDateTime`.
    pub fn minute_with_provider(&self, provider: &impl TimeZoneProvider) -> TemporalResult<u8> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        Ok(iso.time.minute)
    }

    /// Returns the `second` value for this `ZonedDateTime`.
    pub fn second_with_provider(&self, provider: &impl TimeZoneProvider) -> TemporalResult<u8> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        Ok(iso.time.second)
    }

    /// Returns the `millisecond` value for this `ZonedDateTime`.
    pub fn millisecond_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<u16> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        Ok(iso.time.millisecond)
    }

    /// Returns the `microsecond` value for this `ZonedDateTime`.
    pub fn microsecond_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<u16> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        Ok(iso.time.microsecond)
    }

    /// Returns the `nanosecond` value for this `ZonedDateTime`.
    pub fn nanosecond_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<u16> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        Ok(iso.time.nanosecond)
    }

    /// Returns an offset string for the current `ZonedDateTime`.
    pub fn offset_with_provider(&self, provider: &impl TimeZoneProvider) -> TemporalResult<String> {
        let offset = self
            .tz
            .get_offset_nanos_for(self.epoch_nanoseconds().as_i128(), provider)?;
        Ok(nanoseconds_to_formattable_offset(offset).to_string())
    }

    /// Returns the offset nanoseconds for the current `ZonedDateTime`.
    pub fn offset_nanoseconds_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<i64> {
        let offset = self
            .tz
            .get_offset_nanos_for(self.epoch_nanoseconds().as_i128(), provider)?;
        Ok(offset as i64)
    }
}

pub(crate) fn nanoseconds_to_formattable_offset(nanoseconds: i128) -> FormattableOffset {
    let sign = if nanoseconds >= 0 {
        Sign::Positive
    } else {
        Sign::Negative
    };
    let nanos = nanoseconds.unsigned_abs();
    let hour = (nanos / 3_600_000_000_000) as u8;
    let minute = ((nanos / 60_000_000_000) % 60) as u8;
    let second = ((nanos / 1_000_000_000) % 60) as u8;
    let nanosecond = (nanos % 1_000_000_000) as u32;

    let precision = if second == 0 && nanosecond == 0 {
        Precision::Minute
    } else {
        Precision::Auto
    };

    FormattableOffset {
        sign,
        time: FormattableTime {
            hour,
            minute,
            second,
            nanosecond,
            precision,
            include_sep: true,
        },
    }
}

// ==== Core calendar method implementations ====

impl ZonedDateTime {
    /// Returns the era for the current `ZonedDateTime`.
    pub fn era_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Option<TinyAsciiStr<16>>> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        Ok(self.calendar.era(&pdt.iso.date))
    }

    /// Returns the era-specific year for the current `ZonedDateTime`.
    pub fn era_year_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Option<i32>> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        Ok(self.calendar.era_year(&pdt.iso.date))
    }

    /// Returns the calendar day of week value.
    pub fn day_of_week_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<u16> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.day_of_week(&pdt.iso.date)
    }

    /// Returns the calendar day of year value.
    pub fn day_of_year_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<u16> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        Ok(self.calendar.day_of_year(&pdt.iso.date))
    }

    /// Returns the calendar week of year value.
    pub fn week_of_year_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Option<u8>> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        Ok(self.calendar.week_of_year(&pdt.iso.date))
    }

    /// Returns the calendar year of week value.
    pub fn year_of_week_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Option<i32>> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        Ok(self.calendar.year_of_week(&pdt.iso.date))
    }

    /// Returns the calendar days in week value.
    pub fn days_in_week_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<u16> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.days_in_week(&pdt.iso.date)
    }

    /// Returns the calendar days in month value.
    pub fn days_in_month_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<u16> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        Ok(self.calendar.days_in_month(&pdt.iso.date))
    }

    /// Returns the calendar days in year value.
    pub fn days_in_year_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<u16> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        Ok(self.calendar.days_in_year(&pdt.iso.date))
    }

    /// Returns the calendar months in year value.
    pub fn months_in_year_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<u16> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        Ok(self.calendar.months_in_year(&pdt.iso.date))
    }

    /// Returns returns whether the date in a leap year for the given calendar.
    pub fn in_leap_year_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<bool> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        Ok(self.calendar.in_leap_year(&pdt.iso.date))
    }
}

// ==== Core method implementations ====

impl ZonedDateTime {
    /// Creates a new `ZonedDateTime` from the current `ZonedDateTime`
    /// combined with the provided `TimeZone`.
    pub fn with_plain_time_and_provider(
        &self,
        time: PlainTime,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let result_iso = IsoDateTime::new_unchecked(iso.date, time.iso);
        let epoch_ns =
            self.tz
                .get_epoch_nanoseconds_for(result_iso, Disambiguation::Compatible, provider)?;
        Self::try_new(epoch_ns.0, self.calendar.clone(), self.tz.clone())
    }

    /// Add a duration to the current `ZonedDateTime`
    pub fn add_with_provider(
        &self,
        duration: &Duration,
        overflow: Option<ArithmeticOverflow>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        self.add_internal(
            duration,
            overflow.unwrap_or(ArithmeticOverflow::Constrain),
            provider,
        )
    }

    /// Subtract a duration to the current `ZonedDateTime`
    pub fn subtract_with_provider(
        &self,
        duration: &Duration,
        overflow: Option<ArithmeticOverflow>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        self.add_internal(
            &duration.negated(),
            overflow.unwrap_or(ArithmeticOverflow::Constrain),
            provider,
        )
    }

    /// Returns a [`Duration`] representing the period of time from this `ZonedDateTime` since the other `ZonedDateTime`.
    pub fn since_with_provider(
        &self,
        other: &Self,
        options: DifferenceSettings,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Duration> {
        self.diff_internal_with_provider(DifferenceOperation::Since, other, options, provider)
    }

    /// Returns a [`Duration`] representing the period of time from this `ZonedDateTime` since the other `ZonedDateTime`.
    pub fn until_with_provider(
        &self,
        other: &Self,
        options: DifferenceSettings,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Duration> {
        self.diff_internal_with_provider(DifferenceOperation::Until, other, options, provider)
    }

    /// Return a `ZonedDateTime` representing the start of the day
    /// for the current `ZonedDateTime`.
    pub fn start_of_day_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let epoch_nanos = self.tz.get_start_of_day(&iso.date, provider)?;
        Self::try_new(epoch_nanos.0, self.calendar.clone(), self.tz.clone())
    }

    /// Convert the current `ZonedDateTime` to a [`PlainDate`] with
    /// a user defined time zone provider.
    pub fn to_plain_date_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<PlainDate> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        Ok(PlainDate::new_unchecked(iso.date, self.calendar.clone()))
    }

    /// Convert the current `ZonedDateTime` to a [`PlainTime`] with
    /// a user defined time zone provider.
    pub fn to_plain_time_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<PlainTime> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        Ok(PlainTime::new_unchecked(iso.time))
    }

    /// Convert the current `ZonedDateTime` to a [`PlainDateTime`] with
    /// a user defined time zone provider.
    pub fn to_plain_datetime_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<PlainDateTime> {
        let iso = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        Ok(PlainDateTime::new_unchecked(iso, self.calendar.clone()))
    }

    /// Creates a default formatted IXDTF (RFC 9557) date/time string for the provided `ZonedDateTime`.
    pub fn to_string_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<String> {
        self.to_ixdtf_string_with_provider(
            DisplayOffset::Auto,
            DisplayTimeZone::Auto,
            DisplayCalendar::Auto,
            ToStringRoundingOptions::default(),
            provider,
        )
    }

    /// 6.3.39 Temporal.ZonedDateTime.prototype.round
    pub fn round_with_provider(
        &self,
        options: RoundingOptions,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        // 1. Let zonedDateTime be the this value.
        // 2. Perform ? RequireInternalSlot(zonedDateTime, [[InitializedTemporalZonedDateTime]]).
        // 3. If roundTo is undefined, then
        // a. Throw a TypeError exception.
        // 4. If roundTo is a String, then
        // a. Let paramString be roundTo.
        // b. Set roundTo to OrdinaryObjectCreate(null).
        // c. Perform ! CreateDataPropertyOrThrow(roundTo, "smallestUnit", paramString).
        // 5. Else,
        // a. Set roundTo to ? GetOptionsObject(roundTo).
        // 6. NOTE: The following steps read options and perform independent validation in alphabetical order (GetRoundingIncrementOption reads "roundingIncrement" and GetRoundingModeOption reads "roundingMode").
        // 7. Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).
        // 8. Let roundingMode be ? GetRoundingModeOption(roundTo, half-expand).
        // 9. Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo, "smallestUnit", time, required, « day »).
        // 10. If smallestUnit is day, then
        // a. Let maximum be 1.
        // b. Let inclusive be true.
        // 11. Else,
        // a. Let maximum be MaximumTemporalDurationRoundingIncrement(smallestUnit).
        // b. Assert: maximum is not unset.
        // c. Let inclusive be false.
        let resolved = ResolvedRoundingOptions::from_datetime_options(options)?;
        // 12. Perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, inclusive).
        // 13. If maximum is not unset, perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, false).
        // 13. If smallestUnit is nanosecond and roundingIncrement = 1, then
        if resolved.smallest_unit == Unit::Nanosecond
            && resolved.increment == RoundingIncrement::ONE
        {
            // a. Return ! CreateTemporalZonedDateTime(zonedDateTime.[[EpochNanoseconds]], zonedDateTime.[[TimeZone]], zonedDateTime.[[Calendar]]).
            return Ok(self.clone());
        }
        // 14. Let thisNs be zonedDateTime.[[EpochNanoseconds]].
        let this_ns = self.epoch_nanoseconds();
        // 15. Let timeZone be zonedDateTime.[[TimeZone]].
        // 16. Let calendar be zonedDateTime.[[Calendar]].
        // 17. Let isoDateTime be GetISODateTimeFor(timeZone, thisNs).
        // 18. If smallestUnit is day, then
        if resolved.smallest_unit == Unit::Day {
            // a. Let dateStart be isoDateTime.[[ISODate]].
            let iso_start = self.tz.get_iso_datetime_for(&self.instant, provider)?;
            // b. Let dateEnd be BalanceISODate(dateStart.[[Year]], dateStart.[[Month]], dateStart.[[Day]] + 1).
            let iso_end = IsoDate::balance(
                iso_start.date.year,
                iso_start.date.month.into(),
                i32::from(iso_start.date.day + 1),
            );
            // c. Let startNs be ? GetStartOfDay(timeZone, dateStart).
            // d. Assert: thisNs ≥ startNs.
            // e. Let endNs be ? GetStartOfDay(timeZone, dateEnd).
            // f. Assert: thisNs < endNs.
            let start_ns = self.tz.get_start_of_day(&iso_start.date, provider)?;
            let end_ns = self.tz.get_start_of_day(&iso_end, provider)?;
            if !(this_ns.0 >= start_ns.0 && this_ns.0 < end_ns.0) {
                return Err(TemporalError::range()
                    .with_message("ZonedDateTime is outside the expected day bounds"));
            }
            // g. Let dayLengthNs be ℝ(endNs - startNs).
            // h. Let dayProgressNs be TimeDurationFromEpochNanosecondsDifference(thisNs, startNs).
            let day_len_ns =
                NormalizedTimeDuration::from_nanosecond_difference(end_ns.0, start_ns.0)?;
            let day_progress_ns =
                NormalizedTimeDuration::from_nanosecond_difference(this_ns.0, start_ns.0)?;
            // i. Let roundedDayNs be ! RoundTimeDurationToIncrement(dayProgressNs, dayLengthNs, roundingMode).
            let rounded = if let Some(increment) = NonZeroU128::new(day_len_ns.0.unsigned_abs()) {
                IncrementRounder::<i128>::from_signed_num(day_progress_ns.0, increment)?
                    .round(resolved.rounding_mode)
            } else {
                0 // Zero-length day: round to start of day
            };

            // j. Let epochNanoseconds be AddTimeDurationToEpochNanoseconds(roundedDayNs, startNs).
            let candidate = start_ns.0 + rounded;
            Instant::try_new(candidate)?;
            // 20. Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
            ZonedDateTime::try_new(candidate, self.calendar.clone(), self.tz.clone())
        } else {
            // 19. Else,
            // a. Let roundResult be RoundISODateTime(isoDateTime, roundingIncrement, smallestUnit, roundingMode).
            // b. Let offsetNanoseconds be GetOffsetNanosecondsFor(timeZone, thisNs).
            // c. Let epochNanoseconds be ? InterpretISODateTimeOffset(roundResult.[[ISODate]], roundResult.[[Time]], option, offsetNanoseconds, timeZone, compatible, prefer, match-exactly).
            // 20. Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
            let iso_dt = self.tz.get_iso_datetime_for(&self.instant, provider)?;
            let rounded_dt = iso_dt.round(resolved)?;
            let offset_ns = self.tz.get_offset_nanos_for(this_ns.as_i128(), provider)?;

            let epoch_ns = interpret_isodatetime_offset(
                rounded_dt.date,
                Some(rounded_dt.time),
                false,
                Some(offset_ns as i64),
                &self.tz,
                Disambiguation::Compatible,
                OffsetDisambiguation::Prefer,
                true,
                provider,
            )?;

            ZonedDateTime::try_new(epoch_ns.0, self.calendar.clone(), self.tz.clone())
        }
    }

    /// Creates an IXDTF (RFC 9557) date/time string for the provided `ZonedDateTime` according
    /// to the provided display options.
    pub fn to_ixdtf_string_with_provider(
        &self,
        display_offset: DisplayOffset,
        display_timezone: DisplayTimeZone,
        display_calendar: DisplayCalendar,
        options: ToStringRoundingOptions,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<String> {
        let resolved_options = options.resolve()?;
        let result =
            self.instant
                .round_instant(ResolvedRoundingOptions::from_to_string_options(
                    &resolved_options,
                ))?;

        let offset = self.tz.get_offset_nanos_for(result, provider)?;
        let datetime = self.tz.get_iso_datetime_for(&self.instant, provider)?;
        let (sign, hour, minute) = nanoseconds_to_formattable_offset_minutes(offset)?;
        let timezone_id = self.timezone().identifier()?;

        let ixdtf_string = IxdtfStringBuilder::default()
            .with_date(datetime.date)
            .with_time(datetime.time, resolved_options.precision)
            .with_minute_offset(sign, hour, minute, display_offset)
            .with_timezone(&timezone_id, display_timezone)
            .with_calendar(self.calendar.identifier(), display_calendar)
            .build();

        Ok(ixdtf_string)
    }

    // TODO: Should IANA Identifier be prechecked or allow potentially invalid IANA Identifer values here?
    pub fn from_str_with_provider(
        source: &str,
        disambiguation: Disambiguation,
        offset_option: OffsetDisambiguation,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let parse_result = parsers::parse_zoned_date_time(source)?;

        // NOTE (nekevss): `parse_zoned_date_time` guarantees that this value exists.
        let annotation = parse_result.tz.temporal_unwrap()?;

        let timezone = TimeZone::from_time_zone_record(annotation.tz)?;

        let (offset_nanos, is_exact) = parse_result
            .offset
            .map(|record| {
                let UtcOffsetRecordOrZ::Offset(offset) = record else {
                    return (None, true);
                };
                let hours_in_ns = i64::from(offset.hour()) * 3_600_000_000_000_i64;
                let minutes_in_ns = i64::from(offset.minute()) * 60_000_000_000_i64;
                let seconds_in_ns = i64::from(offset.second().unwrap_or(0)) * 1_000_000_000_i64;
                let ns = offset
                    .fraction()
                    .and_then(|x| x.to_nanoseconds())
                    .unwrap_or(0);

                (
                    Some(
                        (hours_in_ns + minutes_in_ns + seconds_in_ns + i64::from(ns))
                            * i64::from(offset.sign() as i8),
                    ),
                    false,
                )
            })
            .unwrap_or((None, false));

        let calendar = parse_result
            .calendar
            .map(Calendar::try_from_utf8)
            .transpose()?
            .unwrap_or_default();

        let time = parse_result
            .time
            .map(IsoTime::from_time_record)
            .transpose()?;

        let Some(parsed_date) = parse_result.date else {
            return Err(
                TemporalError::range().with_message("No valid DateRecord Parse Node was found.")
            );
        };

        let date = IsoDate::new_with_overflow(
            parsed_date.year,
            parsed_date.month,
            parsed_date.day,
            ArithmeticOverflow::Reject,
        )?;

        let epoch_nanos = interpret_isodatetime_offset(
            date,
            time,
            is_exact,
            offset_nanos,
            &timezone,
            disambiguation,
            offset_option,
            true,
            provider,
        )?;

        Ok(Self::new_unchecked(
            Instant::from(epoch_nanos),
            calendar,
            timezone,
        ))
    }
}

/// InterpretISODateTimeOffset
#[allow(clippy::too_many_arguments)]
pub(crate) fn interpret_isodatetime_offset(
    date: IsoDate,
    time: Option<IsoTime>,
    is_exact: bool,
    offset_nanos: Option<i64>,
    timezone: &TimeZone,
    disambiguation: Disambiguation,
    offset_option: OffsetDisambiguation,
    match_minutes: bool,
    provider: &impl TimeZoneProvider,
) -> TemporalResult<EpochNanoseconds> {
    // 1.  If time is start-of-day, then
    let Some(time) = time else {
        // a. Assert: offsetBehaviour is wall.
        // b. Assert: offsetNanoseconds is 0.
        temporal_assert!(offset_nanos.is_none());
        // c. Return ? GetStartOfDay(timeZone, isoDate).
        return timezone.get_start_of_day(&date, provider);
    };

    // 2. Let isoDateTime be CombineISODateAndTimeRecord(isoDate, time).
    // TODO: Deal with offsetBehavior == wall.
    match (is_exact, offset_nanos) {
        // 4. If offsetBehaviour is exact, or offsetBehaviour is option and offsetOption is use, then
        (true, Some(offset)) if offset_option == OffsetDisambiguation::Use => {
            // a. Let balanced be BalanceISODateTime(isoDate.[[Year]], isoDate.[[Month]],
            // isoDate.[[Day]], time.[[Hour]], time.[[Minute]], time.[[Second]], time.[[Millisecond]],
            // time.[[Microsecond]], time.[[Nanosecond]] - offsetNanoseconds).
            let iso = IsoDateTime::balance(
                date.year,
                date.month.into(),
                date.day.into(),
                time.hour.into(),
                time.minute.into(),
                time.second.into(),
                time.millisecond.into(),
                time.microsecond.into(),
                (i64::from(time.nanosecond) - offset).into(),
            );

            // b. Perform ? CheckISODaysRange(balanced.[[ISODate]]).
            iso.date.is_valid_day_range()?;

            // c. Let epochNanoseconds be GetUTCEpochNanoseconds(balanced).
            // d. If IsValidEpochNanoseconds(epochNanoseconds) is false, throw a RangeError exception.
            // e. Return epochNanoseconds.
            iso.as_nanoseconds()
        }
        // 5. Assert: offsetBehaviour is option.
        // 6. Assert: offsetOption is prefer or reject.
        (_, Some(offset))
            if offset_option == OffsetDisambiguation::Prefer
                || offset_option == OffsetDisambiguation::Reject =>
        {
            // 7. Perform ? CheckISODaysRange(isoDate).
            date.is_valid_day_range()?;
            let iso = IsoDateTime::new_unchecked(date, time);
            // 8. Let utcEpochNanoseconds be GetUTCEpochNanoseconds(isoDateTime).
            let utc_epochs = iso.as_nanoseconds()?;
            // 9. Let possibleEpochNs be ? GetPossibleEpochNanoseconds(timeZone, isoDateTime).
            let possible_nanos = timezone.get_possible_epoch_ns_for(iso, provider)?;
            // 10. For each element candidate of possibleEpochNs, do
            for candidate in &possible_nanos {
                // a. Let candidateOffset be utcEpochNanoseconds - candidate.
                let candidate_offset = utc_epochs.0 - candidate.0;
                // b. If candidateOffset = offsetNanoseconds, then
                if candidate_offset == offset.into() {
                    // i. Return candidate.
                    return Ok(*candidate);
                }
                // c. If matchBehaviour is match-minutes, then
                if match_minutes {
                    // i. Let roundedCandidateNanoseconds be RoundNumberToIncrement(candidateOffset, 60 × 10**9, half-expand).
                    let rounded_candidate = IncrementRounder::from_signed_num(
                        candidate_offset,
                        NonZeroU128::new(60_000_000_000).expect("cannot be zero"), // TODO: Add back const { } after MSRV can be bumped
                    )?
                    .round(RoundingMode::HalfExpand);
                    // ii. If roundedCandidateNanoseconds = offsetNanoseconds, then
                    if rounded_candidate == offset.into() {
                        // 1. Return candidate.
                        return Ok(*candidate);
                    }
                }
            }

            // 11. If offsetOption is reject, throw a RangeError exception.
            if offset_option == OffsetDisambiguation::Reject {
                return Err(TemporalError::range()
                    .with_message("Offsets could not be determined without disambiguation"));
            }
            // 12. Return ? DisambiguatePossibleEpochNanoseconds(possibleEpochNs, timeZone, isoDateTime, disambiguation).
            timezone.disambiguate_possible_epoch_nanos(
                possible_nanos,
                iso,
                disambiguation,
                provider,
            )
        }
        // NOTE: This is inverted as the logic works better for matching against
        // 3. If offsetBehaviour is wall, or offsetBehaviour is option and offsetOption is ignore, then
        _ => {
            // a. Return ? GetEpochNanosecondsFor(timeZone, isoDateTime, disambiguation).
            let iso = IsoDateTime::new_unchecked(date, time);
            timezone.get_epoch_nanoseconds_for(iso, disambiguation, provider)
        }
    }
}

// Formatting utils
const NS_PER_MINUTE: i128 = 60_000_000_000;

pub(crate) fn nanoseconds_to_formattable_offset_minutes(
    nanoseconds: i128,
) -> TemporalResult<(Sign, u8, u8)> {
    // Per 11.1.7 this should be rounding
    let nanoseconds = IncrementRounder::from_signed_num(nanoseconds, unsafe {
        NonZeroU128::new_unchecked(NS_PER_MINUTE as u128)
    })?
    .round(RoundingMode::HalfExpand);
    let offset_minutes = (nanoseconds / NS_PER_MINUTE) as i32;
    let sign = if offset_minutes < 0 {
        Sign::Negative
    } else {
        Sign::Positive
    };
    let hour = offset_minutes.abs() / 60;
    let minute = offset_minutes.abs() % 60;
    Ok((sign, hour as u8, minute as u8))
}

#[cfg(all(test, feature = "tzdb"))]
mod tests {
    use super::ZonedDateTime;
    use crate::{
        options::{
            ArithmeticOverflow, DifferenceSettings, Disambiguation, OffsetDisambiguation,
            RoundingIncrement, RoundingMode, RoundingOptions, Unit,
        },
        partial::{PartialDate, PartialTime, PartialZonedDateTime},
        tzdb::FsTzdbProvider,
        unix_time::EpochNanoseconds,
        Calendar, MonthCode, TimeZone,
    };
    use core::str::FromStr;
    use tinystr::tinystr;

    #[test]
    fn basic_zdt_test() {
        let provider = &FsTzdbProvider::default();
        let nov_30_2023_utc = 1_701_308_952_000_000_000i128;

        let zdt = ZonedDateTime::try_new(
            nov_30_2023_utc,
            Calendar::from_str("iso8601").unwrap(),
            TimeZone::try_from_str("Z").unwrap(),
        )
        .unwrap();

        assert_eq!(zdt.year_with_provider(provider).unwrap(), 2023);
        assert_eq!(zdt.month_with_provider(provider).unwrap(), 11);
        assert_eq!(zdt.day_with_provider(provider).unwrap(), 30);
        assert_eq!(zdt.hour_with_provider(provider).unwrap(), 1);
        assert_eq!(zdt.minute_with_provider(provider).unwrap(), 49);
        assert_eq!(zdt.second_with_provider(provider).unwrap(), 12);

        let zdt_minus_five = ZonedDateTime::try_new(
            nov_30_2023_utc,
            Calendar::from_str("iso8601").unwrap(),
            TimeZone::try_from_str("America/New_York").unwrap(),
        )
        .unwrap();

        assert_eq!(zdt_minus_five.year_with_provider(provider).unwrap(), 2023);
        assert_eq!(zdt_minus_five.month_with_provider(provider).unwrap(), 11);
        assert_eq!(zdt_minus_five.day_with_provider(provider).unwrap(), 29);
        assert_eq!(zdt_minus_five.hour_with_provider(provider).unwrap(), 20);
        assert_eq!(zdt_minus_five.minute_with_provider(provider).unwrap(), 49);
        assert_eq!(zdt_minus_five.second_with_provider(provider).unwrap(), 12);

        let zdt_plus_eleven = ZonedDateTime::try_new(
            nov_30_2023_utc,
            Calendar::from_str("iso8601").unwrap(),
            TimeZone::try_from_str("Australia/Sydney").unwrap(),
        )
        .unwrap();

        assert_eq!(zdt_plus_eleven.year_with_provider(provider).unwrap(), 2023);
        assert_eq!(zdt_plus_eleven.month_with_provider(provider).unwrap(), 11);
        assert_eq!(zdt_plus_eleven.day_with_provider(provider).unwrap(), 30);
        assert_eq!(zdt_plus_eleven.hour_with_provider(provider).unwrap(), 12);
        assert_eq!(zdt_plus_eleven.minute_with_provider(provider).unwrap(), 49);
        assert_eq!(zdt_plus_eleven.second_with_provider(provider).unwrap(), 12);
    }

    #[test]
    // https://tc39.es/proposal-temporal/docs/zoneddatetime.html#round
    fn round_with_provider_test() {
        let provider = &FsTzdbProvider::default();
        let dt = "1995-12-07T03:24:30.000003500-08:00[America/Los_Angeles]";
        let zdt = ZonedDateTime::from_str_with_provider(
            dt,
            Disambiguation::default(),
            OffsetDisambiguation::Use,
            provider,
        )
        .unwrap();

        let result = zdt
            .round_with_provider(
                RoundingOptions {
                    smallest_unit: Some(Unit::Hour),
                    ..Default::default()
                },
                provider,
            )
            .unwrap();
        assert_eq!(
            result.to_string_with_provider(provider).unwrap(),
            "1995-12-07T03:00:00-08:00[America/Los_Angeles]"
        );

        let result = zdt
            .round_with_provider(
                RoundingOptions {
                    smallest_unit: Some(Unit::Minute),
                    increment: Some((RoundingIncrement::try_new(30)).unwrap()),
                    ..Default::default()
                },
                provider,
            )
            .unwrap();
        assert_eq!(
            result.to_string_with_provider(provider).unwrap(),
            "1995-12-07T03:30:00-08:00[America/Los_Angeles]"
        );

        let result = zdt
            .round_with_provider(
                RoundingOptions {
                    smallest_unit: Some(Unit::Minute),
                    increment: Some((RoundingIncrement::try_new(30)).unwrap()),
                    rounding_mode: Some(RoundingMode::Floor),
                    ..Default::default()
                },
                provider,
            )
            .unwrap();
        assert_eq!(
            result.to_string_with_provider(provider).unwrap(),
            "1995-12-07T03:00:00-08:00[America/Los_Angeles]"
        );
    }

    #[test]
    fn zdt_from_partial() {
        let provider = &FsTzdbProvider::default();
        let partial = PartialZonedDateTime {
            date: PartialDate {
                year: Some(1970),
                month_code: Some(MonthCode(tinystr!(4, "M01"))),
                day: Some(1),
                ..Default::default()
            },
            time: PartialTime::default(),
            offset: None,
            timezone: Some(TimeZone::default()),
        };

        let result = ZonedDateTime::from_partial_with_provider(partial, None, None, None, provider);
        assert!(result.is_ok());
    }

    #[test]
    fn zdt_from_str() {
        let provider = &FsTzdbProvider::default();

        let zdt_str = "1970-01-01T00:00[UTC][u-ca=iso8601]";
        let result = ZonedDateTime::from_str_with_provider(
            zdt_str,
            Disambiguation::Compatible,
            OffsetDisambiguation::Reject,
            provider,
        );
        assert!(result.is_ok());
    }

    #[test]
    // https://github.com/tc39/test262/blob/d9b10790bc4bb5b3e1aa895f11cbd2d31a5ec743/test/intl402/Temporal/ZonedDateTime/from/dst-skipped-cross-midnight.js
    fn dst_skipped_cross_midnight() {
        let provider = &FsTzdbProvider::default();
        let start_of_day = ZonedDateTime::from_str_with_provider(
            "1919-03-31[America/Toronto]",
            Disambiguation::Compatible,
            OffsetDisambiguation::Reject,
            provider,
        )
        .unwrap();
        let midnight_disambiguated = ZonedDateTime::from_str_with_provider(
            "1919-03-31T00[America/Toronto]",
            Disambiguation::Compatible,
            OffsetDisambiguation::Reject,
            provider,
        )
        .unwrap();

        assert_eq!(
            start_of_day.epoch_nanoseconds(),
            &EpochNanoseconds(-1601753400000000000)
        );
        assert_eq!(
            midnight_disambiguated.epoch_nanoseconds(),
            &EpochNanoseconds(-1601751600000000000)
        );
        let diff = start_of_day
            .instant
            .until(
                &midnight_disambiguated.instant,
                DifferenceSettings {
                    largest_unit: Some(Unit::Hour),
                    smallest_unit: Some(Unit::Nanosecond),
                    ..Default::default()
                },
            )
            .unwrap();
        assert_eq!(diff.years(), 0);
        assert_eq!(diff.months(), 0);
        assert_eq!(diff.weeks(), 0);
        assert_eq!(diff.days(), 0);
        assert_eq!(diff.hours(), 0);
        assert_eq!(diff.minutes(), 30);
        assert_eq!(diff.seconds(), 0);
        assert_eq!(diff.milliseconds(), 0);
        assert_eq!(diff.microseconds(), 0);
        assert_eq!(diff.nanoseconds(), 0);
    }

    // overflow-reject-throws.js
    #[test]
    fn overflow_reject_throws() {
        let provider = &FsTzdbProvider::default();

        let zdt =
            ZonedDateTime::try_new(217178610123456789, Calendar::default(), TimeZone::default())
                .unwrap();

        let overflow = ArithmeticOverflow::Reject;

        let result_1 = zdt.with(
            PartialZonedDateTime {
                date: PartialDate {
                    month: Some(29),
                    ..Default::default()
                },
                time: PartialTime::default(),
                offset: None,
                timezone: None,
            },
            None,
            None,
            Some(overflow),
            provider,
        );

        let result_2 = zdt.with(
            PartialZonedDateTime {
                date: PartialDate {
                    day: Some(31),
                    ..Default::default()
                },
                time: PartialTime::default(),
                offset: None,
                timezone: None,
            },
            None,
            None,
            Some(overflow),
            provider,
        );

        let result_3 = zdt.with(
            PartialZonedDateTime {
                date: PartialDate::default(),
                time: PartialTime {
                    hour: Some(29),
                    ..Default::default()
                },
                offset: None,
                timezone: None,
            },
            None,
            None,
            Some(overflow),
            provider,
        );

        let result_4 = zdt.with(
            PartialZonedDateTime {
                date: PartialDate::default(),
                time: PartialTime {
                    nanosecond: Some(9000),
                    ..Default::default()
                },
                offset: None,
                timezone: None,
            },
            None,
            None,
            Some(overflow),
            provider,
        );

        assert!(result_1.is_err());
        assert!(result_2.is_err());
        assert!(result_3.is_err());
        assert!(result_4.is_err());
    }
}
