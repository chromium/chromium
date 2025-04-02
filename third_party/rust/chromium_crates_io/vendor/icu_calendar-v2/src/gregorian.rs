// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Gregorian calendar.
//!
//! ```rust
//! use icu::calendar::{cal::Gregorian, Date};
//!
//! let date_iso = Date::try_new_iso(1970, 1, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//! let date_gregorian = Date::new_from_iso(date_iso, Gregorian);
//!
//! assert_eq!(date_gregorian.year().era_year_or_extended(), 1970);
//! assert_eq!(date_gregorian.month().ordinal, 1);
//! assert_eq!(date_gregorian.day_of_month().0, 2);
//! ```

use crate::calendar_arithmetic::ArithmeticDate;
use crate::error::DateError;
use crate::iso::{Iso, IsoDateInner};
use crate::{types, Calendar, Date, DateDuration, DateDurationUnit, RangeError};
use tinystr::tinystr;

/// The Gregorian Calendar
///
/// The [Gregorian calendar] is a solar calendar used by most of the world, with twelve months.
///
/// This type can be used with [`Date`] to represent dates in this calendar.
///
/// [Gregorian calendar]: https://en.wikipedia.org/wiki/Gregorian_calendar
///
/// # Era codes
///
/// This calendar supports two era codes: `"bce"`, and `"ce"`, corresponding to the BCE and CE eras
#[derive(Copy, Clone, Debug, Default)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Gregorian;

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
/// The inner date type used for representing [`Date`]s of [`Gregorian`]. See [`Date`] and [`Gregorian`] for more details.
pub struct GregorianDateInner(pub(crate) IsoDateInner);

impl Calendar for Gregorian {
    type DateInner = GregorianDateInner;
    fn date_from_codes(
        &self,
        era: Option<types::Era>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let year = if let Some(era) = era {
            if era.0 == tinystr!(16, "ce") {
                if year <= 0 {
                    return Err(DateError::Range {
                        field: "year",
                        value: year,
                        min: 1,
                        max: i32::MAX,
                    });
                }
                year
            } else if era.0 == tinystr!(16, "bce") {
                if year <= 0 {
                    return Err(DateError::Range {
                        field: "year",
                        value: year,
                        min: 1,
                        max: i32::MAX,
                    });
                }
                1 - year
            } else {
                return Err(DateError::UnknownEra(era));
            }
        } else {
            year
        };

        ArithmeticDate::new_from_codes(self, year, month_code, day)
            .map(IsoDateInner)
            .map(GregorianDateInner)
    }

    fn date_from_iso(&self, iso: Date<Iso>) -> GregorianDateInner {
        GregorianDateInner(*iso.inner())
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso> {
        Date::from_raw(date.0, Iso)
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        Iso.months_in_year(&date.0)
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        Iso.days_in_year(&date.0)
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        Iso.days_in_month(&date.0)
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        Iso.offset_date(&mut date.0, offset.cast_unit())
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
        Iso.until(&date1.0, &date2.0, &Iso, largest_unit, smallest_unit)
            .cast_unit()
    }
    /// The calendar-specific year represented by `date`
    fn year(&self, date: &Self::DateInner) -> types::YearInfo {
        year_as_gregorian(date.0 .0.year)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Iso.is_in_leap_year(&date.0)
    }

    /// The calendar-specific month represented by `date`
    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        Iso.month(&date.0)
    }

    /// The calendar-specific day-of-month represented by `date`
    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        Iso.day_of_month(&date.0)
    }

    /// Information of the day of the year
    fn day_of_year_info(&self, date: &Self::DateInner) -> types::DayOfYearInfo {
        let prev_year = date.0 .0.year.saturating_sub(1);
        let next_year = date.0 .0.year.saturating_add(1);
        types::DayOfYearInfo {
            day_of_year: Iso::day_of_year(date.0),
            days_in_year: Iso::days_in_year_direct(date.0 .0.year),
            prev_year: year_as_gregorian(prev_year),
            days_in_prev_year: Iso::days_in_year_direct(prev_year),
            next_year: year_as_gregorian(next_year),
        }
    }

    fn debug_name(&self) -> &'static str {
        "Gregorian"
    }

    fn any_calendar_kind(&self) -> Option<crate::AnyCalendarKind> {
        Some(crate::any_calendar::IntoAnyCalendar::kind(self))
    }
}

impl Date<Gregorian> {
    /// Construct a new Gregorian Date.
    ///
    /// Years are specified as ISO years.
    ///
    /// ```rust
    /// use icu::calendar::Date;
    ///
    /// // Conversion from ISO to Gregorian
    /// let date_gregorian = Date::try_new_gregorian(1970, 1, 2)
    ///     .expect("Failed to initialize Gregorian Date instance.");
    ///
    /// assert_eq!(date_gregorian.year().era_year_or_extended(), 1970);
    /// assert_eq!(date_gregorian.month().ordinal, 1);
    /// assert_eq!(date_gregorian.day_of_month().0, 2);
    /// ```
    pub fn try_new_gregorian(year: i32, month: u8, day: u8) -> Result<Date<Gregorian>, RangeError> {
        Date::try_new_iso(year, month, day).map(|d| Date::new_from_iso(d, Gregorian))
    }
}

fn year_as_gregorian(year: i32) -> types::YearInfo {
    if year > 0 {
        types::YearInfo::new(
            year,
            types::EraYear {
                standard_era: tinystr!(16, "gregory").into(),
                formatting_era: types::FormattingEra::Index(1, tinystr!(16, "CE")),
                era_year: year,
                ambiguity: match year {
                    ..=999 => types::YearAmbiguity::EraAndCenturyRequired,
                    1000..=1949 => types::YearAmbiguity::CenturyRequired,
                    1950..=2049 => types::YearAmbiguity::Unambiguous,
                    2050.. => types::YearAmbiguity::CenturyRequired,
                },
            },
        )
    } else {
        types::YearInfo::new(
            year,
            types::EraYear {
                standard_era: tinystr!(16, "gregory-inverse").into(),
                formatting_era: types::FormattingEra::Index(0, tinystr!(16, "BCE")),
                era_year: 1_i32.saturating_sub(year),
                ambiguity: types::YearAmbiguity::EraAndCenturyRequired,
            },
        )
    }
}

#[cfg(test)]
mod test {
    use calendrical_calculations::rata_die::RataDie;

    use super::*;
    use types::Era;

    #[test]
    fn day_of_year_info_max() {
        #[derive(Debug)]
        struct MaxCase {
            year: i32,
            month: u8,
            day: u8,
            next_era_year: i32,
            era: &'static str,
        }
        let cases = [
            MaxCase {
                year: i32::MAX,
                month: 7,
                day: 11,
                next_era_year: i32::MAX,
                era: "gregory",
            },
            MaxCase {
                year: i32::MAX,
                month: 7,
                day: 12,
                next_era_year: i32::MAX,
                era: "gregory",
            },
            MaxCase {
                year: i32::MAX,
                month: 8,
                day: 10,
                next_era_year: i32::MAX,
                era: "gregory",
            },
            MaxCase {
                year: i32::MAX - 1,
                month: 7,
                day: 11,
                next_era_year: i32::MAX,
                era: "gregory",
            },
            MaxCase {
                year: -2,
                month: 1,
                day: 1,
                next_era_year: 2,
                era: "gregory-inverse",
            },
            MaxCase {
                year: -1,
                month: 1,
                day: 1,
                next_era_year: 1,
                era: "gregory-inverse",
            },
            MaxCase {
                year: 0,
                month: 1,
                day: 1,
                next_era_year: 1,
                era: "gregory",
            },
            MaxCase {
                year: 1,
                month: 1,
                day: 1,
                next_era_year: 2,
                era: "gregory",
            },
            MaxCase {
                year: 2000,
                month: 6,
                day: 15,
                next_era_year: 2001,
                era: "gregory",
            },
            MaxCase {
                year: 2020,
                month: 12,
                day: 31,
                next_era_year: 2021,
                era: "gregory",
            },
        ];

        for case in cases {
            let date = Date::try_new_gregorian(case.year, case.month, case.day).unwrap();

            assert_eq!(
                Calendar::day_of_year_info(&Gregorian, &date.inner)
                    .next_year
                    .era_year()
                    .unwrap(),
                case.next_era_year,
                "{case:?}",
            );
            assert_eq!(
                Calendar::day_of_year_info(&Gregorian, &date.inner)
                    .next_year
                    .standard_era()
                    .unwrap()
                    .0,
                case.era,
                "{case:?}",
            );
        }
    }

    #[derive(Debug)]
    struct TestCase {
        fixed_date: RataDie,
        iso_year: i32,
        iso_month: u8,
        iso_day: u8,
        expected_year: i32,
        expected_era: Era,
        expected_month: u8,
        expected_day: u8,
    }

    fn check_test_case(case: TestCase) {
        let iso_from_fixed: Date<Iso> = Iso::from_fixed(case.fixed_date);
        let greg_date_from_fixed: Date<Gregorian> = Date::new_from_iso(iso_from_fixed, Gregorian);
        assert_eq!(greg_date_from_fixed.year().era_year_or_extended(), case.expected_year,
            "Failed year check from fixed: {case:?}\nISO: {iso_from_fixed:?}\nGreg: {greg_date_from_fixed:?}");
        assert_eq!(greg_date_from_fixed.year().standard_era().unwrap(), case.expected_era,
            "Failed era check from fixed: {case:?}\nISO: {iso_from_fixed:?}\nGreg: {greg_date_from_fixed:?}");
        assert_eq!(greg_date_from_fixed.month().ordinal, case.expected_month,
            "Failed month check from fixed: {case:?}\nISO: {iso_from_fixed:?}\nGreg: {greg_date_from_fixed:?}");
        assert_eq!(greg_date_from_fixed.day_of_month().0, case.expected_day,
            "Failed day check from fixed: {case:?}\nISO: {iso_from_fixed:?}\nGreg: {greg_date_from_fixed:?}");

        let iso_date_man: Date<Iso> =
            Date::try_new_iso(case.iso_year, case.iso_month, case.iso_day)
                .expect("Failed to initialize ISO date for {case:?}");
        let greg_date_man: Date<Gregorian> = Date::new_from_iso(iso_date_man, Gregorian);
        assert_eq!(iso_from_fixed, iso_date_man,
            "ISO from fixed not equal to ISO generated from manually-input ymd\nCase: {case:?}\nFixed: {iso_from_fixed:?}\nMan: {iso_date_man:?}");
        assert_eq!(greg_date_from_fixed, greg_date_man,
            "Greg. date from fixed not equal to Greg. generated from manually-input ymd\nCase: {case:?}\nFixed: {greg_date_from_fixed:?}\nMan: {greg_date_man:?}");
    }

    #[test]
    fn test_gregorian_ce() {
        // Tests that the Gregorian calendar gives the correct expected
        // day, month, and year for positive years (AD/CE/gregory era)

        let cases = [
            TestCase {
                fixed_date: RataDie::new(1),
                iso_year: 1,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "gregory")),
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                fixed_date: RataDie::new(181),
                iso_year: 1,
                iso_month: 6,
                iso_day: 30,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "gregory")),
                expected_month: 6,
                expected_day: 30,
            },
            TestCase {
                fixed_date: RataDie::new(1155),
                iso_year: 4,
                iso_month: 2,
                iso_day: 29,
                expected_year: 4,
                expected_era: Era(tinystr!(16, "gregory")),
                expected_month: 2,
                expected_day: 29,
            },
            TestCase {
                fixed_date: RataDie::new(1344),
                iso_year: 4,
                iso_month: 9,
                iso_day: 5,
                expected_year: 4,
                expected_era: Era(tinystr!(16, "gregory")),
                expected_month: 9,
                expected_day: 5,
            },
            TestCase {
                fixed_date: RataDie::new(36219),
                iso_year: 100,
                iso_month: 3,
                iso_day: 1,
                expected_year: 100,
                expected_era: Era(tinystr!(16, "gregory")),
                expected_month: 3,
                expected_day: 1,
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }

    #[test]
    fn day_of_year_info_min() {
        #[derive(Debug)]
        struct MinCase {
            year: i32,
            month: u8,
            day: u8,
            prev_era_year: i32,
            era: &'static str,
        }
        let cases = [
            MinCase {
                year: i32::MIN + 4,
                month: 1,
                day: 1,
                prev_era_year: i32::MAX - 1,
                era: "gregory-inverse",
            },
            MinCase {
                year: i32::MIN + 3,
                month: 12,
                day: 31,
                prev_era_year: i32::MAX,
                era: "gregory-inverse",
            },
            MinCase {
                year: i32::MIN + 2,
                month: 2,
                day: 2,
                prev_era_year: i32::MAX,
                era: "gregory-inverse",
            },
            MinCase {
                year: i32::MIN + 1,
                month: 1,
                day: 1,
                prev_era_year: i32::MAX,
                era: "gregory-inverse",
            },
            MinCase {
                year: i32::MIN,
                month: 1,
                day: 1,
                prev_era_year: i32::MAX,
                era: "gregory-inverse",
            },
            MinCase {
                year: 3,
                month: 1,
                day: 1,
                prev_era_year: 2,
                era: "gregory",
            },
            MinCase {
                year: 2,
                month: 1,
                day: 1,
                prev_era_year: 1,
                era: "gregory",
            },
            MinCase {
                year: 1,
                month: 1,
                day: 1,
                prev_era_year: 1,
                era: "gregory-inverse",
            },
            MinCase {
                year: 0,
                month: 1,
                day: 1,
                prev_era_year: 2,
                era: "gregory-inverse",
            },
            MinCase {
                year: -2000,
                month: 6,
                day: 15,
                prev_era_year: 2002,
                era: "gregory-inverse",
            },
            MinCase {
                year: 2020,
                month: 12,
                day: 31,
                prev_era_year: 2019,
                era: "gregory",
            },
        ];

        for case in cases {
            let date = Date::try_new_gregorian(case.year, case.month, case.day).unwrap();

            assert_eq!(
                Calendar::day_of_year_info(&Gregorian, &date.inner)
                    .prev_year
                    .era_year()
                    .unwrap(),
                case.prev_era_year,
                "{case:?}",
            );
            assert_eq!(
                Calendar::day_of_year_info(&Gregorian, &date.inner)
                    .prev_year
                    .standard_era()
                    .unwrap()
                    .0,
                case.era,
                "{case:?}",
            );
        }
    }

    #[test]
    fn test_gregorian_bce() {
        // Tests that the Gregorian calendar gives the correct expected
        // day, month, and year for negative years (BC/BCE/pre-gregory era)

        let cases = [
            TestCase {
                fixed_date: RataDie::new(0),
                iso_year: 0,
                iso_month: 12,
                iso_day: 31,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "gregory-inverse")),
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                fixed_date: RataDie::new(-365), // This is a leap year
                iso_year: 0,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "gregory-inverse")),
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                fixed_date: RataDie::new(-366),
                iso_year: -1,
                iso_month: 12,
                iso_day: 31,
                expected_year: 2,
                expected_era: Era(tinystr!(16, "gregory-inverse")),
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                fixed_date: RataDie::new(-1461),
                iso_year: -4,
                iso_month: 12,
                iso_day: 31,
                expected_year: 5,
                expected_era: Era(tinystr!(16, "gregory-inverse")),
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                fixed_date: RataDie::new(-1826),
                iso_year: -4,
                iso_month: 1,
                iso_day: 1,
                expected_year: 5,
                expected_era: Era(tinystr!(16, "gregory-inverse")),
                expected_month: 1,
                expected_day: 1,
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }

    #[test]
    fn check_gregorian_directionality() {
        // Tests that for a large range of fixed dates, if a fixed date
        // is less than another, the corresponding YMD should also be less
        // than the other, without exception.
        for i in -100..100 {
            for j in -100..100 {
                let iso_i: Date<Iso> = Iso::from_fixed(RataDie::new(i));
                let iso_j: Date<Iso> = Iso::from_fixed(RataDie::new(j));

                let greg_i: Date<Gregorian> = Date::new_from_iso(iso_i, Gregorian);
                let greg_j: Date<Gregorian> = Date::new_from_iso(iso_j, Gregorian);

                assert_eq!(
                    i.cmp(&j),
                    iso_i.cmp(&iso_j),
                    "ISO directionality inconsistent with directionality for i: {i}, j: {j}"
                );
                assert_eq!(
                    i.cmp(&j),
                    greg_i.cmp(&greg_j),
                    "Gregorian directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }
}
