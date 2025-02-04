// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Buddhist calendar.
//!
//! ```rust
//! use icu::calendar::{cal::Buddhist, Date, DateTime};
//!
//! // `Date` type
//! let date_iso = Date::try_new_iso(1970, 1, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//! let date_buddhist = Date::new_from_iso(date_iso, Buddhist);
//!
//! // `DateTime` type
//! let datetime_iso = DateTime::try_new_iso(1970, 1, 2, 13, 1, 0)
//!     .expect("Failed to initialize ISO DateTime instance.");
//! let datetime_buddhist = DateTime::new_from_iso(datetime_iso, Buddhist);
//!
//! // `Date` checks
//! assert_eq!(date_buddhist.year().era_year_or_extended(), 2513);
//! assert_eq!(date_buddhist.month().ordinal, 1);
//! assert_eq!(date_buddhist.day_of_month().0, 2);
//!
//! // `DateTime` type
//! assert_eq!(datetime_buddhist.date.year().era_year_or_extended(), 2513);
//! assert_eq!(datetime_buddhist.date.month().ordinal, 1);
//! assert_eq!(datetime_buddhist.date.day_of_month().0, 2);
//! assert_eq!(datetime_buddhist.time.hour.number(), 13);
//! assert_eq!(datetime_buddhist.time.minute.number(), 1);
//! assert_eq!(datetime_buddhist.time.second.number(), 0);
//! ```

use crate::any_calendar::AnyCalendarKind;
use crate::calendar_arithmetic::ArithmeticDate;
use crate::error::DateError;
use crate::iso::{Iso, IsoDateInner};
use crate::{types, Calendar, Date, DateDuration, DateDurationUnit, DateTime, RangeError, Time};
use tinystr::tinystr;

/// The number of years the Buddhist Era is ahead of C.E. by
///
/// (1 AD = 544 BE)
const BUDDHIST_ERA_OFFSET: i32 = 543;

#[derive(Copy, Clone, Debug, Default)]
/// The [Thai Solar Buddhist Calendar][cal]
///
/// The [Thai Solar Buddhist Calendar][cal] is a solar calendar used in Thailand, with twelve months.
/// The months and days are identical to that of the Gregorian calendar, however the years are counted
/// differently using the Buddhist Era.
///
/// This type can be used with [`Date`] or [`DateTime`] to represent dates in this calendar.
///
/// [cal]: https://en.wikipedia.org/wiki/Thai_solar_calendar
///
/// # Era codes
///
/// This calendar supports one era, `"be"`, with 1 B.E. being 543 BCE.
///
/// # Month codes
///
/// This calendar supports 12 solar month codes (`"M01" - "M12"`)
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Buddhist;

impl Calendar for Buddhist {
    type DateInner = IsoDateInner;

    fn date_from_codes(
        &self,
        era: Option<types::Era>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        if let Some(era) = era {
            if era.0 != tinystr!(16, "be") {
                return Err(DateError::UnknownEra(era));
            }
        }
        let year = year - BUDDHIST_ERA_OFFSET;

        ArithmeticDate::new_from_codes(self, year, month_code, day).map(IsoDateInner)
    }
    fn date_from_iso(&self, iso: Date<Iso>) -> IsoDateInner {
        *iso.inner()
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso> {
        Date::from_raw(*date, Iso)
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        Iso.months_in_year(date)
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        Iso.days_in_year(date)
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        Iso.days_in_month(date)
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        Iso.offset_date(date, offset.cast_unit())
    }

    #[allow(clippy::field_reassign_with_default)] // it's more clear this way
    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        _calendar2: &Self,
        largest_unit: DateDurationUnit,
        smallest_unit: DateDurationUnit,
    ) -> DateDuration<Self> {
        Iso.until(date1, date2, &Iso, largest_unit, smallest_unit)
            .cast_unit()
    }

    /// The calendar-specific year represented by `date`
    fn year(&self, date: &Self::DateInner) -> types::YearInfo {
        iso_year_as_buddhist(date.0.year)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Iso.is_in_leap_year(date)
    }

    /// The calendar-specific month represented by `date`
    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        Iso.month(date)
    }

    /// The calendar-specific day-of-month represented by `date`
    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        Iso.day_of_month(date)
    }

    /// Information of the day of the year
    fn day_of_year_info(&self, date: &Self::DateInner) -> types::DayOfYearInfo {
        let prev_year = date.0.year - 1;
        let next_year = date.0.year + 1;
        types::DayOfYearInfo {
            day_of_year: Iso::day_of_year(*date),
            days_in_year: Iso::days_in_year_direct(date.0.year),
            prev_year: iso_year_as_buddhist(prev_year),
            days_in_prev_year: Iso::days_in_year_direct(prev_year),
            next_year: iso_year_as_buddhist(next_year),
        }
    }

    fn debug_name(&self) -> &'static str {
        "Buddhist"
    }

    fn any_calendar_kind(&self) -> Option<AnyCalendarKind> {
        Some(crate::any_calendar::IntoAnyCalendar::kind(self))
    }
}

impl Date<Buddhist> {
    /// Construct a new Buddhist Date.
    ///
    /// Years are specified as BE years.
    ///
    /// ```rust
    /// use icu::calendar::Date;
    ///
    /// let date_buddhist = Date::try_new_buddhist(1970, 1, 2)
    ///     .expect("Failed to initialize Buddhist Date instance.");
    ///
    /// assert_eq!(date_buddhist.year().era_year_or_extended(), 1970);
    /// assert_eq!(date_buddhist.month().ordinal, 1);
    /// assert_eq!(date_buddhist.day_of_month().0, 2);
    /// ```
    pub fn try_new_buddhist(year: i32, month: u8, day: u8) -> Result<Date<Buddhist>, RangeError> {
        Date::try_new_iso(year - BUDDHIST_ERA_OFFSET, month, day)
            .map(|d| Date::new_from_iso(d, Buddhist))
    }
}

impl DateTime<Buddhist> {
    /// Construct a new Buddhist datetime from integers.
    ///
    /// Years are specified as BE years.
    ///
    /// ```rust
    /// use icu::calendar::DateTime;
    ///
    /// let datetime_buddhist = DateTime::try_new_buddhist(1970, 1, 2, 13, 1, 0)
    ///     .expect("Failed to initialize Buddhist DateTime instance.");
    ///
    /// assert_eq!(datetime_buddhist.date.year().era_year_or_extended(), 1970);
    /// assert_eq!(datetime_buddhist.date.month().ordinal, 1);
    /// assert_eq!(datetime_buddhist.date.day_of_month().0, 2);
    /// assert_eq!(datetime_buddhist.time.hour.number(), 13);
    /// assert_eq!(datetime_buddhist.time.minute.number(), 1);
    /// assert_eq!(datetime_buddhist.time.second.number(), 0);
    /// ```
    pub fn try_new_buddhist(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
    ) -> Result<DateTime<Buddhist>, DateError> {
        Ok(DateTime {
            date: Date::try_new_buddhist(year, month, day)?,
            time: Time::try_new(hour, minute, second, 0)?,
        })
    }
}

fn iso_year_as_buddhist(year: i32) -> types::YearInfo {
    let buddhist_year = year + BUDDHIST_ERA_OFFSET;
    types::YearInfo::new(
        buddhist_year,
        types::EraYear {
            standard_era: tinystr!(16, "buddhist").into(),
            formatting_era: types::FormattingEra::Index(0, tinystr!(16, "BE")),
            era_year: buddhist_year,
            ambiguity: types::YearAmbiguity::CenturyRequired,
        },
    )
}

#[cfg(test)]
mod test {
    use calendrical_calculations::rata_die::RataDie;

    use super::*;

    #[test]
    fn test_buddhist_roundtrip_near_rd_zero() {
        for i in -10000..=10000 {
            let rd = RataDie::new(i);
            let iso1 = Iso::iso_from_fixed(rd);
            let buddhist = iso1.to_calendar(Buddhist);
            let iso2 = buddhist.to_calendar(Iso);
            let result = Iso::fixed_from_iso(iso2.inner);
            assert_eq!(rd, result);
        }
    }

    #[test]
    fn test_buddhist_roundtrip_near_epoch() {
        // Buddhist epoch start RD: -198326
        for i in -208326..=-188326 {
            let rd = RataDie::new(i);
            let iso1 = Iso::iso_from_fixed(rd);
            let buddhist = iso1.to_calendar(Buddhist);
            let iso2 = buddhist.to_calendar(Iso);
            let result = Iso::fixed_from_iso(iso2.inner);
            assert_eq!(rd, result);
        }
    }

    #[test]
    fn test_buddhist_directionality_near_rd_zero() {
        for i in -100..=100 {
            for j in -100..=100 {
                let iso_i = Iso::iso_from_fixed(RataDie::new(i));
                let iso_j = Iso::iso_from_fixed(RataDie::new(j));

                let buddhist_i = Date::new_from_iso(iso_i, Buddhist);
                let buddhist_j = Date::new_from_iso(iso_j, Buddhist);

                assert_eq!(
                    i.cmp(&j),
                    iso_i.cmp(&iso_j),
                    "ISO directionality inconsistent with directionality for i: {i}, j: {j}"
                );

                assert_eq!(
                    i.cmp(&j),
                    buddhist_i.cmp(&buddhist_j),
                    "Buddhist directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }

    #[test]
    fn test_buddhist_directionality_near_epoch() {
        // Buddhist epoch start RD: -198326
        for i in -198426..=-198226 {
            for j in -198426..=-198226 {
                let iso_i = Iso::iso_from_fixed(RataDie::new(i));
                let iso_j = Iso::iso_from_fixed(RataDie::new(j));

                let buddhist_i = Date::new_from_iso(iso_i, Buddhist);
                let buddhist_j = Date::new_from_iso(iso_j, Buddhist);

                assert_eq!(
                    i.cmp(&j),
                    iso_i.cmp(&iso_j),
                    "ISO directionality inconsistent with directionality for i: {i}, j: {j}"
                );

                assert_eq!(
                    i.cmp(&j),
                    buddhist_i.cmp(&buddhist_j),
                    "Buddhist directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }

    #[derive(Debug)]
    struct TestCase {
        iso_year: i32,
        iso_month: u8,
        iso_day: u8,
        buddhist_year: i32,
        buddhist_month: u8,
        buddhist_day: u8,
    }

    fn check_test_case(case: TestCase) {
        let iso_year = case.iso_year;
        let iso_month = case.iso_month;
        let iso_day = case.iso_day;
        let buddhist_year = case.buddhist_year;
        let buddhist_month = case.buddhist_month;
        let buddhist_day = case.buddhist_day;

        let iso1 = Date::try_new_iso(iso_year, iso_month, iso_day).unwrap();
        let buddhist1 = iso1.to_calendar(Buddhist);
        assert_eq!(
            buddhist1.year().era_year_or_extended(),
            buddhist_year,
            "Iso -> Buddhist year check failed for case: {case:?}"
        );
        assert_eq!(
            buddhist1.month().ordinal,
            buddhist_month,
            "Iso -> Buddhist month check failed for case: {case:?}"
        );
        assert_eq!(
            buddhist1.day_of_month().0,
            buddhist_day,
            "Iso -> Buddhist day check failed for case: {case:?}"
        );

        let buddhist2 =
            Date::try_new_buddhist(buddhist_year, buddhist_month, buddhist_day).unwrap();
        let iso2 = buddhist2.to_calendar(Iso);
        assert_eq!(
            iso2.year().era_year_or_extended(),
            iso_year,
            "Buddhist -> Iso year check failed for case: {case:?}"
        );
        assert_eq!(
            iso2.month().ordinal,
            iso_month,
            "Buddhist -> Iso month check failed for case: {case:?}"
        );
        assert_eq!(
            iso2.day_of_month().0,
            iso_day,
            "Buddhist -> Iso day check failed for case: {case:?}"
        );
    }

    #[test]
    fn test_buddhist_cases_near_rd_zero() {
        let cases = [
            TestCase {
                iso_year: -100,
                iso_month: 2,
                iso_day: 15,
                buddhist_year: 443,
                buddhist_month: 2,
                buddhist_day: 15,
            },
            TestCase {
                iso_year: -3,
                iso_month: 10,
                iso_day: 29,
                buddhist_year: 540,
                buddhist_month: 10,
                buddhist_day: 29,
            },
            TestCase {
                iso_year: 0,
                iso_month: 12,
                iso_day: 31,
                buddhist_year: 543,
                buddhist_month: 12,
                buddhist_day: 31,
            },
            TestCase {
                iso_year: 1,
                iso_month: 1,
                iso_day: 1,
                buddhist_year: 544,
                buddhist_month: 1,
                buddhist_day: 1,
            },
            TestCase {
                iso_year: 4,
                iso_month: 2,
                iso_day: 29,
                buddhist_year: 547,
                buddhist_month: 2,
                buddhist_day: 29,
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }

    #[test]
    fn test_buddhist_cases_near_epoch() {
        // 1 BE = 543 BCE = -542 ISO
        let cases = [
            TestCase {
                iso_year: -554,
                iso_month: 12,
                iso_day: 31,
                buddhist_year: -11,
                buddhist_month: 12,
                buddhist_day: 31,
            },
            TestCase {
                iso_year: -553,
                iso_month: 1,
                iso_day: 1,
                buddhist_year: -10,
                buddhist_month: 1,
                buddhist_day: 1,
            },
            TestCase {
                iso_year: -544,
                iso_month: 8,
                iso_day: 31,
                buddhist_year: -1,
                buddhist_month: 8,
                buddhist_day: 31,
            },
            TestCase {
                iso_year: -543,
                iso_month: 5,
                iso_day: 12,
                buddhist_year: 0,
                buddhist_month: 5,
                buddhist_day: 12,
            },
            TestCase {
                iso_year: -543,
                iso_month: 12,
                iso_day: 31,
                buddhist_year: 0,
                buddhist_month: 12,
                buddhist_day: 31,
            },
            TestCase {
                iso_year: -542,
                iso_month: 1,
                iso_day: 1,
                buddhist_year: 1,
                buddhist_month: 1,
                buddhist_day: 1,
            },
            TestCase {
                iso_year: -541,
                iso_month: 7,
                iso_day: 9,
                buddhist_year: 2,
                buddhist_month: 7,
                buddhist_day: 9,
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }
}
