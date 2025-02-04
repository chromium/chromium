// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_calendar::{
    types::{
        DayOfMonth, DayOfYearInfo, IsoHour, IsoMinute, IsoSecond, IsoWeekday, MonthInfo,
        NanoSecond, YearInfo,
    },
    AsCalendar, Calendar, Date, DateTime, Iso, Time,
};
use icu_timezone::{
    CustomZonedDateTime, TimeZoneBcp47Id, TimeZoneInfo, TimeZoneModel, UtcOffset, ZoneVariant,
};

use super::UnstableSealed;

/// A type that can return a certain field `T`.
///
/// This is used as a bound on various datetime functions.
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

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<IsoWeekday> for Date<A> {
    #[inline]
    fn get_field(&self) -> IsoWeekday {
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

impl GetField<IsoHour> for Time {
    #[inline]
    fn get_field(&self) -> IsoHour {
        self.hour
    }
}

impl GetField<IsoMinute> for Time {
    #[inline]
    fn get_field(&self) -> IsoMinute {
        self.minute
    }
}

impl GetField<IsoSecond> for Time {
    #[inline]
    fn get_field(&self) -> IsoSecond {
        self.second
    }
}

impl GetField<NanoSecond> for Time {
    #[inline]
    fn get_field(&self) -> NanoSecond {
        self.nanosecond
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

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<IsoWeekday> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> IsoWeekday {
        self.date.day_of_week()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<DayOfYearInfo> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> DayOfYearInfo {
        self.date.day_of_year_info()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<IsoHour> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> IsoHour {
        self.time.hour
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<IsoMinute> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> IsoMinute {
        self.time.minute
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<IsoSecond> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> IsoSecond {
        self.time.second
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> GetField<NanoSecond> for DateTime<A> {
    #[inline]
    fn get_field(&self) -> NanoSecond {
        self.time.nanosecond
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> UnstableSealed for CustomZonedDateTime<A, Z> {}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<YearInfo> for CustomZonedDateTime<A, Z> {
    #[inline]
    fn get_field(&self) -> YearInfo {
        self.date.year()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<MonthInfo>
    for CustomZonedDateTime<A, Z>
{
    #[inline]
    fn get_field(&self) -> MonthInfo {
        self.date.month()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<DayOfMonth>
    for CustomZonedDateTime<A, Z>
{
    #[inline]
    fn get_field(&self) -> DayOfMonth {
        self.date.day_of_month()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<IsoWeekday>
    for CustomZonedDateTime<A, Z>
{
    #[inline]
    fn get_field(&self) -> IsoWeekday {
        self.date.day_of_week()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<DayOfYearInfo>
    for CustomZonedDateTime<A, Z>
{
    #[inline]
    fn get_field(&self) -> DayOfYearInfo {
        self.date.day_of_year_info()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<IsoHour> for CustomZonedDateTime<A, Z> {
    #[inline]
    fn get_field(&self) -> IsoHour {
        self.time.hour
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<IsoMinute>
    for CustomZonedDateTime<A, Z>
{
    #[inline]
    fn get_field(&self) -> IsoMinute {
        self.time.minute
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<IsoSecond>
    for CustomZonedDateTime<A, Z>
{
    #[inline]
    fn get_field(&self) -> IsoSecond {
        self.time.second
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<NanoSecond>
    for CustomZonedDateTime<A, Z>
{
    #[inline]
    fn get_field(&self) -> NanoSecond {
        self.time.nanosecond
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<Option<UtcOffset>>
    for CustomZonedDateTime<A, Z>
where
    Z: GetField<Option<UtcOffset>>,
{
    #[inline]
    fn get_field(&self) -> Option<UtcOffset> {
        self.zone.get_field()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<TimeZoneBcp47Id>
    for CustomZonedDateTime<A, Z>
where
    Z: GetField<TimeZoneBcp47Id>,
{
    #[inline]
    fn get_field(&self) -> TimeZoneBcp47Id {
        self.zone.get_field()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<ZoneVariant>
    for CustomZonedDateTime<A, Z>
where
    Z: GetField<ZoneVariant>,
{
    #[inline]
    fn get_field(&self) -> ZoneVariant {
        self.zone.get_field()
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<(Date<Iso>, Time)>
    for CustomZonedDateTime<A, Z>
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

impl<O> GetField<TimeZoneBcp47Id> for TimeZoneInfo<O>
where
    O: TimeZoneModel,
{
    #[inline]
    fn get_field(&self) -> TimeZoneBcp47Id {
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

impl<O> GetField<ZoneVariant> for TimeZoneInfo<O>
where
    O: TimeZoneModel<ZoneVariant = ZoneVariant>,
{
    #[inline]
    fn get_field(&self) -> ZoneVariant {
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

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> GetField<()> for CustomZonedDateTime<A, Z> {
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
