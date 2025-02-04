// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Republic of China calendar.
//!
//! ```rust
//! use icu::calendar::{cal::Roc, Date, DateTime};
//!
//! // `Date` type
//! let date_iso = Date::try_new_iso(1970, 1, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//! let date_roc = Date::new_from_iso(date_iso, Roc);
//!
//! // `DateTime` type
//! let datetime_iso = DateTime::try_new_iso(1970, 1, 2, 13, 1, 0)
//!     .expect("Failed to initialize ISO DateTime instance.");
//! let datetime_roc = DateTime::new_from_iso(datetime_iso, Roc);
//!
//! // `Date` checks
//! assert_eq!(date_roc.year().era_year_or_extended(), 59);
//! assert_eq!(date_roc.month().ordinal, 1);
//! assert_eq!(date_roc.day_of_month().0, 2);
//!
//! // `DateTime` checks
//! assert_eq!(datetime_roc.date.year().era_year_or_extended(), 59);
//! assert_eq!(datetime_roc.date.month().ordinal, 1);
//! assert_eq!(datetime_roc.date.day_of_month().0, 2);
//! assert_eq!(datetime_roc.time.hour.number(), 13);
//! assert_eq!(datetime_roc.time.minute.number(), 1);
//! assert_eq!(datetime_roc.time.second.number(), 0);
//! ```

use crate::{
    calendar_arithmetic::ArithmeticDate, error::DateError, iso::IsoDateInner, types, Calendar,
    Date, DateTime, Iso, RangeError, Time,
};
use calendrical_calculations::helpers::i64_to_saturated_i32;
use tinystr::tinystr;

/// Year of the beginning of the Taiwanese (ROC/Minguo) calendar.
/// 1912 ISO = ROC 1
const ROC_ERA_OFFSET: i32 = 1911;

/// The Republic of China (ROC) Calendar
///
/// The [Republic of China calendar] is a solar calendar used in Taiwan and Penghu, as well as by overseas diaspora from
/// those locations. Months and days are identical to the [`Gregorian`] calendar, while years are counted
/// with 1912, the year of the establishment of the Republic of China, as year 1 of the ROC/Minguo/民国/民國 era.
///
/// [Republic of China calendar]: https://en.wikipedia.org/wiki/Republic_of_China_calendar
///
/// The Republic of China calendar should not be confused with the Chinese traditional lunar calendar
/// (see [`Chinese`]).
///
/// # Era codes
///
/// This calendar supports two era codes: `"roc"`, corresponding to years in the 民國 (minguo) era (CE year 1912 and
/// after), and `"roc-inverse"`, corresponding to years before the 民國 (minguo) era (CE year 1911 and before).
///
///
/// # Month codes
///
/// This calendar supports 12 solar month codes (`"M01" - "M12"`)
///
/// [`Chinese`]: crate::chinese::Chinese
/// [`Gregorian`]: crate::Gregorian
#[derive(Copy, Clone, Debug, Default)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Roc;

/// The inner date type used for representing [`Date`]s of [`Roc`]. See [`Date`] and [`Roc`] for more info.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct RocDateInner(IsoDateInner);

impl Calendar for Roc {
    type DateInner = RocDateInner;

    fn date_from_codes(
        &self,
        era: Option<crate::types::Era>,
        year: i32,
        month_code: crate::types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let year = if let Some(era) = era {
            if era.0 == tinystr!(16, "roc") {
                if year <= 0 {
                    return Err(DateError::Range {
                        field: "year",
                        value: year,
                        min: 1,
                        max: i32::MAX,
                    });
                }
                year + ROC_ERA_OFFSET
            } else if era.0 == tinystr!(16, "roc-inverse") {
                if year <= 0 {
                    return Err(DateError::Range {
                        field: "year",
                        value: year,
                        min: 1,
                        max: i32::MAX,
                    });
                }
                1 - year + ROC_ERA_OFFSET
            } else {
                return Err(DateError::UnknownEra(era));
            }
        } else {
            year
        };

        ArithmeticDate::new_from_codes(self, year, month_code, day)
            .map(IsoDateInner)
            .map(RocDateInner)
    }

    fn date_from_iso(&self, iso: crate::Date<crate::Iso>) -> Self::DateInner {
        RocDateInner(*iso.inner())
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> crate::Date<crate::Iso> {
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

    fn offset_date(&self, date: &mut Self::DateInner, offset: crate::DateDuration<Self>) {
        Iso.offset_date(&mut date.0, offset.cast_unit())
    }

    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        _calendar2: &Self,
        largest_unit: crate::DateDurationUnit,
        smallest_unit: crate::DateDurationUnit,
    ) -> crate::DateDuration<Self> {
        Iso.until(&date1.0, &date2.0, &Iso, largest_unit, smallest_unit)
            .cast_unit()
    }

    fn debug_name(&self) -> &'static str {
        "ROC"
    }

    fn year(&self, date: &Self::DateInner) -> crate::types::YearInfo {
        year_as_roc(date.0 .0.year as i64)
    }
    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Iso.is_in_leap_year(&date.0)
    }

    fn month(&self, date: &Self::DateInner) -> crate::types::MonthInfo {
        Iso.month(&date.0)
    }

    fn day_of_month(&self, date: &Self::DateInner) -> crate::types::DayOfMonth {
        Iso.day_of_month(&date.0)
    }

    fn day_of_year_info(&self, date: &Self::DateInner) -> crate::types::DayOfYearInfo {
        let prev_year = date.0 .0.year.saturating_sub(1);
        let next_year = date.0 .0.year.saturating_add(1);
        types::DayOfYearInfo {
            day_of_year: Iso::day_of_year(date.0),
            days_in_year: Iso::days_in_year_direct(date.0 .0.year),
            prev_year: year_as_roc(prev_year as i64),
            days_in_prev_year: Iso::days_in_year_direct(prev_year),
            next_year: year_as_roc(next_year as i64),
        }
    }

    fn any_calendar_kind(&self) -> Option<crate::AnyCalendarKind> {
        Some(crate::any_calendar::IntoAnyCalendar::kind(self))
    }
}

impl Date<Roc> {
    /// Construct a new Republic of China calendar Date.
    ///
    /// Years are specified in the "roc" era. This function accepts an extended year in that era, so dates
    /// before Minguo are negative and year 0 is 1 Before Minguo. To specify dates using explicit era
    /// codes, use [`Roc::date_from_codes()`].
    ///
    /// ```rust
    /// use icu::calendar::Date;
    /// use icu::calendar::cal::Gregorian;
    /// use tinystr::tinystr;
    ///
    /// // Create a new ROC Date
    /// let date_roc = Date::try_new_roc(1, 2, 3)
    ///     .expect("Failed to initialize ROC Date instance.");
    ///
    /// assert_eq!(date_roc.year().standard_era().unwrap().0, tinystr!(16, "roc"));
    /// assert_eq!(date_roc.year().era_year_or_extended(), 1, "ROC year check failed!");
    /// assert_eq!(date_roc.month().ordinal, 2, "ROC month check failed!");
    /// assert_eq!(date_roc.day_of_month().0, 3, "ROC day of month check failed!");
    ///
    /// // Convert to an equivalent Gregorian date
    /// let date_gregorian = date_roc.to_calendar(Gregorian);
    ///
    /// assert_eq!(date_gregorian.year().era_year_or_extended(), 1912, "Gregorian from ROC year check failed!");
    /// assert_eq!(date_gregorian.month().ordinal, 2, "Gregorian from ROC month check failed!");
    /// assert_eq!(date_gregorian.day_of_month().0, 3, "Gregorian from ROC day of month check failed!");
    pub fn try_new_roc(year: i32, month: u8, day: u8) -> Result<Date<Roc>, RangeError> {
        let iso_year = year.saturating_add(ROC_ERA_OFFSET);
        Date::try_new_iso(iso_year, month, day).map(|d| Date::new_from_iso(d, Roc))
    }
}

impl DateTime<Roc> {
    /// Construct a new Republic of China calendar datetime from integers.
    ///
    /// Years are specified in the "roc" era, Before Minguo dates are negative (year 0 is 1 Before Minguo)
    ///
    /// ```rust
    /// use icu::calendar::DateTime;
    /// use tinystr::tinystr;
    ///
    /// // Create a new ROC DateTime
    /// let datetime_roc = DateTime::try_new_roc(1, 2, 3, 13, 1, 0)
    ///     .expect("Failed to initialize ROC DateTime instance.");
    ///
    /// assert_eq!(
    ///     datetime_roc.date.year().standard_era().unwrap().0,
    ///     tinystr!(16, "roc")
    /// );
    /// assert_eq!(
    ///     datetime_roc.date.year().era_year_or_extended(),
    ///     1,
    ///     "ROC year check failed!"
    /// );
    /// assert_eq!(
    ///     datetime_roc.date.month().ordinal,
    ///     2,
    ///     "ROC month check failed!"
    /// );
    /// assert_eq!(
    ///     datetime_roc.date.day_of_month().0,
    ///     3,
    ///     "ROC day of month check failed!"
    /// );
    /// assert_eq!(datetime_roc.time.hour.number(), 13);
    /// assert_eq!(datetime_roc.time.minute.number(), 1);
    /// assert_eq!(datetime_roc.time.second.number(), 0);
    /// ```
    pub fn try_new_roc(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
    ) -> Result<DateTime<Roc>, DateError> {
        Ok(DateTime {
            date: Date::try_new_roc(year, month, day)?,
            time: Time::try_new(hour, minute, second, 0)?,
        })
    }
}

pub(crate) fn year_as_roc(year: i64) -> types::YearInfo {
    let year_i32 = i64_to_saturated_i32(year);
    let offset_i64 = ROC_ERA_OFFSET as i64;
    if year > offset_i64 {
        types::YearInfo::new(
            year_i32,
            types::EraYear {
                standard_era: tinystr!(16, "roc").into(),
                formatting_era: types::FormattingEra::Index(1, tinystr!(16, "ROC")),
                era_year: year_i32.saturating_sub(ROC_ERA_OFFSET),
                ambiguity: types::YearAmbiguity::CenturyRequired,
            },
        )
    } else {
        types::YearInfo::new(
            year_i32,
            types::EraYear {
                standard_era: tinystr!(16, "roc-inverse").into(),
                formatting_era: types::FormattingEra::Index(0, tinystr!(16, "B. ROC")),
                era_year: (ROC_ERA_OFFSET + 1).saturating_sub(year_i32),
                ambiguity: types::YearAmbiguity::EraAndCenturyRequired,
            },
        )
    }
}

#[cfg(test)]
mod test {

    use super::*;
    use crate::types::Era;
    use calendrical_calculations::rata_die::RataDie;

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
        let iso_from_fixed = Iso::iso_from_fixed(case.fixed_date);
        let roc_from_fixed = Date::new_from_iso(iso_from_fixed, Roc);
        assert_eq!(roc_from_fixed.year().era_year().unwrap(), case.expected_year,
            "Failed year check from fixed: {case:?}\nISO: {iso_from_fixed:?}\nROC: {roc_from_fixed:?}");
        assert_eq!(roc_from_fixed.year().standard_era().unwrap(), case.expected_era,
            "Failed era check from fixed: {case:?}\nISO: {iso_from_fixed:?}\nROC: {roc_from_fixed:?}");
        assert_eq!(roc_from_fixed.month().ordinal, case.expected_month,
            "Failed month check from fixed: {case:?}\nISO: {iso_from_fixed:?}\nROC: {roc_from_fixed:?}");
        assert_eq!(roc_from_fixed.day_of_month().0, case.expected_day,
            "Failed day_of_month check from fixed: {case:?}\nISO: {iso_from_fixed:?}\nROC: {roc_from_fixed:?}");

        let iso_from_case = Date::try_new_iso(case.iso_year, case.iso_month, case.iso_day)
            .expect("Failed to initialize ISO date for {case:?}");
        let roc_from_case = Date::new_from_iso(iso_from_case, Roc);
        assert_eq!(iso_from_fixed, iso_from_case,
            "ISO from fixed not equal to ISO generated from manually-input ymd\nCase: {case:?}\nFixed: {iso_from_fixed:?}\nManual: {iso_from_case:?}");
        assert_eq!(roc_from_fixed, roc_from_case,
            "ROC date from fixed not equal to ROC generated from manually-input ymd\nCase: {case:?}\nFixed: {roc_from_fixed:?}\nManual: {roc_from_case:?}");
    }

    #[test]
    fn test_roc_current_era() {
        // Tests that the ROC calendar gives the correct expected day, month, and year for years >= 1912
        // (years in the ROC/minguo era)
        //
        // Jan 1. 1912 CE = RD 697978

        let cases = [
            TestCase {
                fixed_date: RataDie::new(697978),
                iso_year: 1912,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "roc")),
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                fixed_date: RataDie::new(698037),
                iso_year: 1912,
                iso_month: 2,
                iso_day: 29,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "roc")),
                expected_month: 2,
                expected_day: 29,
            },
            TestCase {
                fixed_date: RataDie::new(698524),
                iso_year: 1913,
                iso_month: 6,
                iso_day: 30,
                expected_year: 2,
                expected_era: Era(tinystr!(16, "roc")),
                expected_month: 6,
                expected_day: 30,
            },
            TestCase {
                fixed_date: RataDie::new(738714),
                iso_year: 2023,
                iso_month: 7,
                iso_day: 13,
                expected_year: 112,
                expected_era: Era(tinystr!(16, "roc")),
                expected_month: 7,
                expected_day: 13,
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }

    #[test]
    fn test_roc_prior_era() {
        // Tests that the ROC calendar gives the correct expected day, month, and year for years <= 1911
        // (years in the ROC/minguo era)
        //
        // Jan 1. 1912 CE = RD 697978
        let cases = [
            TestCase {
                fixed_date: RataDie::new(697977),
                iso_year: 1911,
                iso_month: 12,
                iso_day: 31,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "roc-inverse")),
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                fixed_date: RataDie::new(697613),
                iso_year: 1911,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1,
                expected_era: Era(tinystr!(16, "roc-inverse")),
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                fixed_date: RataDie::new(697612),
                iso_year: 1910,
                iso_month: 12,
                iso_day: 31,
                expected_year: 2,
                expected_era: Era(tinystr!(16, "roc-inverse")),
                expected_month: 12,
                expected_day: 31,
            },
            TestCase {
                fixed_date: RataDie::new(696576),
                iso_year: 1908,
                iso_month: 2,
                iso_day: 29,
                expected_year: 4,
                expected_era: Era(tinystr!(16, "roc-inverse")),
                expected_month: 2,
                expected_day: 29,
            },
            TestCase {
                fixed_date: RataDie::new(1),
                iso_year: 1,
                iso_month: 1,
                iso_day: 1,
                expected_year: 1911,
                expected_era: Era(tinystr!(16, "roc-inverse")),
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                fixed_date: RataDie::new(0),
                iso_year: 0,
                iso_month: 12,
                iso_day: 31,
                expected_year: 1912,
                expected_era: Era(tinystr!(16, "roc-inverse")),
                expected_month: 12,
                expected_day: 31,
            },
        ];

        for case in cases {
            check_test_case(case);
        }
    }

    #[test]
    fn test_roc_directionality_near_epoch() {
        // Tests that for a large range of fixed dates near the beginning of the minguo era (CE 1912),
        // the comparison between those two fixed dates should be equal to the comparison between their
        // corresponding YMD.
        let rd_epoch_start = 697978;
        for i in (rd_epoch_start - 100)..=(rd_epoch_start + 100) {
            for j in (rd_epoch_start - 100)..=(rd_epoch_start + 100) {
                let iso_i = Iso::iso_from_fixed(RataDie::new(i));
                let iso_j = Iso::iso_from_fixed(RataDie::new(j));

                let roc_i = iso_i.to_calendar(Roc);
                let roc_j = iso_j.to_calendar(Roc);

                assert_eq!(
                    i.cmp(&j),
                    iso_i.cmp(&iso_j),
                    "ISO directionality inconsistent with directionality for i: {i}, j: {j}"
                );
                assert_eq!(
                    i.cmp(&j),
                    roc_i.cmp(&roc_j),
                    "ROC directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }

    #[test]
    fn test_roc_directionality_near_rd_zero() {
        // Same as `test_directionality_near_epoch`, but with a focus around RD 0
        for i in -100..=100 {
            for j in -100..100 {
                let iso_i = Iso::iso_from_fixed(RataDie::new(i));
                let iso_j = Iso::iso_from_fixed(RataDie::new(j));

                let roc_i = iso_i.to_calendar(Roc);
                let roc_j = iso_j.to_calendar(Roc);

                assert_eq!(
                    i.cmp(&j),
                    iso_i.cmp(&iso_j),
                    "ISO directionality inconsistent with directionality for i: {i}, j: {j}"
                );
                assert_eq!(
                    i.cmp(&j),
                    roc_i.cmp(&roc_j),
                    "ROC directionality inconsistent with directionality for i: {i}, j: {j}"
                );
            }
        }
    }
}
