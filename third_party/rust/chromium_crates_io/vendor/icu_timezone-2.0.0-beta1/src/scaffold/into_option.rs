// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::{TimeZoneBcp47Id, UtcOffset, ZoneVariant};
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

impl IntoOption<IsoWeekday> for IsoWeekday {
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

impl IntoOption<IsoHour> for IsoHour {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<IsoMinute> for IsoMinute {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<IsoSecond> for IsoSecond {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<NanoSecond> for NanoSecond {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

impl IntoOption<TimeZoneBcp47Id> for TimeZoneBcp47Id {
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

impl IntoOption<ZoneVariant> for ZoneVariant {
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
