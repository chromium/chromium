// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Indian national calendar.
//!
//! ```rust
//! use icu::calendar::{cal::Indian, Date};
//!
//! let date_iso = Date::try_new_iso(1970, 1, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//! let date_indian = Date::new_from_iso(date_iso, Indian);
//!
//! assert_eq!(date_indian.year().era_year_or_extended(), 1891);
//! assert_eq!(date_indian.month().ordinal, 10);
//! assert_eq!(date_indian.day_of_month().0, 12);
//! ```

use crate::calendar_arithmetic::{ArithmeticDate, CalendarArithmetic};
use crate::error::DateError;
use crate::iso::Iso;
use crate::{types, Calendar, Date, DateDuration, DateDurationUnit, RangeError};
use tinystr::tinystr;

/// The Indian National Calendar (aka the Saka calendar)
///
/// The [Indian National calendar] is a solar calendar used by the Indian government, with twelve months.
///
/// This type can be used with [`Date`] to represent dates in this calendar.
///
/// [Indian National calendar]: https://en.wikipedia.org/wiki/Indian_national_calendar
///
/// # Era codes
///
/// This calendar has a single era: `"saka"`, with Saka 0 being 78 CE. Dates before this era use negative years.
///
/// # Month codes
///
/// This calendar supports 12 solar month codes (`"M01" - "M12"`)
#[derive(Copy, Clone, Debug, Hash, Default, Eq, PartialEq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Indian;

/// The inner date type used for representing [`Date`]s of [`Indian`]. See [`Date`] and [`Indian`] for more details.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct IndianDateInner(ArithmeticDate<Indian>);

impl CalendarArithmetic for Indian {
    type YearInfo = ();

    fn month_days(year: i32, month: u8, _data: ()) -> u8 {
        if month == 1 {
            if Self::is_leap_year(year, ()) {
                31
            } else {
                30
            }
        } else if (2..=6).contains(&month) {
            31
        } else if (7..=12).contains(&month) {
            30
        } else {
            0
        }
    }

    fn months_for_every_year(_: i32, _data: ()) -> u8 {
        12
    }

    fn is_leap_year(year: i32, _data: ()) -> bool {
        Iso::is_leap_year(year + 78, ())
    }

    fn last_month_day_in_year(_year: i32, _data: ()) -> (u8, u8) {
        (12, 30)
    }

    fn days_in_provided_year(year: i32, _data: ()) -> u16 {
        if Self::is_leap_year(year, ()) {
            366
        } else {
            365
        }
    }
}

/// The Saka calendar starts on the 81st day of the Gregorian year (March 22 or 21)
/// which is an 80 day offset. This number should be subtracted from Gregorian dates
const DAY_OFFSET: u16 = 80;
/// The Saka calendar is 78 years behind Gregorian. This number should be added to Gregorian dates
const YEAR_OFFSET: i32 = 78;

impl Calendar for Indian {
    type DateInner = IndianDateInner;
    fn date_from_codes(
        &self,
        era: Option<types::Era>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        if let Some(era) = era {
            if era.0 != tinystr!(16, "saka") && era.0 != tinystr!(16, "indian") {
                return Err(DateError::UnknownEra(era));
            }
        }

        ArithmeticDate::new_from_codes(self, year, month_code, day).map(IndianDateInner)
    }

    // Algorithms directly implemented in icu_calendar since they're not from the book
    fn date_from_iso(&self, iso: Date<Iso>) -> IndianDateInner {
        // Get day number in year (1 indexed)
        let day_of_year_iso = Iso::day_of_year(*iso.inner());
        // Convert to Saka year
        let mut year = iso.inner().0.year - YEAR_OFFSET;
        // This is in the previous Indian year
        let day_of_year_indian = if day_of_year_iso <= DAY_OFFSET {
            year -= 1;
            let n_days = Self::days_in_provided_year(year, ());

            // calculate day of year in previous year
            n_days + day_of_year_iso - DAY_OFFSET
        } else {
            day_of_year_iso - DAY_OFFSET
        };
        IndianDateInner(ArithmeticDate::date_from_year_day(
            year,
            day_of_year_indian as u32,
        ))
    }

    // Algorithms directly implemented in icu_calendar since they're not from the book
    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso> {
        let day_of_year_indian = date.0.day_of_year(); // 1-indexed
        let days_in_year = date.0.days_in_year();

        let mut year = date.0.year + YEAR_OFFSET;
        // days_in_year is a valid day of the year, so we check > not >=
        let day_of_year_iso = if day_of_year_indian + DAY_OFFSET > days_in_year {
            year += 1;
            // calculate day of year in next year
            day_of_year_indian + DAY_OFFSET - days_in_year
        } else {
            day_of_year_indian + DAY_OFFSET
        };
        Iso::iso_from_year_day(year, day_of_year_iso)
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        date.0.months_in_year()
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        date.0.days_in_year()
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        date.0.days_in_month()
    }

    fn day_of_week(&self, date: &Self::DateInner) -> types::Weekday {
        Iso.day_of_week(Indian.date_to_iso(date).inner())
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        date.0.offset_date(offset, &());
    }

    #[allow(clippy::field_reassign_with_default)]
    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        _calendar2: &Self,
        _largest_unit: DateDurationUnit,
        _smallest_unit: DateDurationUnit,
    ) -> DateDuration<Self> {
        date1.0.until(date2.0, _largest_unit, _smallest_unit)
    }

    fn year(&self, date: &Self::DateInner) -> types::YearInfo {
        year_as_saka(date.0.year)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::is_leap_year(date.0.year, ())
    }

    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        date.0.month()
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        date.0.day_of_month()
    }

    fn day_of_year_info(&self, date: &Self::DateInner) -> types::DayOfYearInfo {
        let prev_year = year_as_saka(date.0.year - 1);
        let next_year = year_as_saka(date.0.year + 1);

        types::DayOfYearInfo {
            day_of_year: date.0.day_of_year(),
            days_in_year: date.0.days_in_year(),
            prev_year,
            days_in_prev_year: Indian::days_in_year_direct(date.0.year - 1),
            next_year,
        }
    }

    fn debug_name(&self) -> &'static str {
        "Indian"
    }

    fn any_calendar_kind(&self) -> Option<crate::AnyCalendarKind> {
        Some(crate::any_calendar::IntoAnyCalendar::kind(self))
    }
}

fn year_as_saka(year: i32) -> types::YearInfo {
    types::YearInfo::new(
        year,
        types::EraYear {
            formatting_era: types::FormattingEra::Index(0, tinystr!(16, "Saka")),
            standard_era: tinystr!(16, "saka").into(),
            era_year: year,
            ambiguity: types::YearAmbiguity::CenturyRequired,
        },
    )
}

impl Indian {
    /// Construct a new Indian Calendar
    pub fn new() -> Self {
        Self
    }

    fn days_in_year_direct(year: i32) -> u16 {
        if Indian::is_leap_year(year, ()) {
            366
        } else {
            365
        }
    }
}

impl Date<Indian> {
    /// Construct new Indian Date, with year provided in the Śaka era.
    ///
    /// ```rust
    /// use icu::calendar::Date;
    ///
    /// let date_indian = Date::try_new_indian(1891, 10, 12)
    ///     .expect("Failed to initialize Indian Date instance.");
    ///
    /// assert_eq!(date_indian.year().era_year_or_extended(), 1891);
    /// assert_eq!(date_indian.month().ordinal, 10);
    /// assert_eq!(date_indian.day_of_month().0, 12);
    /// ```
    pub fn try_new_indian(year: i32, month: u8, day: u8) -> Result<Date<Indian>, RangeError> {
        ArithmeticDate::new_from_ordinals(year, month, day)
            .map(IndianDateInner)
            .map(|inner| Date::from_raw(inner, Indian))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use calendrical_calculations::rata_die::RataDie;
    fn assert_roundtrip(y: i32, m: u8, d: u8, iso_y: i32, iso_m: u8, iso_d: u8) {
        let indian =
            Date::try_new_indian(y, m, d).expect("Indian date should construct successfully");
        let iso = indian.to_iso();

        assert_eq!(
            iso.year().era_year_or_extended(),
            iso_y,
            "{y}-{m}-{d}: ISO year did not match"
        );
        assert_eq!(
            iso.month().ordinal,
            iso_m,
            "{y}-{m}-{d}: ISO month did not match"
        );
        assert_eq!(
            iso.day_of_month().0,
            iso_d,
            "{y}-{m}-{d}: ISO day did not match"
        );

        let roundtrip = iso.to_calendar(Indian);

        assert_eq!(
            roundtrip.year().era_year_or_extended(),
            indian.year().era_year_or_extended(),
            "{y}-{m}-{d}: roundtrip year did not match"
        );
        assert_eq!(
            roundtrip.month().ordinal,
            indian.month().ordinal,
            "{y}-{m}-{d}: roundtrip month did not match"
        );
        assert_eq!(
            roundtrip.day_of_month(),
            indian.day_of_month(),
            "{y}-{m}-{d}: roundtrip day did not match"
        );
    }

    #[test]
    fn roundtrip_indian() {
        // Ultimately the day of the year will always be identical regardless of it
        // being a leap year or not
        // Test dates that occur after and before Chaitra 1 (March 22/21), in all years of
        // a four-year leap cycle, to ensure that all code paths are tested
        assert_roundtrip(1944, 6, 7, 2022, 8, 29);
        assert_roundtrip(1943, 6, 7, 2021, 8, 29);
        assert_roundtrip(1942, 6, 7, 2020, 8, 29);
        assert_roundtrip(1941, 6, 7, 2019, 8, 29);
        assert_roundtrip(1944, 11, 7, 2023, 1, 27);
        assert_roundtrip(1943, 11, 7, 2022, 1, 27);
        assert_roundtrip(1942, 11, 7, 2021, 1, 27);
        assert_roundtrip(1941, 11, 7, 2020, 1, 27);
    }

    #[derive(Debug)]
    struct TestCase {
        iso_year: i32,
        iso_month: u8,
        iso_day: u8,
        expected_year: i32,
        expected_month: u8,
        expected_day: u8,
    }

    fn check_case(case: TestCase) {
        let iso = Date::try_new_iso(case.iso_year, case.iso_month, case.iso_day).unwrap();
        let saka = iso.to_calendar(Indian);
        assert_eq!(
            saka.year().era_year_or_extended(),
            case.expected_year,
            "Year check failed for case: {case:?}"
        );
        assert_eq!(
            saka.month().ordinal,
            case.expected_month,
            "Month check failed for case: {case:?}"
        );
        assert_eq!(
            saka.day_of_month().0,
            case.expected_day,
            "Day check failed for case: {case:?}"
        );
    }

    #[test]
    fn test_cases_near_epoch_start() {
        let cases = [
            TestCase {
                iso_year: 79,
                iso_month: 3,
                iso_day: 23,
                expected_year: 1,
                expected_month: 1,
                expected_day: 2,
            },
            TestCase {
                iso_year: 79,
                iso_month: 3,
                iso_day: 22,
                expected_year: 1,
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                iso_year: 79,
                iso_month: 3,
                iso_day: 21,
                expected_year: 0,
                expected_month: 12,
                expected_day: 30,
            },
            TestCase {
                iso_year: 79,
                iso_month: 3,
                iso_day: 20,
                expected_year: 0,
                expected_month: 12,
                expected_day: 29,
            },
            TestCase {
                iso_year: 78,
                iso_month: 3,
                iso_day: 21,
                expected_year: -1,
                expected_month: 12,
                expected_day: 30,
            },
        ];

        for case in cases {
            check_case(case);
        }
    }

    #[test]
    fn test_cases_near_rd_zero() {
        let cases = [
            TestCase {
                iso_year: 1,
                iso_month: 3,
                iso_day: 22,
                expected_year: -77,
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                iso_year: 1,
                iso_month: 3,
                iso_day: 21,
                expected_year: -78,
                expected_month: 12,
                expected_day: 30,
            },
            TestCase {
                iso_year: 1,
                iso_month: 1,
                iso_day: 1,
                expected_year: -78,
                expected_month: 10,
                expected_day: 11,
            },
            TestCase {
                iso_year: 0,
                iso_month: 3,
                iso_day: 21,
                expected_year: -78,
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                iso_year: 0,
                iso_month: 1,
                iso_day: 1,
                expected_year: -79,
                expected_month: 10,
                expected_day: 11,
            },
            TestCase {
                iso_year: -1,
                iso_month: 3,
                iso_day: 21,
                expected_year: -80,
                expected_month: 12,
                expected_day: 30,
            },
        ];

        for case in cases {
            check_case(case);
        }
    }

    #[test]
    fn test_roundtrip_near_rd_zero() {
        for i in -1000..=1000 {
            let initial = RataDie::new(i);
            let result = Iso::to_fixed(
                Iso::from_fixed(initial)
                    .to_calendar(Indian)
                    .to_calendar(Iso),
            );
            assert_eq!(
                initial, result,
                "Roundtrip failed for initial: {initial:?}, result: {result:?}"
            );
        }
    }

    #[test]
    fn test_roundtrip_near_epoch_start() {
        // Epoch start: RD 28570
        for i in 27570..=29570 {
            let initial = RataDie::new(i);
            let result = Iso::to_fixed(
                Iso::from_fixed(initial)
                    .to_calendar(Indian)
                    .to_calendar(Iso),
            );
            assert_eq!(
                initial, result,
                "Roundtrip failed for initial: {initial:?}, result: {result:?}"
            );
        }
    }

    #[test]
    fn test_directionality_near_rd_zero() {
        for i in -100..=100 {
            for j in -100..=100 {
                let rd_i = RataDie::new(i);
                let rd_j = RataDie::new(j);

                let indian_i = Iso::from_fixed(rd_i).to_calendar(Indian);
                let indian_j = Iso::from_fixed(rd_j).to_calendar(Indian);

                assert_eq!(i.cmp(&j), indian_i.cmp(&indian_j), "Directionality test failed for i: {i}, j: {j}, indian_i: {indian_i:?}, indian_j: {indian_j:?}");
            }
        }
    }

    #[test]
    fn test_directionality_near_epoch_start() {
        // Epoch start: RD 28570
        for i in 28470..=28670 {
            for j in 28470..=28670 {
                let indian_i = Iso::from_fixed(RataDie::new(i)).to_calendar(Indian);
                let indian_j = Iso::from_fixed(RataDie::new(j)).to_calendar(Indian);

                assert_eq!(i.cmp(&j), indian_i.cmp(&indian_j), "Directionality test failed for i: {i}, j: {j}, indian_i: {indian_i:?}, indian_j: {indian_j:?}");
            }
        }
    }
}
