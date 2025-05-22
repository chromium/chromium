//! This module implements `MonthDay` and any directly related algorithms.

use alloc::string::String;
use core::str::FromStr;

use crate::{
    iso::IsoDate,
    options::{ArithmeticOverflow, DisplayCalendar},
    parsers::{FormattableCalendar, FormattableDate, FormattableMonthDay},
    Calendar, MonthCode, TemporalError, TemporalResult, TemporalUnwrap,
};

use super::{PartialDate, PlainDate};

/// The native Rust implementation of `Temporal.PlainMonthDay`
#[non_exhaustive]
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct PlainMonthDay {
    pub iso: IsoDate,
    calendar: Calendar,
}

impl core::fmt::Display for PlainMonthDay {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str(&self.to_ixdtf_string(DisplayCalendar::Auto))
    }
}

impl PlainMonthDay {
    /// Creates a new unchecked `PlainMonthDay`
    #[inline]
    #[must_use]
    pub(crate) fn new_unchecked(iso: IsoDate, calendar: Calendar) -> Self {
        Self { iso, calendar }
    }

    /// Creates a new valid `PlainMonthDay`.
    #[inline]
    pub fn new_with_overflow(
        month: u8,
        day: u8,
        calendar: Calendar,
        overflow: ArithmeticOverflow,
        ref_year: Option<i32>,
    ) -> TemporalResult<Self> {
        let ry = ref_year.unwrap_or(1972);
        // 1972 is the first leap year in the Unix epoch (needed to cover all dates)
        let iso = IsoDate::new_with_overflow(ry, month, day, overflow)?;
        Ok(Self::new_unchecked(iso, calendar))
    }

    // Converts a UTF-8 encoded string into a `PlainMonthDay`.
    pub fn from_utf8(s: &[u8]) -> TemporalResult<Self> {
        let record = crate::parsers::parse_month_day(s)?;

        let calendar = record
            .calendar
            .map(Calendar::try_from_utf8)
            .transpose()?
            .unwrap_or_default();

        // ParseISODateTime
        // Step 4.a.ii.3
        // If goal is TemporalMonthDayString or TemporalYearMonthString, calendar is
        // not empty, and the ASCII-lowercase of calendar is not "iso8601", throw a
        // RangeError exception.
        if !calendar.is_iso() {
            return Err(TemporalError::range().with_message("non-ISO calendar not supported."));
        }

        let date = record.date;

        let date = date.temporal_unwrap()?;

        Self::new_with_overflow(
            date.month,
            date.day,
            calendar,
            ArithmeticOverflow::Reject,
            None,
        )
    }

    pub fn with(
        &self,
        _partial: PartialDate,
        _overflow: ArithmeticOverflow,
    ) -> TemporalResult<Self> {
        Err(TemporalError::general("Not yet implemented."))
    }

    /// Returns the ISO day value of `PlainMonthDay`.
    #[inline]
    #[must_use]
    pub fn iso_day(&self) -> u8 {
        self.iso.day
    }

    // Returns the ISO month value of `PlainMonthDay`.
    #[inline]
    #[must_use]
    pub fn iso_month(&self) -> u8 {
        self.iso.month
    }

    // Returns the ISO year value of `PlainMonthDay`.
    #[inline]
    #[must_use]
    pub fn iso_year(&self) -> i32 {
        self.iso.year
    }

    /// Returns the string identifier for the current `Calendar`.
    #[inline]
    #[must_use]
    pub fn calendar_id(&self) -> &'static str {
        self.calendar.identifier()
    }

    /// Returns a reference to `PlainMonthDay`'s inner `Calendar`.
    #[inline]
    #[must_use]
    pub fn calendar(&self) -> &Calendar {
        &self.calendar
    }

    /// Returns the calendar `monthCode` value of `PlainMonthDay`.
    #[inline]
    pub fn month_code(&self) -> MonthCode {
        self.calendar.month_code(&self.iso)
    }

    /// Returns the calendar day value of `PlainMonthDay`.
    #[inline]
    pub fn day(&self) -> u8 {
        self.calendar.day(&self.iso)
    }

    pub fn to_plain_date(&self) -> TemporalResult<PlainDate> {
        Err(TemporalError::general("Not yet implemented"))
    }

    pub fn to_ixdtf_string(&self, display_calendar: DisplayCalendar) -> String {
        let ixdtf = FormattableMonthDay {
            date: FormattableDate(self.iso_year(), self.iso_month(), self.iso.day),
            calendar: FormattableCalendar {
                show: display_calendar,
                calendar: self.calendar().identifier(),
            },
        };
        ixdtf.to_string()
    }
}

impl FromStr for PlainMonthDay {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::from_utf8(s.as_bytes())
    }
}
