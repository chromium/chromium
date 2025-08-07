// Relevant operations:
//
//  - Time Zone Identifiers
//  - AvailableNamedTimeZoneIdentifiers
//  - SystemTimeZoneIdentifier
//  - IsTimeZoneOffsetString
//  - GetNamedTimeZoneEpochNanoseconds
//     - fn(id, isoDateTimeRecord) -> [epochNanoseconds]
//  - GetNamedTimeZoneOffsetNanoseconds
//     - fn(id, epochNanoseconds) -> [offset]

// TODO: Potentially implement a IsoDateTimeRecord type to decouple
// public facing APIs from IsoDateTime

// Could return type be something like [Option<i128>; 2]

// NOTE: tzif data is computed in glibc's `__tzfile_compute` in `tzfile.c`.
//
// Handling the logic here may be incredibly important for full tzif support.

// NOTES:
//
// Transitions to DST (in march) + 1. Empty list between 2:00-3:00.
// Transitions to Std (in nov) -1. Two elements 1:00-2:00 is repeated twice.

// Transition Seconds + (offset diff)
// where
// offset diff = is_dst { dst_off - std_off } else { std_off - dst_off }, i.e. to_offset - from_offset

use std::path::Path;
#[cfg(target_family = "unix")]
use std::path::PathBuf;

use alloc::borrow::Cow;
use alloc::collections::BTreeMap;
use alloc::string::{String, ToString};
use core::cmp::Ordering;
use core::ops::Range;
use std::sync::RwLock;

use combine::Parser;

use timezone_provider::prelude::*;

use tzif::{
    self,
    data::{
        posix::{
            DstTransitionInfo, PosixTzString, TimeZoneVariantInfo, TransitionDate, TransitionDay,
        },
        time::Seconds,
        tzif::{DataBlock, LocalTimeTypeRecord, TzifData, TzifHeader},
    },
};

use crate::{
    iso::IsoDateTime,
    provider::{
        CandidateEpochNanoseconds, GapEntryOffsets, TimeZoneProvider, TimeZoneTransitionInfo,
        TransitionDirection, UtcOffsetSeconds,
    },
    unix_time::EpochNanoseconds,
    utils, TemporalError, TemporalResult,
};

timezone_provider::iana_normalizer_singleton!();

#[cfg(target_family = "unix")]
const ZONEINFO_DIR: &str = "/usr/share/zoneinfo/";

impl From<&TimeZoneVariantInfo> for UtcOffsetSeconds {
    fn from(value: &TimeZoneVariantInfo) -> Self {
        // The POSIX tz string stores offsets as negative offsets;
        // i.e. "seconds that must be added to reach UTC"
        Self(-value.offset.0)
    }
}

impl From<LocalTimeTypeRecord> for UtcOffsetSeconds {
    fn from(value: LocalTimeTypeRecord) -> Self {
        Self(value.utoff.0)
    }
}

impl From<Seconds> for EpochNanoseconds {
    fn from(value: Seconds) -> Self {
        seconds_to_nanoseconds(value.0).into()
    }
}

// TODO: Workshop record name?
/// The `LocalTimeRecord` result represents the result of searching for a
/// time zone transition without the offset seconds applied to the
/// epoch seconds.
///
/// As a result of the search, it is possible for the resulting search to be either
/// Empty (due to an invalid time being provided that would be in the +1 tz shift)
/// or two time zones (when a time exists in the ambiguous range of a -1 shift).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LocalTimeRecordResult {
    Empty(GapEntryOffsets),
    Single(UtcOffsetSeconds),
    Ambiguous {
        first: UtcOffsetSeconds,
        second: UtcOffsetSeconds,
    },
}

impl From<UtcOffsetSeconds> for LocalTimeRecordResult {
    fn from(value: UtcOffsetSeconds) -> Self {
        Self::Single(value)
    }
}

impl From<LocalTimeTypeRecord> for LocalTimeRecordResult {
    fn from(value: LocalTimeTypeRecord) -> Self {
        Self::Single(value.into())
    }
}

impl From<(LocalTimeTypeRecord, LocalTimeTypeRecord)> for LocalTimeRecordResult {
    fn from(value: (LocalTimeTypeRecord, LocalTimeTypeRecord)) -> Self {
        Self::Ambiguous {
            first: value.0.into(),
            second: value.1.into(),
        }
    }
}

/// `TZif` stands for Time zone information format is laid out by [RFC 8536][rfc8536] and
/// laid out by the [tzdata manual][tzif-manual]
///
/// To be specific, this representation of `TZif` is solely to extend functionality
/// fo the parsed type from the `tzif` [rust crate][tzif-crate], which has further detail on the
/// layout in Rust.
///
/// `TZif` files are compiled via [`zic`][zic-manual], which offers a variety of options for changing the layout
/// and range of a `TZif`.
///
/// [rfc8536]: https://datatracker.ietf.org/doc/html/rfc8536
/// [tzif-manual]: https://man7.org/linux/man-pages/man5/tzfile.5.html
/// [tzif-crate]: https://docs.rs/tzif/latest/tzif/
/// [zic-manual]: https://man7.org/linux/man-pages/man8/zic.8.html
#[derive(Debug, Clone)]
pub struct Tzif {
    pub header1: TzifHeader,
    pub data_block1: DataBlock,
    pub header2: Option<TzifHeader>,
    pub data_block2: Option<DataBlock>,
    pub footer: Option<PosixTzString>,
}

impl From<TzifData> for Tzif {
    fn from(value: TzifData) -> Self {
        let TzifData {
            header1,
            data_block1,
            header2,
            data_block2,
            footer,
        } = value;

        Self {
            header1,
            data_block1,
            header2,
            data_block2,
            footer,
        }
    }
}

impl Tzif {
    pub fn from_bytes(data: &[u8]) -> TemporalResult<Self> {
        let Ok((parse_result, _)) = tzif::parse::tzif::tzif().parse(data) else {
            return Err(TemporalError::general("Illformed Tzif data."));
        };
        Ok(Self::from(parse_result))
    }

    #[cfg(target_family = "unix")]
    pub fn read_tzif(identifier: &str) -> TemporalResult<Self> {
        // Protect from path traversal attacks
        if identifier.starts_with('/') || identifier.contains('.') {
            return Err(TemporalError::range().with_message("Ill-formed timezone identifier"));
        }
        let mut path = PathBuf::from(ZONEINFO_DIR);
        path.push(identifier);
        Self::from_path(&path)
    }

    pub fn from_path(path: &Path) -> TemporalResult<Self> {
        if !path.exists() {
            return Err(TemporalError::range().with_message("Unknown timezone identifier"));
        }
        tzif::parse_tzif_file(path)
            .map(Into::into)
            .map_err(|e| TemporalError::general(e.to_string()))
    }

    pub fn posix_tz_string(&self) -> Option<&PosixTzString> {
        self.footer.as_ref()
    }

    pub fn get_data_block2(&self) -> TemporalResult<&DataBlock> {
        self.data_block2
            .as_ref()
            .ok_or(TemporalError::general("Only Tzif V2+ is supported."))
    }

    pub fn get(&self, epoch_seconds: &Seconds) -> TemporalResult<TimeZoneTransitionInfo> {
        let db = self.get_data_block2()?;

        let result = db.transition_times.binary_search(epoch_seconds);

        match result {
            // The transition time was given. The transition entries *start* at their
            // transition time, so we use the same index
            Ok(idx) => Ok(get_timezone_offset(db, idx)),
            // <https://datatracker.ietf.org/doc/html/rfc8536#section-3.2>
            // If there are no transitions, local time for all timestamps is specified by the TZ
            // string in the footer if present and nonempty; otherwise, it is
            // specified by time type 0.
            Err(_) if db.transition_times.is_empty() => {
                if let Some(posix_tz_string) = self.posix_tz_string() {
                    resolve_posix_tz_string_for_epoch_seconds(posix_tz_string, epoch_seconds.0)
                } else {
                    Ok(TimeZoneTransitionInfo {
                        offset: db.local_time_type_records[0].into(),
                        transition_epoch: None,
                    })
                }
            }
            // Our time is before the first transition.
            // Get the first timezone offset
            Err(0) => Ok(get_first_timezone_offset(db)),
            // Our time is after some transition.
            Err(idx) => {
                if db.transition_times.len() <= idx {
                    // The transition time provided is beyond the length of
                    // the available transition time, so the time zone is
                    // resolved with the POSIX tz string.
                    let mut offset = resolve_posix_tz_string_for_epoch_seconds(
                        self.posix_tz_string().ok_or(TemporalError::general(
                            "No POSIX tz string to resolve with.",
                        ))?,
                        epoch_seconds.0,
                    )?;
                    offset
                        .transition_epoch
                        .get_or_insert_with(|| db.transition_times[idx - 1].0);
                    return Ok(offset);
                }
                // binary_search returns the insertion index, which is one after the
                // index of the closest lower bound. Fetch that bound.
                Ok(get_timezone_offset(db, idx - 1))
            }
        }
    }

    fn get_named_tz_offset_nanoseconds(
        &self,
        utc_epoch: i128,
    ) -> TemporalResult<TimeZoneTransitionInfo> {
        let mut seconds = (utc_epoch / NS_IN_S) as i64;
        // The rounding is inexact. Transitions are only at second
        // boundaries, so the offset at N s is the same as the offset at N.001,
        // but the offset at -Ns is not the same as the offset at -N.001,
        // the latter matches -N - 1 s instead.
        if seconds < 0 && utc_epoch % NS_IN_S != 0 {
            seconds -= 1;
        }
        self.get(&Seconds(seconds))
    }

    // Helper function to call resolve_posix_tz_string
    fn resolve_posix_tz_string(
        &self,
        local_seconds: &Seconds,
    ) -> TemporalResult<LocalTimeRecordResult> {
        resolve_posix_tz_string(
            self.posix_tz_string()
                .ok_or(TemporalError::general("Could not resolve time zone."))?,
            local_seconds.0,
        )
    }

    pub fn get_named_tz_transition(
        &self,
        epoch_nanoseconds: i128,
        direction: TransitionDirection,
    ) -> TemporalResult<Option<EpochNanoseconds>> {
        // First search the tzif data

        let epoch_seconds = Seconds((epoch_nanoseconds / NS_IN_S) as i64);
        // When *exactly* on a transition the spec wants you to
        // get the next one, so it's important to know if we are
        // actually on epoch_seconds or a couple nanoseconds before/after it
        // to handle the exact match case
        let seconds_is_exact = (epoch_nanoseconds % NS_IN_S) == 0;
        let seconds_is_negative = epoch_nanoseconds < 0;
        let db = self.get_data_block2()?;

        let b_search_result = db.transition_times.binary_search(&epoch_seconds);

        let mut transition_idx = match b_search_result {
            Ok(idx) => {
                match (direction, seconds_is_exact, seconds_is_negative) {
                    // If we are N.001 for negative N, the next transition is idx
                    (TransitionDirection::Next, false, true) => idx,
                    // If we are exactly N, or N.001 for positive N, the next transition is idx + 1
                    (TransitionDirection::Next, true, _)
                    | (TransitionDirection::Next, false, false) => idx + 1,
                    // If we are N.001 for positive N, the previous transition the one at idx (= N)
                    (TransitionDirection::Previous, false, false) => idx,
                    // If we are exactly N, or N.0001 for negative N, the previous transition is idx - 1
                    (TransitionDirection::Previous, true, _)
                    | (TransitionDirection::Previous, false, true) => {
                        if let Some(idx) = idx.checked_sub(1) {
                            idx
                        } else {
                            // If we found the first transition, there is no previous one,
                            // return None
                            return Ok(None);
                        }
                    }
                }
            }
            // idx is insertion index here, so it is the index of the closest upper
            // transition
            Err(idx) => match direction {
                TransitionDirection::Next => idx,
                // Special case, we're after the end of the array, we want to make
                // sure we hit the POSIX tz handling and we should not subtract one.
                TransitionDirection::Previous if idx == db.transition_times.len() => idx,
                TransitionDirection::Previous => {
                    // Go one previous
                    if let Some(idx) = idx.checked_sub(1) {
                        idx
                    } else {
                        // If we found the first transition, there is no previous one,
                        // return None
                        return Ok(None);
                    }
                }
            },
        };

        while let Some(tzif_transition) = maybe_get_transition_info(db, transition_idx) {
            // This is not a real transition. Skip it.
            if tzif_transition.prev.utoff == tzif_transition.next.utoff {
                match direction {
                    TransitionDirection::Next => transition_idx += 1,
                    TransitionDirection::Previous if transition_idx == 0 => return Ok(None),
                    TransitionDirection::Previous => transition_idx -= 1,
                }
            } else {
                return Ok(Some(tzif_transition.transition_time.into()));
            }
        }

        // We went past the Tzif transitions. We need to handle the posix string instead.
        let posix_tz_string = self
            .posix_tz_string()
            .ok_or(TemporalError::general("Could not resolve time zone."))?;

        // The last transition in the tzif tables.
        // We should not go back beyond this
        let last_tzif_transition = db.transition_times.last().copied();

        // We need to do a similar backwards iteration to find the last real transition.
        // Do it only when needed, this case will only show up when walking Previous for a date
        // just after the last tzif transition but before the first posix transition.
        let last_real_tzif_transition = || {
            debug_assert!(direction == TransitionDirection::Previous);
            for last_transition_idx in (0..db.transition_times.len()).rev() {
                if let Some(tzif_transition) = maybe_get_transition_info(db, last_transition_idx) {
                    if tzif_transition.prev.utoff == tzif_transition.next.utoff {
                        continue;
                    }
                    return Some(tzif_transition.transition_time);
                }
                break;
            }
            None
        };

        let Some(dst_variant) = &posix_tz_string.dst_info else {
            // There are no further transitions.
            match direction {
                TransitionDirection::Next => return Ok(None),
                TransitionDirection::Previous => {
                    // Go back to the last tzif transition
                    if last_tzif_transition.is_some() {
                        if let Some(last_real_tzif_transition) = last_real_tzif_transition() {
                            return Ok(Some(last_real_tzif_transition.into()));
                        }
                    }
                    return Ok(None);
                }
            }
        };

        let year = utils::epoch_time_to_epoch_year(epoch_seconds.0 * 1000);

        let transition_info = DstTransitionInfoForYear::compute(posix_tz_string, dst_variant, year);

        let range = transition_info.transition_range();

        let mut seconds = match direction {
            TransitionDirection::Next => {
                // Inexact seconds in the negative case means that (seconds == foo) is actually
                // seconds < foo
                //
                // This code will likely not actually be hit: the current Tzif database has no
                // entries with DST offset posix strings where the posix string starts
                // before the unix epoch.
                let seconds_is_inexact_for_negative = seconds_is_negative && !seconds_is_exact;
                // We're before the first transition
                if epoch_seconds < range.start
                    || (epoch_seconds == range.start && seconds_is_inexact_for_negative)
                {
                    range.start
                } else if epoch_seconds < range.end
                    || (epoch_seconds == range.end && seconds_is_inexact_for_negative)
                {
                    // We're between the first and second transition
                    range.end
                } else {
                    // We're after the second transition
                    let transition_info =
                        DstTransitionInfoForYear::compute(posix_tz_string, dst_variant, year + 1);

                    transition_info.transition_range().start
                }
            }
            TransitionDirection::Previous => {
                // Inexact seconds in the positive case means that (seconds == foo) is actually
                // seconds > foo
                let seconds_is_ineexact_for_positive = !seconds_is_negative && !seconds_is_exact;
                // We're after the second transition
                // (note that seconds_is_exact means that epoch_seconds == range.end actually means equality)
                if epoch_seconds > range.end
                    || (epoch_seconds == range.end && seconds_is_ineexact_for_positive)
                {
                    range.end
                } else if epoch_seconds > range.start
                    || (epoch_seconds == range.start && seconds_is_ineexact_for_positive)
                {
                    // We're after the first transition
                    range.start
                } else {
                    // We're before the first transition
                    let transition_info =
                        DstTransitionInfoForYear::compute(posix_tz_string, dst_variant, year - 1);

                    transition_info.transition_range().end
                }
            }
        };

        if let Some(last_tzif_transition) = last_tzif_transition {
            // When going Previous, we went back into the area of tzif transition
            if seconds < last_tzif_transition {
                if let Some(last_real_tzif_transition) = last_real_tzif_transition() {
                    seconds = last_real_tzif_transition;
                } else {
                    return Ok(None);
                }
            }
        }

        Ok(Some(seconds.into()))
    }

    // For more information, see /docs/TZDB.md
    /// This function determines the Time Zone output for a local epoch
    /// nanoseconds value without an offset.
    ///
    /// Basically, if someone provides a DateTime 2017-11-05T01:30:00,
    /// we have no way of knowing if this value is in DST or STD.
    /// Furthermore, for the above example, this should return 2 time
    /// zones due to there being two 2017-11-05T01:30:00. On the other
    /// side of the transition, the DateTime 2017-03-12T02:30:00 could
    /// be provided. This time does NOT exist due to the +1 jump from
    /// 02:00 -> 03:00 (but of course it does as a nanosecond value).
    pub fn v2_estimate_tz_pair(
        &self,
        local_seconds: &Seconds,
    ) -> TemporalResult<LocalTimeRecordResult> {
        // We need to estimate a tz pair.
        // First search the ambiguous seconds.
        let db = self.get_data_block2()?;

        // Note that this search is *approximate*. transition_times
        // is in UTC epoch times, whereas we have a local time.
        //
        // An assumption we make is that this will at worst give us an off-by-one error;
        // transition times should not be less than a day apart.
        let b_search_result = db.transition_times.binary_search(local_seconds);

        let mut estimated_idx = match b_search_result {
            Ok(idx) => idx,
            Err(idx) => idx,
        };
        // If we're either out of bounds or at the last
        // entry, we need to check if we're after it, since if we
        // are we need to use posix_tz_string instead.
        //
        // This includes the last entry (hence `idx + 1`) since our search was approximate.
        if estimated_idx + 1 >= db.transition_times.len() {
            // If we are *well past* the last transition time, we want
            // to use the posix tz string
            let mut use_posix = true;
            if !db.transition_times.is_empty() {
                // In case idx was out of bounds, bring it back in
                estimated_idx = db.transition_times.len() - 1;
                let transition_info = get_transition_info(db, estimated_idx);

                // I'm not fully sure if this is correct.
                // Is the next_offset valid for the last transition time in its
                // vicinity? Probably? It does not seem pleasant to try and do this
                // math using half of the transition info and half of the posix info.
                //
                // TODO(manishearth, nekevss): https://github.com/boa-dev/temporal/issues/469
                if transition_info.transition_time_prev_epoch() > *local_seconds
                    || transition_info.transition_time_next_epoch() > *local_seconds
                {
                    // We're before the transition fully ends; we should resolve
                    // with the regular transition time instead of use_posix
                    use_posix = false;
                }
            }
            if use_posix {
                // The transition time provided is beyond the length of
                // the available transition time, so the time zone is
                // resolved with the POSIX tz string.
                return self.resolve_posix_tz_string(local_seconds);
            }
        }

        debug_assert!(estimated_idx < db.transition_times.len());

        let transition_info = get_transition_info(db, estimated_idx);

        let range = transition_info.offset_range_local();

        if range.contains(local_seconds) {
            return Ok(transition_info.record_for_contains());
        } else if *local_seconds < range.start {
            if estimated_idx == 0 {
                // We're at the beginning, there are no timezones before us
                // So we just return the first offset
                return Ok(LocalTimeRecordResult::Single(transition_info.prev.into()));
            }
            // Otherwise, try the previous offset
            estimated_idx -= 1;
        } else {
            if estimated_idx + 1 == db.transition_times.len() {
                // We're at the end, return posix instead
                return self.resolve_posix_tz_string(local_seconds);
            }
            // Otherwise, try the next offset
            estimated_idx += 1;
        }

        let transition_info = get_transition_info(db, estimated_idx);
        let range = transition_info.offset_range_local();

        if range.contains(local_seconds) {
            Ok(transition_info.record_for_contains())
        } else if *local_seconds < range.start {
            // Note that get_transition_info will correctly fetch the first offset
            // into .prev when working with the first transition.
            Ok(LocalTimeRecordResult::Single(transition_info.prev.into()))
        } else {
            // We're at the end, return posix instead
            if estimated_idx + 1 == db.transition_times.len() {
                return self.resolve_posix_tz_string(local_seconds);
            }
            Ok(LocalTimeRecordResult::Single(transition_info.next.into()))
        }
    }

    /// Given a *local* datetime, return all possible epoch nanosecond values for it
    fn get_named_tz_epoch_nanoseconds(
        &self,
        local_datetime: IsoDateTime,
    ) -> TemporalResult<CandidateEpochNanoseconds> {
        let epoch_nanos = local_datetime.as_nanoseconds();
        let mut seconds = (epoch_nanos.0 / NS_IN_S) as i64;

        // We just rounded our ns value to seconds.
        // This is fine for positive ns: timezones do not transition at sub-second offsets,
        // so the offset at N seconds is always the offset at N.0001 seconds.
        //
        // However, for negative epochs, the offset at -N seconds might be different
        // from that at -N.001 seconds. Instead, we calculate the offset at (-N-1) seconds.
        if seconds < 0 {
            let remainder = epoch_nanos.0 % NS_IN_S;
            if remainder != 0 {
                seconds -= 1;
            }
        }

        let local_time_record_result = self.v2_estimate_tz_pair(&Seconds(seconds))?;
        let result = match local_time_record_result {
            LocalTimeRecordResult::Empty(bounds) => CandidateEpochNanoseconds::Zero(bounds),
            LocalTimeRecordResult::Single(r) => {
                let epoch_ns = EpochNanoseconds::from(epoch_nanos.0 - seconds_to_nanoseconds(r.0));
                CandidateEpochNanoseconds::One(epoch_ns)
            }
            LocalTimeRecordResult::Ambiguous { first, second } => {
                let first_epoch_ns =
                    EpochNanoseconds::from(epoch_nanos.0 - seconds_to_nanoseconds(first.0));
                let second_epoch_ns =
                    EpochNanoseconds::from(epoch_nanos.0 - seconds_to_nanoseconds(second.0));
                CandidateEpochNanoseconds::Two([first_epoch_ns, second_epoch_ns])
            }
        };
        Ok(result)
    }
}

#[inline]
fn get_timezone_offset(db: &DataBlock, idx: usize) -> TimeZoneTransitionInfo {
    // NOTE: Transition type can be empty. If no transition_type exists,
    // then use 0 as the default index of local_time_type_records.
    let offset = db.local_time_type_records[db.transition_types.get(idx).copied().unwrap_or(0)];
    TimeZoneTransitionInfo {
        transition_epoch: db.transition_times.get(idx).map(|s| s.0),
        offset: offset.into(),
    }
}

#[inline]
fn get_first_timezone_offset(db: &DataBlock) -> TimeZoneTransitionInfo {
    let offset = db.local_time_type_records[0];
    TimeZoneTransitionInfo {
        // There was no transition into the first timezone
        transition_epoch: None,
        offset: offset.into(),
    }
}

#[inline]
fn get_local_record(db: &DataBlock, idx: usize) -> LocalTimeTypeRecord {
    // NOTE: Transition type can be empty. If no transition_type exists,
    // then use 0 as the default index of local_time_type_records.
    let idx = db.transition_types.get(idx).copied().unwrap_or(0);

    let get = db.local_time_type_records.get(idx);
    debug_assert!(get.is_some(), "tzif internal invariant violated");
    get.copied().unwrap_or_default()
}

#[inline]
fn get_transition_info(db: &DataBlock, idx: usize) -> TzifTransitionInfo {
    let info = maybe_get_transition_info(db, idx);
    debug_assert!(info.is_some(), "tzif internal invariant violated");
    info.unwrap_or_default()
}

#[inline]
fn maybe_get_transition_info(db: &DataBlock, idx: usize) -> Option<TzifTransitionInfo> {
    let next = get_local_record(db, idx);
    let transition_time = *db.transition_times.get(idx)?;
    let prev = if idx == 0 {
        *db.local_time_type_records.first()?
    } else {
        get_local_record(db, idx - 1)
    };
    Some(TzifTransitionInfo {
        prev,
        next,
        transition_time,
    })
}

/// Information obtained from the tzif file about a transition.
#[derive(Debug, Default)]
struct TzifTransitionInfo {
    /// The time record from before this transition
    prev: LocalTimeTypeRecord,
    /// The time record corresponding to this transition and dates after it
    next: LocalTimeTypeRecord,
    /// The UTC epoch seconds for the transition
    transition_time: Seconds,
}

impl TzifTransitionInfo {
    /// Get the previous offset as a number of seconds from
    /// 1970-01-01 in local time as reckoned by the previous offset
    fn transition_time_prev_epoch(&self) -> Seconds {
        // You always add the UTC offset to get the local time;
        // so a local time in PST (-08:00) will be `utc - 8h`
        self.transition_time + self.prev.utoff
    }
    /// Get the previous offset as a number of seconds from
    /// 1970-01-01 in local time as reckoned by the next offset
    fn transition_time_next_epoch(&self) -> Seconds {
        // You always add the UTC offset to get the local time;
        // so a local time in PST (-08:00) will be `utc - 8h`
        self.transition_time + self.next.utoff
    }

    /// Gets the range of local times where this transition is active
    ///
    /// Note that this will always be start..end, NOT prev..next: if the next
    /// offset is before prev (e.g. for a TransitionKind::Overlap) year,
    /// it will be next..prev.
    ///
    /// You should use .kind() to understand how to interpret this
    fn offset_range_local(&self) -> Range<Seconds> {
        let prev = self.transition_time_prev_epoch();
        let next = self.transition_time_next_epoch();
        match self.kind() {
            TransitionKind::Overlap => next..prev,
            _ => prev..next,
        }
    }

    /// What is the kind of the transition?
    fn kind(&self) -> TransitionKind {
        match self.prev.utoff.cmp(&self.next.utoff) {
            Ordering::Less => TransitionKind::Gap,
            Ordering::Greater => TransitionKind::Overlap,
            Ordering::Equal => TransitionKind::Smooth,
        }
    }

    /// If a time is found to be within self.offset_range_local(),
    /// what is the corresponding LocalTimeRecordResult?
    fn record_for_contains(&self) -> LocalTimeRecordResult {
        match self.kind() {
            TransitionKind::Gap => LocalTimeRecordResult::Empty(GapEntryOffsets {
                offset_before: self.prev.into(),
                offset_after: self.next.into(),
                transition_epoch: self.transition_time.into(),
            }),
            TransitionKind::Overlap => LocalTimeRecordResult::Ambiguous {
                first: self.prev.into(),
                second: self.next.into(),
            },
            TransitionKind::Smooth => LocalTimeRecordResult::Single(self.prev.into()),
        }
    }
}

#[derive(Debug)]
enum TransitionKind {
    // The offsets didn't change (happens when abbreviations/savings values change)
    Smooth,
    // The offsets changed in a way that leaves a gap
    Gap,
    // The offsets changed in a way that produces overlapping time.
    Overlap,
}

/// Stores the information about DST transitions for a given year
struct DstTransitionInfoForYear {
    dst_start_seconds: Seconds,
    dst_end_seconds: Seconds,
    std_offset: UtcOffsetSeconds,
    dst_offset: UtcOffsetSeconds,
}

impl DstTransitionInfoForYear {
    fn compute(
        posix_tz_string: &PosixTzString,
        dst_variant: &DstTransitionInfo,
        year: i32,
    ) -> Self {
        let std_offset = UtcOffsetSeconds::from(&posix_tz_string.std_info);
        let dst_offset = UtcOffsetSeconds::from(&dst_variant.variant_info);
        let dst_start_seconds = Seconds(calculate_transition_seconds_for_year(
            year,
            dst_variant.start_date,
            std_offset,
        ));
        let dst_end_seconds = Seconds(calculate_transition_seconds_for_year(
            year,
            dst_variant.end_date,
            dst_offset,
        ));
        Self {
            dst_start_seconds,
            dst_end_seconds,
            std_offset,
            dst_offset,
        }
    }

    // Returns the range between offsets in this year
    // This may cover DST or standard time, whichever starts first
    pub fn transition_range(&self) -> Range<Seconds> {
        if self.dst_start_seconds > self.dst_end_seconds {
            self.dst_end_seconds..self.dst_start_seconds
        } else {
            self.dst_start_seconds..self.dst_end_seconds
        }
    }
}

// NOTE: seconds here are epoch, so they are exact, not wall time.
#[inline]
fn resolve_posix_tz_string_for_epoch_seconds(
    posix_tz_string: &PosixTzString,
    seconds: i64,
) -> TemporalResult<TimeZoneTransitionInfo> {
    let Some(dst_variant) = &posix_tz_string.dst_info else {
        // Regardless of the time, there is one variant and we can return it.
        return Ok(TimeZoneTransitionInfo {
            transition_epoch: None,
            offset: UtcOffsetSeconds::from(&posix_tz_string.std_info),
        });
    };

    let year = utils::epoch_time_to_epoch_year(seconds * 1000);

    let transition_info = DstTransitionInfoForYear::compute(posix_tz_string, dst_variant, year);
    let dst_start_seconds = transition_info.dst_start_seconds.0;
    let dst_end_seconds = transition_info.dst_end_seconds.0;

    // Need to determine if the range being tested is standard or savings time.
    let dst_is_inversed = dst_end_seconds < dst_start_seconds;

    // We have potentially to different variations of the DST start and end time.
    //
    // Northern hemisphere: dst_start -> dst_end
    // Southern hemisphere: dst_end -> dst_start
    //
    // This is primarily due to the summer / winter months of those areas.
    //
    // For the northern hemispere, we can check if the range contains the seconds. For the
    // southern hemisphere, we check if the range does no contain the value.
    let should_return_dst = (!dst_is_inversed
        && (dst_start_seconds..dst_end_seconds).contains(&seconds))
        || (dst_is_inversed && !(dst_end_seconds..dst_start_seconds).contains(&seconds));

    // Expanding on the above, the state of time zones in the year are:
    //
    // Northern hemisphere: STD -> DST -> STD
    // Southern hemisphere: DST -> STD -> DST
    //
    // This is simple for the returning the offsets, but if the seconds value falls into the first
    // available rule. However, the northern hemisphere's first STD rule and the Southern hemisphere's
    // first DST rule will have different transition times that are based in the year prior, so if the
    // requested seconds falls in that range, we calculate the transition time for the prior year.
    let (new_offset, transition_epoch) = if should_return_dst {
        let transition_epoch = if dst_is_inversed && seconds < dst_end_seconds {
            Some(calculate_transition_seconds_for_year(
                year - 1,
                dst_variant.start_date,
                transition_info.dst_offset,
            ))
        } else {
            Some(dst_start_seconds)
        };
        (transition_info.dst_offset, transition_epoch)
    } else {
        let transition_epoch = if !dst_is_inversed && seconds < dst_start_seconds {
            Some(calculate_transition_seconds_for_year(
                year - 1,
                dst_variant.end_date,
                transition_info.std_offset,
            ))
        } else {
            Some(dst_end_seconds)
        };
        (transition_info.std_offset, transition_epoch)
    };
    Ok(TimeZoneTransitionInfo {
        offset: new_offset,
        transition_epoch,
    })
}

fn calculate_transition_seconds_for_year(
    year: i32,
    transition_date: TransitionDate,
    offset: UtcOffsetSeconds,
) -> i64 {
    // Determine the year of the requested time.
    let year_epoch_seconds = i64::from(utils::epoch_days_for_year(year)) * 86400;
    let leap_day = (utils::mathematical_days_in_year(year) - 365) as u16;

    // Calculate the days in the year for the TransitionDate
    let days = match transition_date.day {
        TransitionDay::NoLeap(day) if day > 59 => day - 1 + leap_day,
        TransitionDay::NoLeap(day) => day - 1,
        TransitionDay::WithLeap(day) => day,
        TransitionDay::Mwd(month, week, day) => {
            let days_to_month = utils::month_to_day((month - 1) as u8, leap_day);
            let days_in_month = u16::from(utils::iso_days_in_month(year, month as u8) - 1);

            // Month starts in the day...
            let day_offset = (u16::from(utils::epoch_seconds_to_day_of_week(year_epoch_seconds))
                + days_to_month)
                .rem_euclid(7);

            // EXAMPLE:
            //
            // 0   1   2   3   4   5   6
            // sun mon tue wed thu fri sat
            // -   -   -   0   1   2   3
            // 4   5   6   7   8   9   10
            // 11  12  13  14  15  16  17
            // 18  19  20  21  22  23  24
            // 25  26  27  28  29  30  -
            //
            // The day_offset = 3, since the month starts on a wednesday.
            //
            // We're looking for the second friday of the month. Thus, since the month started before
            // a friday, we need to start counting from week 0:
            //
            // day_of_month = (week - u16::from(day_offset <= day)) * 7 + day - day_offset = (2 - 1) * 7 + 5 - 3 = 9
            //
            // This works if the month started on a day before the day we want (day_offset <= day). However, if that's not the
            // case, we need to start counting on week 1. For example, calculate the day of the month for the third monday
            // of the month:
            //
            // day_of_month = (week - u16::from(day_offset <= day)) * 7 + day - day_offset = (3 - 0) * 7 + 1 - 3 = 19
            let mut day_of_month = (week - u16::from(day_offset <= day)) * 7 + day - day_offset;

            // If we're on week 5, we need to clamp to the last valid day.
            if day_of_month > days_in_month - 1 {
                day_of_month -= 7
            }

            days_to_month + day_of_month
        }
    };

    // Transition time is on local time, so we need to add the UTC offset to get the correct UTC timestamp
    // for the transition.
    year_epoch_seconds + i64::from(days) * 86400 + transition_date.time.0 - offset.0
}

/// Resolve the footer of a tzif file.
///
/// Seconds are epoch seconds in local time.
#[inline]
fn resolve_posix_tz_string(
    posix_tz_string: &PosixTzString,
    seconds: i64,
) -> TemporalResult<LocalTimeRecordResult> {
    let std = &posix_tz_string.std_info;
    let Some(dst) = &posix_tz_string.dst_info else {
        // Regardless of the time, there is one variant and we can return it.
        return Ok(UtcOffsetSeconds::from(&posix_tz_string.std_info).into());
    };

    // TODO: Resolve safety issue around utils.
    //   Using f64 is a hold over from early implementation days and should
    //   be moved away from.

    // NOTE:
    // STD -> DST == start
    // DST -> STD == end
    let (is_transition_day, mut is_dst) =
        cmp_seconds_to_transitions(&dst.start_date.day, &dst.end_date.day, seconds)?;
    if is_transition_day {
        let time = utils::epoch_ms_to_ms_in_day(seconds * 1_000) as i64 / 1_000;
        let transition_time = if is_dst == TransitionType::Dst {
            dst.start_date.time
        } else {
            dst.end_date.time
        };
        // Convert to UtcOffsetSeconds so that these behave like
        // normal offsets
        let std = UtcOffsetSeconds::from(std);
        let dst = UtcOffsetSeconds::from(&dst.variant_info);
        let transition_diff = if is_dst == TransitionType::Dst {
            dst.0 - std.0
        } else {
            std.0 - dst.0
        };
        let offset = offset_range(transition_time.0 + transition_diff, transition_time.0);
        match offset.contains(&time) {
            true if is_dst == TransitionType::Dst => {
                return Ok(LocalTimeRecordResult::Empty(GapEntryOffsets {
                    offset_before: std,
                    offset_after: dst,
                    transition_epoch: transition_time.into(),
                }));
            }
            true => {
                // Note(nekevss, manishearth): We may need to more carefully
                // handle inverse DST here.
                return Ok(LocalTimeRecordResult::Ambiguous {
                    first: dst,
                    second: std,
                });
            }
            _ => {}
        }

        // We were not contained in the transition above,
        // AND we are before it, which means we are actually in
        // the other transition!
        //
        // NOTE(Manishearth) do we need to do anything special
        // here if we end up back at the tzif transition data?
        if time < offset.start {
            is_dst.invert();
        }
    }

    match is_dst {
        TransitionType::Dst => Ok(UtcOffsetSeconds::from(&dst.variant_info).into()),
        TransitionType::Std => Ok(UtcOffsetSeconds::from(&posix_tz_string.std_info).into()),
    }
}

/// The month, week of month, and day of week value built into the POSIX tz string.
///
/// For more information, see the [POSIX tz string docs](https://sourceware.org/glibc/manual/2.40/html_node/Proleptic-TZ.html)
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
struct Mwd(u16, u16, u16);

impl Mwd {
    fn from_seconds(seconds: i64) -> Self {
        let month = utils::epoch_ms_to_month_in_year(seconds * 1_000) as u16;
        let day_of_month = utils::epoch_seconds_to_day_of_month(seconds);
        let week_of_month = day_of_month / 7 + 1;
        let day_of_week = utils::epoch_seconds_to_day_of_week(seconds);
        Self(month, week_of_month, u16::from(day_of_week))
    }
}

fn cmp_seconds_to_transitions(
    start: &TransitionDay,
    end: &TransitionDay,
    seconds: i64,
) -> TemporalResult<(bool, TransitionType)> {
    let cmp_result = match (start, end) {
        (
            TransitionDay::Mwd(start_month, start_week, start_day),
            TransitionDay::Mwd(end_month, end_week, end_day),
        ) => {
            let mwd = Mwd::from_seconds(seconds);
            let start = Mwd(*start_month, *start_week, *start_day);
            let end = Mwd(*end_month, *end_week, *end_day);

            let is_transition = start == mwd || end == mwd;
            let is_dst = if start > end {
                mwd < end || start <= mwd
            } else {
                start <= mwd && mwd < end
            };

            (is_transition, is_dst)
        }
        (TransitionDay::WithLeap(start), TransitionDay::WithLeap(end)) => {
            let day_in_year = utils::epoch_time_to_day_in_year(seconds * 1_000) as u16;
            let is_transition = *start == day_in_year || *end == day_in_year;
            let is_dst = if start > end {
                day_in_year < *end || *start <= day_in_year
            } else {
                *start <= day_in_year && day_in_year < *end
            };
            (is_transition, is_dst)
        }
        // TODO: do we need to modify the logic for leap years?
        (TransitionDay::NoLeap(start), TransitionDay::NoLeap(end)) => {
            let day_in_year = utils::epoch_time_to_day_in_year(seconds * 1_000) as u16;
            let is_transition = *start == day_in_year || *end == day_in_year;
            let is_dst = if start > end {
                day_in_year < *end || *start <= day_in_year
            } else {
                *start <= day_in_year && day_in_year < *end
            };
            (is_transition, is_dst)
        }
        // NOTE: The assumption here is that mismatched day types on
        // a POSIX string is an illformed string.
        _ => {
            return Err(
                TemporalError::assert().with_message("Mismatched day types on a POSIX string.")
            )
        }
    };

    match cmp_result {
        (true, dst) if dst => Ok((true, TransitionType::Dst)),
        (true, _) => Ok((true, TransitionType::Std)),
        (false, dst) if dst => Ok((false, TransitionType::Dst)),
        (false, _) => Ok((false, TransitionType::Std)),
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum TransitionType {
    Dst,
    Std,
}

impl TransitionType {
    fn invert(&mut self) {
        *self = match *self {
            Self::Dst => Self::Std,
            Self::Std => Self::Dst,
        }
    }
}

fn offset_range(offset_one: i64, offset_two: i64) -> core::ops::Range<i64> {
    if offset_one < offset_two {
        return offset_one..offset_two;
    }
    offset_two..offset_one
}

fn normalize_identifier_with_compiled(identifier: &[u8]) -> TemporalResult<Cow<'static, str>> {
    if let Some(index) = SINGLETON_IANA_NORMALIZER.available_id_index.get(identifier) {
        return SINGLETON_IANA_NORMALIZER
            .normalized_identifiers
            .get(index)
            .map(Cow::Borrowed)
            .ok_or(TemporalError::range().with_message("Unknown time zone identifier"));
    }

    Err(TemporalError::range().with_message("Unknown time zone identifier"))
}

fn canonicalize_identifier_with_compiled(identifier: &[u8]) -> TemporalResult<Cow<'static, str>> {
    let idx = SINGLETON_IANA_NORMALIZER
        .non_canonical_identifiers
        .get(identifier)
        .or(SINGLETON_IANA_NORMALIZER.available_id_index.get(identifier));

    if let Some(index) = idx {
        return SINGLETON_IANA_NORMALIZER
            .normalized_identifiers
            .get(index)
            .map(Cow::Borrowed)
            .ok_or(TemporalError::range().with_message("Unknown time zone identifier"));
    }

    Err(TemporalError::range().with_message("Unknown time zone identifier"))
}

/// Timezone provider that uses compiled data.
///
/// Currently uses jiff_tzdb and performs parsing; will eventually
/// use pure compiled data (<https://github.com/boa-dev/temporal/pull/264>)
#[derive(Debug, Default)]
pub struct CompiledTzdbProvider {
    cache: RwLock<BTreeMap<String, Tzif>>,
}

impl CompiledTzdbProvider {
    /// Get timezone data for a single identifier
    pub fn get(&self, identifier: &str) -> TemporalResult<Tzif> {
        if let Some(tzif) = self
            .cache
            .read()
            .map_err(|_| TemporalError::general("poisoned RWLock"))?
            .get(identifier)
        {
            return Ok(tzif.clone());
        }

        let (identifier, tzif) = {
            let Some((canonical_name, data)) = jiff_tzdb::get(identifier) else {
                return Err(
                    TemporalError::range().with_message("Time zone identifier does not exist.")
                );
            };
            (canonical_name, Tzif::from_bytes(data)?)
        };

        Ok(self
            .cache
            .write()
            .map_err(|_| TemporalError::general("poisoned RWLock"))?
            .entry(identifier.into())
            .or_insert(tzif)
            .clone())
    }
}

impl TimeZoneProvider for CompiledTzdbProvider {
    fn normalize_identifier(&self, ident: &'_ [u8]) -> TemporalResult<Cow<'_, str>> {
        normalize_identifier_with_compiled(ident)
    }
    fn canonicalize_identifier(&self, ident: &'_ [u8]) -> TemporalResult<Cow<'_, str>> {
        canonicalize_identifier_with_compiled(ident)
    }
    fn get_named_tz_epoch_nanoseconds(
        &self,
        identifier: &str,
        local_datetime: IsoDateTime,
    ) -> TemporalResult<CandidateEpochNanoseconds> {
        self.get(identifier)?
            .get_named_tz_epoch_nanoseconds(local_datetime)
    }

    fn get_named_tz_offset_nanoseconds(
        &self,
        identifier: &str,
        utc_epoch: i128,
    ) -> TemporalResult<TimeZoneTransitionInfo> {
        self.get(identifier)?
            .get_named_tz_offset_nanoseconds(utc_epoch)
    }

    fn get_named_tz_transition(
        &self,
        identifier: &str,
        epoch_nanoseconds: i128,
        direction: TransitionDirection,
    ) -> TemporalResult<Option<EpochNanoseconds>> {
        let tzif = self.get(identifier)?;
        tzif.get_named_tz_transition(epoch_nanoseconds, direction)
    }
}

#[derive(Debug, Default)]
pub struct FsTzdbProvider {
    cache: RwLock<BTreeMap<String, Tzif>>,
}

impl FsTzdbProvider {
    pub fn get(&self, identifier: &str) -> TemporalResult<Tzif> {
        if let Some(tzif) = self
            .cache
            .read()
            .map_err(|_| TemporalError::general("poisoned RWLock"))?
            .get(identifier)
        {
            return Ok(tzif.clone());
        }
        #[cfg(target_family = "unix")]
        let (identifier, tzif) = { (identifier, Tzif::read_tzif(identifier)?) };

        #[cfg(any(target_family = "windows", target_family = "wasm"))]
        let (identifier, tzif) = {
            let Some((canonical_name, data)) = jiff_tzdb::get(identifier) else {
                return Err(
                    TemporalError::range().with_message("Time zone identifier does not exist.")
                );
            };
            (canonical_name, Tzif::from_bytes(data)?)
        };

        Ok(self
            .cache
            .write()
            .map_err(|_| TemporalError::general("poisoned RWLock"))?
            .entry(identifier.into())
            .or_insert(tzif)
            .clone())
    }
}

impl TimeZoneProvider for FsTzdbProvider {
    fn normalize_identifier(&self, ident: &'_ [u8]) -> TemporalResult<Cow<'_, str>> {
        normalize_identifier_with_compiled(ident)
    }
    fn canonicalize_identifier(&self, ident: &'_ [u8]) -> TemporalResult<Cow<'_, str>> {
        canonicalize_identifier_with_compiled(ident)
    }

    fn get_named_tz_epoch_nanoseconds(
        &self,
        identifier: &str,
        local_datetime: IsoDateTime,
    ) -> TemporalResult<CandidateEpochNanoseconds> {
        self.get(identifier)?
            .get_named_tz_epoch_nanoseconds(local_datetime)
    }

    fn get_named_tz_offset_nanoseconds(
        &self,
        identifier: &str,
        utc_epoch: i128,
    ) -> TemporalResult<TimeZoneTransitionInfo> {
        self.get(identifier)?
            .get_named_tz_offset_nanoseconds(utc_epoch)
    }

    fn get_named_tz_transition(
        &self,
        identifier: &str,
        epoch_nanoseconds: i128,
        direction: TransitionDirection,
    ) -> TemporalResult<Option<EpochNanoseconds>> {
        let tzif = self.get(identifier)?;
        tzif.get_named_tz_transition(epoch_nanoseconds, direction)
    }
}

const NS_IN_S: i128 = 1_000_000_000;
#[inline]
fn seconds_to_nanoseconds(seconds: i64) -> i128 {
    seconds as i128 * NS_IN_S
}

#[cfg(test)]
mod tests {
    use tzif::data::time::Seconds;

    use crate::{
        builtins::calendar::CalendarFields,
        iso::{IsoDate, IsoDateTime, IsoTime},
        partial::PartialZonedDateTime,
        tzdb::{CompiledTzdbProvider, LocalTimeRecordResult, TimeZoneProvider, UtcOffsetSeconds},
        TimeZone, ZonedDateTime,
    };

    use super::{FsTzdbProvider, Tzif, SINGLETON_IANA_NORMALIZER};

    fn get_singleton_identifier(id: &str) -> Option<&'static str> {
        let index = SINGLETON_IANA_NORMALIZER.available_id_index.get(id)?;
        SINGLETON_IANA_NORMALIZER.normalized_identifiers.get(index)
    }

    #[test]
    fn test_singleton() {
        let id = get_singleton_identifier("uTc");
        assert_eq!(id, Some("UTC"));
        let id = get_singleton_identifier("EURope/BeRlIn").unwrap();
        assert_eq!(id, "Europe/Berlin");
    }

    #[test]
    fn available_ids() {
        let provider = FsTzdbProvider::default();
        assert!(provider.normalize_identifier(b"uTC").is_ok());
        assert!(provider.normalize_identifier(b"Etc/uTc").is_ok());
        assert!(provider.normalize_identifier(b"AMERIca/CHIcago").is_ok());
    }

    #[test]
    fn canonical_time_zone() {
        let provider = FsTzdbProvider::default();
        let valid_iana_identifiers = [
            ("AFRICA/Bissau", "Africa/Bissau", "-01:00"),
            ("America/Belem", "America/Belem", "-03:00"),
            ("Europe/Vienna", "Europe/Vienna", "+01:00"),
            ("America/New_York", "America/New_York", "-05:00"),
            ("Africa/CAIRO", "Africa/Cairo", "+02:00"),
            ("Asia/Ulan_Bator", "Asia/Ulan_Bator", "+07:00"),
            ("GMT", "GMT", "+00:00"),
            ("etc/gmt", "Etc/GMT", "+00:00"),
            (
                "1994-11-05T08:15:30-05:00[America/New_York]",
                "America/New_York",
                "-05:00",
            ),
            (
                "1994-11-05T08:15:30-05[America/Chicago]",
                "America/Chicago",
                "-06:00",
            ),
            ("EuROpe/DUBLIn", "Europe/Dublin", "+01:00"),
        ];

        for (valid_iana_identifier, canonical, offset) in valid_iana_identifiers {
            let time_zone =
                TimeZone::try_from_str_with_provider(valid_iana_identifier, &provider).unwrap();

            assert_eq!(time_zone.identifier(), canonical);
            let result = ZonedDateTime::from_partial_with_provider(
                PartialZonedDateTime::default()
                    .with_calendar_fields(
                        CalendarFields::new()
                            .with_year(1970)
                            .with_month(1)
                            .with_day(1),
                    )
                    .with_timezone(Some(time_zone)),
                None,
                None,
                None,
                &provider,
            )
            .unwrap();
            assert_eq!(result.offset_with_provider(&provider).unwrap(), offset);
        }
    }

    #[test]
    fn exactly_transition_time_after_empty_edge_case() {
        let provider = FsTzdbProvider::default();
        let date = crate::iso::IsoDate {
            year: 2017,
            month: 3,
            day: 12,
        };
        let time = crate::iso::IsoTime {
            hour: 3,
            minute: 0,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let today = IsoDateTime::new(date, time).unwrap();

        let local = provider
            .get_named_tz_epoch_nanoseconds("America/New_York", today)
            .unwrap();
        assert_eq!(local.len(), 1);
    }

    #[test]
    fn one_second_before_empty_edge_case() {
        let provider = FsTzdbProvider::default();
        let date = crate::iso::IsoDate {
            year: 2017,
            month: 3,
            day: 12,
        };
        let time = crate::iso::IsoTime {
            hour: 2,
            minute: 59,
            second: 59,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let today = IsoDateTime::new(date, time).unwrap();

        let local = provider
            .get_named_tz_epoch_nanoseconds("America/New_York", today)
            .unwrap();
        assert!(local.is_empty());
    }

    #[test]
    fn new_york_empty_test_case() {
        let date = crate::iso::IsoDate {
            year: 2017,
            month: 3,
            day: 12,
        };
        let time = crate::iso::IsoTime {
            hour: 2,
            minute: 30,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let edge_case = IsoDateTime::new(date, time).unwrap();
        let edge_case_seconds = (edge_case.as_nanoseconds().0 / 1_000_000_000) as i64;

        #[cfg(not(target_os = "windows"))]
        let new_york = Tzif::read_tzif("America/New_York");
        #[cfg(target_os = "windows")]
        let new_york = {
            let (_, data) = jiff_tzdb::get("America/New_York").unwrap();
            Tzif::from_bytes(data)
        };

        assert!(new_york.is_ok());
        let new_york = new_york.unwrap();

        let locals = new_york
            .v2_estimate_tz_pair(&Seconds(edge_case_seconds))
            .unwrap();
        assert!(matches!(locals, LocalTimeRecordResult::Empty(..)));
    }

    #[test]
    fn sydney_empty_test_case() {
        // Australia Daylight savings day
        let date = crate::iso::IsoDate {
            year: 2017,
            month: 10,
            day: 1,
        };
        let time = crate::iso::IsoTime {
            hour: 2,
            minute: 30,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let today = IsoDateTime::new(date, time).unwrap();
        let seconds = (today.as_nanoseconds().0 / 1_000_000_000) as i64;

        #[cfg(not(target_os = "windows"))]
        let sydney = Tzif::read_tzif("Australia/Sydney");
        #[cfg(target_os = "windows")]
        let sydney = {
            let (_, data) = jiff_tzdb::get("Australia/Sydney").unwrap();
            Tzif::from_bytes(data)
        };

        assert!(sydney.is_ok());
        let sydney = sydney.unwrap();

        let locals = sydney.v2_estimate_tz_pair(&Seconds(seconds)).unwrap();
        assert!(matches!(locals, LocalTimeRecordResult::Empty(..)));
    }

    #[test]
    fn new_york_duplicate_case() {
        // Moves from DST to STD
        let date = crate::iso::IsoDate {
            year: 2017,
            month: 11,
            day: 5,
        };
        let time = crate::iso::IsoTime {
            hour: 1,
            minute: 30,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let edge_case = IsoDateTime::new(date, time).unwrap();
        let edge_case_seconds = (edge_case.as_nanoseconds().0 / 1_000_000_000) as i64;

        #[cfg(not(target_os = "windows"))]
        let new_york = Tzif::read_tzif("America/New_York");
        #[cfg(target_os = "windows")]
        let new_york = {
            let (_, data) = jiff_tzdb::get("America/New_York").unwrap();
            Tzif::from_bytes(data)
        };

        assert!(new_york.is_ok());
        let new_york = new_york.unwrap();

        let locals = new_york
            .v2_estimate_tz_pair(&Seconds(edge_case_seconds))
            .unwrap();

        assert_eq!(
            locals,
            LocalTimeRecordResult::Ambiguous {
                // DST
                first: UtcOffsetSeconds(-14400),
                // STD
                second: UtcOffsetSeconds(-18000),
            }
        );
    }

    #[test]
    fn sydney_duplicate_case() {
        // Australia Daylight savings day
        // Moves from DST to STD
        let date = crate::iso::IsoDate {
            year: 2017,
            month: 4,
            day: 2,
        };
        let time = crate::iso::IsoTime {
            hour: 2,
            minute: 30,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let today = IsoDateTime::new(date, time).unwrap();
        let seconds = (today.as_nanoseconds().0 / 1_000_000_000) as i64;

        #[cfg(not(target_os = "windows"))]
        let sydney = Tzif::read_tzif("Australia/Sydney");
        #[cfg(target_os = "windows")]
        let sydney = {
            let (_, data) = jiff_tzdb::get("Australia/Sydney").unwrap();
            Tzif::from_bytes(data)
        };

        assert!(sydney.is_ok());
        let sydney = sydney.unwrap();

        let locals = sydney.v2_estimate_tz_pair(&Seconds(seconds)).unwrap();

        assert_eq!(
            locals,
            LocalTimeRecordResult::Ambiguous {
                // DST
                first: UtcOffsetSeconds(39600),
                // STD
                second: UtcOffsetSeconds(36000),
            }
        );
    }

    #[test]
    fn new_york_duplicate_with_slim_format() {
        let (_, data) = jiff_tzdb::get("America/New_York").unwrap();
        let new_york = Tzif::from_bytes(data);
        assert!(new_york.is_ok());
        let new_york = new_york.unwrap();

        let date = crate::iso::IsoDate {
            year: 2017,
            month: 11,
            day: 5,
        };
        let time = crate::iso::IsoTime {
            hour: 1,
            minute: 30,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let edge_case = IsoDateTime::new(date, time).unwrap();
        let edge_case_seconds = (edge_case.as_nanoseconds().0 / 1_000_000_000) as i64;

        let locals = new_york
            .v2_estimate_tz_pair(&Seconds(edge_case_seconds))
            .unwrap();

        assert_eq!(
            locals,
            LocalTimeRecordResult::Ambiguous {
                first: UtcOffsetSeconds(-14400),
                second: UtcOffsetSeconds(-18000),
            }
        );
    }

    #[test]
    fn sydney_duplicate_case_with_slim_format() {
        let (_, data) = jiff_tzdb::get("Australia/Sydney").unwrap();
        let sydney = Tzif::from_bytes(data);
        assert!(sydney.is_ok());
        let sydney = sydney.unwrap();

        // Australia Daylight savings day
        let date = crate::iso::IsoDate {
            year: 2017,
            month: 4,
            day: 2,
        };
        let time = crate::iso::IsoTime {
            hour: 2,
            minute: 30,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let today = IsoDateTime::new(date, time).unwrap();
        let seconds = (today.as_nanoseconds().0 / 1_000_000_000) as i64;

        let locals = sydney.v2_estimate_tz_pair(&Seconds(seconds)).unwrap();

        assert_eq!(
            locals,
            LocalTimeRecordResult::Ambiguous {
                first: UtcOffsetSeconds(39600),
                second: UtcOffsetSeconds(36000),
            }
        );
    }

    // TODO: Determine the validity of this test. Primarily, this test
    // goes beyond the regularly historic limit of transition_times, so
    // even when on a DST boundary the first time zone is returned. The
    // question is whether this behavior is consistent with what would
    // be expected.
    #[test]
    fn before_epoch_northern_hemisphere() {
        let date = crate::iso::IsoDate {
            year: 1880,
            month: 11,
            day: 5,
        };
        let time = crate::iso::IsoTime {
            hour: 1,
            minute: 30,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let edge_case = IsoDateTime::new(date, time).unwrap();
        let edge_case_seconds = (edge_case.as_nanoseconds().0 / 1_000_000_000) as i64;

        #[cfg(not(target_os = "windows"))]
        let new_york = Tzif::read_tzif("America/New_York");
        #[cfg(target_os = "windows")]
        let new_york = {
            let (_, data) = jiff_tzdb::get("America/New_York").unwrap();
            Tzif::from_bytes(data)
        };

        assert!(new_york.is_ok());
        let new_york = new_york.unwrap();

        let locals = new_york
            .v2_estimate_tz_pair(&Seconds(edge_case_seconds))
            .unwrap();

        assert!(matches!(locals, LocalTimeRecordResult::Single(_)));
    }

    // TODO: Determine the validity of this test. Primarily, this test
    // goes beyond the regularly historic limit of transition_times, so
    // even when on a DST boundary the first time zone is returned. The
    // question is whether this behavior is consistent with what would
    // be expected.
    #[test]
    fn before_epoch_southern_hemisphere() {
        // Australia Daylight savings day
        let date = crate::iso::IsoDate {
            year: 1880,
            month: 4,
            day: 2,
        };
        let time = crate::iso::IsoTime {
            hour: 2,
            minute: 30,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let today = IsoDateTime::new(date, time).unwrap();
        let seconds = (today.as_nanoseconds().0 / 1_000_000_000) as i64;

        #[cfg(not(target_os = "windows"))]
        let sydney = Tzif::read_tzif("Australia/Sydney");
        #[cfg(target_os = "windows")]
        let sydney = {
            let (_, data) = jiff_tzdb::get("Australia/Sydney").unwrap();
            Tzif::from_bytes(data)
        };

        assert!(sydney.is_ok());
        let sydney = sydney.unwrap();

        let locals = sydney.v2_estimate_tz_pair(&Seconds(seconds)).unwrap();
        assert!(matches!(locals, LocalTimeRecordResult::Single(_)));
    }

    #[test]
    #[cfg(not(target_os = "windows"))]
    fn mwd_transition_epoch() {
        let tzif = Tzif::read_tzif("Europe/Berlin").unwrap();

        let start_date = crate::iso::IsoDate {
            year: 2028,
            month: 3,
            day: 30,
        };
        let start_time = crate::iso::IsoTime {
            hour: 6,
            minute: 0,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let start_dt = IsoDateTime::new(start_date, start_time).unwrap();
        let start_dt_secs = (start_dt.as_nanoseconds().0 / 1_000_000_000) as i64;

        let start_seconds = &Seconds(start_dt_secs);

        assert_eq!(
            tzif.get(start_seconds).unwrap().transition_epoch.unwrap(),
            // Sun, Mar 26 at 2:00 am
            1837645200
        );

        let end_date = crate::iso::IsoDate {
            year: 2028,
            month: 10,
            day: 29,
        };
        let end_time = crate::iso::IsoTime {
            hour: 6,
            minute: 0,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let end_dt = IsoDateTime::new(end_date, end_time).unwrap();
        let end_dt_secs = (end_dt.as_nanoseconds().0 / 1_000_000_000) as i64;

        let end_seconds = &Seconds(end_dt_secs);

        assert_eq!(
            tzif.get(end_seconds).unwrap().transition_epoch.unwrap(),
            // Sun, Oct 29 at 3:00 am
            1856394000
        );
    }

    #[test]
    fn compiled_mwd_transition_epoch() {
        let tzif = CompiledTzdbProvider::default()
            .get("Europe/Berlin")
            .unwrap();

        let start_date = crate::iso::IsoDate {
            year: 2028,
            month: 3,
            day: 30,
        };
        let start_time = crate::iso::IsoTime {
            hour: 6,
            minute: 0,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let start_dt = IsoDateTime::new(start_date, start_time).unwrap();
        let start_dt_secs = (start_dt.as_nanoseconds().0 / 1_000_000_000) as i64;

        let start_seconds = &Seconds(start_dt_secs);

        assert_eq!(
            tzif.get(start_seconds).unwrap().transition_epoch.unwrap(),
            // Sun, Mar 26 at 2:00 am
            1837645200
        );

        let end_date = crate::iso::IsoDate {
            year: 2028,
            month: 10,
            day: 29,
        };
        let end_time = crate::iso::IsoTime {
            hour: 6,
            minute: 0,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        };
        let end_dt = IsoDateTime::new(end_date, end_time).unwrap();
        let end_dt_secs = (end_dt.as_nanoseconds().0 / 1_000_000_000) as i64;

        let end_seconds = &Seconds(end_dt_secs);

        assert_eq!(
            tzif.get(end_seconds).unwrap().transition_epoch.unwrap(),
            // Sun, Oct 29 at 3:00 am
            1856394000
        );
    }

    // This test mimicks the operations present in `temporal_rs`'s `disambiguate_possible_epoch_nanoseconds`
    #[test]
    fn disambiguate_ambiguous_posix_time() {
        let provider = CompiledTzdbProvider::default();

        fn run_disambiguation_logic(
            before: IsoDateTime,
            after: IsoDateTime,
            id: &str,
            before_offset: i64,
            after_offset: i64,
            provider: &impl TimeZoneProvider,
        ) {
            let before_possible = provider.get_named_tz_epoch_nanoseconds(id, before).unwrap();
            assert_eq!(before_possible.len(), 1);

            let after_possible = provider.get_named_tz_epoch_nanoseconds(id, after).unwrap();
            assert_eq!(after_possible.len(), 1);
            let before_seconds = before_possible.first().unwrap();
            let after_seconds = after_possible.first().unwrap();

            let before_transition = provider
                .get_named_tz_offset_nanoseconds(id, before_seconds.0)
                .unwrap();
            let after_transition = provider
                .get_named_tz_offset_nanoseconds(id, after_seconds.0)
                .unwrap();
            assert_ne!(
                before_transition, after_transition,
                "Transition info must not be the same"
            );
            assert_eq!(after_transition.offset.0, after_offset);
            assert_eq!(before_transition.offset.0, before_offset);
        }

        // Test Northern hemisphere
        let before = IsoDateTime::new_unchecked(
            IsoDate::new_unchecked(2020, 3, 7),
            IsoTime::new_unchecked(23, 30, 0, 0, 0, 0),
        );
        let after = IsoDateTime::new_unchecked(
            IsoDate::new_unchecked(2020, 3, 8),
            IsoTime::new_unchecked(5, 30, 0, 0, 0, 0),
        );
        run_disambiguation_logic(
            before,
            after,
            "America/Los_Angeles",
            -28_800,
            -25_200,
            &provider,
        );

        // Test southern hemisphere
        let before = IsoDateTime::new_unchecked(
            IsoDate::new_unchecked(2020, 4, 4),
            IsoTime::new_unchecked(23, 30, 0, 0, 0, 0),
        );
        let after = IsoDateTime::new_unchecked(
            IsoDate::new_unchecked(2020, 4, 5),
            IsoTime::new_unchecked(5, 30, 0, 0, 0, 0),
        );
        run_disambiguation_logic(before, after, "Australia/Sydney", 39_600, 36_000, &provider);
    }
}
