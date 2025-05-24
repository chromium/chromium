//! The `TimeZoneProvider` trait.

use core::str::FromStr;

use crate::{iso::IsoDateTime, unix_time::EpochNanoseconds, TemporalResult};
use alloc::vec::Vec;

/// `TimeZoneOffset` represents the number of seconds to be added to UT in order to determine local time.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TimeZoneOffset {
    /// The transition time epoch at which the offset needs to be applied.
    pub transition_epoch: Option<i64>,
    /// The time zone offset in seconds.
    pub offset: i64,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum TransitionDirection {
    Next,
    Previous,
}

#[derive(Debug, Clone, Copy)]
pub struct ParseDirectionError;

impl core::fmt::Display for ParseDirectionError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str("provided string was not a valid direction.")
    }
}

impl FromStr for TransitionDirection {
    type Err = ParseDirectionError;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "next" => Ok(Self::Next),
            "previous" => Ok(Self::Previous),
            _ => Err(ParseDirectionError),
        }
    }
}

impl core::fmt::Display for TransitionDirection {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::Next => "next",
            Self::Previous => "previous",
        }
        .fmt(f)
    }
}

// NOTE: It may be a good idea to eventually move this into it's
// own individual crate rather than having it tied directly into `temporal_rs`
/// The `TimeZoneProvider` trait provides methods required for a provider
/// to implement in order to source time zone data from that provider.
pub trait TimeZoneProvider {
    fn check_identifier(&self, identifier: &str) -> bool;

    fn get_named_tz_epoch_nanoseconds(
        &self,
        identifier: &str,
        local_datetime: IsoDateTime,
    ) -> TemporalResult<Vec<EpochNanoseconds>>;

    fn get_named_tz_offset_nanoseconds(
        &self,
        identifier: &str,
        epoch_nanoseconds: i128,
    ) -> TemporalResult<TimeZoneOffset>;

    // TODO: implement and stabalize
    fn get_named_tz_transition(
        &self,
        identifier: &str,
        epoch_nanoseconds: i128,
        direction: TransitionDirection,
    ) -> TemporalResult<Option<EpochNanoseconds>>;
}

pub struct NeverProvider;

impl TimeZoneProvider for NeverProvider {
    fn check_identifier(&self, _: &str) -> bool {
        unimplemented!()
    }

    fn get_named_tz_epoch_nanoseconds(
        &self,
        _: &str,
        _: IsoDateTime,
    ) -> TemporalResult<Vec<EpochNanoseconds>> {
        unimplemented!()
    }

    fn get_named_tz_offset_nanoseconds(&self, _: &str, _: i128) -> TemporalResult<TimeZoneOffset> {
        unimplemented!()
    }

    fn get_named_tz_transition(
        &self,
        _: &str,
        _: i128,
        _: TransitionDirection,
    ) -> TemporalResult<Option<EpochNanoseconds>> {
        unimplemented!()
    }
}
