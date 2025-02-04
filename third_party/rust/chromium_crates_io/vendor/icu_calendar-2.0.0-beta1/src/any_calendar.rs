// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Module for working with multiple calendars at once

use crate::buddhist::Buddhist;
use crate::chinese::Chinese;
use crate::coptic::Coptic;
use crate::dangi::Dangi;
use crate::error::DateError;
use crate::ethiopian::{Ethiopian, EthiopianEraStyle};
use crate::gregorian::Gregorian;
use crate::hebrew::Hebrew;
use crate::indian::Indian;
use crate::islamic::{IslamicCivil, IslamicObservational, IslamicTabular, IslamicUmmAlQura};
use crate::iso::Iso;
use crate::japanese::{Japanese, JapaneseExtended};
use crate::persian::Persian;
use crate::roc::Roc;
use crate::{types, AsCalendar, Calendar, Date, DateDuration, DateDurationUnit, DateTime, Ref};

use icu_locale_core::extensions::unicode::{key, value, Value};
use icu_locale_core::preferences::define_preferences;
use icu_locale_core::preferences::extensions::unicode::keywords::{
    CalendarAlgorithm, IslamicCalendarAlgorithm,
};
use icu_locale_core::subtags::language;
use icu_locale_core::Locale;
use icu_provider::prelude::*;

use core::fmt;

define_preferences!(
    /// The prefs for datetime formatting.
    [Copy]
    AnyCalendarPreferences,
    {
        /// The user's preferred calendar system.
        calendar_algorithm: CalendarAlgorithm
    }
);

/// This is a calendar that encompasses all formattable calendars supported by this crate
///
/// This allows for the construction of [`Date`] objects that have their calendar known at runtime.
///
/// This can be constructed by calling `.into()` on a concrete calendar type if the calendar type is known at
/// compile time. When the type is known at runtime, the [`AnyCalendar::new_for_kind()`] and sibling methods may be used.
///
/// [`Date`] can also be converted to [`AnyCalendar`]-compatible ones
/// via [`Date::to_any()`](crate::Date::to_any()).
///
/// There are many ways of constructing an AnyCalendar'd date:
/// ```
/// use icu::calendar::{AnyCalendar, DateTime, cal::Japanese, Time, types::{Era, MonthCode}};
/// use icu::locale::locale;
/// use tinystr::tinystr;
/// # use std::rc::Rc;
///
/// let locale = locale!("en-u-ca-japanese"); // English with the Japanese calendar
///
/// let calendar = AnyCalendar::try_new(locale.into()).unwrap();
/// let calendar = Rc::new(calendar); // Avoid cloning it each time
///                                   // If everything is a local reference, you may use icu::calendar::Ref instead.
///
/// // manually construct a datetime in this calendar
/// let manual_time = Time::try_new(12, 33, 12, 0).expect("failed to construct Time");
/// // construct from era code, year, month code, day, time, and a calendar
/// // This is March 28, 15 Heisei
/// let manual_datetime = DateTime::try_new_from_codes(Some(Era(tinystr!(16, "heisei"))), 15, MonthCode(tinystr!(4, "M03")), 28,
///                                                manual_time, calendar.clone())
///                     .expect("Failed to construct DateTime manually");
///
///
/// // construct another datetime by converting from ISO
/// let iso_datetime = DateTime::try_new_iso(2020, 9, 1, 12, 34, 28)
///     .expect("Failed to construct ISO DateTime.");
/// let iso_converted = iso_datetime.to_calendar(calendar);
///
/// // Construct a datetime in the appropriate typed calendar and convert
/// let japanese_calendar = Japanese::new();
/// let japanese_datetime = DateTime::try_new_japanese_with_calendar(Era(tinystr!(16, "heisei")), 15, 3, 28,
///                                                         12, 33, 12, japanese_calendar).unwrap();
/// // This is a DateTime<AnyCalendar>
/// let any_japanese_datetime = japanese_datetime.to_any();
/// ```
#[derive(Debug, Clone)]
#[non_exhaustive]
pub enum AnyCalendar {
    /// A [`Buddhist`] calendar
    Buddhist(Buddhist),
    /// A [`Chinese`] calendar
    Chinese(Chinese),
    /// A [`Coptic`] calendar
    Coptic(Coptic),
    /// A [`Dangi`] calendar
    Dangi(Dangi),
    /// An [`Ethiopian`] calendar
    Ethiopian(Ethiopian),
    /// A [`Gregorian`] calendar
    Gregorian(Gregorian),
    /// A [`Hebrew`] calendar
    Hebrew(Hebrew),
    /// An [`Indian`] calendar
    Indian(Indian),
    /// An [`IslamicCivil`] calendar
    IslamicCivil(IslamicCivil),
    /// An [`IslamicObservational`] calendar
    IslamicObservational(IslamicObservational),
    /// An [`IslamicTabular`] calendar
    IslamicTabular(IslamicTabular),
    /// An [`IslamicUmmAlQura`] calendar
    IslamicUmmAlQura(IslamicUmmAlQura),
    /// An [`Iso`] calendar
    Iso(Iso),
    /// A [`Japanese`] calendar
    Japanese(Japanese),
    /// A [`JapaneseExtended`] calendar
    JapaneseExtended(JapaneseExtended),
    /// A [`Persian`] calendar
    Persian(Persian),
    /// A [`Roc`] calendar
    Roc(Roc),
}

// TODO(#3469): Decide on the best way to implement Ord.
/// The inner date type for [`AnyCalendar`]
#[derive(Clone, PartialEq, Eq, Debug)]
#[non_exhaustive]
pub enum AnyDateInner {
    /// A date for a [`Buddhist`] calendar
    Buddhist(<Buddhist as Calendar>::DateInner),
    /// A date for a [`Chinese`] calendar
    Chinese(<Chinese as Calendar>::DateInner),
    /// A date for a [`Coptic`] calendar
    Coptic(<Coptic as Calendar>::DateInner),
    /// A date for a [`Dangi`] calendar
    Dangi(<Dangi as Calendar>::DateInner),
    /// A date for an [`Ethiopian`] calendar
    Ethiopian(<Ethiopian as Calendar>::DateInner),
    /// A date for a [`Gregorian`] calendar
    Gregorian(<Gregorian as Calendar>::DateInner),
    /// A date for a [`Hebrew`] calendar
    Hebrew(<Hebrew as Calendar>::DateInner),
    /// A date for an [`Indian`] calendar
    Indian(<Indian as Calendar>::DateInner),
    /// A date for an [`IslamicCivil`] calendar
    IslamicCivil(<IslamicCivil as Calendar>::DateInner),
    /// A date for an [`IslamicObservational`] calendar
    IslamicObservational(<IslamicObservational as Calendar>::DateInner),
    /// A date for an [`IslamicTabular`] calendar
    IslamicTabular(<IslamicTabular as Calendar>::DateInner),
    /// A date for an [`IslamicUmmAlQura`] calendar
    IslamicUmmAlQura(<IslamicUmmAlQura as Calendar>::DateInner),
    /// A date for an [`Iso`] calendar
    Iso(<Iso as Calendar>::DateInner),
    /// A date for a [`Japanese`] calendar
    Japanese(<Japanese as Calendar>::DateInner),
    /// A date for a [`JapaneseExtended`] calendar
    JapaneseExtended(<JapaneseExtended as Calendar>::DateInner),
    /// A date for a [`Persian`] calendar
    Persian(<Persian as Calendar>::DateInner),
    /// A date for a [`Roc`] calendar
    Roc(<Roc as Calendar>::DateInner),
}

macro_rules! match_cal_and_date {
    (match ($cal:ident, $date:ident): ($cal_matched:ident, $date_matched:ident) => $e:expr) => {
        match ($cal, $date) {
            (&Self::Buddhist(ref $cal_matched), &AnyDateInner::Buddhist(ref $date_matched)) => $e,
            (&Self::Chinese(ref $cal_matched), &AnyDateInner::Chinese(ref $date_matched)) => $e,
            (&Self::Coptic(ref $cal_matched), &AnyDateInner::Coptic(ref $date_matched)) => $e,
            (&Self::Dangi(ref $cal_matched), &AnyDateInner::Dangi(ref $date_matched)) => $e,
            (&Self::Ethiopian(ref $cal_matched), &AnyDateInner::Ethiopian(ref $date_matched)) => $e,
            (&Self::Gregorian(ref $cal_matched), &AnyDateInner::Gregorian(ref $date_matched)) => $e,
            (&Self::Hebrew(ref $cal_matched), &AnyDateInner::Hebrew(ref $date_matched)) => $e,
            (&Self::Indian(ref $cal_matched), &AnyDateInner::Indian(ref $date_matched)) => $e,
            (
                &Self::IslamicCivil(ref $cal_matched),
                &AnyDateInner::IslamicCivil(ref $date_matched),
            ) => $e,
            (
                &Self::IslamicObservational(ref $cal_matched),
                &AnyDateInner::IslamicObservational(ref $date_matched),
            ) => $e,
            (
                &Self::IslamicTabular(ref $cal_matched),
                &AnyDateInner::IslamicTabular(ref $date_matched),
            ) => $e,
            (
                &Self::IslamicUmmAlQura(ref $cal_matched),
                &AnyDateInner::IslamicUmmAlQura(ref $date_matched),
            ) => $e,
            (&Self::Iso(ref $cal_matched), &AnyDateInner::Iso(ref $date_matched)) => $e,
            (&Self::Japanese(ref $cal_matched), &AnyDateInner::Japanese(ref $date_matched)) => $e,
            (
                &Self::JapaneseExtended(ref $cal_matched),
                &AnyDateInner::JapaneseExtended(ref $date_matched),
            ) => $e,
            (&Self::Persian(ref $cal_matched), &AnyDateInner::Persian(ref $date_matched)) => $e,
            (&Self::Roc(ref $cal_matched), &AnyDateInner::Roc(ref $date_matched)) => $e,
            _ => panic!(
                "Found AnyCalendar with mixed calendar type {:?} and date type {:?}!",
                $cal.kind().debug_name(),
                $date.kind().debug_name()
            ),
        }
    };
}

impl Calendar for AnyCalendar {
    type DateInner = AnyDateInner;
    fn date_from_codes(
        &self,
        era: Option<types::Era>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let ret = match *self {
            Self::Buddhist(ref c) => {
                AnyDateInner::Buddhist(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::Chinese(ref c) => {
                AnyDateInner::Chinese(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::Coptic(ref c) => {
                AnyDateInner::Coptic(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::Dangi(ref c) => {
                AnyDateInner::Dangi(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::Ethiopian(ref c) => {
                AnyDateInner::Ethiopian(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::Gregorian(ref c) => {
                AnyDateInner::Gregorian(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::Hebrew(ref c) => {
                AnyDateInner::Hebrew(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::Indian(ref c) => {
                AnyDateInner::Indian(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::IslamicCivil(ref c) => {
                AnyDateInner::IslamicCivil(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::IslamicObservational(ref c) => {
                AnyDateInner::IslamicObservational(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::IslamicTabular(ref c) => {
                AnyDateInner::IslamicTabular(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::IslamicUmmAlQura(ref c) => {
                AnyDateInner::IslamicUmmAlQura(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::Iso(ref c) => AnyDateInner::Iso(c.date_from_codes(era, year, month_code, day)?),
            Self::Japanese(ref c) => {
                AnyDateInner::Japanese(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::JapaneseExtended(ref c) => {
                AnyDateInner::JapaneseExtended(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::Persian(ref c) => {
                AnyDateInner::Persian(c.date_from_codes(era, year, month_code, day)?)
            }
            Self::Roc(ref c) => AnyDateInner::Roc(c.date_from_codes(era, year, month_code, day)?),
        };
        Ok(ret)
    }
    fn date_from_iso(&self, iso: Date<Iso>) -> AnyDateInner {
        match *self {
            Self::Buddhist(ref c) => AnyDateInner::Buddhist(c.date_from_iso(iso)),
            Self::Chinese(ref c) => AnyDateInner::Chinese(c.date_from_iso(iso)),
            Self::Coptic(ref c) => AnyDateInner::Coptic(c.date_from_iso(iso)),
            Self::Dangi(ref c) => AnyDateInner::Dangi(c.date_from_iso(iso)),
            Self::Ethiopian(ref c) => AnyDateInner::Ethiopian(c.date_from_iso(iso)),
            Self::Gregorian(ref c) => AnyDateInner::Gregorian(c.date_from_iso(iso)),
            Self::Hebrew(ref c) => AnyDateInner::Hebrew(c.date_from_iso(iso)),
            Self::Indian(ref c) => AnyDateInner::Indian(c.date_from_iso(iso)),
            Self::IslamicCivil(ref c) => AnyDateInner::IslamicCivil(c.date_from_iso(iso)),
            Self::IslamicObservational(ref c) => {
                AnyDateInner::IslamicObservational(c.date_from_iso(iso))
            }
            Self::IslamicTabular(ref c) => AnyDateInner::IslamicTabular(c.date_from_iso(iso)),
            Self::IslamicUmmAlQura(ref c) => AnyDateInner::IslamicUmmAlQura(c.date_from_iso(iso)),
            Self::Iso(ref c) => AnyDateInner::Iso(c.date_from_iso(iso)),
            Self::Japanese(ref c) => AnyDateInner::Japanese(c.date_from_iso(iso)),
            Self::JapaneseExtended(ref c) => AnyDateInner::JapaneseExtended(c.date_from_iso(iso)),
            Self::Persian(ref c) => AnyDateInner::Persian(c.date_from_iso(iso)),
            Self::Roc(ref c) => AnyDateInner::Roc(c.date_from_iso(iso)),
        }
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso> {
        match_cal_and_date!(match (self, date): (c, d) => c.date_to_iso(d))
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        match_cal_and_date!(match (self, date): (c, d) => c.months_in_year(d))
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        match_cal_and_date!(match (self, date): (c, d) => c.days_in_year(d))
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        match_cal_and_date!(match (self, date): (c, d) => c.days_in_month(d))
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        match (self, date) {
            (Self::Buddhist(c), &mut AnyDateInner::Buddhist(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Chinese(c), &mut AnyDateInner::Chinese(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Coptic(c), &mut AnyDateInner::Coptic(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Dangi(c), &mut AnyDateInner::Dangi(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Ethiopian(c), &mut AnyDateInner::Ethiopian(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Gregorian(c), &mut AnyDateInner::Gregorian(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Hebrew(c), &mut AnyDateInner::Hebrew(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Indian(c), &mut AnyDateInner::Indian(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::IslamicCivil(c), &mut AnyDateInner::IslamicCivil(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::IslamicObservational(c), &mut AnyDateInner::IslamicObservational(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::IslamicTabular(c), &mut AnyDateInner::IslamicTabular(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::IslamicUmmAlQura(c), &mut AnyDateInner::IslamicUmmAlQura(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Iso(c), &mut AnyDateInner::Iso(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Japanese(c), &mut AnyDateInner::Japanese(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::JapaneseExtended(c), &mut AnyDateInner::JapaneseExtended(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Persian(c), &mut AnyDateInner::Persian(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            (Self::Roc(c), &mut AnyDateInner::Roc(ref mut d)) => {
                c.offset_date(d, offset.cast_unit())
            }
            // This is only reached from misuse of from_raw, a semi-internal api
            #[allow(clippy::panic)]
            (_, d) => panic!(
                "Found AnyCalendar with mixed calendar type {} and date type {}!",
                self.kind().debug_name(),
                d.kind().debug_name()
            ),
        }
    }

    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        calendar2: &Self,
        largest_unit: DateDurationUnit,
        smallest_unit: DateDurationUnit,
    ) -> DateDuration<Self> {
        match (self, calendar2, date1, date2) {
            (
                Self::Buddhist(c1),
                Self::Buddhist(c2),
                AnyDateInner::Buddhist(d1),
                AnyDateInner::Buddhist(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Chinese(c1),
                Self::Chinese(c2),
                AnyDateInner::Chinese(d1),
                AnyDateInner::Chinese(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Coptic(c1),
                Self::Coptic(c2),
                AnyDateInner::Coptic(d1),
                AnyDateInner::Coptic(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Dangi(c1),
                Self::Dangi(c2),
                AnyDateInner::Dangi(d1),
                AnyDateInner::Dangi(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Ethiopian(c1),
                Self::Ethiopian(c2),
                AnyDateInner::Ethiopian(d1),
                AnyDateInner::Ethiopian(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Gregorian(c1),
                Self::Gregorian(c2),
                AnyDateInner::Gregorian(d1),
                AnyDateInner::Gregorian(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Hebrew(c1),
                Self::Hebrew(c2),
                AnyDateInner::Hebrew(d1),
                AnyDateInner::Hebrew(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Indian(c1),
                Self::Indian(c2),
                AnyDateInner::Indian(d1),
                AnyDateInner::Indian(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::IslamicCivil(c1),
                Self::IslamicCivil(c2),
                AnyDateInner::IslamicCivil(d1),
                AnyDateInner::IslamicCivil(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::IslamicObservational(c1),
                Self::IslamicObservational(c2),
                AnyDateInner::IslamicObservational(d1),
                AnyDateInner::IslamicObservational(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::IslamicTabular(c1),
                Self::IslamicTabular(c2),
                AnyDateInner::IslamicTabular(d1),
                AnyDateInner::IslamicTabular(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::IslamicUmmAlQura(c1),
                Self::IslamicUmmAlQura(c2),
                AnyDateInner::IslamicUmmAlQura(d1),
                AnyDateInner::IslamicUmmAlQura(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (Self::Iso(c1), Self::Iso(c2), AnyDateInner::Iso(d1), AnyDateInner::Iso(d2)) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Japanese(c1),
                Self::Japanese(c2),
                AnyDateInner::Japanese(d1),
                AnyDateInner::Japanese(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::JapaneseExtended(c1),
                Self::JapaneseExtended(c2),
                AnyDateInner::JapaneseExtended(d1),
                AnyDateInner::JapaneseExtended(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (
                Self::Persian(c1),
                Self::Persian(c2),
                AnyDateInner::Persian(d1),
                AnyDateInner::Persian(d2),
            ) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            (Self::Roc(c1), Self::Roc(c2), AnyDateInner::Roc(d1), AnyDateInner::Roc(d2)) => c1
                .until(d1, d2, c2, largest_unit, smallest_unit)
                .cast_unit(),
            _ => {
                // attempt to convert
                let iso = calendar2.date_to_iso(date2);

                match_cal_and_date!(match (self, date1):
                    (c1, d1) => {
                        let d2 = c1.date_from_iso(iso);
                        let until = c1.until(d1, &d2, c1, largest_unit, smallest_unit);
                        until.cast_unit::<AnyCalendar>()
                    }
                )
            }
        }
    }

    fn year(&self, date: &Self::DateInner) -> types::YearInfo {
        match_cal_and_date!(match (self, date): (c, d) => c.year(d))
    }
    /// The calendar-specific check if `date` is in a leap year
    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        match_cal_and_date!(match (self, date): (c, d) => c.is_in_leap_year(d))
    }

    /// The calendar-specific month represented by `date`
    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        match_cal_and_date!(match (self, date): (c, d) => c.month(d))
    }

    /// The calendar-specific day-of-month represented by `date`
    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        match_cal_and_date!(match (self, date): (c, d) => c.day_of_month(d))
    }

    /// Information of the day of the year
    fn day_of_year_info(&self, date: &Self::DateInner) -> types::DayOfYearInfo {
        match_cal_and_date!(match (self, date): (c, d) => c.day_of_year_info(d))
    }

    fn debug_name(&self) -> &'static str {
        match *self {
            Self::Buddhist(_) => "AnyCalendar (Buddhist)",
            Self::Chinese(_) => "AnyCalendar (Chinese)",
            Self::Coptic(_) => "AnyCalendar (Coptic)",
            Self::Dangi(_) => "AnyCalendar (Dangi)",
            Self::Ethiopian(_) => "AnyCalendar (Ethiopian)",
            Self::Gregorian(_) => "AnyCalendar (Gregorian)",
            Self::Hebrew(_) => "AnyCalendar (Hebrew)",
            Self::Indian(_) => "AnyCalendar (Indian)",
            Self::IslamicCivil(_) => "AnyCalendar (Islamic, civil)",
            Self::IslamicObservational(_) => "AnyCalendar (Islamic, observational)",
            Self::IslamicTabular(_) => "AnyCalendar (Islamic, tabular)",
            Self::IslamicUmmAlQura(_) => "AnyCalendar (Islamic, Umm al-Qura)",
            Self::Iso(_) => "AnyCalendar (Iso)",
            Self::Japanese(_) => "AnyCalendar (Japanese)",
            Self::JapaneseExtended(_) => "AnyCalendar (Japanese, historical era data)",
            Self::Persian(_) => "AnyCalendar (Persian)",
            Self::Roc(_) => "AnyCalendar (Roc)",
        }
    }

    fn any_calendar_kind(&self) -> Option<AnyCalendarKind> {
        Some(self.kind())
    }
}

impl AnyCalendar {
    /// Constructs an AnyCalendar for a given calendar kind from compiled data.
    ///
    /// As this requires a valid [`AnyCalendarKind`] to work, it does not do any kind of locale-based
    /// fallbacking. If this is desired, use [`Self::try_new()`].
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new_for_kind(kind: AnyCalendarKind) -> Self {
        match kind {
            AnyCalendarKind::Buddhist => AnyCalendar::Buddhist(Buddhist),
            AnyCalendarKind::Chinese => AnyCalendar::Chinese(Chinese::new()),
            AnyCalendarKind::Coptic => AnyCalendar::Coptic(Coptic),
            AnyCalendarKind::Dangi => AnyCalendar::Dangi(Dangi::new()),
            AnyCalendarKind::Ethiopian => AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(
                EthiopianEraStyle::AmeteMihret,
            )),
            AnyCalendarKind::EthiopianAmeteAlem => {
                AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem))
            }
            AnyCalendarKind::Gregorian => AnyCalendar::Gregorian(Gregorian),
            AnyCalendarKind::Hebrew => AnyCalendar::Hebrew(Hebrew),
            AnyCalendarKind::Indian => AnyCalendar::Indian(Indian),
            AnyCalendarKind::IslamicCivil => AnyCalendar::IslamicCivil(IslamicCivil),
            AnyCalendarKind::IslamicObservational => {
                AnyCalendar::IslamicObservational(IslamicObservational::new())
            }
            AnyCalendarKind::IslamicTabular => AnyCalendar::IslamicTabular(IslamicTabular),
            AnyCalendarKind::IslamicUmmAlQura => {
                AnyCalendar::IslamicUmmAlQura(IslamicUmmAlQura::new())
            }
            AnyCalendarKind::Iso => AnyCalendar::Iso(Iso),
            AnyCalendarKind::Japanese => AnyCalendar::Japanese(Japanese::new()),
            AnyCalendarKind::JapaneseExtended => {
                AnyCalendar::JapaneseExtended(JapaneseExtended::new())
            }
            AnyCalendarKind::Persian => AnyCalendar::Persian(Persian),
            AnyCalendarKind::Roc => AnyCalendar::Roc(Roc),
        }
    }

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(ANY, Self::new_for_kind)]
    pub fn try_new_for_kind_with_any_provider<P>(
        provider: &P,
        kind: AnyCalendarKind,
    ) -> Result<Self, DataError>
    where
        P: AnyProvider + ?Sized,
    {
        Ok(match kind {
            AnyCalendarKind::Buddhist => AnyCalendar::Buddhist(Buddhist),
            AnyCalendarKind::Chinese => {
                AnyCalendar::Chinese(Chinese::try_new_with_any_provider(provider)?)
            }
            AnyCalendarKind::Coptic => AnyCalendar::Coptic(Coptic),
            AnyCalendarKind::Dangi => {
                AnyCalendar::Dangi(Dangi::try_new_with_any_provider(provider)?)
            }
            AnyCalendarKind::Ethiopian => AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(
                EthiopianEraStyle::AmeteMihret,
            )),
            AnyCalendarKind::EthiopianAmeteAlem => {
                AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem))
            }
            AnyCalendarKind::Gregorian => AnyCalendar::Gregorian(Gregorian),
            AnyCalendarKind::Hebrew => AnyCalendar::Hebrew(Hebrew),
            AnyCalendarKind::Indian => AnyCalendar::Indian(Indian),
            AnyCalendarKind::IslamicCivil => AnyCalendar::IslamicCivil(IslamicCivil),
            AnyCalendarKind::IslamicObservational => AnyCalendar::IslamicObservational(
                IslamicObservational::try_new_with_any_provider(provider)?,
            ),
            AnyCalendarKind::IslamicTabular => AnyCalendar::IslamicTabular(IslamicTabular),
            AnyCalendarKind::IslamicUmmAlQura => AnyCalendar::IslamicUmmAlQura(
                IslamicUmmAlQura::try_new_with_any_provider(provider)?,
            ),
            AnyCalendarKind::Iso => AnyCalendar::Iso(Iso),
            AnyCalendarKind::Japanese => {
                AnyCalendar::Japanese(Japanese::try_new_with_any_provider(provider)?)
            }
            AnyCalendarKind::JapaneseExtended => AnyCalendar::JapaneseExtended(
                JapaneseExtended::try_new_with_any_provider(provider)?,
            ),
            AnyCalendarKind::Persian => AnyCalendar::Persian(Persian),
            AnyCalendarKind::Roc => AnyCalendar::Roc(Roc),
        })
    }

    #[cfg(feature = "serde")]
    #[doc = icu_provider::gen_any_buffer_unstable_docs!(BUFFER, Self::new_for_kind)]
    pub fn try_new_for_kind_with_buffer_provider<P>(
        provider: &P,
        kind: AnyCalendarKind,
    ) -> Result<Self, DataError>
    where
        P: BufferProvider + ?Sized,
    {
        Ok(match kind {
            AnyCalendarKind::Buddhist => AnyCalendar::Buddhist(Buddhist),
            AnyCalendarKind::Chinese => {
                AnyCalendar::Chinese(Chinese::try_new_with_buffer_provider(provider)?)
            }
            AnyCalendarKind::Coptic => AnyCalendar::Coptic(Coptic),
            AnyCalendarKind::Dangi => {
                AnyCalendar::Dangi(Dangi::try_new_with_buffer_provider(provider)?)
            }
            AnyCalendarKind::Ethiopian => AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(
                EthiopianEraStyle::AmeteMihret,
            )),
            AnyCalendarKind::EthiopianAmeteAlem => {
                AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem))
            }
            AnyCalendarKind::Gregorian => AnyCalendar::Gregorian(Gregorian),
            AnyCalendarKind::Hebrew => AnyCalendar::Hebrew(Hebrew),
            AnyCalendarKind::Indian => AnyCalendar::Indian(Indian),
            AnyCalendarKind::IslamicCivil => AnyCalendar::IslamicCivil(IslamicCivil),
            AnyCalendarKind::IslamicObservational => AnyCalendar::IslamicObservational(
                IslamicObservational::try_new_with_buffer_provider(provider)?,
            ),
            AnyCalendarKind::IslamicTabular => AnyCalendar::IslamicTabular(IslamicTabular),
            AnyCalendarKind::IslamicUmmAlQura => AnyCalendar::IslamicUmmAlQura(
                IslamicUmmAlQura::try_new_with_buffer_provider(provider)?,
            ),
            AnyCalendarKind::Iso => AnyCalendar::Iso(Iso),
            AnyCalendarKind::Japanese => {
                AnyCalendar::Japanese(Japanese::try_new_with_buffer_provider(provider)?)
            }
            AnyCalendarKind::JapaneseExtended => AnyCalendar::JapaneseExtended(
                JapaneseExtended::try_new_with_buffer_provider(provider)?,
            ),
            AnyCalendarKind::Persian => AnyCalendar::Persian(Persian),
            AnyCalendarKind::Roc => AnyCalendar::Roc(Roc),
        })
    }

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new_for_kind)]
    pub fn try_new_for_kind_unstable<P>(
        provider: &P,
        kind: AnyCalendarKind,
    ) -> Result<Self, DataError>
    where
        P: DataProvider<crate::provider::JapaneseErasV1Marker>
            + DataProvider<crate::provider::JapaneseExtendedErasV1Marker>
            + DataProvider<crate::provider::ChineseCacheV1Marker>
            + DataProvider<crate::provider::DangiCacheV1Marker>
            + DataProvider<crate::provider::IslamicObservationalCacheV1Marker>
            + DataProvider<crate::provider::IslamicUmmAlQuraCacheV1Marker>
            + ?Sized,
    {
        Ok(match kind {
            AnyCalendarKind::Buddhist => AnyCalendar::Buddhist(Buddhist),
            AnyCalendarKind::Chinese => AnyCalendar::Chinese(Chinese::try_new_unstable(provider)?),
            AnyCalendarKind::Coptic => AnyCalendar::Coptic(Coptic),
            AnyCalendarKind::Dangi => AnyCalendar::Dangi(Dangi::try_new_unstable(provider)?),
            AnyCalendarKind::Ethiopian => AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(
                EthiopianEraStyle::AmeteMihret,
            )),
            AnyCalendarKind::EthiopianAmeteAlem => {
                AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem))
            }
            AnyCalendarKind::Gregorian => AnyCalendar::Gregorian(Gregorian),
            AnyCalendarKind::Hebrew => AnyCalendar::Hebrew(Hebrew),
            AnyCalendarKind::Indian => AnyCalendar::Indian(Indian),
            AnyCalendarKind::IslamicCivil => AnyCalendar::IslamicCivil(IslamicCivil),
            AnyCalendarKind::IslamicObservational => {
                AnyCalendar::IslamicObservational(IslamicObservational::try_new_unstable(provider)?)
            }
            AnyCalendarKind::IslamicTabular => AnyCalendar::IslamicTabular(IslamicTabular),
            AnyCalendarKind::IslamicUmmAlQura => {
                AnyCalendar::IslamicUmmAlQura(IslamicUmmAlQura::try_new_unstable(provider)?)
            }
            AnyCalendarKind::Iso => AnyCalendar::Iso(Iso),
            AnyCalendarKind::Japanese => {
                AnyCalendar::Japanese(Japanese::try_new_unstable(provider)?)
            }
            AnyCalendarKind::JapaneseExtended => {
                AnyCalendar::JapaneseExtended(JapaneseExtended::try_new_unstable(provider)?)
            }
            AnyCalendarKind::Persian => AnyCalendar::Persian(Persian),
            AnyCalendarKind::Roc => AnyCalendar::Roc(Roc),
        })
    }

    /// Constructs an AnyCalendar for a given preference bag from compiled data.
    ///
    /// In case the locale's calendar is unknown or unspecified, it will attempt to load the default
    /// calendar for the locale, falling back to gregorian.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new(prefs: AnyCalendarPreferences) -> Result<Self, DataError> {
        // NOTE: This returns DataError because this operation should become data-driven
        let kind = AnyCalendarKind::from_prefs_with_fallback(prefs);
        Ok(Self::new_for_kind(kind))
    }

    icu_provider::gen_any_buffer_data_constructors!(
        (prefs: AnyCalendarPreferences) -> error: DataError,
        functions: [
            try_new: skip,
            try_new_with_any_provider,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<P>(
        provider: &P,
        prefs: AnyCalendarPreferences,
    ) -> Result<Self, DataError>
    where
        P: DataProvider<crate::provider::JapaneseErasV1Marker>
            + DataProvider<crate::provider::JapaneseExtendedErasV1Marker>
            + DataProvider<crate::provider::ChineseCacheV1Marker>
            + DataProvider<crate::provider::DangiCacheV1Marker>
            + DataProvider<crate::provider::IslamicObservationalCacheV1Marker>
            + DataProvider<crate::provider::IslamicUmmAlQuraCacheV1Marker>
            + ?Sized,
    {
        let kind = AnyCalendarKind::from_prefs_with_fallback(prefs);
        Self::try_new_for_kind_unstable(provider, kind)
    }

    /// The [`AnyCalendarKind`] corresponding to the calendar this contains
    pub fn kind(&self) -> AnyCalendarKind {
        match *self {
            Self::Buddhist(_) => AnyCalendarKind::Buddhist,
            Self::Chinese(_) => AnyCalendarKind::Chinese,
            Self::Coptic(_) => AnyCalendarKind::Coptic,
            Self::Dangi(_) => AnyCalendarKind::Dangi,
            #[allow(clippy::expect_used)] // Invariant known at compile time
            Self::Ethiopian(ref e) => e
                .any_calendar_kind()
                .expect("Ethiopian calendar known to have an AnyCalendarKind"),
            Self::Gregorian(_) => AnyCalendarKind::Gregorian,
            Self::Hebrew(_) => AnyCalendarKind::Hebrew,
            Self::Indian(_) => AnyCalendarKind::Indian,
            Self::IslamicCivil(_) => AnyCalendarKind::IslamicCivil,
            Self::IslamicObservational(_) => AnyCalendarKind::IslamicObservational,
            Self::IslamicTabular(_) => AnyCalendarKind::IslamicTabular,
            Self::IslamicUmmAlQura(_) => AnyCalendarKind::IslamicUmmAlQura,
            Self::Iso(_) => AnyCalendarKind::Iso,
            Self::Japanese(_) => AnyCalendarKind::Japanese,
            Self::JapaneseExtended(_) => AnyCalendarKind::JapaneseExtended,
            Self::Persian(_) => AnyCalendarKind::Persian,
            Self::Roc(_) => AnyCalendarKind::Roc,
        }
    }

    /// Given an AnyCalendar date, convert that date to another AnyCalendar date in this calendar,
    /// if conversion is needed
    pub fn convert_any_date<'a>(
        &'a self,
        date: &Date<impl AsCalendar<Calendar = AnyCalendar>>,
    ) -> Date<Ref<'a, AnyCalendar>> {
        if self.kind() != date.calendar.as_calendar().kind() {
            Date::new_from_iso(date.to_iso(), Ref(self))
        } else {
            Date {
                inner: date.inner.clone(),
                calendar: Ref(self),
            }
        }
    }

    /// Given an AnyCalendar datetime, convert that date to another AnyCalendar datetime in this calendar,
    /// if conversion is needed
    pub fn convert_any_datetime<'a>(
        &'a self,
        date: &DateTime<impl AsCalendar<Calendar = AnyCalendar>>,
    ) -> DateTime<Ref<'a, AnyCalendar>> {
        DateTime {
            time: date.time,
            date: self.convert_any_date(&date.date),
        }
    }
}

impl AnyDateInner {
    fn kind(&self) -> AnyCalendarKind {
        match *self {
            AnyDateInner::Buddhist(_) => AnyCalendarKind::Buddhist,
            AnyDateInner::Chinese(_) => AnyCalendarKind::Chinese,
            AnyDateInner::Coptic(_) => AnyCalendarKind::Coptic,
            AnyDateInner::Dangi(_) => AnyCalendarKind::Dangi,
            AnyDateInner::Ethiopian(_) => AnyCalendarKind::Ethiopian,
            AnyDateInner::Gregorian(_) => AnyCalendarKind::Gregorian,
            AnyDateInner::Hebrew(_) => AnyCalendarKind::Hebrew,
            AnyDateInner::Indian(_) => AnyCalendarKind::Indian,
            AnyDateInner::IslamicCivil(_) => AnyCalendarKind::IslamicCivil,
            AnyDateInner::IslamicObservational(_) => AnyCalendarKind::IslamicObservational,
            AnyDateInner::IslamicTabular(_) => AnyCalendarKind::IslamicTabular,
            AnyDateInner::IslamicUmmAlQura(_) => AnyCalendarKind::IslamicUmmAlQura,
            AnyDateInner::Iso(_) => AnyCalendarKind::Iso,
            AnyDateInner::Japanese(_) => AnyCalendarKind::Japanese,
            AnyDateInner::JapaneseExtended(_) => AnyCalendarKind::JapaneseExtended,
            AnyDateInner::Persian(_) => AnyCalendarKind::Persian,
            AnyDateInner::Roc(_) => AnyCalendarKind::Roc,
        }
    }
}

/// Convenient type for selecting the kind of AnyCalendar to construct
#[non_exhaustive]
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug)]
pub enum AnyCalendarKind {
    /// The kind of a [`Buddhist`] calendar
    Buddhist,
    /// The kind of a [`Chinese`] calendar
    Chinese,
    /// The kind of a [`Coptic`] calendar
    Coptic,
    /// The kind of a [`Dangi`] calendar
    Dangi,
    /// The kind of an [`Ethiopian`] calendar, with Amete Mihret era
    Ethiopian,
    /// The kind of an [`Ethiopian`] calendar, with Amete Alem era
    EthiopianAmeteAlem,
    /// The kind of a [`Gregorian`] calendar
    Gregorian,
    /// The kind of a [`Hebrew`] calendar
    Hebrew,
    /// The kind of a [`Indian`] calendar
    Indian,
    /// The kind of an [`IslamicCivil`] calendar
    IslamicCivil,
    /// The kind of an [`IslamicObservational`] calendar
    IslamicObservational,
    /// The kind of an [`IslamicTabular`] calendar
    IslamicTabular,
    /// The kind of an [`IslamicUmmAlQura`] calendar
    IslamicUmmAlQura,
    /// The kind of an [`Iso`] calendar
    Iso,
    /// The kind of a [`Japanese`] calendar
    Japanese,
    /// The kind of a [`JapaneseExtended`] calendar
    JapaneseExtended,
    /// The kind of a [`Persian`] calendar
    Persian,
    /// The kind of a [`Roc`] calendar
    Roc,
}

impl AnyCalendarKind {
    /// Construct from a BCP-47 string
    ///
    /// Returns `None` if the calendar is unknown.
    pub fn get_for_bcp47_string(x: &str) -> Option<Self> {
        Self::get_for_bcp47_bytes(x.as_bytes())
    }
    /// Construct from a BCP-47 byte string
    ///
    /// Returns `None` if the calendar is unknown.
    pub fn get_for_bcp47_bytes(x: &[u8]) -> Option<Self> {
        Some(match x {
            b"buddhist" => AnyCalendarKind::Buddhist,
            b"chinese" => AnyCalendarKind::Chinese,
            b"coptic" => AnyCalendarKind::Coptic,
            b"dangi" => AnyCalendarKind::Dangi,
            b"ethioaa" => AnyCalendarKind::EthiopianAmeteAlem,
            b"ethiopic" => AnyCalendarKind::Ethiopian,
            b"gregory" => AnyCalendarKind::Gregorian,
            b"hebrew" => AnyCalendarKind::Hebrew,
            b"indian" => AnyCalendarKind::Indian,
            b"islamic-civil" | b"islamicc" => AnyCalendarKind::IslamicCivil,
            b"islamic-tbla" => AnyCalendarKind::IslamicTabular,
            b"islamic-umalqura" => AnyCalendarKind::IslamicUmmAlQura,
            b"islamic" => AnyCalendarKind::IslamicObservational,
            b"iso" => AnyCalendarKind::Iso,
            b"japanese" => AnyCalendarKind::Japanese,
            b"japanext" => AnyCalendarKind::JapaneseExtended,
            b"persian" => AnyCalendarKind::Persian,
            b"roc" => AnyCalendarKind::Roc,
            _ => {
                // Log a warning when a calendar value is passed in but doesn't match any calendars
                DataError::custom("bcp47_bytes did not match any calendars").with_debug_context(x);
                return None;
            }
        })
    }
    /// Construct from a BCP-47 [`Value`]
    ///
    /// Returns `None` if the calendar is unknown.
    pub fn get_for_bcp47_value(x: &Value) -> Option<Self> {
        match x.as_subtags_slice() {
            [first] if first == "buddhist" => Some(AnyCalendarKind::Buddhist),
            [first] if first == "chinese" => Some(AnyCalendarKind::Chinese),
            [first] if first == "coptic" => Some(AnyCalendarKind::Coptic),
            [first] if first == "dangi" => Some(AnyCalendarKind::Dangi),
            [first] if first == "ethioaa" => Some(AnyCalendarKind::EthiopianAmeteAlem),
            [first] if first == "ethiopic" => Some(AnyCalendarKind::Ethiopian),
            [first] if first == "gregory" => Some(AnyCalendarKind::Gregorian),
            [first] if first == "hebrew" => Some(AnyCalendarKind::Hebrew),
            [first] if first == "indian" => Some(AnyCalendarKind::Indian),
            [first] if first == "islamic" => Some(AnyCalendarKind::IslamicObservational),
            [first] if first == "islamicc" => Some(AnyCalendarKind::IslamicCivil),
            [first, second] if first == "islamic" && second == "civil" => {
                Some(AnyCalendarKind::IslamicCivil)
            }
            [first, second] if first == "islamic" && second == "tbla" => {
                Some(AnyCalendarKind::IslamicTabular)
            }
            [first, second] if first == "islamic" && second == "umalqura" => {
                Some(AnyCalendarKind::IslamicUmmAlQura)
            }
            [first] if first == "iso" => Some(AnyCalendarKind::Iso),
            [first] if first == "japanese" => Some(AnyCalendarKind::Japanese),
            [first] if first == "japanext" => Some(AnyCalendarKind::JapaneseExtended),
            [first] if first == "persian" => Some(AnyCalendarKind::Persian),
            [first] if first == "roc" => Some(AnyCalendarKind::Roc),
            _ => {
                // Log a warning when a calendar value is passed in but doesn't match any calendars
                DataError::custom("bcp47_value did not match any calendars")
                    .with_display_context(x);
                None
            }
        }
    }

    /// Convert to a BCP-47 string
    pub fn as_bcp47_string(self) -> &'static str {
        match self {
            AnyCalendarKind::Buddhist => "buddhist",
            AnyCalendarKind::Chinese => "chinese",
            AnyCalendarKind::Coptic => "coptic",
            AnyCalendarKind::Dangi => "dangi",
            AnyCalendarKind::Ethiopian => "ethiopic",
            AnyCalendarKind::EthiopianAmeteAlem => "ethioaa",
            AnyCalendarKind::Gregorian => "gregory",
            AnyCalendarKind::Hebrew => "hebrew",
            AnyCalendarKind::Indian => "indian",
            AnyCalendarKind::IslamicCivil => "islamic-civil",
            AnyCalendarKind::IslamicObservational => "islamic",
            AnyCalendarKind::IslamicTabular => "islamic-tbla",
            AnyCalendarKind::IslamicUmmAlQura => "islamic-umalqura",
            AnyCalendarKind::Iso => "iso",
            AnyCalendarKind::Japanese => "japanese",
            AnyCalendarKind::JapaneseExtended => "japanext",
            AnyCalendarKind::Persian => "persian",
            AnyCalendarKind::Roc => "roc",
        }
    }

    /// Convert to a BCP-47 `Value`
    #[allow(clippy::unwrap_used)] // these are known-good BCP47 unicode extension values
    pub fn as_bcp47_value(self) -> Value {
        match self {
            AnyCalendarKind::Buddhist => value!("buddhist"),
            AnyCalendarKind::Chinese => value!("chinese"),
            AnyCalendarKind::Coptic => value!("coptic"),
            AnyCalendarKind::Dangi => value!("dangi"),
            AnyCalendarKind::Ethiopian => value!("ethiopic"),
            AnyCalendarKind::EthiopianAmeteAlem => value!("ethioaa"),
            AnyCalendarKind::Gregorian => value!("gregory"),
            AnyCalendarKind::Hebrew => value!("hebrew"),
            AnyCalendarKind::Indian => value!("indian"),
            AnyCalendarKind::IslamicCivil => Value::try_from_str("islamic-civil").unwrap(),
            AnyCalendarKind::IslamicObservational => value!("islamic"),
            AnyCalendarKind::IslamicTabular => Value::try_from_str("islamic-tbla").unwrap(),
            AnyCalendarKind::IslamicUmmAlQura => Value::try_from_str("islamic-umalqura").unwrap(),
            AnyCalendarKind::Iso => value!("iso"),
            AnyCalendarKind::Japanese => value!("japanese"),
            AnyCalendarKind::JapaneseExtended => value!("japanext"),
            AnyCalendarKind::Persian => value!("persian"),
            AnyCalendarKind::Roc => value!("roc"),
        }
    }

    fn get_for_preferences_calendar(pcal: CalendarAlgorithm) -> Option<Self> {
        match pcal {
            CalendarAlgorithm::Buddhist => Some(Self::Buddhist),
            CalendarAlgorithm::Chinese => Some(Self::Chinese),
            CalendarAlgorithm::Coptic => Some(Self::Coptic),
            CalendarAlgorithm::Dangi => Some(Self::Dangi),
            CalendarAlgorithm::Ethioaa => Some(Self::EthiopianAmeteAlem),
            CalendarAlgorithm::Ethiopic => Some(Self::Ethiopian),
            CalendarAlgorithm::Gregory => Some(Self::Gregorian),
            CalendarAlgorithm::Hebrew => Some(Self::Hebrew),
            CalendarAlgorithm::Indian => Some(Self::Indian),
            CalendarAlgorithm::Islamic(None) => Some(Self::IslamicObservational),
            CalendarAlgorithm::Islamic(Some(islamic)) => match islamic {
                IslamicCalendarAlgorithm::Umalqura => Some(Self::IslamicUmmAlQura),
                IslamicCalendarAlgorithm::Tbla => Some(Self::IslamicTabular),
                IslamicCalendarAlgorithm::Civil => Some(Self::IslamicCivil),
                // Rgsa is not supported
                IslamicCalendarAlgorithm::Rgsa => None,
                _ => {
                    debug_assert!(false, "unknown calendar algorithm {pcal:?}");
                    None
                }
            },
            CalendarAlgorithm::Iso8601 => Some(Self::Iso),
            CalendarAlgorithm::Japanese => Some(Self::Japanese),
            CalendarAlgorithm::Persian => Some(Self::Persian),
            CalendarAlgorithm::Roc => Some(Self::Roc),
            _ => {
                debug_assert!(false, "unknown calendar algorithm {pcal:?}");
                None
            }
        }
    }

    fn debug_name(self) -> &'static str {
        match self {
            AnyCalendarKind::Buddhist => Buddhist.debug_name(),
            AnyCalendarKind::Chinese => Chinese::DEBUG_NAME,
            AnyCalendarKind::Coptic => Coptic.debug_name(),
            AnyCalendarKind::Dangi => Dangi::DEBUG_NAME,
            AnyCalendarKind::Ethiopian => Ethiopian(false).debug_name(),
            AnyCalendarKind::EthiopianAmeteAlem => Ethiopian(true).debug_name(),
            AnyCalendarKind::Gregorian => Gregorian.debug_name(),
            AnyCalendarKind::Hebrew => Hebrew.debug_name(),
            AnyCalendarKind::Indian => Indian.debug_name(),
            AnyCalendarKind::IslamicCivil => IslamicCivil.debug_name(),
            AnyCalendarKind::IslamicObservational => IslamicObservational::DEBUG_NAME,
            AnyCalendarKind::IslamicTabular => IslamicTabular.debug_name(),
            AnyCalendarKind::IslamicUmmAlQura => IslamicUmmAlQura::DEBUG_NAME,
            AnyCalendarKind::Iso => Iso.debug_name(),
            AnyCalendarKind::Japanese => Japanese::DEBUG_NAME,
            AnyCalendarKind::JapaneseExtended => JapaneseExtended::DEBUG_NAME,
            AnyCalendarKind::Persian => Persian.debug_name(),
            AnyCalendarKind::Roc => Roc.debug_name(),
        }
    }

    /// Extract the calendar component from a [`Locale`]
    ///
    /// Returns `None` if the calendar is not specified or unknown.
    pub fn get_for_locale(l: &Locale) -> Option<Self> {
        l.extensions
            .unicode
            .keywords
            .get(&key!("ca"))
            .and_then(Self::get_for_bcp47_value)
    }

    /// Extract the calendar component from a [`AnyCalendarPreferences`]
    ///
    /// Returns `None` if the calendar is not specified or unknown.
    fn get_for_prefs(prefs: AnyCalendarPreferences) -> Option<Self> {
        prefs
            .calendar_algorithm
            .and_then(Self::get_for_preferences_calendar)
    }

    // Do not make public, this will eventually need fallback
    // data from the provider
    fn from_prefs_with_fallback(prefs: AnyCalendarPreferences) -> Self {
        if let Some(kind) = Self::get_for_prefs(prefs) {
            kind
        } else {
            let lang = prefs.locale_prefs.language;
            if lang == language!("th") {
                Self::Buddhist
            } else if lang == language!("sa") {
                Self::IslamicUmmAlQura
            } else if lang == language!("af") || lang == language!("ir") {
                Self::Persian
            } else {
                Self::Gregorian
            }
        }
    }
}

impl fmt::Display for AnyCalendarKind {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(self, f)
    }
}

/// Trait for calendars that may be converted to [`AnyCalendar`]
pub trait IntoAnyCalendar: Calendar + Sized {
    /// Convert this calendar into an [`AnyCalendar`], moving it
    ///
    /// You should not need to call this method directly
    fn to_any(self) -> AnyCalendar;

    /// The [`AnyCalendarKind`] enum variant associated with this calendar
    fn kind(&self) -> AnyCalendarKind;

    /// Convert this calendar into an [`AnyCalendar`], cloning it
    ///
    /// You should not need to call this method directly
    fn to_any_cloned(&self) -> AnyCalendar;

    /// Move an [`AnyCalendar`] into a `Self`, or returning it as an error
    /// if the types do not match.
    ///
    /// You should not need to call this method directly
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar>;

    /// Convert an [`AnyCalendar`] reference into a `Self` reference.
    ///
    /// You should not need to call this method directly
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self>;

    /// Convert a date for this calendar into an [`AnyDateInner`]
    ///
    /// You should not need to call this method directly
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner;
}

impl IntoAnyCalendar for AnyCalendar {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        self
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        self.kind()
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        self.clone()
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        Ok(any)
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        Some(any)
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        d.clone()
    }
}

impl IntoAnyCalendar for Buddhist {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Buddhist(Buddhist)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Buddhist
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::Buddhist(Buddhist)
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Buddhist(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Buddhist(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Buddhist(*d)
    }
}

impl From<Buddhist> for AnyCalendar {
    fn from(value: Buddhist) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Chinese {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Chinese(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Chinese
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::Chinese(self.clone())
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Chinese(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Chinese(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Chinese(*d)
    }
}

impl From<Chinese> for AnyCalendar {
    fn from(value: Chinese) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Coptic {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Coptic(Coptic)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Coptic
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::Coptic(Coptic)
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Coptic(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Coptic(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Coptic(*d)
    }
}

impl From<Coptic> for AnyCalendar {
    fn from(value: Coptic) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Dangi {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Dangi(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Dangi
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::Dangi(self.clone())
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Dangi(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Dangi(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Dangi(*d)
    }
}

impl From<Dangi> for AnyCalendar {
    fn from(value: Dangi) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Ethiopian {
    // Amete Mihret calendars are the default
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Ethiopian(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        if self.0 {
            AnyCalendarKind::EthiopianAmeteAlem
        } else {
            AnyCalendarKind::Ethiopian
        }
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::Ethiopian(*self)
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Ethiopian(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Ethiopian(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Ethiopian(*d)
    }
}

impl From<Ethiopian> for AnyCalendar {
    fn from(value: Ethiopian) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Gregorian {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Gregorian(Gregorian)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Gregorian
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::Gregorian(Gregorian)
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Gregorian(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Gregorian(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Gregorian(*d)
    }
}

impl From<Gregorian> for AnyCalendar {
    fn from(value: Gregorian) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Hebrew {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Hebrew(Hebrew)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Hebrew
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::Hebrew(Hebrew)
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Hebrew(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Hebrew(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Hebrew(*d)
    }
}

impl From<Hebrew> for AnyCalendar {
    fn from(value: Hebrew) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Indian {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Indian(Indian)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Indian
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::Indian(Indian)
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Indian(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Indian(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Indian(*d)
    }
}

impl From<Indian> for AnyCalendar {
    fn from(value: Indian) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for IslamicCivil {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::IslamicCivil(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::IslamicCivil
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::IslamicCivil(*self)
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::IslamicCivil(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::IslamicCivil(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::IslamicCivil(*d)
    }
}

impl From<IslamicCivil> for AnyCalendar {
    fn from(value: IslamicCivil) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for IslamicObservational {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::IslamicObservational(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::IslamicObservational
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::IslamicObservational(self.clone())
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::IslamicObservational(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::IslamicObservational(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::IslamicObservational(*d)
    }
}

impl From<IslamicObservational> for AnyCalendar {
    fn from(value: IslamicObservational) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for IslamicTabular {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::IslamicTabular(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::IslamicTabular
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::IslamicTabular(*self)
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::IslamicTabular(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::IslamicTabular(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::IslamicTabular(*d)
    }
}

impl From<IslamicTabular> for AnyCalendar {
    fn from(value: IslamicTabular) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for IslamicUmmAlQura {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::IslamicUmmAlQura(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::IslamicUmmAlQura
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::IslamicUmmAlQura(self.clone())
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::IslamicUmmAlQura(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::IslamicUmmAlQura(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::IslamicUmmAlQura(*d)
    }
}

impl From<IslamicUmmAlQura> for AnyCalendar {
    fn from(value: IslamicUmmAlQura) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Iso {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Iso(Iso)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Iso
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::Iso(Iso)
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Iso(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Iso(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Iso(*d)
    }
}

impl From<Iso> for AnyCalendar {
    fn from(value: Iso) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Japanese {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Japanese(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Japanese
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::Japanese(self.clone())
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Japanese(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Japanese(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Japanese(*d)
    }
}

impl From<Japanese> for AnyCalendar {
    fn from(value: Japanese) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for JapaneseExtended {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::JapaneseExtended(self)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::JapaneseExtended
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::JapaneseExtended(self.clone())
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::JapaneseExtended(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::JapaneseExtended(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::JapaneseExtended(*d)
    }
}

impl From<JapaneseExtended> for AnyCalendar {
    fn from(value: JapaneseExtended) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Persian {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Persian(Persian)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Persian
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::Persian(Persian)
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Persian(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Persian(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Persian(*d)
    }
}

impl From<Persian> for AnyCalendar {
    fn from(value: Persian) -> AnyCalendar {
        value.to_any()
    }
}

impl IntoAnyCalendar for Roc {
    #[inline]
    fn to_any(self) -> AnyCalendar {
        AnyCalendar::Roc(Roc)
    }
    #[inline]
    fn kind(&self) -> AnyCalendarKind {
        AnyCalendarKind::Roc
    }
    #[inline]
    fn to_any_cloned(&self) -> AnyCalendar {
        AnyCalendar::Roc(Roc)
    }
    #[inline]
    fn from_any(any: AnyCalendar) -> Result<Self, AnyCalendar> {
        if let AnyCalendar::Roc(cal) = any {
            Ok(cal)
        } else {
            Err(any)
        }
    }
    #[inline]
    fn from_any_ref(any: &AnyCalendar) -> Option<&Self> {
        if let AnyCalendar::Roc(cal) = any {
            Some(cal)
        } else {
            None
        }
    }
    #[inline]
    fn date_to_any(&self, d: &Self::DateInner) -> AnyDateInner {
        AnyDateInner::Roc(*d)
    }
}

impl From<Roc> for AnyCalendar {
    fn from(value: Roc) -> AnyCalendar {
        value.to_any()
    }
}

#[cfg(test)]
mod tests {
    use tinystr::tinystr;
    use types::MonthCode;

    use super::*;
    use crate::Ref;

    fn single_test_roundtrip(
        calendar: Ref<AnyCalendar>,
        era: &str,
        year: i32,
        month_code: &str,
        day: u8,
    ) {
        let era = types::Era(era.parse().expect("era must parse"));
        let month = types::MonthCode(month_code.parse().expect("month code must parse"));

        let date =
            Date::try_new_from_codes(Some(era), year, month, day, calendar).unwrap_or_else(|e| {
                panic!(
                    "Failed to construct date for {} with {era:?}, {year}, {month}, {day}: {e:?}",
                    calendar.debug_name(),
                )
            });

        let roundtrip_year = date.year();
        // FIXME: these APIs should be improved
        let roundtrip_year = roundtrip_year.era_year_or_extended();
        let roundtrip_month = date.month().standard_code;
        let roundtrip_day = date.day_of_month().0;

        assert_eq!(
            (year, month, day),
            (roundtrip_year, roundtrip_month, roundtrip_day),
            "Failed to roundtrip for calendar {}",
            calendar.debug_name()
        );

        let iso = date.to_iso();
        let reconstructed = Date::new_from_iso(iso, calendar);
        assert_eq!(
            date, reconstructed,
            "Failed to roundtrip via iso with {era:?}, {year}, {month}, {day}"
        )
    }

    fn single_test_error(
        calendar: Ref<AnyCalendar>,
        era: &str,
        year: i32,
        month_code: &str,
        day: u8,
        error: DateError,
    ) {
        let era = types::Era(era.parse().expect("era must parse"));
        let month = types::MonthCode(month_code.parse().expect("month code must parse"));

        let date = Date::try_new_from_codes(Some(era), year, month, day, calendar);
        assert_eq!(
            date,
            Err(error),
            "Construction with {era:?}, {year}, {month}, {day} did not return {error:?}"
        )
    }

    #[test]
    fn test_any_construction() {
        let buddhist = AnyCalendar::new_for_kind(AnyCalendarKind::Buddhist);
        let chinese = AnyCalendar::new_for_kind(AnyCalendarKind::Chinese);
        let coptic = AnyCalendar::new_for_kind(AnyCalendarKind::Coptic);
        let dangi = AnyCalendar::new_for_kind(AnyCalendarKind::Dangi);
        let ethioaa = AnyCalendar::new_for_kind(AnyCalendarKind::EthiopianAmeteAlem);
        let ethiopian = AnyCalendar::new_for_kind(AnyCalendarKind::Ethiopian);
        let gregorian = AnyCalendar::new_for_kind(AnyCalendarKind::Gregorian);
        let hebrew = AnyCalendar::new_for_kind(AnyCalendarKind::Hebrew);
        let indian = AnyCalendar::new_for_kind(AnyCalendarKind::Indian);
        let islamic_civil: AnyCalendar = AnyCalendar::new_for_kind(AnyCalendarKind::IslamicCivil);
        let islamic_observational: AnyCalendar =
            AnyCalendar::new_for_kind(AnyCalendarKind::IslamicObservational);
        let islamic_tabular: AnyCalendar =
            AnyCalendar::new_for_kind(AnyCalendarKind::IslamicTabular);
        let islamic_umm_al_qura: AnyCalendar =
            AnyCalendar::new_for_kind(AnyCalendarKind::IslamicUmmAlQura);
        let japanese = AnyCalendar::new_for_kind(AnyCalendarKind::Japanese);
        let japanext = AnyCalendar::new_for_kind(AnyCalendarKind::JapaneseExtended);
        let persian = AnyCalendar::new_for_kind(AnyCalendarKind::Persian);
        let roc = AnyCalendar::new_for_kind(AnyCalendarKind::Roc);
        let buddhist = Ref(&buddhist);
        let chinese = Ref(&chinese);
        let coptic = Ref(&coptic);
        let dangi = Ref(&dangi);
        let ethioaa = Ref(&ethioaa);
        let ethiopian = Ref(&ethiopian);
        let gregorian = Ref(&gregorian);
        let hebrew = Ref(&hebrew);
        let indian = Ref(&indian);
        let islamic_civil = Ref(&islamic_civil);
        let islamic_observational = Ref(&islamic_observational);
        let islamic_tabular = Ref(&islamic_tabular);
        let islamic_umm_al_qura = Ref(&islamic_umm_al_qura);
        let japanese = Ref(&japanese);
        let japanext = Ref(&japanext);
        let persian = Ref(&persian);
        let roc = Ref(&roc);

        single_test_roundtrip(buddhist, "be", 100, "M03", 1);
        single_test_roundtrip(buddhist, "be", 2000, "M03", 1);
        single_test_roundtrip(buddhist, "be", -100, "M03", 1);
        single_test_error(
            buddhist,
            "be",
            100,
            "M13",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M13"))),
        );

        single_test_roundtrip(coptic, "ad", 100, "M03", 1);
        single_test_roundtrip(coptic, "ad", 2000, "M03", 1);
        // fails ISO roundtrip
        // single_test_roundtrip(coptic, "bd", 100, "M03", 1);
        single_test_roundtrip(coptic, "ad", 100, "M13", 1);
        single_test_error(
            coptic,
            "ad",
            100,
            "M14",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M14"))),
        );
        single_test_error(
            coptic,
            "ad",
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );
        single_test_error(
            coptic,
            "bd",
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );

        single_test_roundtrip(ethiopian, "incar", 100, "M03", 1);
        single_test_roundtrip(ethiopian, "incar", 2000, "M03", 1);
        single_test_roundtrip(ethiopian, "incar", 2000, "M13", 1);
        // Fails ISO roundtrip due to https://github.com/unicode-org/icu4x/issues/2254
        // single_test_roundtrip(ethiopian, "pre-incar", 100, "M03", 1);
        single_test_error(
            ethiopian,
            "incar",
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );
        single_test_error(
            ethiopian,
            "pre-incar",
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );
        single_test_error(
            ethiopian,
            "incar",
            100,
            "M14",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M14"))),
        );

        single_test_roundtrip(ethioaa, "mundi", 7000, "M13", 1);
        single_test_roundtrip(ethioaa, "mundi", 7000, "M13", 1);
        // Fails ISO roundtrip due to https://github.com/unicode-org/icu4x/issues/2254
        // single_test_roundtrip(ethioaa, "mundi", 100, "M03", 1);
        single_test_error(
            ethiopian,
            "mundi",
            100,
            "M14",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M14"))),
        );

        single_test_roundtrip(gregorian, "ce", 100, "M03", 1);
        single_test_roundtrip(gregorian, "ce", 2000, "M03", 1);
        single_test_roundtrip(gregorian, "bce", 100, "M03", 1);
        single_test_error(
            gregorian,
            "ce",
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );
        single_test_error(
            gregorian,
            "bce",
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );

        single_test_error(
            gregorian,
            "bce",
            100,
            "M13",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M13"))),
        );

        single_test_roundtrip(indian, "saka", 100, "M03", 1);
        single_test_roundtrip(indian, "saka", 2000, "M12", 1);
        single_test_roundtrip(indian, "saka", -100, "M03", 1);
        single_test_roundtrip(indian, "saka", 0, "M03", 1);
        single_test_error(
            indian,
            "saka",
            100,
            "M13",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M13"))),
        );

        single_test_roundtrip(chinese, "chinese", 400, "M02", 5);
        single_test_roundtrip(chinese, "chinese", 4660, "M07", 29);
        single_test_roundtrip(chinese, "chinese", -100, "M11", 12);
        single_test_error(
            chinese,
            "chinese",
            4658,
            "M13",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M13"))),
        );

        single_test_roundtrip(dangi, "dangi", 400, "M02", 5);
        single_test_roundtrip(dangi, "dangi", 4660, "M08", 29);
        single_test_roundtrip(dangi, "dangi", -1300, "M11", 12);
        single_test_error(
            dangi,
            "dangi",
            10393,
            "M00L",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M00L"))),
        );

        single_test_roundtrip(japanese, "reiwa", 3, "M03", 1);
        single_test_roundtrip(japanese, "heisei", 6, "M12", 1);
        single_test_roundtrip(japanese, "meiji", 10, "M03", 1);
        single_test_roundtrip(japanese, "ce", 1000, "M03", 1);
        single_test_roundtrip(japanese, "bce", 10, "M03", 1);
        single_test_error(
            japanese,
            "ce",
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );
        single_test_error(
            japanese,
            "bce",
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );

        single_test_error(
            japanese,
            "reiwa",
            2,
            "M13",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M13"))),
        );

        single_test_roundtrip(japanext, "reiwa", 3, "M03", 1);
        single_test_roundtrip(japanext, "heisei", 6, "M12", 1);
        single_test_roundtrip(japanext, "meiji", 10, "M03", 1);
        single_test_roundtrip(japanext, "tenpyokampo-749", 1, "M04", 20);
        single_test_roundtrip(japanext, "ce", 100, "M03", 1);
        single_test_roundtrip(japanext, "bce", 10, "M03", 1);
        single_test_error(
            japanext,
            "ce",
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );
        single_test_error(
            japanext,
            "bce",
            0,
            "M03",
            1,
            DateError::Range {
                field: "year",
                value: 0,
                min: 1,
                max: i32::MAX,
            },
        );

        single_test_error(
            japanext,
            "reiwa",
            2,
            "M13",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M13"))),
        );

        single_test_roundtrip(persian, "ah", 477, "M03", 1);
        single_test_roundtrip(persian, "ah", 2083, "M07", 21);
        single_test_roundtrip(persian, "ah", 1600, "M12", 20);
        single_test_error(
            persian,
            "ah",
            100,
            "M9",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M9"))),
        );

        single_test_roundtrip(hebrew, "hebrew", 5773, "M03", 1);
        single_test_roundtrip(hebrew, "hebrew", 4993, "M07", 21);
        single_test_roundtrip(hebrew, "hebrew", 5012, "M12", 20);
        single_test_error(
            hebrew,
            "hebrew",
            100,
            "M9",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M9"))),
        );

        single_test_roundtrip(roc, "roc", 10, "M05", 3);
        single_test_roundtrip(roc, "roc-inverse", 15, "M01", 10);
        single_test_roundtrip(roc, "roc", 100, "M10", 30);

        single_test_roundtrip(islamic_observational, "ah", 477, "M03", 1);
        single_test_roundtrip(islamic_observational, "ah", 2083, "M07", 21);
        single_test_roundtrip(islamic_observational, "ah", 1600, "M12", 20);
        single_test_error(
            islamic_observational,
            "ah",
            100,
            "M9",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M9"))),
        );

        single_test_roundtrip(islamic_civil, "ah", 477, "M03", 1);
        single_test_roundtrip(islamic_civil, "ah", 2083, "M07", 21);
        single_test_roundtrip(islamic_civil, "ah", 1600, "M12", 20);
        single_test_error(
            islamic_civil,
            "ah",
            100,
            "M9",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M9"))),
        );

        single_test_roundtrip(islamic_umm_al_qura, "ah", 477, "M03", 1);
        single_test_roundtrip(islamic_umm_al_qura, "ah", 2083, "M07", 21);
        single_test_roundtrip(islamic_umm_al_qura, "ah", 1600, "M12", 20);
        single_test_error(
            islamic_umm_al_qura,
            "ah",
            100,
            "M9",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M9"))),
        );

        single_test_roundtrip(islamic_tabular, "ah", 477, "M03", 1);
        single_test_roundtrip(islamic_tabular, "ah", 2083, "M07", 21);
        single_test_roundtrip(islamic_tabular, "ah", 1600, "M12", 20);
        single_test_error(
            islamic_tabular,
            "ah",
            100,
            "M9",
            1,
            DateError::UnknownMonthCode(MonthCode(tinystr!(4, "M9"))),
        );
    }
}
