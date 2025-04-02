// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::{
    zone::{TimeZoneVariant, UtcOffset},
    Hour, Minute, Nanosecond, Second, Time, TimeZone,
};
use icu_calendar::{types::*, AnyCalendarKind, Date, Iso};

/// Converts Self to an `Option<T>`, either `Some(T)` if able or `None`
pub trait IntoOption<T> {
    /// Return `self` as an `Option<T>`
    fn into_option(self) -> Option<T>;
}

impl<T> IntoOption<T> for Option<T> {
    #[inline]
    fn into_option(self) -> Option<T> {
        self
    }
}

impl<T> IntoOption<T> for () {
    #[inline]
    fn into_option(self) -> Option<T> {
        None
    }
}

impl IntoOption<YearInfo> for YearInfo {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<MonthInfo> for MonthInfo {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<DayOfMonth> for DayOfMonth {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<Weekday> for Weekday {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<DayOfYearInfo> for DayOfYearInfo {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<AnyCalendarKind> for AnyCalendarKind {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<Hour> for Hour {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<Minute> for Minute {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<Second> for Second {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<Nanosecond> for Nanosecond {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<TimeZone> for TimeZone {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<UtcOffset> for UtcOffset {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<TimeZoneVariant> for TimeZoneVariant {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<(Date<Iso>, Time)> for (Date<Iso>, Time) {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}
