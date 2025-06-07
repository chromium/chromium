//! This module implements the Temporal `TimeZone` and components.

use alloc::string::{String, ToString};
use alloc::{vec, vec::Vec};

use ixdtf::parsers::records::{MinutePrecisionOffset, TimeZoneRecord, UtcOffsetRecord};
use ixdtf::parsers::TimeZoneParser;
use num_traits::ToPrimitive;

use crate::builtins::core::duration::DateDuration;
use crate::parsers::{
    parse_allowed_timezone_formats, parse_identifier, FormattableOffset, FormattableTime, Precision,
};
use crate::provider::{TimeZoneOffset, TimeZoneProvider};
use crate::{
    builtins::core::{duration::normalized::NormalizedTimeDuration, Instant},
    iso::{IsoDate, IsoDateTime, IsoTime},
    options::Disambiguation,
    unix_time::EpochNanoseconds,
    TemporalError, TemporalResult, ZonedDateTime,
};
use crate::{Calendar, Sign};

const NS_IN_HOUR: i128 = 60 * 60 * 1000 * 1000 * 1000;

/// A UTC time zone offset stored in minutes
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct UtcOffset(pub(crate) i16);

impl UtcOffset {
    pub(crate) fn from_ixdtf_record(record: MinutePrecisionOffset) -> Self {
        // NOTE: ixdtf parser restricts minute/second to 0..=60
        let minutes = i16::from(record.hour) * 60 + record.minute as i16;
        Self(minutes * i16::from(record.sign as i8))
    }

    pub fn from_utf8(source: &[u8]) -> TemporalResult<Self> {
        let record = TimeZoneParser::from_utf8(source)
            .parse_offset()
            .map_err(|e| TemporalError::range().with_message(e.to_string()))?;
        match record {
            UtcOffsetRecord::MinutePrecision(offset) => Ok(Self::from_ixdtf_record(offset)),
            _ => {
                Err(TemporalError::range().with_message("offset must be a minute precision offset"))
            }
        }
    }

    pub fn to_string(&self) -> TemporalResult<String> {
        let sign = if self.0 < 0 {
            Sign::Negative
        } else {
            Sign::Positive
        };
        let hour = (self.0.abs() / 60) as u8;
        let minute = (self.0.abs() % 60) as u8;
        let formattable_offset = FormattableOffset {
            sign,
            time: FormattableTime {
                hour,
                minute,
                second: 0,
                nanosecond: 0,
                precision: Precision::Minute,
                include_sep: true,
            },
        };
        Ok(formattable_offset.to_string())
    }
}

impl core::str::FromStr for UtcOffset {
    type Err = TemporalError;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::from_utf8(s.as_bytes())
    }
}

// TODO: Potentially migrate to Cow<'a, str>
// TODO: There may be an argument to have Offset minutes be a (Cow<'a, str>,, i16) to
// prevent allocations / writing, TBD
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum TimeZone {
    IanaIdentifier(String),
    UtcOffset(UtcOffset),
}

impl TimeZone {
    // Create a `TimeZone` from an ixdtf `TimeZoneRecord`.
    #[inline]
    pub(crate) fn from_time_zone_record(record: TimeZoneRecord) -> TemporalResult<Self> {
        let timezone = match record {
            TimeZoneRecord::Name(s) => {
                TimeZone::IanaIdentifier(String::from_utf8_lossy(s).into_owned())
            }
            TimeZoneRecord::Offset(offset_record) => {
                let offset = UtcOffset::from_ixdtf_record(offset_record);
                TimeZone::UtcOffset(offset)
            }
            // TimeZoneRecord is non_exhaustive, but all current branches are matching.
            _ => return Err(TemporalError::assert()),
        };

        Ok(timezone)
    }

    /// Parses a `TimeZone` from a provided `&str`.
    pub fn try_from_identifier_str(identifier: &str) -> TemporalResult<Self> {
        if identifier == "Z" {
            return Ok(TimeZone::UtcOffset(UtcOffset(0)));
        }
        parse_identifier(identifier)
    }

    pub fn try_from_str(src: &str) -> TemporalResult<Self> {
        if let Ok(timezone) = Self::try_from_identifier_str(src) {
            return Ok(timezone);
        }

        parse_allowed_timezone_formats(src)
            .ok_or_else(|| TemporalError::range().with_message("Not a valid time zone string"))
    }

    /// Returns the current `TimeZoneSlot`'s identifier.
    pub fn identifier(&self) -> TemporalResult<String> {
        match self {
            TimeZone::IanaIdentifier(s) => Ok(s.clone()),
            TimeZone::UtcOffset(offset) => offset.to_string(),
        }
    }
}

impl Default for TimeZone {
    fn default() -> Self {
        Self::IanaIdentifier("UTC".into())
    }
}

impl From<&ZonedDateTime> for TimeZone {
    fn from(value: &ZonedDateTime) -> Self {
        value.timezone().clone()
    }
}

impl TimeZone {
    pub(crate) fn get_iso_datetime_for(
        &self,
        instant: &Instant,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<IsoDateTime> {
        let nanos = self.get_offset_nanos_for(instant.as_i128(), provider)?;
        IsoDateTime::from_epoch_nanos(instant.epoch_nanoseconds(), nanos.to_i64().unwrap_or(0))
    }

    /// Get the offset for this current `TimeZoneSlot`.
    pub(crate) fn get_offset_nanos_for(
        &self,
        utc_epoch: i128,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<i128> {
        // 1. Let parseResult be ! ParseTimeZoneIdentifier(timeZone).
        match self {
            // 2. If parseResult.[[OffsetMinutes]] is not empty, return parseResult.[[OffsetMinutes]] × (60 × 10**9).
            Self::UtcOffset(offset) => Ok(i128::from(offset.0) * 60_000_000_000i128),
            // 3. Return GetNamedTimeZoneOffsetNanoseconds(parseResult.[[Name]], epochNs).
            Self::IanaIdentifier(identifier) => provider
                .get_named_tz_offset_nanoseconds(identifier, utc_epoch)
                .map(|offset| i128::from(offset.offset) * 1_000_000_000),
        }
    }

    pub(crate) fn get_epoch_nanoseconds_for(
        &self,
        iso: IsoDateTime,
        disambiguation: Disambiguation,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<EpochNanoseconds> {
        // 1. Let possibleEpochNs be ? GetPossibleEpochNanoseconds(timeZone, isoDateTime).
        let possible_nanos = self.get_possible_epoch_ns_for(iso, provider)?;
        // 2. Return ? DisambiguatePossibleEpochNanoseconds(possibleEpochNs, timeZone, isoDateTime, disambiguation).
        self.disambiguate_possible_epoch_nanos(possible_nanos, iso, disambiguation, provider)
    }

    /// Get the possible `Instant`s for this `TimeZoneSlot`.
    pub(crate) fn get_possible_epoch_ns_for(
        &self,
        iso: IsoDateTime,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Vec<EpochNanoseconds>> {
        // 1.Let parseResult be ! ParseTimeZoneIdentifier(timeZone).
        let possible_nanoseconds = match self {
            // 2. If parseResult.[[OffsetMinutes]] is not empty, then
            Self::UtcOffset(UtcOffset(minutes)) => {
                // a. Let balanced be
                // BalanceISODateTime(isoDateTime.[[ISODate]].[[Year]],
                // isoDateTime.[[ISODate]].[[Month]],
                // isoDateTime.[[ISODate]].[[Day]],
                // isoDateTime.[[Time]].[[Hour]],
                // isoDateTime.[[Time]].[[Minute]] -
                // parseResult.[[OffsetMinutes]],
                // isoDateTime.[[Time]].[[Second]],
                // isoDateTime.[[Time]].[[Millisecond]],
                // isoDateTime.[[Time]].[[Microsecond]],
                // isoDateTime.[[Time]].[[Nanosecond]]).
                let balanced = IsoDateTime::balance(
                    iso.date.year,
                    iso.date.month.into(),
                    iso.date.day.into(),
                    iso.time.hour.into(),
                    (i16::from(iso.time.minute) - minutes).into(),
                    iso.time.second.into(),
                    iso.time.millisecond.into(),
                    iso.time.microsecond.into(),
                    iso.time.nanosecond.into(),
                );
                // b. Perform ? CheckISODaysRange(balanced.[[ISODate]]).
                balanced.date.is_valid_day_range()?;
                // c. Let epochNanoseconds be GetUTCEpochNanoseconds(balanced).
                let epoch_ns = balanced.as_nanoseconds()?;
                // d. Let possibleEpochNanoseconds be « epochNanoseconds ».
                vec![epoch_ns]
            }
            // 3. Else,
            Self::IanaIdentifier(identifier) => {
                // a. Perform ? CheckISODaysRange(isoDateTime.[[ISODate]]).
                iso.date.is_valid_day_range()?;
                // b. Let possibleEpochNanoseconds be
                // GetNamedTimeZoneEpochNanoseconds(parseResult.[[Name]],
                // isoDateTime).
                provider.get_named_tz_epoch_nanoseconds(identifier, iso)?
            }
        };
        // 4. For each value epochNanoseconds in possibleEpochNanoseconds, do
        // a . If IsValidEpochNanoseconds(epochNanoseconds) is false, throw a RangeError exception.
        // 5. Return possibleEpochNanoseconds.
        Ok(possible_nanoseconds)
    }
}

impl TimeZone {
    // TODO: This can be optimized by just not using a vec.
    pub(crate) fn disambiguate_possible_epoch_nanos(
        &self,
        nanos: Vec<EpochNanoseconds>,
        iso: IsoDateTime,
        disambiguation: Disambiguation,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<EpochNanoseconds> {
        // 1. Let n be possibleEpochNs's length.
        let n = nanos.len();
        // 2. If n = 1, then
        if n == 1 {
            // a. Return possibleEpochNs[0].
            return Ok(nanos[0]);
        // 3. If n ≠ 0, then
        } else if n != 0 {
            match disambiguation {
                // a. If disambiguation is earlier or compatible, then
                // i. Return possibleEpochNs[0].
                Disambiguation::Compatible | Disambiguation::Earlier => return Ok(nanos[0]),
                // b. If disambiguation is later, then
                // i. Return possibleEpochNs[n - 1].
                Disambiguation::Later => return Ok(nanos[n - 1]),
                // c. Assert: disambiguation is reject.
                // d. Throw a RangeError exception.
                Disambiguation::Reject => {
                    return Err(
                        TemporalError::range().with_message("Rejecting ambiguous time zones.")
                    )
                }
            }
        }
        // 4. Assert: n = 0.
        // 5. If disambiguation is reject, then
        if disambiguation == Disambiguation::Reject {
            // a. Throw a RangeError exception.
            return Err(TemporalError::range().with_message("Rejecting ambiguous time zones."));
        }

        // NOTE: Below is rather greedy, but should in theory work.
        //
        // Primarily moving hour +/-3 to account Australia/Troll as
        // the precision of before/after does not entirely matter as
        // long is it is distinctly before / after any transition.

        // 6. Let before be the latest possible ISO Date-Time Record for
        //    which CompareISODateTime(before, isoDateTime) = -1 and !
        //    GetPossibleEpochNanoseconds(timeZone, before) is not
        //    empty.
        let before = iso.add_date_duration(
            Calendar::default(),
            &DateDuration::default(),
            NormalizedTimeDuration(-3 * NS_IN_HOUR),
            None,
        )?;

        // 7. Let after be the earliest possible ISO Date-Time Record
        //    for which CompareISODateTime(after, isoDateTime) = 1 and !
        //    GetPossibleEpochNanoseconds(timeZone, after) is not empty.
        let after = iso.add_date_duration(
            Calendar::default(),
            &DateDuration::default(),
            NormalizedTimeDuration(3 * NS_IN_HOUR),
            None,
        )?;

        // 8. Let beforePossible be !
        //    GetPossibleEpochNanoseconds(timeZone, before).
        // 9. Assert: beforePossible's length is 1.
        let before_possible = self.get_possible_epoch_ns_for(before, provider)?;
        debug_assert_eq!(before_possible.len(), 1);
        // 10. Let afterPossible be !
        //     GetPossibleEpochNanoseconds(timeZone, after).
        // 11. Assert: afterPossible's length is 1.
        let after_possible = self.get_possible_epoch_ns_for(after, provider)?;
        debug_assert_eq!(after_possible.len(), 1);
        // 12. Let offsetBefore be GetOffsetNanosecondsFor(timeZone,
        //     beforePossible[0]).
        let offset_before = self.get_offset_nanos_for(before_possible[0].0, provider)?;
        // 13. Let offsetAfter be GetOffsetNanosecondsFor(timeZone,
        //     afterPossible[0]).
        let offset_after = self.get_offset_nanos_for(after_possible[0].0, provider)?;
        // 14. Let nanoseconds be offsetAfter - offsetBefore.
        let nanoseconds = offset_after - offset_before;
        // 15. Assert: abs(nanoseconds) ≤ nsPerDay.
        // 16. If disambiguation is earlier, then
        if disambiguation == Disambiguation::Earlier {
            // a. Let timeDuration be TimeDurationFromComponents(0, 0, 0, 0, 0, -nanoseconds).
            let time_duration = NormalizedTimeDuration(-nanoseconds);
            // b. Let earlierTime be AddTime(isoDateTime.[[Time]], timeDuration).
            let earlier_time = iso.time.add(time_duration);
            // c. Let earlierDate be BalanceISODate(isoDateTime.[[ISODate]].[[Year]],
            // isoDateTime.[[ISODate]].[[Month]],
            // isoDateTime.[[ISODate]].[[Day]] + earlierTime.[[Days]]).
            let earlier_date = IsoDate::try_balance(
                iso.date.year,
                iso.date.month.into(),
                i64::from(iso.date.day) + earlier_time.0,
            )?;

            // d. Let earlierDateTime be
            // CombineISODateAndTimeRecord(earlierDate, earlierTime).
            let earlier = IsoDateTime::new_unchecked(earlier_date, earlier_time.1);
            // e. Set possibleEpochNs to ? GetPossibleEpochNanoseconds(timeZone, earlierDateTime).
            let possible = self.get_possible_epoch_ns_for(earlier, provider)?;
            // f. Assert: possibleEpochNs is not empty.
            // g. Return possibleEpochNs[0].
            return Ok(possible[0]);
        }
        // 17. Assert: disambiguation is compatible or later.
        // 18. Let timeDuration be TimeDurationFromComponents(0, 0, 0, 0, 0, nanoseconds).
        let time_duration = NormalizedTimeDuration(nanoseconds);
        // 19. Let laterTime be AddTime(isoDateTime.[[Time]], timeDuration).
        let later_time = iso.time.add(time_duration);
        // 20. Let laterDate be BalanceISODate(isoDateTime.[[ISODate]].[[Year]],
        // isoDateTime.[[ISODate]].[[Month]], isoDateTime.[[ISODate]].[[Day]] + laterTime.[[Days]]).
        let later_date = IsoDate::try_balance(
            iso.date.year,
            iso.date.month.into(),
            i64::from(iso.date.day) + later_time.0,
        )?;
        // 21. Let laterDateTime be CombineISODateAndTimeRecord(laterDate, laterTime).
        let later = IsoDateTime::new_unchecked(later_date, later_time.1);
        // 22. Set possibleEpochNs to ? GetPossibleEpochNanoseconds(timeZone, laterDateTime).
        let possible = self.get_possible_epoch_ns_for(later, provider)?;
        // 23. Set n to possibleEpochNs's length.
        let n = possible.len();
        // 24. Assert: n ≠ 0.
        // 25. Return possibleEpochNs[n - 1].
        Ok(possible[n - 1])
    }

    pub(crate) fn get_start_of_day(
        &self,
        iso_date: &IsoDate,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<EpochNanoseconds> {
        // 1. Let isoDateTime be CombineISODateAndTimeRecord(isoDate, MidnightTimeRecord()).
        let iso = IsoDateTime::new_unchecked(*iso_date, IsoTime::default());
        // 2. Let possibleEpochNs be ? GetPossibleEpochNanoseconds(timeZone, isoDateTime).
        let possible_nanos = self.get_possible_epoch_ns_for(iso, provider)?;
        // 3. If possibleEpochNs is not empty, return possibleEpochNs[0].
        if !possible_nanos.is_empty() {
            return Ok(possible_nanos[0]);
        }
        let TimeZone::IanaIdentifier(identifier) = self else {
            debug_assert!(
                false,
                "4. Assert: IsOffsetTimeZoneIdentifier(timeZone) is false."
            );
            return Err(
                TemporalError::assert().with_message("Timezone was not an Iana identifier.")
            );
        };
        // 5. Let possibleEpochNsAfter be GetNamedTimeZoneEpochNanoseconds(timeZone, isoDateTimeAfter), where
        // isoDateTimeAfter is the ISO Date-Time Record for which ! DifferenceISODateTime(isoDateTime,
        // isoDateTimeAfter, "iso8601", hour).[[Time]] is the smallest possible value > 0 for which
        // possibleEpochNsAfter is not empty (i.e., isoDateTimeAfter represents the first local time
        // after the transition).

        // Similar to disambiguation, we need to first get the possible epoch for the current start of day +
        // 3 hours, then get the timestamp for the transition epoch.
        let after = IsoDateTime::new_unchecked(
            *iso_date,
            IsoTime {
                hour: 3,
                ..Default::default()
            },
        );
        let Some(after_epoch) = self
            .get_possible_epoch_ns_for(after, provider)?
            .into_iter()
            .next()
        else {
            return Err(TemporalError::r#type()
                .with_message("Could not determine the start of day for the provided date."));
        };

        let TimeZoneOffset {
            transition_epoch: Some(transition_epoch),
            ..
        } = provider.get_named_tz_offset_nanoseconds(identifier, after_epoch.0)?
        else {
            return Err(TemporalError::r#type()
                .with_message("Could not determine the start of day for the provided date."));
        };

        // let provider.
        // 6. Assert: possibleEpochNsAfter's length = 1.
        // 7. Return possibleEpochNsAfter[0].
        EpochNanoseconds::try_from(i128::from(transition_epoch) * 1_000_000_000)
    }
}

#[cfg(test)]
mod tests {
    use super::TimeZone;

    #[test]
    fn from_and_to_string() {
        let src = "+09:30";
        let tz = TimeZone::try_from_identifier_str(src).unwrap();
        assert_eq!(tz.identifier().unwrap(), src);

        let src = "-09:30";
        let tz = TimeZone::try_from_identifier_str(src).unwrap();
        assert_eq!(tz.identifier().unwrap(), src);

        let src = "-12:30";
        let tz = TimeZone::try_from_identifier_str(src).unwrap();
        assert_eq!(tz.identifier().unwrap(), src);

        let src = "America/New_York";
        let tz = TimeZone::try_from_identifier_str(src).unwrap();
        assert_eq!(tz.identifier().unwrap(), src);
    }
}
