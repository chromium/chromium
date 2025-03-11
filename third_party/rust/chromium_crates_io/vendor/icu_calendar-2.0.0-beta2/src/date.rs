// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::any_calendar::{AnyCalendar, IntoAnyCalendar};
use crate::error::DateError;
use crate::week::{WeekCalculator, WeekOf};
use crate::{types, Calendar, DateDuration, DateDurationUnit, Iso};
#[cfg(feature = "alloc")]
use alloc::rc::Rc;
#[cfg(feature = "alloc")]
use alloc::sync::Arc;
use core::fmt;
use core::ops::Deref;

/// Types that contain a calendar
///
/// This allows one to use [`Date`] with wrappers around calendars,
/// e.g. reference counted calendars.
pub trait AsCalendar {
    /// The calendar being wrapped
    type Calendar: Calendar;
    /// Obtain the inner calendar
    fn as_calendar(&self) -> &Self::Calendar;
}

impl<C: Calendar> AsCalendar for C {
    type Calendar = C;
    #[inline]
    fn as_calendar(&self) -> &Self {
        self
    }
}

#[cfg(feature = "alloc")]
impl<C: AsCalendar> AsCalendar for Rc<C> {
    type Calendar = C::Calendar;
    #[inline]
    fn as_calendar(&self) -> &Self::Calendar {
        self.as_ref().as_calendar()
    }
}

#[cfg(feature = "alloc")]
impl<C: AsCalendar> AsCalendar for Arc<C> {
    type Calendar = C::Calendar;
    #[inline]
    fn as_calendar(&self) -> &Self::Calendar {
        self.as_ref().as_calendar()
    }
}

/// This exists as a wrapper around `&'a T` so that
/// `Date<&'a C>` is possible for calendar `C`.
///
/// Unfortunately,
/// [`AsCalendar`] cannot be implemented on `&'a T` directly because
/// `&'a T` is `#[fundamental]` and the impl would clash with the one above with
/// `AsCalendar` for `C: Calendar`.
///
/// Use `Date<Ref<'a, C>>` where you would use `Date<&'a C>`
#[allow(clippy::exhaustive_structs)] // newtype
#[derive(PartialEq, Eq, Debug)]
pub struct Ref<'a, C>(pub &'a C);

impl<C> Copy for Ref<'_, C> {}

impl<C> Clone for Ref<'_, C> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<C: AsCalendar> AsCalendar for Ref<'_, C> {
    type Calendar = C::Calendar;
    #[inline]
    fn as_calendar(&self) -> &Self::Calendar {
        self.0.as_calendar()
    }
}

impl<C> Deref for Ref<'_, C> {
    type Target = C;
    fn deref(&self) -> &C {
        self.0
    }
}

/// A date for a given calendar.
///
/// This can work with wrappers around [`Calendar`] types,
/// e.g. `Rc<C>`, via the [`AsCalendar`] trait.
///
/// This can be constructed  constructed
/// from its fields via [`Self::try_new_from_codes()`], or can be constructed with one of the
/// `new_<calendar>_date()` per-calendar methods (and then freely converted between calendars).
///
/// ```rust
/// use icu::calendar::Date;
///
/// // Example: creation of ISO date from integers.
/// let date_iso = Date::try_new_iso(1970, 1, 2)
///     .expect("Failed to initialize ISO Date instance.");
///
/// assert_eq!(date_iso.year().era_year_or_extended(), 1970);
/// assert_eq!(date_iso.month().ordinal, 1);
/// assert_eq!(date_iso.day_of_month().0, 2);
/// ```
pub struct Date<A: AsCalendar> {
    pub(crate) inner: <A::Calendar as Calendar>::DateInner,
    pub(crate) calendar: A,
}

impl<A: AsCalendar> Date<A> {
    /// Construct a date from from era/month codes and fields, and some calendar representation
    ///
    /// The year is `extended_year` if no era is provided
    #[inline]
    pub fn try_new_from_codes(
        era: Option<types::Era>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
        calendar: A,
    ) -> Result<Self, DateError> {
        let inner = calendar
            .as_calendar()
            .date_from_codes(era, year, month_code, day)?;
        Ok(Date { inner, calendar })
    }

    /// Construct a date from an ISO date and some calendar representation
    #[inline]
    pub fn new_from_iso(iso: Date<Iso>, calendar: A) -> Self {
        let inner = calendar.as_calendar().date_from_iso(iso);
        Date { inner, calendar }
    }

    /// Convert the Date to an ISO Date
    #[inline]
    pub fn to_iso(&self) -> Date<Iso> {
        self.calendar.as_calendar().date_to_iso(self.inner())
    }

    /// Convert the Date to a date in a different calendar
    #[inline]
    pub fn to_calendar<A2: AsCalendar>(&self, calendar: A2) -> Date<A2> {
        Date::new_from_iso(self.to_iso(), calendar)
    }

    /// The number of months in the year of this date
    #[inline]
    pub fn months_in_year(&self) -> u8 {
        self.calendar.as_calendar().months_in_year(self.inner())
    }

    /// The number of days in the year of this date
    #[inline]
    pub fn days_in_year(&self) -> u16 {
        self.calendar.as_calendar().days_in_year(self.inner())
    }

    /// The number of days in the month of this date
    #[inline]
    pub fn days_in_month(&self) -> u8 {
        self.calendar.as_calendar().days_in_month(self.inner())
    }

    /// The day of the week for this date
    ///
    /// Monday is 1, Sunday is 7, according to ISO
    #[inline]
    pub fn day_of_week(&self) -> types::Weekday {
        self.calendar.as_calendar().day_of_week(self.inner())
    }

    /// Add a `duration` to this date, mutating it
    #[doc(hidden)] // unstable
    #[inline]
    pub fn add(&mut self, duration: DateDuration<A::Calendar>) {
        self.calendar
            .as_calendar()
            .offset_date(&mut self.inner, duration)
    }

    /// Add a `duration` to this date, returning the new one
    #[doc(hidden)] // unstable
    #[inline]
    pub fn added(mut self, duration: DateDuration<A::Calendar>) -> Self {
        self.add(duration);
        self
    }

    /// Calculating the duration between `other - self`
    #[doc(hidden)] // unstable
    #[inline]
    pub fn until<B: AsCalendar<Calendar = A::Calendar>>(
        &self,
        other: &Date<B>,
        largest_unit: DateDurationUnit,
        smallest_unit: DateDurationUnit,
    ) -> DateDuration<A::Calendar> {
        self.calendar.as_calendar().until(
            self.inner(),
            other.inner(),
            other.calendar.as_calendar(),
            largest_unit,
            smallest_unit,
        )
    }

    /// The calendar-specific year represented by `self`
    #[inline]
    pub fn year(&self) -> types::YearInfo {
        self.calendar.as_calendar().year(&self.inner)
    }

    /// Returns whether `self` is in a calendar-specific leap year
    #[inline]
    pub fn is_in_leap_year(&self) -> bool {
        self.calendar.as_calendar().is_in_leap_year(&self.inner)
    }

    /// The calendar-specific month represented by `self`
    #[inline]
    pub fn month(&self) -> types::MonthInfo {
        self.calendar.as_calendar().month(&self.inner)
    }

    /// The calendar-specific day-of-month represented by `self`
    #[inline]
    pub fn day_of_month(&self) -> types::DayOfMonth {
        self.calendar.as_calendar().day_of_month(&self.inner)
    }

    /// The calendar-specific day-of-month represented by `self`
    #[inline]
    pub fn day_of_year_info(&self) -> types::DayOfYearInfo {
        self.calendar.as_calendar().day_of_year_info(&self.inner)
    }

    /// The week of the month containing this date.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::types::WeekOfMonth;
    /// use icu::calendar::types::Weekday;
    /// use icu::calendar::Date;
    ///
    /// let date = Date::try_new_iso(2022, 8, 10).unwrap(); // second Wednesday
    ///
    /// // The following info is usually locale-specific
    /// let first_weekday = Weekday::Sunday;
    ///
    /// assert_eq!(date.week_of_month(first_weekday), WeekOfMonth(2));
    /// ```
    pub fn week_of_month(&self, first_weekday: types::Weekday) -> types::WeekOfMonth {
        let config = WeekCalculator {
            first_weekday,
            min_week_days: 0, // ignored
            weekend: None,
        };
        config.week_of_month(self.day_of_month(), self.day_of_week())
    }

    /// The week of the year containing this date.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::week::RelativeUnit;
    /// use icu::calendar::week::WeekCalculator;
    /// use icu::calendar::week::WeekOf;
    /// use icu::calendar::Date;
    ///
    /// let date = Date::try_new_iso(2022, 8, 26).unwrap();
    ///
    /// // The following info is usually locale-specific
    /// let week_calculator = WeekCalculator::default();
    ///
    /// assert_eq!(
    ///     date.week_of_year(&week_calculator),
    ///     WeekOf {
    ///         week: 35,
    ///         unit: RelativeUnit::Current
    ///     }
    /// );
    /// ```
    pub fn week_of_year(&self, config: &WeekCalculator) -> WeekOf {
        config.week_of_year(self.day_of_year_info(), self.day_of_week())
    }

    /// Construct a date from raw values for a given calendar. This does not check any
    /// invariants for the date and calendar, and should only be called by calendar implementations.
    ///
    /// Calling this outside of calendar implementations is sound, but calendar implementations are not
    /// expected to do anything sensible with such invalid dates.
    ///
    /// AnyCalendar *will* panic if AnyCalendar [`Date`] objects with mismatching
    /// date and calendar types are constructed
    #[inline]
    pub fn from_raw(inner: <A::Calendar as Calendar>::DateInner, calendar: A) -> Self {
        Self { inner, calendar }
    }

    /// Get the inner date implementation. Should not be called outside of calendar implementations
    #[inline]
    pub fn inner(&self) -> &<A::Calendar as Calendar>::DateInner {
        &self.inner
    }

    /// Get a reference to the contained calendar
    #[inline]
    pub fn calendar(&self) -> &A::Calendar {
        self.calendar.as_calendar()
    }

    /// Get a reference to the contained calendar wrapper
    ///
    /// (Useful in case the user wishes to e.g. clone an Rc)
    #[inline]
    pub fn calendar_wrapper(&self) -> &A {
        &self.calendar
    }
}

impl Date<Iso> {
    /// Calculates the number of days between two dates.
    ///
    /// ```rust
    /// use icu::calendar::Date;
    /// use icu::calendar::DateDurationUnit;
    ///
    /// let a = Date::try_new_iso(1994, 12, 10).unwrap();
    /// let b = Date::try_new_iso(2024, 10, 30).unwrap();
    ///
    /// assert_eq!(b.days_since(a), 10_917);
    /// ```
    #[doc(hidden)] // unstable
    pub fn days_since(&self, other: Date<Iso>) -> i32 {
        (Iso::to_fixed(*self) - Iso::to_fixed(other)) as i32
    }
}

impl<C: IntoAnyCalendar> Date<C> {
    /// Type-erase the date, converting it to a date for [`AnyCalendar`]
    pub fn to_any(self) -> Date<AnyCalendar> {
        Date::from_raw(
            self.calendar.date_to_any(&self.inner),
            self.calendar.to_any(),
        )
    }
}

impl<A: AsCalendar> Date<A> {
    /// Wrap the calendar type in `Rc<T>`
    ///
    /// Useful when paired with [`Self::to_any()`] to obtain a `Date<Rc<AnyCalendar>>`
    #[cfg(feature = "alloc")]
    pub fn wrap_calendar_in_rc(self) -> Date<Rc<A>> {
        Date::from_raw(self.inner, Rc::new(self.calendar))
    }

    /// Wrap the calendar type in `Arc<T>`
    ///
    /// Useful when paired with [`Self::to_any()`] to obtain a `Date<Rc<AnyCalendar>>`
    #[cfg(feature = "alloc")]
    pub fn wrap_calendar_in_arc(self) -> Date<Arc<A>> {
        Date::from_raw(self.inner, Arc::new(self.calendar))
    }

    /// Wrap the calendar type in `Ref<T>`
    ///
    /// Useful for converting a `&Date<C>` into an equivalent `Date<D>` without cloning
    /// the calendar.
    pub fn wrap_calendar_in_ref(&self) -> Date<Ref<A>> {
        Date::from_raw(self.inner, Ref(&self.calendar))
    }
}

impl<C, A, B> PartialEq<Date<B>> for Date<A>
where
    C: Calendar,
    A: AsCalendar<Calendar = C>,
    B: AsCalendar<Calendar = C>,
{
    fn eq(&self, other: &Date<B>) -> bool {
        self.inner.eq(&other.inner)
    }
}

impl<A: AsCalendar> Eq for Date<A> {}

impl<C, A, B> PartialOrd<Date<B>> for Date<A>
where
    C: Calendar,
    C::DateInner: PartialOrd,
    A: AsCalendar<Calendar = C>,
    B: AsCalendar<Calendar = C>,
{
    fn partial_cmp(&self, other: &Date<B>) -> Option<core::cmp::Ordering> {
        self.inner.partial_cmp(&other.inner)
    }
}

impl<C, A> Ord for Date<A>
where
    C: Calendar,
    C::DateInner: Ord,
    A: AsCalendar<Calendar = C>,
{
    fn cmp(&self, other: &Self) -> core::cmp::Ordering {
        self.inner.cmp(&other.inner)
    }
}

impl<A: AsCalendar> fmt::Debug for Date<A> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
        let month = self.month().ordinal;
        let day = self.day_of_month().0;
        let calendar = self.calendar.as_calendar().debug_name();
        match self.year().kind {
            types::YearKind::Era(e) => {
                let era = e.standard_era.0;
                let era_year = e.era_year;
                write!(
                    f,
                    "Date({era_year}-{month}-{day}, {era} era, for calendar {calendar})"
                )
            }
            types::YearKind::Cyclic(cy) => {
                let year = cy.year;
                let related_iso = cy.related_iso;
                write!(
                    f,
                    "Date({year}-{month}-{day}, ISO year {related_iso}, for calendar {calendar})"
                )
            }
        }
    }
}

impl<A: AsCalendar + Clone> Clone for Date<A> {
    fn clone(&self) -> Self {
        Self {
            inner: self.inner,
            calendar: self.calendar.clone(),
        }
    }
}

impl<A> Copy for Date<A> where A: AsCalendar + Copy {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ord() {
        let dates_in_order = [
            Date::try_new_iso(-10, 1, 1).unwrap(),
            Date::try_new_iso(-10, 1, 2).unwrap(),
            Date::try_new_iso(-10, 2, 1).unwrap(),
            Date::try_new_iso(-1, 1, 1).unwrap(),
            Date::try_new_iso(-1, 1, 2).unwrap(),
            Date::try_new_iso(-1, 2, 1).unwrap(),
            Date::try_new_iso(0, 1, 1).unwrap(),
            Date::try_new_iso(0, 1, 2).unwrap(),
            Date::try_new_iso(0, 2, 1).unwrap(),
            Date::try_new_iso(1, 1, 1).unwrap(),
            Date::try_new_iso(1, 1, 2).unwrap(),
            Date::try_new_iso(1, 2, 1).unwrap(),
            Date::try_new_iso(10, 1, 1).unwrap(),
            Date::try_new_iso(10, 1, 2).unwrap(),
            Date::try_new_iso(10, 2, 1).unwrap(),
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
