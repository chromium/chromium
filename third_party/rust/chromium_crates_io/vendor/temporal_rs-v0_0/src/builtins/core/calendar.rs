//! This module implements the calendar traits and related components.
//!
//! The goal of the calendar module of `boa_temporal` is to provide
//! Temporal compatible calendar implementations.

use crate::{
    builtins::core::{
        duration::{DateDuration, TimeDuration},
        Duration, PlainDate, PlainDateTime, PlainMonthDay, PlainYearMonth,
    },
    iso::IsoDate,
    options::{ArithmeticOverflow, Unit},
    parsers::parse_allowed_calendar_formats,
    TemporalError, TemporalResult,
};
use alloc::string::ToString;
use core::str::FromStr;

use icu_calendar::{
    cal::{
        Buddhist, Chinese, Coptic, Dangi, Ethiopian, EthiopianEraStyle, Hebrew, HijriSimulated,
        HijriTabular, HijriUmmAlQura, Indian, Japanese, JapaneseExtended, Persian, Roc,
    },
    AnyCalendar, AnyCalendarKind, Calendar as IcuCalendar, Iso, Ref,
};
use icu_calendar::{
    cal::{HijriTabularEpoch, HijriTabularLeapYears},
    preferences::CalendarAlgorithm,
    types::MonthCode as IcuMonthCode,
    Gregorian,
};
use icu_locale::extensions::unicode::Value;
use tinystr::{tinystr, TinyAsciiStr};

use super::{PartialDate, ZonedDateTime};

mod era;
mod types;

pub(crate) use types::{month_to_month_code, ResolutionType};
pub use types::{MonthCode, ResolvedCalendarFields};

use era::EraInfo;

/// The core `Calendar` type for `temporal_rs`
///
/// A `Calendar` in `temporal_rs` can be any calendar that is currently
/// supported by [`icu_calendar`].
#[derive(Debug, Clone)]
pub struct Calendar(Ref<'static, AnyCalendar>);

impl Default for Calendar {
    fn default() -> Self {
        Self::ISO
    }
}

impl PartialEq for Calendar {
    fn eq(&self, other: &Self) -> bool {
        self.identifier() == other.identifier()
    }
}

impl Eq for Calendar {}

impl Calendar {
    /// The ISO 8601 calendar
    pub const ISO: Self = Self::new(AnyCalendarKind::Iso);

    /// Create a `Calendar` from an ICU [`AnyCalendarKind`].
    #[warn(clippy::wildcard_enum_match_arm)] // Warns if the calendar kind gets out of sync.
    pub const fn new(kind: AnyCalendarKind) -> Self {
        let cal = match kind {
            AnyCalendarKind::Buddhist => &AnyCalendar::Buddhist(Buddhist),
            AnyCalendarKind::Chinese => const { &AnyCalendar::Chinese(Chinese::new()) },
            AnyCalendarKind::Coptic => &AnyCalendar::Coptic(Coptic),
            AnyCalendarKind::Dangi => const { &AnyCalendar::Dangi(Dangi::new()) },
            AnyCalendarKind::Ethiopian => {
                const {
                    &AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(
                        EthiopianEraStyle::AmeteMihret,
                    ))
                }
            }
            AnyCalendarKind::EthiopianAmeteAlem => {
                const {
                    &AnyCalendar::Ethiopian(Ethiopian::new_with_era_style(
                        EthiopianEraStyle::AmeteAlem,
                    ))
                }
            }
            AnyCalendarKind::Gregorian => &AnyCalendar::Gregorian(Gregorian),
            AnyCalendarKind::Hebrew => &AnyCalendar::Hebrew(Hebrew),
            AnyCalendarKind::Indian => &AnyCalendar::Indian(Indian),
            AnyCalendarKind::HijriTabularTypeIIFriday => {
                const {
                    &AnyCalendar::HijriTabular(HijriTabular::new(
                        HijriTabularLeapYears::TypeII,
                        HijriTabularEpoch::Friday,
                    ))
                }
            }
            AnyCalendarKind::HijriSimulatedMecca => {
                const { &AnyCalendar::HijriSimulated(HijriSimulated::new_mecca()) }
            }
            AnyCalendarKind::HijriTabularTypeIIThursday => {
                const {
                    &AnyCalendar::HijriTabular(HijriTabular::new(
                        HijriTabularLeapYears::TypeII,
                        HijriTabularEpoch::Thursday,
                    ))
                }
            }
            AnyCalendarKind::HijriUmmAlQura => {
                const { &AnyCalendar::HijriUmmAlQura(HijriUmmAlQura::new()) }
            }
            AnyCalendarKind::Iso => &AnyCalendar::Iso(Iso),
            AnyCalendarKind::Japanese => const { &AnyCalendar::Japanese(Japanese::new()) },
            AnyCalendarKind::JapaneseExtended => {
                const { &AnyCalendar::JapaneseExtended(JapaneseExtended::new()) }
            }
            AnyCalendarKind::Persian => &AnyCalendar::Persian(Persian),
            AnyCalendarKind::Roc => &AnyCalendar::Roc(Roc),
            // NOTE: `unreachable!` is not const, but panic is.
            _ => panic!("Unreachable: match must handle all variants of `AnyCalendarKind`"),
        };

        Self(Ref(cal))
    }

    /// Create a `Calendar` from an ICU [`CalendarAlgorithm`].
    pub fn try_from_calendar_algorithm(algorithm: CalendarAlgorithm) -> TemporalResult<Self> {
        let calendar_kind = match AnyCalendarKind::try_from(algorithm) {
            Ok(c) => c,
            // Handle `islamic` calendar idenitifier.
            //
            // This should be updated depending on `icu_calendar` support and
            // intl-era-monthcode.
            Err(()) if algorithm == CalendarAlgorithm::Hijri(None) => {
                AnyCalendarKind::HijriTabularTypeIIFriday
            }
            Err(()) => return Err(TemporalError::range().with_message("unknown calendar")),
        };
        Ok(Calendar::new(calendar_kind))
    }

    /// Returns a `Calendar` from the a slice of UTF-8 encoded bytes.
    pub fn try_from_utf8(bytes: &[u8]) -> TemporalResult<Self> {
        // TODO: Determine the best way to handle "julian" here.
        // Not supported by `CalendarAlgorithm`
        let icu_locale_value = Value::try_from_utf8(&bytes.to_ascii_lowercase())
            .map_err(|e| TemporalError::range().with_message(e.to_string()))?;
        let algorithm = CalendarAlgorithm::try_from(&icu_locale_value)
            .map_err(|e| TemporalError::range().with_message(e.to_string()))?;
        Self::try_from_calendar_algorithm(algorithm)
    }
}

impl FromStr for Calendar {
    type Err = TemporalError;

    // 13.34 ParseTemporalCalendarString ( string )
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match parse_allowed_calendar_formats(s) {
            Some([]) => Ok(Calendar::ISO),
            Some(result) => Calendar::try_from_utf8(result),
            None => Calendar::try_from_utf8(s.as_bytes()),
        }
    }
}

// ==== Public `CalendarSlot` methods ====

impl Calendar {
    /// Returns whether the current calendar is `ISO`
    #[inline]
    pub fn is_iso(&self) -> bool {
        matches!(self.0 .0, AnyCalendar::Iso(_))
    }

    /// `CalendarDateFromFields`
    pub fn date_from_partial(
        &self,
        partial: &PartialDate,
        overflow: ArithmeticOverflow,
    ) -> TemporalResult<PlainDate> {
        let resolved_fields =
            ResolvedCalendarFields::try_from_partial(partial, overflow, ResolutionType::Date)?;

        if self.is_iso() {
            // Resolve month and monthCode;
            return PlainDate::new_with_overflow(
                resolved_fields.era_year.year,
                resolved_fields.month_code.to_month_integer(),
                resolved_fields.day,
                self.clone(),
                overflow,
            );
        }

        let calendar_date = self
            .0
            .from_codes(
                Some(resolved_fields.era_year.era.0.as_str()),
                resolved_fields.era_year.year,
                IcuMonthCode(resolved_fields.month_code.0),
                resolved_fields.day,
            )
            .map_err(TemporalError::from_icu4x)?;
        let iso = self.0.to_iso(&calendar_date);
        PlainDate::new_with_overflow(
            Iso.extended_year(&iso),
            Iso.month(&iso).ordinal,
            Iso.day_of_month(&iso).0,
            self.clone(),
            overflow,
        )
    }

    /// `CalendarPlainMonthDayFromFields`
    pub fn month_day_from_partial(
        &self,
        partial: &PartialDate,
        overflow: ArithmeticOverflow,
    ) -> TemporalResult<PlainMonthDay> {
        let resolved_fields =
            ResolvedCalendarFields::try_from_partial(partial, overflow, ResolutionType::MonthDay)?;
        if self.is_iso() {
            return PlainMonthDay::new_with_overflow(
                resolved_fields.month_code.to_month_integer(),
                resolved_fields.day,
                self.clone(),
                overflow,
                None,
            );
        }

        // TODO: This may get complicated...
        // For reference: https://github.com/tc39/proposal-temporal/blob/main/polyfill/lib/calendar.mjs#L1275.
        Err(TemporalError::range().with_message("Not yet implemented/supported."))
    }

    /// `CalendarPlainYearMonthFromFields`
    pub fn year_month_from_partial(
        &self,
        partial: &PartialDate,
        overflow: ArithmeticOverflow,
    ) -> TemporalResult<PlainYearMonth> {
        let resolved_fields =
            ResolvedCalendarFields::try_from_partial(partial, overflow, ResolutionType::YearMonth)?;
        if self.is_iso() {
            return PlainYearMonth::new_with_overflow(
                resolved_fields.era_year.year,
                resolved_fields.month_code.to_month_integer(),
                Some(resolved_fields.day),
                self.clone(),
                overflow,
            );
        }

        // NOTE: This might preemptively throw as `ICU4X` does not support regulating.
        let calendar_date = self
            .0
            .from_codes(
                Some(resolved_fields.era_year.era.0.as_str()),
                resolved_fields.era_year.year,
                IcuMonthCode(resolved_fields.month_code.0),
                resolved_fields.day,
            )
            .map_err(TemporalError::from_icu4x)?;
        let iso = self.0.to_iso(&calendar_date);
        PlainYearMonth::new_with_overflow(
            Iso.year_info(&iso).year,
            Iso.month(&iso).ordinal,
            Some(Iso.days_in_month(&iso)),
            self.clone(),
            overflow,
        )
    }

    /// `CalendarDateAdd`
    pub fn date_add(
        &self,
        date: &IsoDate,
        duration: &Duration,
        overflow: ArithmeticOverflow,
    ) -> TemporalResult<PlainDate> {
        if self.is_iso() {
            // 8. Let norm be NormalizeTimeDuration(duration.[[Hours]], duration.[[Minutes]], duration.[[Seconds]],
            // duration.[[Milliseconds]], duration.[[Microseconds]], duration.[[Nanoseconds]]).
            // 9. Let balanceResult be BalanceTimeDuration(norm, "day").
            let (balance_days, _) =
                TimeDuration::from_normalized(duration.time().to_normalized(), Unit::Day)?;

            // 10. Let result be ? AddISODate(date.[[ISOYear]], date.[[ISOMonth]], date.[[ISODay]], duration.[[Years]],
            // duration.[[Months]], duration.[[Weeks]], duration.[[Days]] + balanceResult.[[Days]], overflow).
            let result = date.add_date_duration(
                &DateDuration::new_unchecked(
                    duration.years(),
                    duration.months(),
                    duration.weeks(),
                    duration
                        .days()
                        .checked_add(balance_days)
                        .ok_or(TemporalError::range())?,
                ),
                overflow,
            )?;
            // 11. Return ? CreateTemporalDate(result.[[Year]], result.[[Month]], result.[[Day]], "iso8601").
            return PlainDate::try_new(result.year, result.month, result.day, self.clone());
        }

        Err(TemporalError::range().with_message("Not yet implemented."))
    }

    /// `CalendarDateUntil`
    pub fn date_until(
        &self,
        one: &IsoDate,
        two: &IsoDate,
        largest_unit: Unit,
    ) -> TemporalResult<Duration> {
        if self.is_iso() {
            let date_duration = one.diff_iso_date(two, largest_unit)?;
            return Ok(Duration::from(date_duration));
        }
        Err(TemporalError::range().with_message("Not yet implemented."))
    }

    /// `CalendarEra`
    pub fn era(&self, iso_date: &IsoDate) -> Option<TinyAsciiStr<16>> {
        if self.is_iso() {
            return None;
        }
        let calendar_date = self.0.from_iso(*iso_date.to_icu4x().inner());
        self.0
            .year_info(&calendar_date)
            .era()
            .map(|era_info| era_info.era)
    }

    /// `CalendarEraYear`
    pub fn era_year(&self, iso_date: &IsoDate) -> Option<i32> {
        if self.is_iso() {
            return None;
        }
        let calendar_date = self.0.from_iso(*iso_date.to_icu4x().inner());
        self.0
            .year_info(&calendar_date)
            .era()
            .map(|era_info| era_info.year)
    }

    /// `CalendarYear`
    pub fn year(&self, iso_date: &IsoDate) -> i32 {
        if self.is_iso() {
            return iso_date.year;
        }
        let calendar_date = self.0.from_iso(*iso_date.to_icu4x().inner());
        self.0.year_info(&calendar_date).era_year_or_related_iso()
    }

    /// `CalendarMonth`
    pub fn month(&self, iso_date: &IsoDate) -> u8 {
        if self.is_iso() {
            return iso_date.month;
        }
        let calendar_date = self.0.from_iso(*iso_date.to_icu4x().inner());
        self.0.month(&calendar_date).month_number()
    }

    /// `CalendarMonthCode`
    pub fn month_code(&self, iso_date: &IsoDate) -> MonthCode {
        if self.is_iso() {
            let mc = iso_date.to_icu4x().month().standard_code.0;
            return MonthCode(mc);
        }
        let calendar_date = self.0.from_iso(*iso_date.to_icu4x().inner());
        MonthCode(self.0.month(&calendar_date).standard_code.0)
    }

    /// `CalendarDay`
    pub fn day(&self, iso_date: &IsoDate) -> u8 {
        if self.is_iso() {
            return iso_date.day;
        }
        let calendar_date = self.0.from_iso(*iso_date.to_icu4x().inner());
        self.0.day_of_month(&calendar_date).0
    }

    /// `CalendarDayOfWeek`
    pub fn day_of_week(&self, iso_date: &IsoDate) -> TemporalResult<u16> {
        if self.is_iso() {
            return Ok(iso_date.to_icu4x().day_of_week() as u16);
        }
        // TODO: Update or update in icu_calendar
        Err(TemporalError::range().with_message("dayOfWeek is not for the provided calendar."))
    }

    /// `CalendarDayOfYear`
    pub fn day_of_year(&self, iso_date: &IsoDate) -> u16 {
        if self.is_iso() {
            return iso_date.to_icu4x().day_of_year().0;
        }
        let calendar_date = self.0.from_iso(*iso_date.to_icu4x().inner());
        self.0.day_of_year(&calendar_date).0
    }

    /// `CalendarWeekOfYear`
    pub fn week_of_year(&self, iso_date: &IsoDate) -> Option<u8> {
        if self.is_iso() {
            return Some(iso_date.to_icu4x().week_of_year().week_number);
        }
        // TODO: Research in ICU4X and determine best approach.
        None
    }

    /// `CalendarYearOfWeek`
    pub fn year_of_week(&self, iso_date: &IsoDate) -> Option<i32> {
        if self.is_iso() {
            return Some(iso_date.to_icu4x().week_of_year().iso_year);
        }
        // TODO: Research in ICU4X and determine best approach.
        None
    }

    /// `CalendarDaysInWeek`
    pub fn days_in_week(&self, _iso_date: &IsoDate) -> TemporalResult<u16> {
        if self.is_iso() {
            return Ok(7);
        }
        // TODO: Research in ICU4X and determine best approach.
        Err(TemporalError::range().with_message("Not yet implemented."))
    }

    /// `CalendarDaysInMonth`
    pub fn days_in_month(&self, iso_date: &IsoDate) -> u16 {
        if self.is_iso() {
            return iso_date.to_icu4x().days_in_month() as u16;
        }
        let calendar_date = self.0.from_iso(*iso_date.to_icu4x().inner());
        self.0.days_in_month(&calendar_date) as u16
    }

    /// `CalendarDaysInYear`
    pub fn days_in_year(&self, iso_date: &IsoDate) -> u16 {
        if self.is_iso() {
            return iso_date.to_icu4x().days_in_year();
        }
        let calendar_date = self.0.from_iso(*iso_date.to_icu4x().inner());
        self.0.days_in_year(&calendar_date)
    }

    /// `CalendarMonthsInYear`
    pub fn months_in_year(&self, iso_date: &IsoDate) -> u16 {
        if self.is_iso() {
            return 12;
        }
        let calendar_date = self.0.from_iso(*iso_date.to_icu4x().inner());
        self.0.months_in_year(&calendar_date) as u16
    }

    /// `CalendarInLeapYear`
    pub fn in_leap_year(&self, iso_date: &IsoDate) -> bool {
        if self.is_iso() {
            return iso_date.to_icu4x().is_in_leap_year();
        }
        let calendar_date = self.0.from_iso(*iso_date.to_icu4x().inner());
        self.0.is_in_leap_year(&calendar_date)
    }

    /// Returns the identifier of this calendar slot.
    pub fn identifier(&self) -> &'static str {
        // icu_calendar lists iso8601 and julian as None
        match self.0.calendar_algorithm() {
            Some(c) => c.as_str(),
            None if self.is_iso() => "iso8601",
            None => "julian",
        }
    }
}

impl Calendar {
    pub(crate) fn get_era_info(&self, era_alias: &TinyAsciiStr<19>) -> Option<EraInfo> {
        match self.0 .0.kind() {
            AnyCalendarKind::Buddhist if era::BUDDHIST_ERA_IDENTIFIERS.contains(era_alias) => {
                Some(era::BUDDHIST_ERA)
            }
            AnyCalendarKind::Chinese if *era_alias == tinystr!(19, "chinese") => {
                Some(era::CHINESE_ERA)
            }
            AnyCalendarKind::Coptic if *era_alias == tinystr!(19, "coptic") => {
                Some(era::COPTIC_ERA)
            }
            AnyCalendarKind::Coptic if *era_alias == tinystr!(19, "coptic-inverse") => {
                Some(era::COPTIC_INVERSE_ERA)
            }
            AnyCalendarKind::Dangi if *era_alias == tinystr!(19, "dangi") => Some(era::DANGI_ERA),
            AnyCalendarKind::Ethiopian if era::ETHIOPIC_ERA_IDENTIFIERS.contains(era_alias) => {
                Some(era::ETHIOPIC_ERA)
            }
            AnyCalendarKind::Ethiopian
                if era::ETHIOPIC_ETHOPICAA_ERA_IDENTIFIERS.contains(era_alias) =>
            {
                Some(era::ETHIOPIC_ETHIOAA_ERA)
            }
            AnyCalendarKind::EthiopianAmeteAlem
                if era::ETHIOAA_ERA_IDENTIFIERS.contains(era_alias) =>
            {
                Some(era::ETHIOAA_ERA)
            }
            AnyCalendarKind::Gregorian if era::GREGORY_ERA_IDENTIFIERS.contains(era_alias) => {
                Some(era::GREGORY_ERA)
            }
            AnyCalendarKind::Gregorian
                if era::GREGORY_INVERSE_ERA_IDENTIFIERS.contains(era_alias) =>
            {
                Some(era::GREGORY_INVERSE_ERA)
            }
            AnyCalendarKind::Hebrew if era::HEBREW_ERA_IDENTIFIERS.contains(era_alias) => {
                Some(era::HEBREW_ERA)
            }
            AnyCalendarKind::Indian if era::INDIAN_ERA_IDENTIFIERS.contains(era_alias) => {
                Some(era::INDIAN_ERA)
            }
            // TODO: Determine whether observational is islamic or islamic-rgsa
            AnyCalendarKind::HijriTabularTypeIIFriday
                if era::ISLAMIC_CIVIL_ERA_IDENTIFIERS.contains(era_alias) =>
            {
                Some(era::ISLAMIC_CIVIL_ERA)
            }
            AnyCalendarKind::HijriSimulatedMecca
                if era::ISLAMIC_ERA_IDENTIFIERS.contains(era_alias) =>
            {
                Some(era::ISLAMIC_ERA)
            }
            AnyCalendarKind::HijriTabularTypeIIThursday
                if era::ISLAMIC_TBLA_ERA_IDENTIFIERS.contains(era_alias) =>
            {
                Some(era::ISLAMIC_TBLA_ERA)
            }
            AnyCalendarKind::HijriUmmAlQura
                if era::ISLAMIC_UMALQURA_ERA_IDENTIFIERS.contains(era_alias) =>
            {
                Some(era::ISLAMIC_UMALQURA_ERA)
            }
            AnyCalendarKind::Iso if *era_alias == tinystr!(19, "default") => Some(era::ISO_ERA),
            AnyCalendarKind::Japanese if *era_alias == tinystr!(19, "heisei") => {
                Some(era::HEISEI_ERA)
            }
            AnyCalendarKind::Japanese if era::JAPANESE_ERA_IDENTIFIERS.contains(era_alias) => {
                Some(era::JAPANESE_ERA)
            }
            AnyCalendarKind::Japanese
                if era::JAPANESE_INVERSE_ERA_IDENTIFIERS.contains(era_alias) =>
            {
                Some(era::JAPANESE_INVERSE_ERA)
            }
            AnyCalendarKind::Japanese if *era_alias == tinystr!(19, "mejei") => {
                Some(era::MEJEI_ERA)
            }
            AnyCalendarKind::Japanese if *era_alias == tinystr!(19, "reiwa") => {
                Some(era::REIWA_ERA)
            }
            AnyCalendarKind::Japanese if *era_alias == tinystr!(19, "showa") => {
                Some(era::SHOWA_ERA)
            }
            AnyCalendarKind::Japanese if *era_alias == tinystr!(19, "taisho") => {
                Some(era::TAISHO_ERA)
            }
            AnyCalendarKind::Persian if era::PERSIAN_ERA_IDENTIFIERS.contains(era_alias) => {
                Some(era::PERSIAN_ERA)
            }
            AnyCalendarKind::Roc if era::ROC_ERA_IDENTIFIERS.contains(era_alias) => {
                Some(era::ROC_ERA)
            }
            AnyCalendarKind::Roc if era::ROC_INVERSE_ERA_IDENTIFIERS.contains(era_alias) => {
                Some(era::ROC_INVERSE_ERA)
            }
            _ => None,
        }
    }

    pub(crate) fn get_calendar_default_era(&self) -> Option<EraInfo> {
        match self.0 .0.kind() {
            AnyCalendarKind::Buddhist => Some(era::BUDDHIST_ERA),
            AnyCalendarKind::Chinese => Some(era::CHINESE_ERA),
            AnyCalendarKind::Dangi => Some(era::DANGI_ERA),
            AnyCalendarKind::EthiopianAmeteAlem => Some(era::ETHIOAA_ERA),
            AnyCalendarKind::Hebrew => Some(era::HEBREW_ERA),
            AnyCalendarKind::Indian => Some(era::INDIAN_ERA),
            AnyCalendarKind::HijriSimulatedMecca => Some(era::ISLAMIC_ERA),
            AnyCalendarKind::HijriTabularTypeIIFriday => Some(era::ISLAMIC_CIVIL_ERA),
            AnyCalendarKind::HijriTabularTypeIIThursday => Some(era::ISLAMIC_TBLA_ERA),
            AnyCalendarKind::HijriUmmAlQura => Some(era::ISLAMIC_UMALQURA_ERA),
            AnyCalendarKind::Iso => Some(era::ISO_ERA),
            AnyCalendarKind::Persian => Some(era::PERSIAN_ERA),
            _ => None,
        }
    }
}

impl From<PlainDate> for Calendar {
    fn from(value: PlainDate) -> Self {
        value.calendar().clone()
    }
}

impl From<PlainDateTime> for Calendar {
    fn from(value: PlainDateTime) -> Self {
        value.calendar().clone()
    }
}

impl From<ZonedDateTime> for Calendar {
    fn from(value: ZonedDateTime) -> Self {
        value.calendar().clone()
    }
}

impl From<PlainMonthDay> for Calendar {
    fn from(value: PlainMonthDay) -> Self {
        value.calendar().clone()
    }
}

impl From<PlainYearMonth> for Calendar {
    fn from(value: PlainYearMonth) -> Self {
        value.calendar().clone()
    }
}

#[cfg(test)]
mod tests {
    use crate::{iso::IsoDate, options::Unit};
    use core::str::FromStr;

    use super::Calendar;

    #[test]
    fn calendar_from_str_is_case_insensitive() {
        let cal_str = "iSo8601";
        let calendar = Calendar::try_from_utf8(cal_str.as_bytes()).unwrap();
        assert_eq!(calendar, Calendar::default());

        let cal_str = "iSO8601";
        let calendar = Calendar::try_from_utf8(cal_str.as_bytes()).unwrap();
        assert_eq!(calendar, Calendar::default());
    }

    #[test]
    fn calendar_invalid_ascii_value() {
        let cal_str = "İSO8601";
        let _err = Calendar::from_str(cal_str).unwrap_err();

        let cal_str = "\u{0130}SO8601";
        let _err = Calendar::from_str(cal_str).unwrap_err();

        // Verify that an empty calendar is an error.
        let cal_str = "2025-02-07T01:24:00-06:00[u-ca=]";
        let _err = Calendar::from_str(cal_str).unwrap_err();
    }

    #[test]
    fn date_until_largest_year() {
        // tests format: (Date one, PlainDate two, Duration result)
        let tests = [
            ((2021, 7, 16), (2021, 7, 16), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0)),
            ((2021, 7, 16), (2021, 7, 17), (0, 0, 0, 1, 0, 0, 0, 0, 0, 0)),
            ((2021, 7, 16), (2021, 7, 23), (0, 0, 0, 7, 0, 0, 0, 0, 0, 0)),
            ((2021, 7, 16), (2021, 8, 16), (0, 1, 0, 0, 0, 0, 0, 0, 0, 0)),
            (
                (2020, 12, 16),
                (2021, 1, 16),
                (0, 1, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            ((2021, 1, 5), (2021, 2, 5), (0, 1, 0, 0, 0, 0, 0, 0, 0, 0)),
            ((2021, 1, 7), (2021, 3, 7), (0, 2, 0, 0, 0, 0, 0, 0, 0, 0)),
            ((2021, 7, 16), (2021, 8, 17), (0, 1, 0, 1, 0, 0, 0, 0, 0, 0)),
            (
                (2021, 7, 16),
                (2021, 8, 13),
                (0, 0, 0, 28, 0, 0, 0, 0, 0, 0),
            ),
            ((2021, 7, 16), (2021, 9, 16), (0, 2, 0, 0, 0, 0, 0, 0, 0, 0)),
            ((2021, 7, 16), (2022, 7, 16), (1, 0, 0, 0, 0, 0, 0, 0, 0, 0)),
            (
                (2021, 7, 16),
                (2031, 7, 16),
                (10, 0, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            ((2021, 7, 16), (2022, 7, 19), (1, 0, 0, 3, 0, 0, 0, 0, 0, 0)),
            ((2021, 7, 16), (2022, 9, 19), (1, 2, 0, 3, 0, 0, 0, 0, 0, 0)),
            (
                (2021, 7, 16),
                (2031, 12, 16),
                (10, 5, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            (
                (1997, 12, 16),
                (2021, 7, 16),
                (23, 7, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            (
                (1997, 7, 16),
                (2021, 7, 16),
                (24, 0, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            (
                (1997, 7, 16),
                (2021, 7, 15),
                (23, 11, 0, 29, 0, 0, 0, 0, 0, 0),
            ),
            (
                (1997, 6, 16),
                (2021, 6, 15),
                (23, 11, 0, 30, 0, 0, 0, 0, 0, 0),
            ),
            (
                (1960, 2, 16),
                (2020, 3, 16),
                (60, 1, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            (
                (1960, 2, 16),
                (2021, 3, 15),
                (61, 0, 0, 27, 0, 0, 0, 0, 0, 0),
            ),
            (
                (1960, 2, 16),
                (2020, 3, 15),
                (60, 0, 0, 28, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 3, 30),
                (2021, 7, 16),
                (0, 3, 0, 16, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2020, 3, 30),
                (2021, 7, 16),
                (1, 3, 0, 16, 0, 0, 0, 0, 0, 0),
            ),
            (
                (1960, 3, 30),
                (2021, 7, 16),
                (61, 3, 0, 16, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2019, 12, 30),
                (2021, 7, 16),
                (1, 6, 0, 16, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2020, 12, 30),
                (2021, 7, 16),
                (0, 6, 0, 16, 0, 0, 0, 0, 0, 0),
            ),
            (
                (1997, 12, 30),
                (2021, 7, 16),
                (23, 6, 0, 16, 0, 0, 0, 0, 0, 0),
            ),
            (
                (1, 12, 25),
                (2021, 7, 16),
                (2019, 6, 0, 21, 0, 0, 0, 0, 0, 0),
            ),
            ((2019, 12, 30), (2021, 3, 5), (1, 2, 0, 5, 0, 0, 0, 0, 0, 0)),
            (
                (2021, 7, 17),
                (2021, 7, 16),
                (0, 0, 0, -1, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 7, 23),
                (2021, 7, 16),
                (0, 0, 0, -7, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 8, 16),
                (2021, 7, 16),
                (0, -1, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 1, 16),
                (2020, 12, 16),
                (0, -1, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            ((2021, 2, 5), (2021, 1, 5), (0, -1, 0, 0, 0, 0, 0, 0, 0, 0)),
            ((2021, 3, 7), (2021, 1, 7), (0, -2, 0, 0, 0, 0, 0, 0, 0, 0)),
            (
                (2021, 8, 17),
                (2021, 7, 16),
                (0, -1, 0, -1, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 8, 13),
                (2021, 7, 16),
                (0, 0, 0, -28, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 9, 16),
                (2021, 7, 16),
                (0, -2, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2022, 7, 16),
                (2021, 7, 16),
                (-1, 0, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2031, 7, 16),
                (2021, 7, 16),
                (-10, 0, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2022, 7, 19),
                (2021, 7, 16),
                (-1, 0, 0, -3, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2022, 9, 19),
                (2021, 7, 16),
                (-1, -2, 0, -3, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2031, 12, 16),
                (2021, 7, 16),
                (-10, -5, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 7, 16),
                (1997, 12, 16),
                (-23, -7, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 7, 16),
                (1997, 7, 16),
                (-24, 0, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 7, 15),
                (1997, 7, 16),
                (-23, -11, 0, -30, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 6, 15),
                (1997, 6, 16),
                (-23, -11, 0, -29, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2020, 3, 16),
                (1960, 2, 16),
                (-60, -1, 0, 0, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 3, 15),
                (1960, 2, 16),
                (-61, 0, 0, -28, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2020, 3, 15),
                (1960, 2, 16),
                (-60, 0, 0, -28, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 7, 16),
                (2021, 3, 30),
                (0, -3, 0, -17, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 7, 16),
                (2020, 3, 30),
                (-1, -3, 0, -17, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 7, 16),
                (1960, 3, 30),
                (-61, -3, 0, -17, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 7, 16),
                (2019, 12, 30),
                (-1, -6, 0, -17, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 7, 16),
                (2020, 12, 30),
                (0, -6, 0, -17, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 7, 16),
                (1997, 12, 30),
                (-23, -6, 0, -17, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 7, 16),
                (1, 12, 25),
                (-2019, -6, 0, -22, 0, 0, 0, 0, 0, 0),
            ),
            (
                (2021, 3, 5),
                (2019, 12, 30),
                (-1, -2, 0, -6, 0, 0, 0, 0, 0, 0),
            ),
        ];

        let calendar = Calendar::default();

        for test in tests {
            let first = IsoDate::new_unchecked(test.0 .0, test.0 .1, test.0 .2);
            let second = IsoDate::new_unchecked(test.1 .0, test.1 .1, test.1 .2);
            let result = calendar.date_until(&first, &second, Unit::Year).unwrap();
            assert_eq!(
                result.years() as i32,
                test.2 .0,
                "year failed for test \"{test:?}\""
            );
            assert_eq!(
                result.months() as i32,
                test.2 .1,
                "months failed for test \"{test:?}\""
            );
            assert_eq!(
                result.weeks() as i32,
                test.2 .2,
                "weeks failed for test \"{test:?}\""
            );
            assert_eq!(
                result.days(),
                test.2 .3,
                "days failed for test \"{test:?}\""
            );
        }
    }
}
