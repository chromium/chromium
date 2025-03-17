// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Julian calendar.
//!
//! ```rust
//! use icu::calendar::{cal::Julian, Date};
//!
//! let date_iso = Date::try_new_iso(1970, 1, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//! let date_julian = Date::new_from_iso(date_iso, Julian);
//!
//! assert_eq!(date_julian.year().era_year_or_extended(), 1969);
//! assert_eq!(date_julian.month().ordinal, 12);
//! assert_eq!(date_julian.day_of_month().0, 20);
//! ```

use crate::any_calendar::AnyCalendarKind;
use crate::calendar_arithmetic::{ArithmeticDate, CalendarArithmetic};
use crate::error::DateError;
use crate::iso::Iso;
use crate::{types, Calendar, Date, DateDuration, DateDurationUnit, RangeError};
use calendrical_calculations::helpers::I32CastError;
use calendrical_calculations::rata_die::RataDie;
use tinystr::tinystr;

/// The [Julian Calendar]
///
/// The [Julian calendar] is a solar calendar that was used commonly historically, with twelve months.
///
/// This type can be used with [`Date`] to represent dates in this calendar.
///
/// [Julian calendar]: https://en.wikipedia.org/wiki/Julian_calendar
///
/// # Era codes
///
/// This calendar supports two era codes: `"bce"`, and `"ce"`, corresponding to the BCE/BC and CE/AD eras
///
/// # Month codes
///
/// This calendar supports 12 solar month codes (`"M01" - "M12"`)
#[derive(Copy, Clone, Debug, Hash, Default, Eq, PartialEq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Julian;

/// The inner date type used for representing [`Date`]s of [`Julian`]. See [`Date`] and [`Julian`] for more details.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq)]
// The inner date type used for representing Date<Julian>
pub struct JulianDateInner(pub(crate) ArithmeticDate<Julian>);

impl CalendarArithmetic for Julian {
    type YearInfo = ();

    fn month_days(year: i32, month: u8, _data: ()) -> u8 {
        match month {
            4 | 6 | 9 | 11 => 30,
            2 if Self::is_leap_year(year, ()) => 29,
            2 => 28,
            1 | 3 | 5 | 7 | 8 | 10 | 12 => 31,
            _ => 0,
        }
    }

    fn months_for_every_year(_: i32, _data: ()) -> u8 {
        12
    }

    fn is_leap_year(year: i32, _data: ()) -> bool {
        calendrical_calculations::julian::is_leap_year(year)
    }

    fn last_month_day_in_year(_year: i32, _data: ()) -> (u8, u8) {
        (12, 31)
    }

    fn days_in_provided_year(year: i32, _data: ()) -> u16 {
        if Self::is_leap_year(year, ()) {
            366
        } else {
            365
        }
    }
}

impl Calendar for Julian {
    type DateInner = JulianDateInner;
    fn date_from_codes(
        &self,
        era: Option<types::Era>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let year = if let Some(era) = era {
            if era.0 == tinystr!(16, "ce") || era.0 == tinystr!(16, "julian") {
                if year <= 0 {
                    return Err(DateError::Range {
                        field: "year",
                        value: year,
                        min: 1,
                        max: i32::MAX,
                    });
                }
                year
            } else if era.0 == tinystr!(16, "bce") || era.0 == tinystr!(16, "julian-inverse") {
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

        ArithmeticDate::new_from_codes(self, year, month_code, day).map(JulianDateInner)
    }
    fn date_from_iso(&self, iso: Date<Iso>) -> JulianDateInner {
        let fixed_iso = Iso::to_fixed(iso);
        Self::julian_from_fixed(fixed_iso)
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso> {
        let fixed_julian = Julian::fixed_from_julian(date.0);
        Iso::from_fixed(fixed_julian)
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
        Iso.day_of_week(Julian.date_to_iso(date).inner())
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

    /// The calendar-specific year represented by `date`
    /// Julian has the same era scheme as Gregorian
    fn year(&self, date: &Self::DateInner) -> types::YearInfo {
        year_as_julian(date.0.year)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::is_leap_year(date.0.year, ())
    }

    /// The calendar-specific month represented by `date`
    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        date.0.month()
    }

    /// The calendar-specific day-of-month represented by `date`
    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        date.0.day_of_month()
    }

    fn day_of_year_info(&self, date: &Self::DateInner) -> types::DayOfYearInfo {
        let prev_year = date.0.year - 1;
        let next_year = date.0.year + 1;
        types::DayOfYearInfo {
            day_of_year: date.0.day_of_year(),
            days_in_year: date.0.days_in_year(),
            prev_year: year_as_julian(prev_year),
            days_in_prev_year: Julian::days_in_year_direct(prev_year),
            next_year: year_as_julian(next_year),
        }
    }

    fn debug_name(&self) -> &'static str {
        "Julian"
    }

    fn any_calendar_kind(&self) -> Option<AnyCalendarKind> {
        None
    }
}

fn year_as_julian(year: i32) -> types::YearInfo {
    if year > 0 {
        types::YearInfo::new(
            year,
            types::EraYear {
                standard_era: tinystr!(16, "julian").into(),
                formatting_era: types::FormattingEra::Index(1, tinystr!(16, "AD")),
                era_year: year,
                ambiguity: types::YearAmbiguity::CenturyRequired,
            },
        )
    } else {
        types::YearInfo::new(
            year,
            types::EraYear {
                standard_era: tinystr!(16, "julian-inverse").into(),
                formatting_era: types::FormattingEra::Index(0, tinystr!(16, "BC")),
                era_year: 1_i32.saturating_sub(year),
                ambiguity: types::YearAmbiguity::EraAndCenturyRequired,
            },
        )
    }
}
impl Julian {
    /// Construct a new Julian Calendar
    pub fn new() -> Self {
        Self
    }

    // "Fixed" is a day count representation of calendars staring from Jan 1st of year 1 of the Georgian Calendar.
    pub(crate) const fn fixed_from_julian(date: ArithmeticDate<Julian>) -> RataDie {
        calendrical_calculations::julian::fixed_from_julian(date.year, date.month, date.day)
    }

    /// Convenience function so we can call days_in_year without
    /// needing to construct a full ArithmeticDate
    fn days_in_year_direct(year: i32) -> u16 {
        if Julian::is_leap_year(year, ()) {
            366
        } else {
            365
        }
    }

    fn julian_from_fixed(date: RataDie) -> JulianDateInner {
        let (year, month, day) = match calendrical_calculations::julian::julian_from_fixed(date) {
            Err(I32CastError::BelowMin) => return JulianDateInner(ArithmeticDate::min_date()),
            Err(I32CastError::AboveMax) => return JulianDateInner(ArithmeticDate::max_date()),
            Ok(ymd) => ymd,
        };
        JulianDateInner(ArithmeticDate::new_unchecked(year, month, day))
    }
}

impl Date<Julian> {
    /// Construct new Julian Date.
    ///
    /// Years are arithmetic, meaning there is a year 0. Zero and negative years are in BC, with year 0 = 1 BC
    ///
    /// ```rust
    /// use icu::calendar::Date;
    ///
    /// let date_julian = Date::try_new_julian(1969, 12, 20)
    ///     .expect("Failed to initialize Julian Date instance.");
    ///
    /// assert_eq!(date_julian.year().era_year_or_extended(), 1969);
    /// assert_eq!(date_julian.month().ordinal, 12);
    /// assert_eq!(date_julian.day_of_month().0, 20);
    /// ```
    pub fn try_new_julian(year: i32, month: u8, day: u8) -> Result<Date<Julian>, RangeError> {
        ArithmeticDate::new_from_ordinals(year, month, day)
            .map(JulianDateInner)
            .map(|inner| Date::from_raw(inner, Julian))
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use types::Era;

    #[test]
    fn test_day_iso_to_julian() {
        // March 1st 200 is same on both calendars
        let iso_date = Date::try_new_iso(200, 3, 1).unwrap();
        let julian_date = Julian.date_from_iso(iso_date);
        assert_eq!(julian_date.0.year, 200);
        assert_eq!(julian_date.0.month, 3);
        assert_eq!(julian_date.0.day, 1);

        // Feb 28th, 200 (iso) = Feb 29th, 200 (julian)
        let iso_date = Date::try_new_iso(200, 2, 28).unwrap();
        let julian_date = Julian.date_from_iso(iso_date);
        assert_eq!(julian_date.0.year, 200);
        assert_eq!(julian_date.0.month, 2);
        assert_eq!(julian_date.0.day, 29);

        // March 1st 400 (iso) = Feb 29th, 400 (julian)
        let iso_date = Date::try_new_iso(400, 3, 1).unwrap();
        let julian_date = Julian.date_from_iso(iso_date);
        assert_eq!(julian_date.0.year, 400);
        assert_eq!(julian_date.0.month, 2);
        assert_eq!(julian_date.0.day, 29);

        // Jan 1st, 2022 (iso) = Dec 19, 2021 (julian)
        let iso_date = Date::try_new_iso(2022, 1, 1).unwrap();
        let julian_date = Julian.date_from_iso(iso_date);
        assert_eq!(julian_date.0.year, 2021);
        assert_eq!(julian_date.0.month, 12);
        assert_eq!(julian_date.0.day, 19);
    }

    #[test]
    fn test_day_julian_to_iso() {
        // March 1st 200 is same on both calendars
        let julian_date = Date::try_new_julian(200, 3, 1).unwrap();
        let iso_date = Julian.date_to_iso(julian_date.inner());
        let iso_expected_date = Date::try_new_iso(200, 3, 1).unwrap();
        assert_eq!(iso_date, iso_expected_date);

        // Feb 28th, 200 (iso) = Feb 29th, 200 (julian)
        let julian_date = Date::try_new_julian(200, 2, 29).unwrap();
        let iso_date = Julian.date_to_iso(julian_date.inner());
        let iso_expected_date = Date::try_new_iso(200, 2, 28).unwrap();
        assert_eq!(iso_date, iso_expected_date);

        // March 1st 400 (iso) = Feb 29th, 400 (julian)
        let julian_date = Date::try_new_julian(400, 2, 29).unwrap();
        let iso_date = Julian.date_to_iso(julian_date.inner());
        let iso_expected_date = Date::try_new_iso(400, 3, 1).unwrap();
        assert_eq!(iso_date, iso_expected_date);

        // Jan 1st, 2022 (iso) = Dec 19, 2021 (julian)
        let julian_date = Date::try_new_julian(2021, 12, 19).unwrap();
        let iso_date = Julian.date_to_iso(julian_date.inner());
        let iso_expected_date = Date::try_new_iso(2022, 1, 1).unwrap();
        assert_eq!(iso_date, iso_expected_date);

        // March 1st, 2022 (iso) = Feb 16, 2022 (julian)
        let julian_date = Date::try_new_julian(2022, 2, 16).unwrap();
        let iso_date = Julian.date_to_iso(julian_date.inner());
        let iso_expected_date = Date::try_new_iso(2022, 3, 1).unwrap();
        assert_eq!(iso_date, iso_expected_date);
    }

    #[test]
    fn test_roundtrip_negative() {
        // https://github.com/unicode-org/icu4x/issues/2254
        let iso_date = Date::try_new_iso(-1000, 3, 3).unwrap();
        let julian = iso_date.to_calendar(Julian::new());
        let recovered_iso = julian.to_iso();
        assert_eq!(iso_date, recovered_iso);
    }

    #[test]
    fn test_julian_near_era_change() {
        // Tests that the Julian calendar gives the correct expected
        // day, month, and year for positive years (CE)

        #[derive(Debug)]
        struct TestCase {
            fixed_date: i64,
            iso_year: i32,
            iso_month: u8,
            iso_day: u8,
            expected_year: i32,
            expected_era: Era,
            expected_month: u8,
            expected_day: u8,
        }

        let cases = [
            TestCase {
                fixed_date: 1,
                iso_year: 1,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "julian")),
                expected_month: 1,
                expected_day: 3,
            },
            TestCase {
                fixed_date: 0,
                iso_year: 0,
                iso_month: 12,
                iso_day: 31,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "julian")),
                expected_month: 1,
                expected_day: 2,
            },
            TestCase {
                fixed_date: -1,
                iso_year: 0,
                iso_month: 12,
                iso_day: 30,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "julian")),
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                fixed_date: -2,
                iso_year: 0,
                iso_month: 12,
                iso_day: 29,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "julian-inverse")),
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                fixed_date: -3,
                iso_year: 0,
                iso_month: 12,
                iso_day: 28,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "julian-inverse")),
                expected_month: 12,
                expected_day: 30,
            },
            TestCase {
                fixed_date: -367,
                iso_year: -1,
                iso_month: 12,
                iso_day: 30,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "julian-inverse")),
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                fixed_date: -368,
                iso_year: -1,
                iso_month: 12,
                iso_day: 29,
                expected_year: 2,
                expected_era: Era(tinystr!(16, "julian-inverse")),
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                fixed_date: -1462,
                iso_year: -4,
                iso_month: 12,
                iso_day: 30,
                expected_year: 4,
                expected_era: Era(tinystr!(16, "julian-inverse")),
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                fixed_date: -1463,
                iso_year: -4,
                iso_month: 12,
                iso_day: 29,
                expected_year: 5,
                expected_era: Era(tinystr!(16, "julian-inverse")),
                expected_month: 12,
                expected_day: 31,
            },
        ];

        for case in cases {
            let iso_from_fixed: Date<Iso> = Iso::from_fixed(RataDie::new(case.fixed_date));
            let julian_from_fixed: Date<Julian> = Date::new_from_iso(iso_from_fixed, Julian);
            assert_eq!(julian_from_fixed.year().era_year().unwrap(), case.expected_year,
                "Failed year check from fixed: {case:?}\nISO: {iso_from_fixed:?}\nJulian: {julian_from_fixed:?}");
            assert_eq!(julian_from_fixed.year().standard_era().unwrap(), case.expected_era,
                "Failed era check from fixed: {case:?}\nISO: {iso_from_fixed:?}\nJulian: {julian_from_fixed:?}");
            assert_eq!(julian_from_fixed.month().ordinal, case.expected_month,
                "Failed month check from fixed: {case:?}\nISO: {iso_from_fixed:?}\nJulian: {julian_from_fixed:?}");
            assert_eq!(julian_from_fixed.day_of_month().0, case.expected_day,
                "Failed day check from fixed: {case:?}\nISO: {iso_from_fixed:?}\nJulian: {julian_from_fixed:?}");

            let iso_date_man: Date<Iso> =
                Date::try_new_iso(case.iso_year, case.iso_month, case.iso_day)
                    .expect("Failed to initialize ISO date for {case:?}");
            let julian_date_man: Date<Julian> = Date::new_from_iso(iso_date_man, Julian);
            assert_eq!(iso_from_fixed, iso_date_man,
                "ISO from fixed not equal to ISO generated from manually-input ymd\nCase: {case:?}\nFixed: {iso_from_fixed:?}\nMan: {iso_date_man:?}");
            assert_eq!(julian_from_fixed, julian_date_man,
                "Julian from fixed not equal to Julian generated from manually-input ymd\nCase: {case:?}\nFixed: {julian_from_fixed:?}\nMan: {julian_date_man:?}");
        }
    }

    #[test]
    fn test_julian_fixed_date_conversion() {
        // Tests that converting from fixed date to Julian then
        // back to fixed date yields the same fixed date
        for i in -10000..=10000 {
            let fixed = RataDie::new(i);
            let julian = Julian::julian_from_fixed(fixed);
            let new_fixed = Julian::fixed_from_julian(julian.0);
            assert_eq!(fixed, new_fixed);
        }
    }

    #[test]
    fn test_julian_directionality() {
        // Tests that for a large range of fixed dates, if a fixed date
        // is less than another, the corresponding YMD should also be less
        // than the other, without exception.
        for i in -100..=100 {
            for j in -100..=100 {
                let julian_i = Julian::julian_from_fixed(RataDie::new(i)).0;
                let julian_j = Julian::julian_from_fixed(RataDie::new(j)).0;

                assert_eq!(
                    i.cmp(&j),
                    julian_i.cmp(&julian_j),
                    "Julian directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }

    #[test]
    fn test_hebrew_epoch() {
        assert_eq!(
            calendrical_calculations::julian::fixed_from_julian_book_version(-3761, 10, 7),
            RataDie::new(-1373427)
        );
    }

    #[test]
    fn test_julian_leap_years() {
        assert!(Julian::is_leap_year(4, ()));
        assert!(Julian::is_leap_year(0, ()));
        assert!(Julian::is_leap_year(-4, ()));

        Date::try_new_julian(2020, 2, 29).unwrap();
    }
}
