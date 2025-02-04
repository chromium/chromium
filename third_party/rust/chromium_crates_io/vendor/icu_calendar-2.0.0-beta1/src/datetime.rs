// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::any_calendar::{AnyCalendar, IntoAnyCalendar};
use crate::error::DateError;
use crate::types::{self, Time};
use crate::{AsCalendar, Calendar, Date, Iso};
use alloc::rc::Rc;
use alloc::sync::Arc;

/// A date+time for a given calendar.
///
/// This can work with wrappers around [`Calendar`](crate::Calendar) types,
/// e.g. `Rc<C>`, via the [`AsCalendar`] trait, much like
/// [`Date`].
///
/// This can be constructed manually from a [`Date`] and [`Time`], or can be constructed
/// from its fields via [`Self::try_new_from_codes()`], or can be constructed with one of the
/// `new_<calendar>_datetime()` per-calendar methods (and then freely converted between calendars).
///
/// ```rust
/// use icu::calendar::DateTime;
///
/// // Example: Construction of ISO datetime from integers.
/// let datetime_iso = DateTime::try_new_iso(1970, 1, 2, 13, 1, 0)
///     .expect("Failed to initialize ISO DateTime instance.");
///
/// assert_eq!(datetime_iso.date.year().era_year_or_extended(), 1970);
/// assert_eq!(datetime_iso.date.month().ordinal, 1);
/// assert_eq!(datetime_iso.date.day_of_month().0, 2);
/// assert_eq!(datetime_iso.time.hour.number(), 13);
/// assert_eq!(datetime_iso.time.minute.number(), 1);
/// assert_eq!(datetime_iso.time.second.number(), 0);
/// ```
#[derive(Debug)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct DateTime<A: AsCalendar> {
    /// The date
    pub date: Date<A>,
    /// The time
    pub time: Time,
}

impl<A: AsCalendar> DateTime<A> {
    /// Construct a [`DateTime`] for a given [`Date`] and [`Time`]
    pub fn new(date: Date<A>, time: Time) -> Self {
        DateTime { date, time }
    }

    /// Construct a datetime from from era/month codes and fields,
    /// and some calendar representation
    #[inline]
    pub fn try_new_from_codes(
        era: Option<types::Era>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
        time: Time,
        calendar: A,
    ) -> Result<Self, DateError> {
        let date = Date::try_new_from_codes(era, year, month_code, day, calendar)?;
        Ok(DateTime { date, time })
    }

    /// Construct a DateTime from an ISO datetime and some calendar representation
    #[inline]
    pub fn new_from_iso(iso: DateTime<Iso>, calendar: A) -> Self {
        let date = Date::new_from_iso(iso.date, calendar);
        DateTime {
            date,
            time: iso.time,
        }
    }

    /// Convert the DateTime to an ISO DateTime
    #[inline]
    pub fn to_iso(&self) -> DateTime<Iso> {
        DateTime {
            date: self.date.to_iso(),
            time: self.time,
        }
    }

    /// Convert the DateTime to a DateTime in a different calendar
    #[inline]
    pub fn to_calendar<A2: AsCalendar>(&self, calendar: A2) -> DateTime<A2> {
        DateTime {
            date: self.date.to_calendar(calendar),
            time: self.time,
        }
    }
}

impl<C: IntoAnyCalendar, A: AsCalendar<Calendar = C>> DateTime<A> {
    /// Type-erase the date, converting it to a date for [`AnyCalendar`]
    pub fn to_any(&self) -> DateTime<AnyCalendar> {
        DateTime {
            date: self.date.to_any(),
            time: self.time,
        }
    }
}

impl<C: Calendar> DateTime<C> {
    /// Wrap the calendar type in `Rc<T>`
    ///
    /// Useful when paired with [`Self::to_any()`] to obtain a `DateTime<Rc<AnyCalendar>>`
    pub fn wrap_calendar_in_rc(self) -> DateTime<Rc<C>> {
        DateTime {
            date: self.date.wrap_calendar_in_rc(),
            time: self.time,
        }
    }

    /// Wrap the calendar type in `Arc<T>`
    ///
    /// Useful when paired with [`Self::to_any()`] to obtain a `DateTime<Rc<AnyCalendar>>`
    pub fn wrap_calendar_in_arc(self) -> DateTime<Arc<C>> {
        DateTime {
            date: self.date.wrap_calendar_in_arc(),
            time: self.time,
        }
    }
}

impl<C, A, B> PartialEq<DateTime<B>> for DateTime<A>
where
    C: Calendar,
    A: AsCalendar<Calendar = C>,
    B: AsCalendar<Calendar = C>,
{
    fn eq(&self, other: &DateTime<B>) -> bool {
        self.date == other.date && self.time == other.time
    }
}

// We can do this since DateInner is required to be Eq by the Calendar trait
impl<A: AsCalendar> Eq for DateTime<A> {}

impl<C, A, B> PartialOrd<DateTime<B>> for DateTime<A>
where
    C: Calendar,
    C::DateInner: PartialOrd,
    A: AsCalendar<Calendar = C>,
    B: AsCalendar<Calendar = C>,
{
    fn partial_cmp(&self, other: &DateTime<B>) -> Option<core::cmp::Ordering> {
        match self.date.partial_cmp(&other.date) {
            Some(core::cmp::Ordering::Equal) => self.time.partial_cmp(&other.time),
            other => other,
        }
    }
}

impl<C, A> Ord for DateTime<A>
where
    C: Calendar,
    C::DateInner: Ord,
    A: AsCalendar<Calendar = C>,
{
    fn cmp(&self, other: &Self) -> core::cmp::Ordering {
        (&self.date, &self.time).cmp(&(&other.date, &other.time))
    }
}

impl<A: AsCalendar + Clone> Clone for DateTime<A> {
    fn clone(&self) -> Self {
        Self {
            date: self.date.clone(),
            time: self.time,
        }
    }
}

impl<A> Copy for DateTime<A>
where
    A: AsCalendar + Copy,
    <<A as AsCalendar>::Calendar as Calendar>::DateInner: Copy,
{
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ord() {
        let dates_in_order = [
            DateTime::try_new_iso(0, 1, 1, 0, 0, 0).unwrap(),
            DateTime::try_new_iso(0, 1, 1, 0, 0, 1).unwrap(),
            DateTime::try_new_iso(0, 1, 1, 0, 1, 0).unwrap(),
            DateTime::try_new_iso(0, 1, 1, 1, 0, 0).unwrap(),
            DateTime::try_new_iso(0, 1, 2, 0, 0, 0).unwrap(),
            DateTime::try_new_iso(0, 2, 1, 0, 0, 0).unwrap(),
            DateTime::try_new_iso(1, 1, 1, 0, 0, 0).unwrap(),
        ];
        for (i, i_date) in dates_in_order.iter().enumerate() {
            for (j, j_date) in dates_in_order.iter().enumerate() {
                let result1 = i_date.cmp(j_date);
                let result2 = j_date.cmp(i_date);
                assert_eq!(result1.reverse(), result2);
                assert_eq!(i.cmp(&j), i_date.cmp(j_date));
            }
        }
    }
}
