// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Ethiopian calendar.
//!
//! ```rust
//! use icu::calendar::{cal::Ethiopian, Date, DateTime};
//!
//! // `Date` type
//! let date_iso = Date::try_new_iso(1970, 1, 2)
//!     .expect("Failed to initialize ISO Date instance.");
//! let date_ethiopian = Date::new_from_iso(date_iso, Ethiopian::new());
//!
//! // `DateTime` type
//! let datetime_iso = DateTime::try_new_iso(1970, 1, 2, 13, 1, 0)
//!     .expect("Failed to initialize ISO DateTime instance.");
//! let datetime_ethiopian =
//!     DateTime::new_from_iso(datetime_iso, Ethiopian::new());
//!
//! // `Date` checks
//! assert_eq!(date_ethiopian.year().era_year_or_extended(), 1962);
//! assert_eq!(date_ethiopian.month().ordinal, 4);
//! assert_eq!(date_ethiopian.day_of_month().0, 24);
//!
//! // `DateTime` type
//! assert_eq!(datetime_ethiopian.date.year().era_year_or_extended(), 1962);
//! assert_eq!(datetime_ethiopian.date.month().ordinal, 4);
//! assert_eq!(datetime_ethiopian.date.day_of_month().0, 24);
//! assert_eq!(datetime_ethiopian.time.hour.number(), 13);
//! assert_eq!(datetime_ethiopian.time.minute.number(), 1);
//! assert_eq!(datetime_ethiopian.time.second.number(), 0);
//! ```

use crate::calendar_arithmetic::{ArithmeticDate, CalendarArithmetic};
use crate::error::DateError;
use crate::iso::Iso;
use crate::{types, Calendar, Date, DateDuration, DateDurationUnit, DateTime, RangeError, Time};
use calendrical_calculations::helpers::I32CastError;
use calendrical_calculations::rata_die::RataDie;
use tinystr::tinystr;

/// The number of years the Amete Alem epoch precedes the Amete Mihret epoch
const AMETE_ALEM_OFFSET: i32 = 5500;

/// Which era style the ethiopian calendar uses
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
#[non_exhaustive]
pub enum EthiopianEraStyle {
    /// Use an era scheme of pre- and post- Incarnation eras,
    /// anchored at the date of the Incarnation of Jesus in this calendar
    AmeteMihret,
    /// Use an era scheme of the Anno Mundi era, anchored at the date of Creation
    /// in this calendar
    AmeteAlem,
}

/// The [Ethiopian Calendar]
///
/// The [Ethiopian calendar] is a solar calendar used by the Coptic Orthodox Church, with twelve normal months
/// and a thirteenth small epagomenal month.
///
/// This type can be used with [`Date`] or [`DateTime`] to represent dates in this calendar.
///
/// It can be constructed in two modes: using the Amete Alem era scheme, or the Amete Mihret era scheme (the default),
/// see [`EthiopianEraStyle`] for more info.
///
/// [Ethiopian calendar]: https://en.wikipedia.org/wiki/Ethiopian_calendar
///
/// # Era codes
///
/// This calendar supports three era codes, based on what mode it is in. In the Amete Mihret scheme it has
/// the `"incar"` and `"pre-incar"` eras, 1 Incarnation is 9 CE. In the Amete Alem scheme, it instead has a single era,
/// `"mundi`, where 1 Anno Mundi is 5493 BCE. Dates before that use negative year numbers.
///
/// # Month codes
///
/// This calendar supports 13 solar month codes (`"M01" - "M13"`), with `"M13"` being used for the short epagomenal month
/// at the end of the year.
// The bool specifies whether dates should be in the Amete Alem era scheme
#[derive(Copy, Clone, Debug, Hash, Default, Eq, PartialEq, PartialOrd, Ord)]
pub struct Ethiopian(pub(crate) bool);

/// The inner date type used for representing [`Date`]s of [`Ethiopian`]. See [`Date`] and [`Ethiopian`] for more details.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct EthiopianDateInner(ArithmeticDate<Ethiopian>);

impl CalendarArithmetic for Ethiopian {
    type YearInfo = ();

    fn month_days(year: i32, month: u8, _data: ()) -> u8 {
        if (1..=12).contains(&month) {
            30
        } else if month == 13 {
            if Self::is_leap_year(year, ()) {
                6
            } else {
                5
            }
        } else {
            0
        }
    }

    fn months_for_every_year(_: i32, _data: ()) -> u8 {
        13
    }

    fn is_leap_year(year: i32, _data: ()) -> bool {
        year.rem_euclid(4) == 3
    }

    fn last_month_day_in_year(year: i32, _data: ()) -> (u8, u8) {
        if Self::is_leap_year(year, ()) {
            (13, 6)
        } else {
            (13, 5)
        }
    }

    fn days_in_provided_year(year: i32, _data: ()) -> u16 {
        if Self::is_leap_year(year, ()) {
            366
        } else {
            365
        }
    }
}

impl Calendar for Ethiopian {
    type DateInner = EthiopianDateInner;
    fn date_from_codes(
        &self,
        era: Option<types::Era>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let year = if let Some(era) = era {
            if era.0 == tinystr!(16, "incar") || era.0 == tinystr!(16, "ethiopic") {
                if year <= 0 {
                    return Err(DateError::Range {
                        field: "year",
                        value: year,
                        min: 1,
                        max: i32::MAX,
                    });
                }
                year
            } else if era.0 == tinystr!(16, "pre-incar")
                || era.0 == tinystr!(16, "ethiopic-inverse")
            {
                if year <= 0 {
                    return Err(DateError::Range {
                        field: "year",
                        value: year,
                        min: 1,
                        max: i32::MAX,
                    });
                }
                1 - year
            } else if era.0 == tinystr!(16, "mundi") || era.0 == tinystr!(16, "ethioaa") {
                year - AMETE_ALEM_OFFSET
            } else {
                return Err(DateError::UnknownEra(era));
            }
        } else {
            year
        };
        ArithmeticDate::new_from_codes(self, year, month_code, day).map(EthiopianDateInner)
    }
    fn date_from_iso(&self, iso: Date<Iso>) -> EthiopianDateInner {
        let fixed_iso = Iso::fixed_from_iso(*iso.inner());
        Self::ethiopian_from_fixed(fixed_iso)
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso> {
        let fixed_ethiopian = Ethiopian::fixed_from_ethiopian(date.0);
        Iso::iso_from_fixed(fixed_ethiopian)
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

    fn day_of_week(&self, date: &Self::DateInner) -> types::IsoWeekday {
        Iso.day_of_week(self.date_to_iso(date).inner())
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
        Self::year_as_ethiopian(date.0.year, self.0)
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
        let prev_year = date.0.year - 1;
        let next_year = date.0.year + 1;
        types::DayOfYearInfo {
            day_of_year: date.0.day_of_year(),
            days_in_year: date.0.days_in_year(),
            prev_year: Self::year_as_ethiopian(prev_year, self.0),
            days_in_prev_year: Ethiopian::days_in_year_direct(prev_year),
            next_year: Self::year_as_ethiopian(next_year, self.0),
        }
    }

    fn debug_name(&self) -> &'static str {
        "Ethiopian"
    }

    fn any_calendar_kind(&self) -> Option<crate::AnyCalendarKind> {
        Some(crate::any_calendar::IntoAnyCalendar::kind(self))
    }
}

impl Ethiopian {
    /// Construct a new Ethiopian Calendar for the Amete Mihret era naming scheme
    pub const fn new() -> Self {
        Self(false)
    }
    /// Construct a new Ethiopian Calendar with a value specifying whether or not it is Amete Alem
    pub const fn new_with_era_style(era_style: EthiopianEraStyle) -> Self {
        Self(matches!(era_style, EthiopianEraStyle::AmeteAlem))
    }
    /// Set whether or not this uses the Amete Alem era scheme
    pub fn set_era_style(&mut self, era_style: EthiopianEraStyle) {
        self.0 = era_style == EthiopianEraStyle::AmeteAlem
    }

    /// Returns whether this has the Amete Alem era
    pub fn era_style(&self) -> EthiopianEraStyle {
        if self.0 {
            EthiopianEraStyle::AmeteAlem
        } else {
            EthiopianEraStyle::AmeteMihret
        }
    }

    fn fixed_from_ethiopian(date: ArithmeticDate<Ethiopian>) -> RataDie {
        calendrical_calculations::ethiopian::fixed_from_ethiopian(date.year, date.month, date.day)
    }

    fn ethiopian_from_fixed(date: RataDie) -> EthiopianDateInner {
        let (year, month, day) =
            match calendrical_calculations::ethiopian::ethiopian_from_fixed(date) {
                Err(I32CastError::BelowMin) => {
                    return EthiopianDateInner(ArithmeticDate::min_date())
                }
                Err(I32CastError::AboveMax) => {
                    return EthiopianDateInner(ArithmeticDate::max_date())
                }
                Ok(ymd) => ymd,
            };
        EthiopianDateInner(ArithmeticDate::new_unchecked(year, month, day))
    }

    fn days_in_year_direct(year: i32) -> u16 {
        if Ethiopian::is_leap_year(year, ()) {
            366
        } else {
            365
        }
    }
    fn year_as_ethiopian(year: i32, amete_alem: bool) -> types::YearInfo {
        if amete_alem {
            types::YearInfo::new(
                year,
                types::EraYear {
                    standard_era: tinystr!(16, "ethioaa").into(),
                    formatting_era: types::FormattingEra::Index(0, tinystr!(16, "Anno Mundi")),
                    era_year: year + AMETE_ALEM_OFFSET,
                    ambiguity: types::YearAmbiguity::CenturyRequired,
                },
            )
        } else if year > 0 {
            types::YearInfo::new(
                year,
                types::EraYear {
                    standard_era: tinystr!(16, "ethiopic").into(),
                    formatting_era: types::FormattingEra::Index(2, tinystr!(16, "Incarnation")),
                    era_year: year,
                    ambiguity: types::YearAmbiguity::CenturyRequired,
                },
            )
        } else {
            types::YearInfo::new(
                year,
                types::EraYear {
                    standard_era: tinystr!(16, "ethiopic-inverse").into(),
                    formatting_era: types::FormattingEra::Index(1, tinystr!(16, "Pre-Incarnation")),
                    era_year: 1 - year,
                    ambiguity: types::YearAmbiguity::EraAndCenturyRequired,
                },
            )
        }
    }
}

impl Date<Ethiopian> {
    /// Construct new Ethiopian Date.
    ///
    /// For the Amete Mihret era style, negative years work with
    /// year 0 as 1 pre-Incarnation, year -1 as 2 pre-Incarnation,
    /// and so on.
    ///
    /// ```rust
    /// use icu::calendar::cal::EthiopianEraStyle;
    /// use icu::calendar::Date;
    ///
    /// let date_ethiopian =
    ///     Date::try_new_ethiopian(EthiopianEraStyle::AmeteMihret, 2014, 8, 25)
    ///         .expect("Failed to initialize Ethopic Date instance.");
    ///
    /// assert_eq!(date_ethiopian.year().era_year_or_extended(), 2014);
    /// assert_eq!(date_ethiopian.month().ordinal, 8);
    /// assert_eq!(date_ethiopian.day_of_month().0, 25);
    /// ```
    pub fn try_new_ethiopian(
        era_style: EthiopianEraStyle,
        mut year: i32,
        month: u8,
        day: u8,
    ) -> Result<Date<Ethiopian>, RangeError> {
        if era_style == EthiopianEraStyle::AmeteAlem {
            year -= AMETE_ALEM_OFFSET;
        }
        ArithmeticDate::new_from_ordinals(year, month, day)
            .map(EthiopianDateInner)
            .map(|inner| Date::from_raw(inner, Ethiopian::new_with_era_style(era_style)))
    }
}

impl DateTime<Ethiopian> {
    /// Construct a new Ethiopian datetime from integers.
    ///
    /// For the Amete Mihret era style, negative years work with
    /// year 0 as 1 pre-Incarnation, year -1 as 2 pre-Incarnation,
    /// and so on.
    ///
    /// ```rust
    /// use icu::calendar::cal::EthiopianEraStyle;
    /// use icu::calendar::DateTime;
    ///
    /// let datetime_ethiopian = DateTime::try_new_ethiopian(
    ///     EthiopianEraStyle::AmeteMihret,
    ///     2014,
    ///     8,
    ///     25,
    ///     13,
    ///     1,
    ///     0,
    /// )
    /// .expect("Failed to initialize Ethiopian DateTime instance.");
    ///
    /// assert_eq!(datetime_ethiopian.date.year().era_year_or_extended(), 2014);
    /// assert_eq!(datetime_ethiopian.date.month().ordinal, 8);
    /// assert_eq!(datetime_ethiopian.date.day_of_month().0, 25);
    /// assert_eq!(datetime_ethiopian.time.hour.number(), 13);
    /// assert_eq!(datetime_ethiopian.time.minute.number(), 1);
    /// assert_eq!(datetime_ethiopian.time.second.number(), 0);
    /// ```
    pub fn try_new_ethiopian(
        era_style: EthiopianEraStyle,
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
    ) -> Result<DateTime<Ethiopian>, DateError> {
        Ok(DateTime {
            date: Date::try_new_ethiopian(era_style, year, month, day)?,
            time: Time::try_new(hour, minute, second, 0)?,
        })
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_leap_year() {
        // 11th September 2023 in gregorian is 6/13/2015 in ethiopian
        let iso_date = Date::try_new_iso(2023, 9, 11).unwrap();
        let ethiopian_date = Ethiopian::new().date_from_iso(iso_date);
        assert_eq!(ethiopian_date.0.year, 2015);
        assert_eq!(ethiopian_date.0.month, 13);
        assert_eq!(ethiopian_date.0.day, 6);
    }

    #[test]
    fn test_iso_to_ethiopian_conversion_and_back() {
        let iso_date = Date::try_new_iso(1970, 1, 2).unwrap();
        let date_ethiopian = Date::new_from_iso(iso_date, Ethiopian::new());

        assert_eq!(date_ethiopian.inner.0.year, 1962);
        assert_eq!(date_ethiopian.inner.0.month, 4);
        assert_eq!(date_ethiopian.inner.0.day, 24);

        assert_eq!(
            date_ethiopian.to_iso(),
            Date::try_new_iso(1970, 1, 2).unwrap()
        );
    }

    #[test]
    fn test_roundtrip_negative() {
        // https://github.com/unicode-org/icu4x/issues/2254
        let iso_date = Date::try_new_iso(-1000, 3, 3).unwrap();
        let ethiopian = iso_date.to_calendar(Ethiopian::new());
        let recovered_iso = ethiopian.to_iso();
        assert_eq!(iso_date, recovered_iso);
    }
}
