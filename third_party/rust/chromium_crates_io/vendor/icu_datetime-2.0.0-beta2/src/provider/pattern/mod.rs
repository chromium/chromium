// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Structured datetime pattern types for datagen and the data provider.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>

mod common;
mod error;
mod hour_cycle;
mod item;
pub mod reference;
pub mod runtime;

use crate::provider::fields;
pub use error::PatternError;
#[cfg(feature = "datagen")]
pub(crate) use hour_cycle::naively_apply_preferences;
pub use hour_cycle::CoarseHourCycle;
use icu_provider::prelude::*;
pub use item::{GenericPatternItem, PatternItem};

/// The granularity of time represented in a [`Pattern`](runtime::Pattern).
/// Ordered from least granular to most granular for comparison.
#[derive(
    Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, yoke::Yokeable, zerofrom::ZeroFrom,
)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::pattern))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[non_exhaustive]
pub enum TimeGranularity {
    /// No time is in the pattern.
    None,
    /// Smallest time unit = hours.
    Hours,
    /// Smallest time unit = minutes.
    Minutes,
    /// Smallest time unit = seconds.
    Seconds,
    /// Smallest time unit = Nanoseconds.
    Nanoseconds,
}

impl Default for TimeGranularity {
    fn default() -> Self {
        Self::None
    }
}

impl TimeGranularity {
    /// Returns [`true`] if the most granular time being displayed will align with
    /// the top of the hour, otherwise returns [`false`].
    /// e.g. `12:00:00` is at the top of the hour for any display granularity.
    /// e.g. `12:00:05` is only at the top of the hour if the seconds are not displayed.
    pub fn is_top_of_hour(self, minute: u8, second: u8, subsecond: u32) -> bool {
        match self {
            Self::None | Self::Hours => true,
            Self::Minutes => minute == 0,
            Self::Seconds => minute == 0 && second == 0,
            Self::Nanoseconds => minute == 0 && second == 0 && subsecond == 0,
        }
    }

    #[inline]
    pub(crate) fn from_ordinal(ordinal: u8) -> TimeGranularity {
        use TimeGranularity::*;
        match ordinal {
            1 => Hours,
            2 => Minutes,
            3 => Seconds,
            4 => Nanoseconds,
            _ => None,
        }
    }

    #[inline]
    pub(crate) const fn ordinal(self) -> u8 {
        use TimeGranularity::*;
        match self {
            None => 0,
            Hours => 1,
            Minutes => 2,
            Seconds => 3,
            Nanoseconds => 4,
        }
    }
}

impl From<PatternItem> for TimeGranularity {
    /// Retrieves the granularity of time represented by a [`PatternItem`].
    /// If the [`PatternItem`] is not time-related, returns [`None`].
    fn from(item: PatternItem) -> Self {
        match item {
            PatternItem::Field(field) => match field.symbol {
                fields::FieldSymbol::Hour(_) => Self::Hours,
                fields::FieldSymbol::Minute => Self::Minutes,
                fields::FieldSymbol::Second(_) => Self::Seconds,
                fields::FieldSymbol::DecimalSecond(_) => Self::Nanoseconds,
                _ => Self::None,
            },
            _ => Self::None,
        }
    }
}
