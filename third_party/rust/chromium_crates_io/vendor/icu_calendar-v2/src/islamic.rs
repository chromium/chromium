// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Islamic calendars.
//!
//! ```rust
//! use icu::calendar::cal::IslamicObservational;
//! use icu::calendar::Date;
//!
//! let islamic = IslamicObservational::new_always_calculating();
//! let islamic_date = Date::try_new_observational_islamic_with_calendar(
//!     1348, 10, 11, islamic,
//! )
//! .expect("Failed to initialize islamic Date instance.");
//!
//! assert_eq!(islamic_date.year().era_year_or_extended(), 1348);
//! assert_eq!(islamic_date.month().ordinal, 10);
//! assert_eq!(islamic_date.day_of_month().0, 11);
//! ```

use crate::calendar_arithmetic::PrecomputedDataSource;
use crate::calendar_arithmetic::{ArithmeticDate, CalendarArithmetic};
use crate::error::DateError;
use crate::provider::islamic::{
    CalendarIslamicObservationalV1, CalendarIslamicUmmalquraV1, IslamicCache, PackedIslamicYearInfo,
};
use crate::Iso;
use crate::{types, Calendar, Date, DateDuration, DateDurationUnit};
use crate::{AsCalendar, RangeError};
use calendrical_calculations::islamic::{
    IslamicBasedMarker, ObservationalIslamicMarker, SaudiIslamicMarker,
};
use calendrical_calculations::rata_die::RataDie;
use core::marker::PhantomData;
use icu_provider::prelude::*;
use tinystr::tinystr;

fn year_as_islamic(standard_era: tinystr::TinyStr16, year: i32) -> types::YearInfo {
    types::YearInfo::new(
        year,
        types::EraYear {
            formatting_era: types::FormattingEra::Index(0, tinystr!(16, "AH")),
            standard_era: standard_era.into(),
            era_year: year,
            ambiguity: types::YearAmbiguity::CenturyRequired,
        },
    )
}

/// Islamic Observational Calendar (Default)
///
/// # Era codes
///
/// This calendar supports a single era code, Anno Mundi, with code `"ah"`
///
/// # Month codes
///
/// This calendar is a pure lunar calendar with no leap months. It uses month codes
/// `"M01" - "M12"`.
#[derive(Clone, Debug, Default)]
pub struct IslamicObservational {
    data: Option<DataPayload<CalendarIslamicObservationalV1>>,
}

/// Civil / Arithmetical Islamic Calendar (Used for administrative purposes)
///
/// # Era codes
///
/// This calendar supports a single era code, Anno Mundi, with code `"ah"`
///
/// # Month codes
///
/// This calendar is a pure lunar calendar with no leap months. It uses month codes
/// `"M01" - "M12"`.
#[derive(Copy, Clone, Debug, Default, Hash, Eq, PartialEq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_structs)] // unit struct
pub struct IslamicCivil;

/// Umm al-Qura Hijri Calendar (Used in Saudi Arabia)
///
/// # Era codes
///
/// This calendar supports a single era code, Anno Mundi, with code `"ah"`
///
/// # Month codes
///
/// This calendar is a pure lunar calendar with no leap months. It uses month codes
/// `"M01" - "M12"`.
#[derive(Clone, Debug, Default)]
pub struct IslamicUmmAlQura {
    data: Option<DataPayload<CalendarIslamicUmmalquraV1>>,
}

/// A Tabular version of the Arithmetical Islamic Calendar
///
/// # Era codes
///
/// This calendar supports a single era code, Anno Mundi, with code `"ah"`
///
/// # Month codes
///
/// This calendar is a pure lunar calendar with no leap months. It uses month codes
/// `"M01" - "M12"`.
#[derive(Copy, Clone, Debug, Default, Hash, Eq, PartialEq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_structs)] // unit struct
pub struct IslamicTabular;

impl IslamicObservational {
    /// Creates a new [`IslamicObservational`] with some compiled data containing precomputed calendrical calculations.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new() -> Self {
        Self {
            data: Some(DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_CALENDAR_ISLAMIC_OBSERVATIONAL_V1,
            )),
        }
    }

    icu_provider::gen_buffer_data_constructors!(() -> error: DataError,
        functions: [
            new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self,
    ]);

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<D: DataProvider<CalendarIslamicObservationalV1> + ?Sized>(
        provider: &D,
    ) -> Result<Self, DataError> {
        Ok(Self {
            data: Some(provider.load(Default::default())?.payload),
        })
    }

    /// Construct a new [`IslamicObservational`] without any precomputed calendrical calculations.
    pub fn new_always_calculating() -> Self {
        Self { data: None }
    }
}

impl IslamicCivil {
    /// Construct a new [`IslamicCivil`]
    pub fn new() -> Self {
        Self
    }
}

impl IslamicUmmAlQura {
    /// Creates a new [`IslamicUmmAlQura`] with some compiled data containing precomputed calendrical calculations.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new() -> Self {
        Self {
            data: Some(DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_CALENDAR_ISLAMIC_UMMALQURA_V1,
            )),
        }
    }

    icu_provider::gen_buffer_data_constructors!(() -> error: DataError,
        functions: [
            new: skip,
                        try_new_with_buffer_provider,
            try_new_unstable,
            Self,
    ]);

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<D: DataProvider<CalendarIslamicUmmalquraV1> + ?Sized>(
        provider: &D,
    ) -> Result<Self, DataError> {
        Ok(Self {
            data: Some(provider.load(Default::default())?.payload),
        })
    }

    /// Construct a new [`IslamicUmmAlQura`] without any precomputed calendrical calculations.
    pub fn new_always_calculating() -> Self {
        Self { data: None }
    }
}

impl IslamicTabular {
    /// Construct a new [`IslamicTabular`]
    pub fn new() -> Self {
        Self
    }
}

/// Compact representation of the length of an Islamic year.
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
enum IslamicYearLength {
    /// Long (355-day) Islamic year
    L355,
    /// Short (354-day) Islamic year
    L354,
    /// Unexpectedly Short (353-day) Islamic year
    ///
    /// It is probably a bug when this year length is returned. See:
    /// <https://github.com/unicode-org/icu4x/issues/4930>
    L353,
}

impl Default for IslamicYearLength {
    fn default() -> Self {
        Self::L354
    }
}

impl IslamicYearLength {
    fn try_from_int(value: i64) -> Option<Self> {
        match value {
            355 => Some(Self::L355),
            354 => Some(Self::L354),
            353 => Some(Self::L353),
            _ => None,
        }
    }
    fn to_int(self) -> u16 {
        match self {
            Self::L355 => 355,
            Self::L354 => 354,
            Self::L353 => 353,
        }
    }
}

#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) struct IslamicYearInfo {
    packed_data: PackedIslamicYearInfo,
    prev_year_length: IslamicYearLength,
}

impl IslamicYearInfo {
    pub(crate) const LONG_YEAR_LEN: u16 = 355;
    const SHORT_YEAR_LEN: u16 = 354;
    pub(crate) fn new(
        prev_packed: PackedIslamicYearInfo,
        this_packed: PackedIslamicYearInfo,
        extended_year: i32,
    ) -> (Self, i32) {
        let days_in_year = prev_packed.days_in_year();
        let days_in_year = match IslamicYearLength::try_from_int(days_in_year as i64) {
            Some(x) => x,
            None => {
                debug_assert!(false, "Found wrong year length for Islamic year {extended_year}: Expected 355, 354, or 353, got {days_in_year}");
                Default::default()
            }
        };
        let year_info = Self {
            prev_year_length: days_in_year,
            packed_data: this_packed,
        };
        (year_info, extended_year)
    }

    fn compute<IB: IslamicBasedMarker>(extended_year: i32) -> Self {
        let ny = IB::fixed_from_islamic(extended_year, 1, 1);
        let packed_data = PackedIslamicYearInfo::compute_with_ny::<IB>(extended_year, ny);
        let prev_ny = IB::fixed_from_islamic(extended_year - 1, 1, 1);
        let rd_diff = ny - prev_ny;
        let rd_diff = match IslamicYearLength::try_from_int(rd_diff) {
            Some(x) => x,
            None => {
                debug_assert!(false, "({}) Found wrong year length for Islamic year {extended_year}: Expected 355, 354, or 353, got {rd_diff}", IB::DEBUG_NAME);
                Default::default()
            }
        };
        Self {
            prev_year_length: rd_diff,
            packed_data,
        }
    }
    /// Get the new year R.D. given the extended year that this yearinfo is for    
    fn new_year<IB: IslamicBasedMarker>(self, extended_year: i32) -> RataDie {
        IB::mean_synodic_ny(extended_year) + i64::from(self.packed_data.ny_offset())
    }

    /// Get the date's R.D. given (y, m, d) in this info's year
    fn rd_for<IB: IslamicBasedMarker>(self, extended_year: i32, month: u8, day: u8) -> RataDie {
        let ny = self.new_year::<IB>(extended_year);
        let month_offset = if month == 1 {
            0
        } else {
            self.packed_data.last_day_of_month(month - 1)
        };
        // -1 since the offset is 1-indexed but the new year is also day 1
        ny - 1 + month_offset.into() + day.into()
    }

    #[inline]
    fn days_in_prev_year(self) -> u16 {
        self.prev_year_length.to_int()
    }
}

/// Contains any loaded precomputed data. If constructed with Default, will
/// *not* contain any extra data and will always compute stuff from scratch
#[derive(Default)]
pub(crate) struct IslamicPrecomputedData<'a, IB: IslamicBasedMarker> {
    data: Option<&'a IslamicCache<'a>>,
    _ib: PhantomData<IB>,
}

impl<IB: IslamicBasedMarker> PrecomputedDataSource<IslamicYearInfo>
    for IslamicPrecomputedData<'_, IB>
{
    fn load_or_compute_info(&self, extended_year: i32) -> IslamicYearInfo {
        self.data
            .and_then(|d| d.get_for_extended_year(extended_year))
            .unwrap_or_else(|| IslamicYearInfo::compute::<IB>(extended_year))
    }
}

/// Given a year info and the first month it is possible for this date to be in, return the
/// month and day this is in
fn compute_month_day(info: IslamicYearInfo, mut possible_month: u8, day_of_year: u16) -> (u8, u8) {
    let mut last_day_of_month = info.packed_data.last_day_of_month(possible_month);
    let mut last_day_of_prev_month = if possible_month == 1 {
        0
    } else {
        info.packed_data.last_day_of_month(possible_month - 1)
    };

    while day_of_year > last_day_of_month && possible_month <= 12 {
        possible_month += 1;
        last_day_of_prev_month = last_day_of_month;
        last_day_of_month = info.packed_data.last_day_of_month(possible_month);
    }
    let day = u8::try_from(day_of_year - last_day_of_prev_month);
    debug_assert!(
        day.is_ok(),
        "Found day {} that doesn't fit in month!",
        day_of_year - last_day_of_prev_month
    );
    (possible_month, day.unwrap_or(29))
}
impl<'b, IB: IslamicBasedMarker> IslamicPrecomputedData<'b, IB> {
    pub(crate) fn new(data: Option<&'b IslamicCache<'b>>) -> Self {
        Self {
            data,
            _ib: PhantomData,
        }
    }
    /// Given an ISO date (in both ArithmeticDate and R.D. format), returns the IslamicYearInfo and extended year for that date, loading
    /// from cache or computing.
    fn load_or_compute_info_for_iso(&self, fixed: RataDie) -> (IslamicYearInfo, i32, u8, u8) {
        let cached = self.data.and_then(|d| d.get_for_fixed::<IB>(fixed));
        if let Some((cached, year)) = cached {
            let ny = cached.packed_data.ny::<IB>(year);
            let day_of_year = (fixed - ny) as u16 + 1;
            debug_assert!(day_of_year < 360);
            // We divide by 30, not 29, to account for the case where all months before this
            // were length 30 (possible near the beginning of the year)
            // We add +1 because months are 1-indexed
            let possible_month = u8::try_from(1 + (day_of_year / 30)).unwrap_or(1);
            let (m, d) = compute_month_day(cached, possible_month, day_of_year);
            return (cached, year, m, d);
        };
        // compute

        let (y, m, d) = IB::islamic_from_fixed(fixed);
        let info = IslamicYearInfo::compute::<IB>(y);
        let ny = info.packed_data.ny::<IB>(y);
        let day_of_year = (fixed - ny) as u16 + 1;
        // We can't use the m/d from islamic_from_fixed because that code
        // occasionally throws up 31-day months, which we normalize out. So we instead back-compute, starting with the previous month
        let (m, d) = if m > 1 {
            compute_month_day(info, m - 1, day_of_year)
        } else {
            (m, d)
        };
        (info, y, m, d)
    }
}

/// The inner date type used for representing [`Date`]s of [`IslamicObservational`]. See [`Date`] and [`IslamicObservational`] for more details.

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct IslamicDateInner(ArithmeticDate<IslamicObservational>);

impl CalendarArithmetic for IslamicObservational {
    type YearInfo = IslamicYearInfo;

    fn month_days(_year: i32, month: u8, year_info: IslamicYearInfo) -> u8 {
        year_info.packed_data.days_in_month(month)
    }

    fn months_for_every_year(_year: i32, _year_info: IslamicYearInfo) -> u8 {
        12
    }

    fn days_in_provided_year(_year: i32, year_info: IslamicYearInfo) -> u16 {
        year_info.packed_data.days_in_year()
    }

    // As an true lunar calendar, it does not have leap years.
    fn is_leap_year(_year: i32, year_info: IslamicYearInfo) -> bool {
        year_info.packed_data.days_in_year() != IslamicYearInfo::SHORT_YEAR_LEN
    }

    fn last_month_day_in_year(year: i32, year_info: IslamicYearInfo) -> (u8, u8) {
        let days = Self::month_days(year, 12, year_info);

        (12, days)
    }
}

impl Calendar for IslamicObservational {
    type DateInner = IslamicDateInner;
    fn date_from_codes(
        &self,
        era: Option<types::Era>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        if let Some(era) = era {
            if era.0 != tinystr!(16, "islamic") && era.0 != tinystr!(16, "ah") {
                return Err(DateError::UnknownEra(era));
            }
        }
        let month = if let Some((ordinal, false)) = month_code.parsed() {
            ordinal
        } else {
            return Err(DateError::UnknownMonthCode(month_code));
        };
        Ok(IslamicDateInner(
            ArithmeticDate::new_from_ordinals_with_info(
                year,
                month,
                day,
                self.precomputed_data().load_or_compute_info(year),
            )?,
        ))
    }

    fn date_from_iso(&self, iso: Date<crate::Iso>) -> Self::DateInner {
        let fixed_iso = Iso::to_fixed(iso);

        let (year_info, y, m, d) = self
            .precomputed_data()
            .load_or_compute_info_for_iso(fixed_iso);
        IslamicDateInner(ArithmeticDate::new_unchecked_with_info(y, m, d, year_info))
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<crate::Iso> {
        let fixed = date.0.year_info.rd_for::<ObservationalIslamicMarker>(
            date.0.year,
            date.0.month,
            date.0.day,
        );
        Iso::from_fixed(fixed)
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
        Iso.day_of_week(self.date_to_iso(date).inner())
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        date.0.offset_date(offset, &self.precomputed_data())
    }

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

    fn debug_name(&self) -> &'static str {
        Self::DEBUG_NAME
    }

    fn year(&self, date: &Self::DateInner) -> types::YearInfo {
        year_as_islamic(tinystr!(16, "islamic"), date.0.year)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::is_leap_year(date.0.year, date.0.year_info)
    }

    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        date.0.month()
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        date.0.day_of_month()
    }

    fn day_of_year_info(&self, date: &Self::DateInner) -> types::DayOfYearInfo {
        let prev_year = date.0.year.saturating_sub(1);
        let next_year = date.0.year.saturating_add(1);
        types::DayOfYearInfo {
            day_of_year: date.0.day_of_year(),
            days_in_year: date.0.days_in_year(),
            prev_year: year_as_islamic(tinystr!(16, "islamic"), prev_year),
            days_in_prev_year: date.0.year_info.days_in_prev_year(),
            next_year: year_as_islamic(tinystr!(16, "islamic"), next_year),
        }
    }

    fn any_calendar_kind(&self) -> Option<crate::AnyCalendarKind> {
        Some(crate::any_calendar::IntoAnyCalendar::kind(self))
    }
}

impl IslamicObservational {
    fn precomputed_data(&self) -> IslamicPrecomputedData<ObservationalIslamicMarker> {
        IslamicPrecomputedData::new(self.data.as_ref().map(|x| x.get()))
    }

    pub(crate) const DEBUG_NAME: &'static str = "Islamic (observational)";
}

impl<A: AsCalendar<Calendar = IslamicObservational>> Date<A> {
    /// Construct new Islamic Observational Date.
    ///
    /// Has no negative years, only era is the AH.
    ///
    /// ```rust
    /// use icu::calendar::cal::IslamicObservational;
    /// use icu::calendar::Date;
    ///
    /// let islamic = IslamicObservational::new_always_calculating();
    ///
    /// let date_islamic =
    ///     Date::try_new_observational_islamic_with_calendar(1392, 4, 25, islamic)
    ///         .expect("Failed to initialize Islamic Date instance.");
    ///
    /// assert_eq!(date_islamic.year().era_year_or_extended(), 1392);
    /// assert_eq!(date_islamic.month().ordinal, 4);
    /// assert_eq!(date_islamic.day_of_month().0, 25);
    /// ```
    pub fn try_new_observational_islamic_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, RangeError> {
        let year_info = calendar
            .as_calendar()
            .precomputed_data()
            .load_or_compute_info(year);
        ArithmeticDate::new_from_ordinals_with_info(year, month, day, year_info)
            .map(IslamicDateInner)
            .map(|inner| Date::from_raw(inner, calendar))
    }
}

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
/// The inner date type used for representing [`Date`]s of [`IslamicUmmAlQura`]. See [`Date`] and [`IslamicUmmAlQura`] for more details.
pub struct IslamicUmmAlQuraDateInner(ArithmeticDate<IslamicUmmAlQura>);

impl CalendarArithmetic for IslamicUmmAlQura {
    type YearInfo = IslamicYearInfo;

    fn month_days(_year: i32, month: u8, year_info: IslamicYearInfo) -> u8 {
        year_info.packed_data.days_in_month(month)
    }

    fn months_for_every_year(_year: i32, _year_info: IslamicYearInfo) -> u8 {
        12
    }

    fn days_in_provided_year(_year: i32, year_info: IslamicYearInfo) -> u16 {
        year_info.packed_data.days_in_year()
    }

    // As an true lunar calendar, it does not have leap years.
    fn is_leap_year(_year: i32, year_info: IslamicYearInfo) -> bool {
        year_info.packed_data.days_in_year() != IslamicYearInfo::SHORT_YEAR_LEN
    }

    fn last_month_day_in_year(year: i32, year_info: IslamicYearInfo) -> (u8, u8) {
        let days = Self::month_days(year, 12, year_info);

        (12, days)
    }
}

impl Calendar for IslamicUmmAlQura {
    type DateInner = IslamicUmmAlQuraDateInner;
    fn date_from_codes(
        &self,
        era: Option<types::Era>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        if let Some(era) = era {
            if era.0 != tinystr!(16, "islamic-umalqura")
                && era.0 != tinystr!(16, "islamic")
                && era.0 != tinystr!(16, "ah")
            {
                return Err(DateError::UnknownEra(era));
            }
        }

        let month = if let Some((ordinal, false)) = month_code.parsed() {
            ordinal
        } else {
            return Err(DateError::UnknownMonthCode(month_code));
        };
        Ok(IslamicUmmAlQuraDateInner(
            ArithmeticDate::new_from_ordinals_with_info(
                year,
                month,
                day,
                self.precomputed_data().load_or_compute_info(year),
            )?,
        ))
    }

    fn date_from_iso(&self, iso: Date<Iso>) -> Self::DateInner {
        let fixed_iso = Iso::to_fixed(iso);

        let (year_info, y, m, d) = self
            .precomputed_data()
            .load_or_compute_info_for_iso(fixed_iso);
        IslamicUmmAlQuraDateInner(ArithmeticDate::new_unchecked_with_info(y, m, d, year_info))
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso> {
        let fixed =
            date.0
                .year_info
                .rd_for::<SaudiIslamicMarker>(date.0.year, date.0.month, date.0.day);
        Iso::from_fixed(fixed)
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

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        date.0.offset_date(offset, &self.precomputed_data())
    }

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

    fn debug_name(&self) -> &'static str {
        Self::DEBUG_NAME
    }

    fn year(&self, date: &Self::DateInner) -> types::YearInfo {
        year_as_islamic(tinystr!(16, "islamic-umalqura"), date.0.year)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::is_leap_year(date.0.year, date.0.year_info)
    }

    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        date.0.month()
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        date.0.day_of_month()
    }

    fn day_of_year_info(&self, date: &Self::DateInner) -> types::DayOfYearInfo {
        let prev_year = date.0.year.saturating_sub(1);
        let next_year = date.0.year.saturating_add(1);
        types::DayOfYearInfo {
            day_of_year: date.0.day_of_year(),
            days_in_year: date.0.days_in_year(),
            prev_year: year_as_islamic(tinystr!(16, "islamic-umalqura"), prev_year),
            days_in_prev_year: date.0.year_info.days_in_prev_year(),
            next_year: year_as_islamic(tinystr!(16, "islamic-umalqura"), next_year),
        }
    }

    fn any_calendar_kind(&self) -> Option<crate::AnyCalendarKind> {
        Some(crate::any_calendar::IntoAnyCalendar::kind(self))
    }
}

impl IslamicUmmAlQura {
    fn precomputed_data(&self) -> IslamicPrecomputedData<SaudiIslamicMarker> {
        IslamicPrecomputedData::new(self.data.as_ref().map(|x| x.get()))
    }

    pub(crate) const DEBUG_NAME: &'static str = "Islamic (Umm al-Qura)";
}

impl<A: AsCalendar<Calendar = IslamicUmmAlQura>> Date<A> {
    /// Construct new Islamic Umm al-Qura Date.
    ///
    /// Has no negative years, only era is the AH.
    ///
    /// ```rust
    /// use icu::calendar::cal::IslamicUmmAlQura;
    /// use icu::calendar::Date;
    ///
    /// let islamic = IslamicUmmAlQura::new_always_calculating();
    ///
    /// let date_islamic =
    ///     Date::try_new_ummalqura_with_calendar(1392, 4, 25, islamic)
    ///         .expect("Failed to initialize Islamic Date instance.");
    ///
    /// assert_eq!(date_islamic.year().era_year_or_extended(), 1392);
    /// assert_eq!(date_islamic.month().ordinal, 4);
    /// assert_eq!(date_islamic.day_of_month().0, 25);
    /// ```
    pub fn try_new_ummalqura_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, RangeError> {
        let year_info = calendar
            .as_calendar()
            .precomputed_data()
            .load_or_compute_info(year);
        Ok(Date::from_raw(
            IslamicUmmAlQuraDateInner(ArithmeticDate::new_from_ordinals_with_info(
                year, month, day, year_info,
            )?),
            calendar,
        ))
    }
}

/// The inner date type used for representing [`Date`]s of [`IslamicCivil`]. See [`Date`] and [`IslamicCivil`] for more details.

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct IslamicCivilDateInner(ArithmeticDate<IslamicCivil>);

impl CalendarArithmetic for IslamicCivil {
    type YearInfo = ();

    fn month_days(year: i32, month: u8, _data: ()) -> u8 {
        match month {
            1 | 3 | 5 | 7 | 9 | 11 => 30,
            2 | 4 | 6 | 8 | 10 => 29,
            12 if Self::is_leap_year(year, ()) => 30,
            12 => 29,
            _ => 0,
        }
    }

    fn months_for_every_year(_year: i32, _data: ()) -> u8 {
        12
    }

    fn days_in_provided_year(year: i32, _data: ()) -> u16 {
        if Self::is_leap_year(year, ()) {
            IslamicYearInfo::LONG_YEAR_LEN
        } else {
            IslamicYearInfo::SHORT_YEAR_LEN
        }
    }

    fn is_leap_year(year: i32, _data: ()) -> bool {
        (14 + 11 * year).rem_euclid(30) < 11
    }

    fn last_month_day_in_year(year: i32, _data: ()) -> (u8, u8) {
        if Self::is_leap_year(year, ()) {
            (12, 30)
        } else {
            (12, 29)
        }
    }
}

impl Calendar for IslamicCivil {
    type DateInner = IslamicCivilDateInner;

    fn date_from_codes(
        &self,
        era: Option<types::Era>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        if let Some(era) = era {
            if era.0 != tinystr!(16, "islamic-civil")
                && era.0 != tinystr!(16, "islamicc")
                && era.0 != tinystr!(16, "islamic")
                && era.0 != tinystr!(16, "ah")
            {
                return Err(DateError::UnknownEra(era));
            }
        }

        ArithmeticDate::new_from_codes(self, year, month_code, day).map(IslamicCivilDateInner)
    }

    fn date_from_iso(&self, iso: Date<Iso>) -> Self::DateInner {
        let fixed_iso = Iso::to_fixed(iso);
        Self::islamic_from_fixed(fixed_iso).inner
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso> {
        let fixed_islamic = Self::fixed_from_islamic(*date);
        Iso::from_fixed(fixed_islamic)
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
        Iso.day_of_week(self.date_to_iso(date).inner())
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        date.0.offset_date(offset, &())
    }

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

    fn debug_name(&self) -> &'static str {
        "Islamic (civil)"
    }

    fn year(&self, date: &Self::DateInner) -> types::YearInfo {
        year_as_islamic(tinystr!(16, "islamic-civil"), date.0.year)
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
        let prev_year = date.0.year.saturating_sub(1);
        let next_year = date.0.year.saturating_add(1);
        types::DayOfYearInfo {
            day_of_year: date.0.day_of_year(),
            days_in_year: date.0.days_in_year(),
            prev_year: year_as_islamic(tinystr!(16, "islamic-civil"), prev_year),
            days_in_prev_year: Self::days_in_provided_year(prev_year, ()),
            next_year: year_as_islamic(tinystr!(16, "islamic-civil"), next_year),
        }
    }

    fn any_calendar_kind(&self) -> Option<crate::AnyCalendarKind> {
        Some(crate::any_calendar::IntoAnyCalendar::kind(self))
    }
}

impl IslamicCivil {
    fn fixed_from_islamic(i_date: IslamicCivilDateInner) -> RataDie {
        calendrical_calculations::islamic::fixed_from_islamic_civil(
            i_date.0.year,
            i_date.0.month,
            i_date.0.day,
        )
    }

    fn islamic_from_fixed(date: RataDie) -> Date<IslamicCivil> {
        let (y, m, d) = calendrical_calculations::islamic::islamic_civil_from_fixed(date);

        debug_assert!(Date::try_new_islamic_civil_with_calendar(y, m, d, IslamicCivil).is_ok());
        Date::from_raw(
            IslamicCivilDateInner(ArithmeticDate::new_unchecked(y, m, d)),
            IslamicCivil,
        )
    }
}

impl<A: AsCalendar<Calendar = IslamicCivil>> Date<A> {
    /// Construct new Civil Islamic Date.
    ///
    /// Has no negative years, only era is the AH.
    ///
    /// ```rust
    /// use icu::calendar::cal::IslamicCivil;
    /// use icu::calendar::Date;
    ///
    /// let islamic = IslamicCivil::new();
    ///
    /// let date_islamic =
    ///     Date::try_new_islamic_civil_with_calendar(1392, 4, 25, islamic)
    ///         .expect("Failed to initialize Islamic Date instance.");
    ///
    /// assert_eq!(date_islamic.year().era_year_or_extended(), 1392);
    /// assert_eq!(date_islamic.month().ordinal, 4);
    /// assert_eq!(date_islamic.day_of_month().0, 25);
    /// ```
    pub fn try_new_islamic_civil_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, RangeError> {
        ArithmeticDate::new_from_ordinals(year, month, day)
            .map(IslamicCivilDateInner)
            .map(|inner| Date::from_raw(inner, calendar))
    }
}

/// The inner date type used for representing [`Date`]s of [`IslamicTabular`]. See [`Date`] and [`IslamicTabular`] for more details.

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct IslamicTabularDateInner(ArithmeticDate<IslamicTabular>);

impl CalendarArithmetic for IslamicTabular {
    type YearInfo = ();

    fn month_days(year: i32, month: u8, _data: ()) -> u8 {
        match month {
            1 | 3 | 5 | 7 | 9 | 11 => 30,
            2 | 4 | 6 | 8 | 10 => 29,
            12 if Self::is_leap_year(year, ()) => 30,
            12 => 29,
            _ => 0,
        }
    }

    fn months_for_every_year(_year: i32, _data: ()) -> u8 {
        12
    }

    fn days_in_provided_year(year: i32, _data: ()) -> u16 {
        if Self::is_leap_year(year, ()) {
            IslamicYearInfo::LONG_YEAR_LEN
        } else {
            IslamicYearInfo::SHORT_YEAR_LEN
        }
    }

    fn is_leap_year(year: i32, _data: ()) -> bool {
        (14 + 11 * year).rem_euclid(30) < 11
    }

    fn last_month_day_in_year(year: i32, _data: ()) -> (u8, u8) {
        if Self::is_leap_year(year, ()) {
            (12, 30)
        } else {
            (12, 29)
        }
    }
}

impl Calendar for IslamicTabular {
    type DateInner = IslamicTabularDateInner;

    fn date_from_codes(
        &self,
        era: Option<types::Era>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        if let Some(era) = era {
            if era.0 != tinystr!(16, "islamic-tbla")
                && era.0 != tinystr!(16, "islamic")
                && era.0 != tinystr!(16, "ah")
            {
                return Err(DateError::UnknownEra(era));
            }
        }

        ArithmeticDate::new_from_codes(self, year, month_code, day).map(IslamicTabularDateInner)
    }

    fn date_from_iso(&self, iso: Date<Iso>) -> Self::DateInner {
        let fixed_iso = Iso::to_fixed(iso);
        Self::islamic_from_fixed(fixed_iso).inner
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso> {
        let fixed_islamic = Self::fixed_from_islamic(*date);
        Iso::from_fixed(fixed_islamic)
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
        Iso.day_of_week(self.date_to_iso(date).inner())
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        date.0.offset_date(offset, &())
    }

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

    fn debug_name(&self) -> &'static str {
        "Islamic (tabular)"
    }

    fn year(&self, date: &Self::DateInner) -> types::YearInfo {
        year_as_islamic(tinystr!(16, "islamic-tbla"), date.0.year)
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
        let prev_year = date.0.year.saturating_sub(1);
        let next_year = date.0.year.saturating_add(1);
        types::DayOfYearInfo {
            day_of_year: date.0.day_of_year(),
            days_in_year: date.0.days_in_year(),
            prev_year: year_as_islamic(tinystr!(16, "islamic-tbla"), prev_year),
            days_in_prev_year: Self::days_in_provided_year(prev_year, ()),
            next_year: year_as_islamic(tinystr!(16, "islamic-tbla"), next_year),
        }
    }

    fn any_calendar_kind(&self) -> Option<crate::AnyCalendarKind> {
        Some(crate::any_calendar::IntoAnyCalendar::kind(self))
    }
}

impl IslamicTabular {
    fn fixed_from_islamic(i_date: IslamicTabularDateInner) -> RataDie {
        calendrical_calculations::islamic::fixed_from_islamic_tabular(
            i_date.0.year,
            i_date.0.month,
            i_date.0.day,
        )
    }

    fn islamic_from_fixed(date: RataDie) -> Date<IslamicTabular> {
        let (y, m, d) = calendrical_calculations::islamic::islamic_tabular_from_fixed(date);

        debug_assert!(Date::try_new_islamic_civil_with_calendar(y, m, d, IslamicCivil).is_ok());
        Date::from_raw(
            IslamicTabularDateInner(ArithmeticDate::new_unchecked(y, m, d)),
            IslamicTabular,
        )
    }
}

impl<A: AsCalendar<Calendar = IslamicTabular>> Date<A> {
    /// Construct new Tabular Islamic Date.
    ///
    /// Has no negative years, only era is the AH.
    ///
    /// ```rust
    /// use icu::calendar::cal::IslamicTabular;
    /// use icu::calendar::Date;
    ///
    /// let islamic = IslamicTabular::new();
    ///
    /// let date_islamic =
    ///     Date::try_new_islamic_tabular_with_calendar(1392, 4, 25, islamic)
    ///         .expect("Failed to initialize Islamic Date instance.");
    ///
    /// assert_eq!(date_islamic.year().era_year_or_extended(), 1392);
    /// assert_eq!(date_islamic.month().ordinal, 4);
    /// assert_eq!(date_islamic.day_of_month().0, 25);
    /// ```
    pub fn try_new_islamic_tabular_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, RangeError> {
        ArithmeticDate::new_from_ordinals(year, month, day)
            .map(IslamicTabularDateInner)
            .map(|inner| Date::from_raw(inner, calendar))
    }
}

#[cfg(test)]
mod test {
    use types::{Era, MonthCode};

    use super::*;
    use crate::Ref;

    const START_YEAR: i32 = -1245;
    const END_YEAR: i32 = 1518;

    #[derive(Debug)]
    struct DateCase {
        year: i32,
        month: u8,
        day: u8,
    }

    static TEST_FIXED_DATE: [i64; 33] = [
        -214193, -61387, 25469, 49217, 171307, 210155, 253427, 369740, 400085, 434355, 452605,
        470160, 473837, 507850, 524156, 544676, 567118, 569477, 601716, 613424, 626596, 645554,
        664224, 671401, 694799, 704424, 708842, 709409, 709580, 727274, 728714, 744313, 764652,
    ];
    // Removed: 601716 and 727274 fixed dates
    static TEST_FIXED_DATE_UMMALQURA: [i64; 31] = [
        -214193, -61387, 25469, 49217, 171307, 210155, 253427, 369740, 400085, 434355, 452605,
        470160, 473837, 507850, 524156, 544676, 567118, 569477, 613424, 626596, 645554, 664224,
        671401, 694799, 704424, 708842, 709409, 709580, 728714, 744313, 764652,
    ];

    static UMMALQURA_DATE_EXPECTED: [DateCase; 31] = [
        DateCase {
            year: -1245,
            month: 12,
            day: 11,
        },
        DateCase {
            year: -813,
            month: 2,
            day: 26,
        },
        DateCase {
            year: -568,
            month: 4,
            day: 3,
        },
        DateCase {
            year: -501,
            month: 4,
            day: 8,
        },
        DateCase {
            year: -157,
            month: 10,
            day: 18,
        },
        DateCase {
            year: -47,
            month: 6,
            day: 4,
        },
        DateCase {
            year: 75,
            month: 7,
            day: 14,
        },
        DateCase {
            year: 403,
            month: 10,
            day: 6,
        },
        DateCase {
            year: 489,
            month: 5,
            day: 23,
        },
        DateCase {
            year: 586,
            month: 2,
            day: 8,
        },
        DateCase {
            year: 637,
            month: 8,
            day: 8,
        },
        DateCase {
            year: 687,
            month: 2,
            day: 22,
        },
        DateCase {
            year: 697,
            month: 7,
            day: 8,
        },
        DateCase {
            year: 793,
            month: 7,
            day: 1,
        },
        DateCase {
            year: 839,
            month: 7,
            day: 7,
        },
        DateCase {
            year: 897,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 960,
            month: 10,
            day: 1,
        },
        DateCase {
            year: 967,
            month: 5,
            day: 28,
        },
        DateCase {
            year: 1091,
            month: 6,
            day: 4,
        },
        DateCase {
            year: 1128,
            month: 8,
            day: 5,
        },
        DateCase {
            year: 1182,
            month: 2,
            day: 4,
        },
        DateCase {
            year: 1234,
            month: 10,
            day: 11,
        },
        DateCase {
            year: 1255,
            month: 1,
            day: 11,
        },
        DateCase {
            year: 1321,
            month: 1,
            day: 21,
        },
        DateCase {
            year: 1348,
            month: 3,
            day: 20,
        },
        DateCase {
            year: 1360,
            month: 9,
            day: 8,
        },
        DateCase {
            year: 1362,
            month: 4,
            day: 14,
        },
        DateCase {
            year: 1362,
            month: 10,
            day: 8,
        },
        DateCase {
            year: 1416,
            month: 10,
            day: 6,
        },
        DateCase {
            year: 1460,
            month: 10,
            day: 13,
        },
        DateCase {
            year: 1518,
            month: 3,
            day: 6,
        },
    ];

    static OBSERVATIONAL_CASES: [DateCase; 33] = [
        DateCase {
            year: -1245,
            month: 12,
            day: 11,
        },
        DateCase {
            year: -813,
            month: 2,
            day: 25,
        },
        DateCase {
            year: -568,
            month: 4,
            day: 2,
        },
        DateCase {
            year: -501,
            month: 4,
            day: 7,
        },
        DateCase {
            year: -157,
            month: 10,
            day: 18,
        },
        DateCase {
            year: -47,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 75,
            month: 7,
            day: 13,
        },
        DateCase {
            year: 403,
            month: 10,
            day: 5,
        },
        DateCase {
            year: 489,
            month: 5,
            day: 22,
        },
        DateCase {
            year: 586,
            month: 2,
            day: 7,
        },
        DateCase {
            year: 637,
            month: 8,
            day: 7,
        },
        DateCase {
            year: 687,
            month: 2,
            day: 21,
        },
        DateCase {
            year: 697,
            month: 7,
            day: 7,
        },
        DateCase {
            year: 793,
            month: 6,
            day: 30,
        },
        DateCase {
            year: 839,
            month: 7,
            day: 6,
        },
        DateCase {
            year: 897,
            month: 6,
            day: 2,
        },
        DateCase {
            year: 960,
            month: 9,
            day: 30,
        },
        DateCase {
            year: 967,
            month: 5,
            day: 27,
        },
        DateCase {
            year: 1058,
            month: 5,
            day: 18,
        },
        DateCase {
            year: 1091,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 1128,
            month: 8,
            day: 4,
        },
        DateCase {
            year: 1182,
            month: 2,
            day: 4,
        },
        DateCase {
            year: 1234,
            month: 10,
            day: 10,
        },
        DateCase {
            year: 1255,
            month: 1,
            day: 11,
        },
        DateCase {
            year: 1321,
            month: 1,
            day: 20,
        },
        DateCase {
            year: 1348,
            month: 3,
            day: 19,
        },
        DateCase {
            year: 1360,
            month: 9,
            day: 7,
        },
        DateCase {
            year: 1362,
            month: 4,
            day: 14,
        },
        DateCase {
            year: 1362,
            month: 10,
            day: 7,
        },
        DateCase {
            year: 1412,
            month: 9,
            day: 12,
        },
        DateCase {
            year: 1416,
            month: 10,
            day: 5,
        },
        DateCase {
            year: 1460,
            month: 10,
            day: 12,
        },
        DateCase {
            year: 1518,
            month: 3,
            day: 5,
        },
    ];

    static ARITHMETIC_CASES: [DateCase; 33] = [
        DateCase {
            year: -1245,
            month: 12,
            day: 9,
        },
        DateCase {
            year: -813,
            month: 2,
            day: 23,
        },
        DateCase {
            year: -568,
            month: 4,
            day: 1,
        },
        DateCase {
            year: -501,
            month: 4,
            day: 6,
        },
        DateCase {
            year: -157,
            month: 10,
            day: 17,
        },
        DateCase {
            year: -47,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 75,
            month: 7,
            day: 13,
        },
        DateCase {
            year: 403,
            month: 10,
            day: 5,
        },
        DateCase {
            year: 489,
            month: 5,
            day: 22,
        },
        DateCase {
            year: 586,
            month: 2,
            day: 7,
        },
        DateCase {
            year: 637,
            month: 8,
            day: 7,
        },
        DateCase {
            year: 687,
            month: 2,
            day: 20,
        },
        DateCase {
            year: 697,
            month: 7,
            day: 7,
        },
        DateCase {
            year: 793,
            month: 7,
            day: 1,
        },
        DateCase {
            year: 839,
            month: 7,
            day: 6,
        },
        DateCase {
            year: 897,
            month: 6,
            day: 1,
        },
        DateCase {
            year: 960,
            month: 9,
            day: 30,
        },
        DateCase {
            year: 967,
            month: 5,
            day: 27,
        },
        DateCase {
            year: 1058,
            month: 5,
            day: 18,
        },
        DateCase {
            year: 1091,
            month: 6,
            day: 2,
        },
        DateCase {
            year: 1128,
            month: 8,
            day: 4,
        },
        DateCase {
            year: 1182,
            month: 2,
            day: 3,
        },
        DateCase {
            year: 1234,
            month: 10,
            day: 10,
        },
        DateCase {
            year: 1255,
            month: 1,
            day: 11,
        },
        DateCase {
            year: 1321,
            month: 1,
            day: 21,
        },
        DateCase {
            year: 1348,
            month: 3,
            day: 19,
        },
        DateCase {
            year: 1360,
            month: 9,
            day: 8,
        },
        DateCase {
            year: 1362,
            month: 4,
            day: 13,
        },
        DateCase {
            year: 1362,
            month: 10,
            day: 7,
        },
        DateCase {
            year: 1412,
            month: 9,
            day: 13,
        },
        DateCase {
            year: 1416,
            month: 10,
            day: 5,
        },
        DateCase {
            year: 1460,
            month: 10,
            day: 12,
        },
        DateCase {
            year: 1518,
            month: 3,
            day: 5,
        },
    ];

    static TABULAR_CASES: [DateCase; 33] = [
        DateCase {
            year: -1245,
            month: 12,
            day: 10,
        },
        DateCase {
            year: -813,
            month: 2,
            day: 24,
        },
        DateCase {
            year: -568,
            month: 4,
            day: 2,
        },
        DateCase {
            year: -501,
            month: 4,
            day: 7,
        },
        DateCase {
            year: -157,
            month: 10,
            day: 18,
        },
        DateCase {
            year: -47,
            month: 6,
            day: 4,
        },
        DateCase {
            year: 75,
            month: 7,
            day: 14,
        },
        DateCase {
            year: 403,
            month: 10,
            day: 6,
        },
        DateCase {
            year: 489,
            month: 5,
            day: 23,
        },
        DateCase {
            year: 586,
            month: 2,
            day: 8,
        },
        DateCase {
            year: 637,
            month: 8,
            day: 8,
        },
        DateCase {
            year: 687,
            month: 2,
            day: 21,
        },
        DateCase {
            year: 697,
            month: 7,
            day: 8,
        },
        DateCase {
            year: 793,
            month: 7,
            day: 2,
        },
        DateCase {
            year: 839,
            month: 7,
            day: 7,
        },
        DateCase {
            year: 897,
            month: 6,
            day: 2,
        },
        DateCase {
            year: 960,
            month: 10,
            day: 1,
        },
        DateCase {
            year: 967,
            month: 5,
            day: 28,
        },
        DateCase {
            year: 1058,
            month: 5,
            day: 19,
        },
        DateCase {
            year: 1091,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 1128,
            month: 8,
            day: 5,
        },
        DateCase {
            year: 1182,
            month: 2,
            day: 4,
        },
        DateCase {
            year: 1234,
            month: 10,
            day: 11,
        },
        DateCase {
            year: 1255,
            month: 1,
            day: 12,
        },
        DateCase {
            year: 1321,
            month: 1,
            day: 22,
        },
        DateCase {
            year: 1348,
            month: 3,
            day: 20,
        },
        DateCase {
            year: 1360,
            month: 9,
            day: 9,
        },
        DateCase {
            year: 1362,
            month: 4,
            day: 14,
        },
        DateCase {
            year: 1362,
            month: 10,
            day: 8,
        },
        DateCase {
            year: 1412,
            month: 9,
            day: 14,
        },
        DateCase {
            year: 1416,
            month: 10,
            day: 6,
        },
        DateCase {
            year: 1460,
            month: 10,
            day: 13,
        },
        DateCase {
            year: 1518,
            month: 3,
            day: 6,
        },
    ];

    #[test]
    fn test_observational_islamic_from_fixed() {
        let calendar = IslamicObservational::new();
        let calendar = Ref(&calendar);
        for (case, f_date) in OBSERVATIONAL_CASES.iter().zip(TEST_FIXED_DATE.iter()) {
            let date = Date::try_new_observational_islamic_with_calendar(
                case.year, case.month, case.day, calendar,
            )
            .unwrap();
            let iso = Iso::from_fixed(RataDie::new(*f_date));

            assert_eq!(iso.to_calendar(calendar).inner, date.inner, "{case:?}");
        }
    }

    #[test]
    fn test_fixed_from_observational_islamic() {
        let calendar = IslamicObservational::new();
        let calendar = Ref(&calendar);
        for (case, f_date) in OBSERVATIONAL_CASES.iter().zip(TEST_FIXED_DATE.iter()) {
            let date = Date::try_new_observational_islamic_with_calendar(
                case.year, case.month, case.day, calendar,
            )
            .unwrap();
            assert_eq!(
                Iso::to_fixed(date.to_iso()),
                RataDie::new(*f_date),
                "{case:?}"
            );
        }
    }

    #[test]
    fn test_fixed_from_islamic() {
        let calendar = IslamicCivil::new();
        let calendar = Ref(&calendar);
        for (case, f_date) in ARITHMETIC_CASES.iter().zip(TEST_FIXED_DATE.iter()) {
            let date = Date::try_new_islamic_civil_with_calendar(
                case.year, case.month, case.day, calendar,
            )
            .unwrap();
            assert_eq!(
                Iso::to_fixed(date.to_iso()),
                RataDie::new(*f_date),
                "{case:?}"
            );
        }
    }

    #[test]
    fn test_islamic_from_fixed() {
        let calendar = IslamicCivil::new();
        let calendar = Ref(&calendar);
        for (case, f_date) in ARITHMETIC_CASES.iter().zip(TEST_FIXED_DATE.iter()) {
            let date = Date::try_new_islamic_civil_with_calendar(
                case.year, case.month, case.day, calendar,
            )
            .unwrap();
            let iso = Iso::from_fixed(RataDie::new(*f_date));

            assert_eq!(iso.to_calendar(calendar).inner, date.inner, "{case:?}");
        }
    }

    #[test]
    fn test_fixed_from_islamic_tbla() {
        let calendar = IslamicTabular::new();
        let calendar = Ref(&calendar);
        for (case, f_date) in TABULAR_CASES.iter().zip(TEST_FIXED_DATE.iter()) {
            let date = Date::try_new_islamic_tabular_with_calendar(
                case.year, case.month, case.day, calendar,
            )
            .unwrap();
            assert_eq!(
                Iso::to_fixed(date.to_iso()),
                RataDie::new(*f_date),
                "{case:?}"
            );
        }
    }

    #[test]
    fn test_islamic_tbla_from_fixed() {
        let calendar = IslamicTabular::new();
        let calendar = Ref(&calendar);
        for (case, f_date) in TABULAR_CASES.iter().zip(TEST_FIXED_DATE.iter()) {
            let date = Date::try_new_islamic_tabular_with_calendar(
                case.year, case.month, case.day, calendar,
            )
            .unwrap();
            let iso = Iso::from_fixed(RataDie::new(*f_date));

            assert_eq!(iso.to_calendar(calendar).inner, date.inner, "{case:?}");
        }
    }

    #[test]
    fn test_saudi_islamic_from_fixed() {
        let calendar = IslamicUmmAlQura::new();
        let calendar = Ref(&calendar);
        for (case, f_date) in UMMALQURA_DATE_EXPECTED
            .iter()
            .zip(TEST_FIXED_DATE_UMMALQURA.iter())
        {
            let date =
                Date::try_new_ummalqura_with_calendar(case.year, case.month, case.day, calendar)
                    .unwrap();
            let iso = Iso::from_fixed(RataDie::new(*f_date));

            assert_eq!(iso.to_calendar(calendar).inner, date.inner, "{case:?}");
        }
    }

    #[test]
    fn test_fixed_from_saudi_islamic() {
        let calendar = IslamicUmmAlQura::new();
        let calendar = Ref(&calendar);
        for (case, f_date) in UMMALQURA_DATE_EXPECTED
            .iter()
            .zip(TEST_FIXED_DATE_UMMALQURA.iter())
        {
            let date =
                Date::try_new_ummalqura_with_calendar(case.year, case.month, case.day, calendar)
                    .unwrap();
            assert_eq!(
                Iso::to_fixed(date.to_iso()),
                RataDie::new(*f_date),
                "{case:?}"
            );
        }
    }

    #[ignore] // slow
    #[test]
    fn test_days_in_provided_year_observational() {
        let calendar = IslamicObservational::new();
        let calendar = Ref(&calendar);
        // -1245 1 1 = -214526 (R.D Date)
        // 1518 1 1 = 764589 (R.D Date)
        let sum_days_in_year: i64 = (START_YEAR..END_YEAR)
            .map(|year| {
                IslamicObservational::days_in_provided_year(
                    year,
                    IslamicYearInfo::compute::<ObservationalIslamicMarker>(year),
                ) as i64
            })
            .sum();
        let expected_number_of_days = (Iso::to_fixed(
            (Date::try_new_observational_islamic_with_calendar(END_YEAR, 1, 1, calendar).unwrap())
                .to_iso(),
        )) - Iso::to_fixed(
            (Date::try_new_observational_islamic_with_calendar(START_YEAR, 1, 1, calendar)
                .unwrap())
            .to_iso(),
        ); // The number of days between Islamic years -1245 and 1518
        let tolerance = 1; // One day tolerance (See Astronomical::month_length for more context)

        assert!(
            (sum_days_in_year - expected_number_of_days).abs() <= tolerance,
            "Difference between sum_days_in_year and expected_number_of_days is more than the tolerance"
        );
    }

    #[ignore] // slow
    #[test]
    fn test_days_in_provided_year_ummalqura() {
        let calendar = IslamicUmmAlQura::new();
        let calendar = Ref(&calendar);
        // -1245 1 1 = -214528 (R.D Date)
        // 1518 1 1 = 764588 (R.D Date)
        let sum_days_in_year: i64 = (START_YEAR..END_YEAR)
            .map(|year| {
                IslamicUmmAlQura::days_in_provided_year(
                    year,
                    IslamicYearInfo::compute::<SaudiIslamicMarker>(year),
                ) as i64
            })
            .sum();
        let expected_number_of_days = (Iso::to_fixed(
            (Date::try_new_ummalqura_with_calendar(END_YEAR, 1, 1, calendar).unwrap()).to_iso(),
        )) - Iso::to_fixed(
            (Date::try_new_ummalqura_with_calendar(START_YEAR, 1, 1, calendar).unwrap()).to_iso(),
        ); // The number of days between Umm al-Qura Islamic years -1245 and 1518

        assert_eq!(sum_days_in_year, expected_number_of_days);
    }

    #[test]
    fn test_regression_3868() {
        // This date used to panic on creation
        let iso = Date::try_new_iso(2011, 4, 4).unwrap();
        let islamic = iso.to_calendar(IslamicUmmAlQura::new());
        // Data from https://www.ummulqura.org.sa/Index.aspx
        assert_eq!(islamic.day_of_month().0, 30);
        assert_eq!(islamic.month().ordinal, 4);
        assert_eq!(islamic.year().era_year_or_extended(), 1432);
    }

    #[test]
    fn test_regression_4914() {
        // https://github.com/unicode-org/icu4x/issues/4914
        let cal = IslamicUmmAlQura::new_always_calculating();
        let era = Era(tinystr!(16, "islamic"));
        let year = -6823;
        let month_code = MonthCode(tinystr!(4, "M01"));
        let dt = cal.date_from_codes(Some(era), year, month_code, 1).unwrap();
        assert_eq!(dt.0.day, 1);
        assert_eq!(dt.0.month, 1);
        assert_eq!(dt.0.year, -6823);
    }
}
