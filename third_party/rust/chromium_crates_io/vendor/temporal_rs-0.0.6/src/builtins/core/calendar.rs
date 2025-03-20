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
    options::{ArithmeticOverflow, TemporalUnit},
    parsers::parse_allowed_calendar_formats,
    TemporalError, TemporalResult,
};
use core::str::FromStr;

use icu_calendar::types::{Era as IcuEra, MonthCode as IcuMonthCode, MonthInfo, YearInfo};
use icu_calendar::{
    any_calendar::AnyDateInner,
    cal::{
        Buddhist, Chinese, Coptic, Dangi, Ethiopian, EthiopianEraStyle, Hebrew, Indian,
        IslamicCivil, IslamicObservational, IslamicTabular, IslamicUmmAlQura, Japanese,
        JapaneseExtended, Persian, Roc,
    },
    types::{DayOfMonth, DayOfYearInfo},
    week::{RelativeUnit, WeekCalculator},
    AnyCalendar, AnyCalendarKind, Calendar as IcuCalendar, DateDuration as IcuDateDuration,
    DateDurationUnit as IcuDateDurationUnit, Gregorian, Iso, Ref,
};
use tinystr::{tinystr, TinyAsciiStr};

use super::{PartialDate, ZonedDateTime};

mod era;
mod types;

pub(crate) use types::{month_to_month_code, ResolutionType};
pub use types::{MonthCode, ResolvedCalendarFields};

use era::EraInfo;

#[derive(Debug, Clone)]
pub struct Calendar(Ref<'static, AnyCalendar>);

impl Default for Calendar {
    fn default() -> Self {
        Calendar::new(AnyCalendarKind::Iso)
    }
}

impl PartialEq for Calendar {
    fn eq(&self, other: &Self) -> bool {
        self.identifier() == other.identifier()
    }
}

impl Eq for Calendar {}

impl IcuCalendar for Calendar {
    type DateInner = AnyDateInner;

    fn date_from_codes(
        &self,
        era: Option<icu_calendar::types::Era>,
        year: i32,
        month_code: icu_calendar::types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, icu_calendar::DateError> {
        self.0.date_from_codes(era, year, month_code, day)
    }

    fn date_from_iso(&self, iso: icu_calendar::Date<Iso>) -> Self::DateInner {
        self.0.date_from_iso(iso)
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> icu_calendar::Date<Iso> {
        self.0.date_to_iso(date)
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        self.0.months_in_year(date)
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        self.0.days_in_year(date)
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        self.0.days_in_month(date)
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: IcuDateDuration<Self>) {
        self.0.offset_date(date, offset.cast_unit())
    }

    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        calendar2: &Self,
        largest_unit: IcuDateDurationUnit,
        smallest_unit: IcuDateDurationUnit,
    ) -> IcuDateDuration<Self> {
        self.0
            .until(date1, date2, &calendar2.0, largest_unit, smallest_unit)
            .cast_unit()
    }

    fn debug_name(&self) -> &'static str {
        self.0.debug_name()
    }

    fn year(&self, date: &Self::DateInner) -> YearInfo {
        self.0.year(date)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        self.0.is_in_leap_year(date)
    }

    fn month(&self, date: &Self::DateInner) -> MonthInfo {
        self.0.month(date)
    }

    fn day_of_month(&self, date: &Self::DateInner) -> DayOfMonth {
        self.0.day_of_month(date)
    }

    fn day_of_year_info(&self, date: &Self::DateInner) -> DayOfYearInfo {
        self.0.day_of_year_info(date)
    }
}

impl Calendar {
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
            AnyCalendarKind::IslamicCivil => &AnyCalendar::IslamicCivil(IslamicCivil),
            AnyCalendarKind::IslamicObservational => {
                const { &AnyCalendar::IslamicObservational(IslamicObservational::new()) }
            }
            AnyCalendarKind::IslamicTabular => &AnyCalendar::IslamicTabular(IslamicTabular),
            AnyCalendarKind::IslamicUmmAlQura => {
                const { &AnyCalendar::IslamicUmmAlQura(IslamicUmmAlQura::new()) }
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

    /// Returns a `Calendar`` from the a slice of UTF-8 encoded bytes.
    pub fn from_utf8(bytes: &[u8]) -> TemporalResult<Self> {
        // NOTE(nekesss): Catch the iso identifier here, as `iso8601` is not a valid ID below.
        if bytes.to_ascii_lowercase() == "iso8601".as_bytes() {
            return Ok(Self::default());
        }

        let Some(cal) = AnyCalendarKind::get_for_bcp47_bytes(&bytes.to_ascii_lowercase()) else {
            return Err(TemporalError::range().with_message("Not a builtin calendar."));
        };

        Ok(Calendar::new(cal))
    }
}

impl FromStr for Calendar {
    type Err = TemporalError;

    // 13.34 ParseTemporalCalendarString ( string )
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match parse_allowed_calendar_formats(s) {
            Some([]) => Ok(Calendar::default()),
            Some(result) => Calendar::from_utf8(result),
            None => Calendar::from_utf8(s.as_bytes()),
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
            .date_from_codes(
                Some(IcuEra(resolved_fields.era_year.era.0)),
                resolved_fields.era_year.year,
                IcuMonthCode(resolved_fields.month_code.0),
                resolved_fields.day,
            )
            .map_err(TemporalError::from_icu4x)?;
        let iso = self.0.date_to_iso(&calendar_date);
        PlainDate::new_with_overflow(
            iso.year().extended_year,
            iso.month().ordinal,
            iso.day_of_month().0,
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
            .date_from_codes(
                Some(IcuEra(resolved_fields.era_year.era.0)),
                resolved_fields.era_year.year,
                IcuMonthCode(resolved_fields.month_code.0),
                resolved_fields.day,
            )
            .map_err(TemporalError::from_icu4x)?;
        let iso = self.0.date_to_iso(&calendar_date);
        PlainYearMonth::new_with_overflow(
            iso.year().extended_year,
            iso.month().ordinal,
            Some(iso.day_of_month().0),
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
                TimeDuration::from_normalized(duration.time().to_normalized(), TemporalUnit::Day)?;

            // 10. Let result be ? AddISODate(date.[[ISOYear]], date.[[ISOMonth]], date.[[ISODay]], duration.[[Years]],
            // duration.[[Months]], duration.[[Weeks]], duration.[[Days]] + balanceResult.[[Days]], overflow).
            let result = date.add_date_duration(
                &DateDuration::new_unchecked(
                    duration.years(),
                    duration.months(),
                    duration.weeks(),
                    duration.days().checked_add(&balance_days)?,
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
        largest_unit: TemporalUnit,
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
        let calendar_date = self.0.date_from_iso(iso_date.to_icu4x());
        self.0.year(&calendar_date).standard_era().map(|era| era.0)
    }

    /// `CalendarEraYear`
    pub fn era_year(&self, iso_date: &IsoDate) -> Option<i32> {
        if self.is_iso() {
            return None;
        }
        let calendar_date = self.0.date_from_iso(iso_date.to_icu4x());
        self.0.year(&calendar_date).era_year()
    }

    /// `CalendarYear`
    pub fn year(&self, iso_date: &IsoDate) -> i32 {
        if self.is_iso() {
            return iso_date.year;
        }
        let calendar_date = self.0.date_from_iso(iso_date.to_icu4x());
        self.0.year(&calendar_date).extended_year
    }

    /// `CalendarMonth`
    pub fn month(&self, iso_date: &IsoDate) -> u8 {
        if self.is_iso() {
            return iso_date.month;
        }
        let calendar_date = self.0.date_from_iso(iso_date.to_icu4x());
        self.0.month(&calendar_date).month_number()
    }

    /// `CalendarMonthCode`
    pub fn month_code(&self, iso_date: &IsoDate) -> MonthCode {
        if self.is_iso() {
            let mc = iso_date.to_icu4x().month().standard_code.0;
            return MonthCode(mc);
        }
        let calendar_date = self.0.date_from_iso(iso_date.to_icu4x());
        MonthCode(self.0.month(&calendar_date).standard_code.0)
    }

    /// `CalendarDay`
    pub fn day(&self, iso_date: &IsoDate) -> u8 {
        if self.is_iso() {
            return iso_date.day;
        }
        let calendar_date = self.0.date_from_iso(iso_date.to_icu4x());
        self.0.day_of_month(&calendar_date).0
    }

    /// `CalendarDayOfWeek`
    pub fn day_of_week(&self, iso_date: &IsoDate) -> u16 {
        if self.is_iso() {
            return iso_date.to_icu4x().day_of_week() as u16;
        }
        let calendar_date = self.0.date_from_iso(iso_date.to_icu4x());
        // TODO: Understand ICU4X's decision for `IsoWeekDay` to be `i8`
        self.0.day_of_week(&calendar_date) as u16
    }

    /// `CalendarDayOfYear`
    pub fn day_of_year(&self, iso_date: &IsoDate) -> u16 {
        if self.is_iso() {
            return iso_date.to_icu4x().day_of_year_info().day_of_year;
        }
        let calendar_date = self.0.date_from_iso(iso_date.to_icu4x());
        self.0.day_of_year_info(&calendar_date).day_of_year
    }

    /// `CalendarWeekOfYear`
    pub fn week_of_year(&self, iso_date: &IsoDate) -> TemporalResult<Option<u16>> {
        if self.is_iso() {
            let date = iso_date.to_icu4x();
            let week_calculator = WeekCalculator::default();
            let week_of = date.week_of_year(&week_calculator);
            return Ok(Some(week_of.week as u16));
        }
        // TODO: Research in ICU4X and determine best approach.
        Err(TemporalError::range().with_message("Not yet implemented."))
    }

    /// `CalendarYearOfWeek`
    pub fn year_of_week(&self, iso_date: &IsoDate) -> TemporalResult<Option<i32>> {
        if self.is_iso() {
            let date = iso_date.to_icu4x();

            let week_calculator = WeekCalculator::default();

            let week_of = date.week_of_year(&week_calculator);

            return match week_of.unit {
                RelativeUnit::Previous => Ok(Some(date.year().extended_year - 1)),
                RelativeUnit::Current => Ok(Some(date.year().extended_year)),
                RelativeUnit::Next => Ok(Some(date.year().extended_year + 1)),
            };
        }
        // TODO: Research in ICU4X and determine best approach.
        Err(TemporalError::range().with_message("Not yet implemented."))
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
        let calendar_date = self.0.date_from_iso(iso_date.to_icu4x());
        self.0.days_in_month(&calendar_date) as u16
    }

    /// `CalendarDaysInYear`
    pub fn days_in_year(&self, iso_date: &IsoDate) -> u16 {
        if self.is_iso() {
            return iso_date.to_icu4x().days_in_year();
        }
        let calendar_date = self.0.date_from_iso(iso_date.to_icu4x());
        self.0.days_in_year(&calendar_date)
    }

    /// `CalendarMonthsInYear`
    pub fn months_in_year(&self, iso_date: &IsoDate) -> u16 {
        if self.is_iso() {
            return 12;
        }
        let calendar_date = self.0.date_from_iso(iso_date.to_icu4x());
        self.0.months_in_year(&calendar_date) as u16
    }

    /// `CalendarInLeapYear`
    pub fn in_leap_year(&self, iso_date: &IsoDate) -> bool {
        if self.is_iso() {
            return iso_date.to_icu4x().is_in_leap_year();
        }
        let calendar_date = self.0.date_from_iso(iso_date.to_icu4x());
        self.0.is_in_leap_year(&calendar_date)
    }

    /// Returns the identifier of this calendar slot.
    pub fn identifier(&self) -> &'static str {
        if self.is_iso() {
            return "iso8601";
        }
        self.0 .0.kind().as_bcp47_string()
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
            AnyCalendarKind::IslamicCivil
                if era::ISLAMIC_CIVIL_ERA_IDENTIFIERS.contains(era_alias) =>
            {
                Some(era::ISLAMIC_CIVIL_ERA)
            }
            AnyCalendarKind::IslamicObservational
                if era::ISLAMIC_ERA_IDENTIFIERS.contains(era_alias) =>
            {
                Some(era::ISLAMIC_ERA)
            }
            AnyCalendarKind::IslamicTabular
                if era::ISLAMIC_TBLA_ERA_IDENTIFIERS.contains(era_alias) =>
            {
                Some(era::ISLAMIC_TBLA_ERA)
            }
            AnyCalendarKind::IslamicUmmAlQura
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
            AnyCalendarKind::IslamicCivil => Some(era::ISLAMIC_CIVIL_ERA),
            AnyCalendarKind::IslamicObservational => Some(era::ISLAMIC_ERA),
            AnyCalendarKind::IslamicTabular => Some(era::ISLAMIC_TBLA_ERA),
            AnyCalendarKind::IslamicUmmAlQura => Some(era::ISLAMIC_UMALQURA_ERA),
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
    use crate::{iso::IsoDate, options::TemporalUnit};
    use core::str::FromStr;

    use super::Calendar;

    #[test]
    fn calendar_from_str_is_case_insensitive() {
        let cal_str = "iSo8601";
        let calendar = Calendar::from_utf8(cal_str.as_bytes()).unwrap();
        assert_eq!(calendar, Calendar::default());

        let cal_str = "iSO8601";
        let calendar = Calendar::from_utf8(cal_str.as_bytes()).unwrap();
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
            let result = calendar
                .date_until(&first, &second, TemporalUnit::Year)
                .unwrap();
            assert_eq!(
                result.years().0 as i32,
                test.2 .0,
                "year failed for test \"{test:?}\""
            );
            assert_eq!(
                result.months().0 as i32,
                test.2 .1,
                "months failed for test \"{test:?}\""
            );
            assert_eq!(
                result.weeks().0 as i32,
                test.2 .2,
                "weeks failed for test \"{test:?}\""
            );
            assert_eq!(
                result.days().0 as i32,
                test.2 .3,
                "days failed for test \"{test:?}\""
            );
        }
    }
}
