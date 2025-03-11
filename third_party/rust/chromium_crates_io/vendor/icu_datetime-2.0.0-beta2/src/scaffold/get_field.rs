// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_calendar::{
    types::{DayOfMonth, DayOfYearInfo, MonthInfo, Weekday, YearInfo},
    AsCalendar, Calendar, Date, Iso,
};
use icu_time::{
    zone::{models::TimeZoneModel, TimeZoneVariant, UtcOffset},
    DateTime, Hour, Minute, Nanosecond, Second, Time, TimeZone, TimeZoneInfo, ZonedDateTime,
};

use super::UnstableSealed;

/// A type that can return a certain field `T`.
///
/// This is used as a bound on various datetime functions.
///
/// <div class="stab unstable">
/// 🚧 This trait is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not implement this trait in userland unless you are prepared for things to occasionally break.
/// </div>
pub trait GetField<T>: UnstableSealed {
    /// Returns the value of this trait's field `T`.
    fn get_field(&self) -> T;
}

impl<T> GetField<T> for T
where
    T: Copy + UnstableSealed,
{
    #[inline]
    fn get_field(&self) -> T {
        *self
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> UnstableSealed for Date<A> {}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<YearInfo> for Date<A> {
    #[inline]
    fn get_field(&self) -> YearInfo {
        self.year()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<MonthInfo> for Date<A> {
    #[inline]
    fn get_field(&self) -> MonthInfo {
        self.month()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<DayOfMonth> for Date<A> {
    #[inline]
    fn get_field(&self) -> DayOfMonth {
        self.day_of_month()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<Weekday> for Date<A> {
    #[inline]
    fn get_field(&self) -> Weekday {
        self.day_of_week()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<DayOfYearInfo> for Date<A> {
    #[inline]
    fn get_field(&self) -> DayOfYearInfo {
        self.day_of_year_info()
    }
}

impl UnstableSealed for Time {}

impl GetField<Hour> for Time {
    #[inline]
    fn get_field(&self) -> Hour {
        self.hour
    }
}

impl GetField<Minute> for Time {
    #[inline]
    fn get_field(&self) -> Minute {
        self.minute
    }
}

impl GetField<Second> for Time {
    #[inline]
    fn get_field(&self) -> Second {
        self.second
    }
}

impl GetField<Nanosecond> for Time {
    #[inline]
    fn get_field(&self) -> Nanosecond {
        self.subsecond
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> UnstableSealed for DateTime<A> {}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<YearInfo> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> YearInfo {
        self.date.year()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<MonthInfo> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> MonthInfo {
        self.date.month()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<DayOfMonth> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> DayOfMonth {
        self.date.day_of_month()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<Weekday> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> Weekday {
        self.date.day_of_week()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<DayOfYearInfo> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> DayOfYearInfo {
        self.date.day_of_year_info()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<Hour> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> Hour {
        self.time.hour
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<Minute> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> Minute {
        self.time.minute
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<Second> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> Second {
        self.time.second
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<Nanosecond> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> Nanosecond {
        self.time.subsecond
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> UnstableSealed for ZonedDateTime<A, Z> {}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<YearInfo> for ZonedDateTime<A, Z> {
    #[inline]
    fn get_field(&self) -> YearInfo {
        self.date.year()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<MonthInfo> for ZonedDateTime<A, Z> {
    #[inline]
    fn get_field(&self) -> MonthInfo {
        self.date.month()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<DayOfMonth> for ZonedDateTime<A, Z> {
    #[inline]
    fn get_field(&self) -> DayOfMonth {
        self.date.day_of_month()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<Weekday> for ZonedDateTime<A, Z> {
    #[inline]
    fn get_field(&self) -> Weekday {
        self.date.day_of_week()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<DayOfYearInfo> for ZonedDateTime<A, Z> {
    #[inline]
    fn get_field(&self) -> DayOfYearInfo {
        self.date.day_of_year_info()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<Hour> for ZonedDateTime<A, Z> {
    #[inline]
    fn get_field(&self) -> Hour {
        self.time.hour
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<Minute> for ZonedDateTime<A, Z> {
    #[inline]
    fn get_field(&self) -> Minute {
        self.time.minute
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<Second> for ZonedDateTime<A, Z> {
    #[inline]
    fn get_field(&self) -> Second {
        self.time.second
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<Nanosecond> for ZonedDateTime<A, Z> {
    #[inline]
    fn get_field(&self) -> Nanosecond {
        self.time.subsecond
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<Option<UtcOffset>>
    for ZonedDateTime<A, Z>
where
    Z: GetField<Option<UtcOffset>>,
{
    #[inline]
    fn get_field(&self) -> Option<UtcOffset> {
        self.zone.get_field()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<TimeZone> for ZonedDateTime<A, Z>
where
    Z: GetField<TimeZone>,
{
    #[inline]
    fn get_field(&self) -> TimeZone {
        self.zone.get_field()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<TimeZoneVariant> for ZonedDateTime<A, Z>
where
    Z: GetField<TimeZoneVariant>,
{
    #[inline]
    fn get_field(&self) -> TimeZoneVariant {
        self.zone.get_field()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<(Date<Iso>, Time)>
    for ZonedDateTime<A, Z>
where
    Z: GetField<(Date<Iso>, Time)>,
{
    #[inline]
    fn get_field(&self) -> (Date<Iso>, Time) {
        self.zone.get_field()
    }
}

impl UnstableSealed for UtcOffset {}

impl GetField<Option<UtcOffset>> for UtcOffset {
    #[inline]
    fn get_field(&self) -> Option<UtcOffset> {
        Some(*self)
    }
}

impl<O: TimeZoneModel> UnstableSealed for TimeZoneInfo<O> {}

impl<O> GetField<TimeZone> for TimeZoneInfo<O>
where
    O: TimeZoneModel,
{
    #[inline]
    fn get_field(&self) -> TimeZone {
        self.time_zone_id()
    }
}

impl<O> GetField<Option<UtcOffset>> for TimeZoneInfo<O>
where
    O: TimeZoneModel,
{
    #[inline]
    fn get_field(&self) -> Option<UtcOffset> {
        self.offset()
    }
}

impl<O> GetField<TimeZoneVariant> for TimeZoneInfo<O>
where
    O: TimeZoneModel<TimeZoneVariant = TimeZoneVariant>,
{
    #[inline]
    fn get_field(&self) -> TimeZoneVariant {
        self.zone_variant()
    }
}

impl<O> GetField<(Date<Iso>, Time)> for TimeZoneInfo<O>
where
    O: TimeZoneModel<LocalTime = (Date<Iso>, Time)>,
{
    #[inline]
    fn get_field(&self) -> (Date<Iso>, Time) {
        self.local_time()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<()> for Date<A> {
    #[inline]
    fn get_field(&self) {}
}

impl GetField<()> for Time {
    #[inline]
    fn get_field(&self) {}
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<()> for DateTime<A> {
    #[inline]
    fn get_field(&self) {}
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<()> for ZonedDateTime<A, Z> {
    #[inline]
    fn get_field(&self) {}
}

impl GetField<()> for UtcOffset {
    #[inline]
    fn get_field(&self) {}
}

impl<O: TimeZoneModel> GetField<()> for TimeZoneInfo<O> {
    #[inline]
    fn get_field(&self) {}
}
