//! The `temporal_rs` crate is an implementation of ECMAScript's Temporal
//! built-in objects in Rust.
//!
//! ```rust
//! use temporal_rs::{PlainDate, Calendar};
//! use tinystr::tinystr;
//! use core::str::FromStr;
//!
//! // Create a date with an ISO calendar
//! let iso8601_date = PlainDate::try_new(2025, 3, 3, Calendar::default()).unwrap();
//!
//! // Create a new date with the japanese calendar
//! let japanese_date = iso8601_date.with_calendar(Calendar::from_str("japanese").unwrap()).unwrap();
//! assert_eq!(japanese_date.era(), Some(tinystr!(16, "reiwa")));
//! assert_eq!(japanese_date.era_year(), Some(7));
//! assert_eq!(japanese_date.month(), 3)
//! ```
//!
//! [`Temporal`][proposal] is the Stage 3 proposal for ECMAScript that
//! provides new JS objects and functions for working with dates and
//! times that fully supports time zones and non-gregorian calendars.
//!
//! This library's primary source is the Temporal Proposal
//! [specification][spec].
//!
//! [proposal]: https://github.com/tc39/proposal-temporal
//! [spec]: https://tc39.es/proposal-temporal/
#![doc(
    html_logo_url = "https://raw.githubusercontent.com/boa-dev/boa/main/assets/logo.svg",
    html_favicon_url = "https://raw.githubusercontent.com/boa-dev/boa/main/assets/logo.svg"
)]
#![no_std]
#![cfg_attr(not(test), forbid(clippy::unwrap_used))]
#![allow(
    // Currently throws a false positive regarding dependencies that are only used in benchmarks.
    unused_crate_dependencies,
    clippy::module_name_repetitions,
    clippy::redundant_pub_crate,
    clippy::too_many_lines,
    clippy::cognitive_complexity,
    clippy::missing_errors_doc,
    clippy::let_unit_value,
    clippy::option_if_let_else,

    // It may be worth to look if we can fix the issues highlighted by these lints.
    clippy::cast_possible_truncation,
    clippy::cast_sign_loss,
    clippy::cast_precision_loss,
    clippy::cast_possible_wrap,

    // Add temporarily - Needs addressing
    clippy::missing_panics_doc,
)]

extern crate alloc;
extern crate core;

#[cfg(feature = "std")]
extern crate std;

pub mod error;
pub mod iso;
pub mod options;
pub mod parsers;
pub mod primitive;
pub mod provider;

#[cfg(feature = "sys")]
pub(crate) mod sys;

mod builtins;
mod epoch_nanoseconds;

#[cfg(feature = "tzdb")]
pub mod tzdb;

#[doc(hidden)]
pub(crate) mod rounding;
#[doc(hidden)]
pub(crate) mod utils;

use core::cmp::Ordering;

// TODO: evaluate positives and negatives of using tinystr. Re-exporting
// tinystr as a convenience, as it is currently tied into the API.
/// Re-export of `TinyAsciiStr` from `tinystr`.
pub use tinystr::TinyAsciiStr;

#[doc(inline)]
pub use error::TemporalError;

/// The `Temporal` result type
pub type TemporalResult<T> = Result<T, TemporalError>;

pub mod partial {
    //! Partial Date/Time component records.
    //!
    //! The partial records are `temporal_rs`'s method of addressing
    //! `TemporalFields` in the specification.
    pub use crate::builtins::{
        core::PartialDuration, PartialDate, PartialDateTime, PartialTime, PartialZonedDateTime,
    };
}

// TODO: Potentially bikeshed how `EpochNanoseconds` should be exported.
pub mod time {
    pub use crate::epoch_nanoseconds::EpochNanoseconds;
}

pub use crate::builtins::{
    calendar::{Calendar, MonthCode},
    core::timezone::{TimeZone, UtcOffset},
    DateDuration, Duration, Instant, Now, PlainDate, PlainDateTime, PlainMonthDay, PlainTime,
    PlainYearMonth, TimeDuration, ZonedDateTime,
};

/// A library specific trait for unwrapping assertions.
pub(crate) trait TemporalUnwrap {
    type Output;

    /// `temporal_rs` based assertion for unwrapping. This will panic in
    /// debug builds, but throws error during runtime.
    fn temporal_unwrap(self) -> TemporalResult<Self::Output>;
}

impl<T> TemporalUnwrap for Option<T> {
    type Output = T;

    fn temporal_unwrap(self) -> TemporalResult<Self::Output> {
        debug_assert!(self.is_some());
        self.ok_or(TemporalError::assert())
    }
}

#[doc(hidden)]
#[macro_export]
macro_rules! temporal_assert {
    ($condition:expr $(,)*) => {
        if !$condition {
            return Err(TemporalError::assert());
        }
    };
    ($condition:expr, $($args:tt)+) => {
        if !$condition {
            #[cfg(feature = "log")]
            log::error!($($args)+);
            return Err(TemporalError::assert());
        }
    };
}

// TODO: Determine final home or leave in the top level? ops::Sign,
// types::Sign, etc.
/// A general Sign type.
#[repr(i8)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum Sign {
    #[default]
    Positive = 1,
    Zero = 0,
    Negative = -1,
}

impl From<i8> for Sign {
    fn from(value: i8) -> Self {
        match value.cmp(&0) {
            Ordering::Greater => Self::Positive,
            Ordering::Equal => Self::Zero,
            Ordering::Less => Self::Negative,
        }
    }
}

impl Sign {
    /// Coerces the current `Sign` to be either negative or positive.
    pub(crate) fn as_sign_multiplier(&self) -> i8 {
        if matches!(self, Self::Zero) {
            return 1;
        }
        *self as i8
    }

    pub(crate) fn negate(&self) -> Sign {
        Sign::from(-(*self as i8))
    }
}

// Relevant numeric constants
/// Nanoseconds per day constant: 8.64e+13
pub const NS_PER_DAY: u64 = MS_PER_DAY as u64 * 1_000_000;
/// Milliseconds per day constant: 8.64e+7
pub const MS_PER_DAY: u32 = 24 * 60 * 60 * 1000;
/// Max Instant nanosecond constant
#[doc(hidden)]
pub(crate) const NS_MAX_INSTANT: i128 = NS_PER_DAY as i128 * 100_000_000i128;
/// Min Instant nanosecond constant
#[doc(hidden)]
pub(crate) const NS_MIN_INSTANT: i128 = -NS_MAX_INSTANT;
