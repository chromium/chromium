#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {
    use crate::duration::ffi::Duration;
    use crate::error::ffi::TemporalError;
    use crate::iso::ffi::IsoDate;
    use crate::options::ffi::{ArithmeticOverflow, Unit};
    use crate::plain_date::ffi::{PartialDate, PlainDate};
    use crate::plain_month_day::ffi::PlainMonthDay;
    use crate::plain_year_month::ffi::PlainYearMonth;
    use alloc::boxed::Box;
    use core::fmt::Write;
    use diplomat_runtime::DiplomatStr;
    use icu_calendar::preferences::CalendarAlgorithm;

    #[diplomat::enum_convert(icu_calendar::AnyCalendarKind, needs_wildcard)]
    pub enum AnyCalendarKind {
        Buddhist,
        Chinese,
        Coptic,
        Dangi,
        Ethiopian,
        EthiopianAmeteAlem,
        Gregorian,
        Hebrew,
        Indian,
        HijriTabularTypeIIFriday,
        HijriSimulatedMecca,
        HijriTabularTypeIIThursday,
        HijriUmmAlQura,
        Iso,
        Japanese,
        JapaneseExtended,
        Persian,
        Roc,
    }

    impl AnyCalendarKind {
        pub fn get_for_str(s: &DiplomatStr) -> Option<Self> {
            let value = icu_locale::extensions::unicode::Value::try_from_utf8(s).ok()?;
            let algorithm = CalendarAlgorithm::try_from(&value).ok()?;
            match icu_calendar::AnyCalendarKind::try_from(algorithm) {
                Ok(c) => Some(c.into()),
                Err(()) if algorithm == CalendarAlgorithm::Hijri(None) => {
                    Some(Self::HijriTabularTypeIIFriday)
                }
                Err(()) => None,
            }
        }
    }

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    pub struct Calendar(pub temporal_rs::Calendar);

    impl Calendar {
        pub fn create(kind: AnyCalendarKind) -> Box<Self> {
            Box::new(Calendar(temporal_rs::Calendar::new(kind.into())))
        }

        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::Calendar::try_from_utf8(s)
                .map(|c| Box::new(Calendar(c)))
                .map_err(Into::into)
        }

        pub fn is_iso(&self) -> bool {
            self.0.is_iso()
        }

        pub fn identifier(&self) -> &'static str {
            self.0.identifier()
        }

        pub fn date_from_partial(
            &self,
            partial: PartialDate,
            overflow: ArithmeticOverflow,
        ) -> Result<Box<PlainDate>, TemporalError> {
            self.0
                .date_from_partial(&partial.try_into()?, overflow.into())
                .map(|c| Box::new(PlainDate(c)))
                .map_err(Into::into)
        }

        pub fn month_day_from_partial(
            &self,
            partial: PartialDate,
            overflow: ArithmeticOverflow,
        ) -> Result<Box<PlainMonthDay>, TemporalError> {
            self.0
                .month_day_from_partial(&partial.try_into()?, overflow.into())
                .map(|c| Box::new(PlainMonthDay(c)))
                .map_err(Into::into)
        }
        pub fn year_month_from_partial(
            &self,
            partial: PartialDate,
            overflow: ArithmeticOverflow,
        ) -> Result<Box<PlainYearMonth>, TemporalError> {
            self.0
                .year_month_from_partial(&partial.try_into()?, overflow.into())
                .map(|c| Box::new(PlainYearMonth(c)))
                .map_err(Into::into)
        }
        pub fn date_add(
            &self,
            date: IsoDate,
            duration: &Duration,
            overflow: ArithmeticOverflow,
        ) -> Result<Box<PlainDate>, TemporalError> {
            self.0
                .date_add(&date.into(), &duration.0, overflow.into())
                .map(|c| Box::new(PlainDate(c)))
                .map_err(Into::into)
        }
        pub fn date_until(
            &self,
            one: IsoDate,
            two: IsoDate,
            largest_unit: Unit,
        ) -> Result<Box<Duration>, TemporalError> {
            self.0
                .date_until(&one.into(), &two.into(), largest_unit.into())
                .map(|c| Box::new(Duration(c)))
                .map_err(Into::into)
        }

        // Writes an empty string for no era
        pub fn era(&self, date: IsoDate, write: &mut DiplomatWrite) -> Result<(), TemporalError> {
            let era = self.0.era(&date.into());
            if let Some(era) = era {
                // throw away the error, this should always succeed
                let _ = write.write_str(&era);
            }
            Ok(())
        }

        pub fn era_year(&self, date: IsoDate) -> Option<i32> {
            self.0.era_year(&date.into())
        }

        pub fn year(&self, date: IsoDate) -> i32 {
            self.0.year(&date.into())
        }
        pub fn month(&self, date: IsoDate) -> u8 {
            self.0.month(&date.into())
        }
        pub fn month_code(
            &self,
            date: IsoDate,
            write: &mut DiplomatWrite,
        ) -> Result<(), TemporalError> {
            let code = self.0.month_code(&date.into());
            // throw away the error, this should always succeed
            let _ = write.write_str(code.as_str());
            Ok(())
        }
        pub fn day(&self, date: IsoDate) -> u8 {
            self.0.day(&date.into())
        }
        pub fn day_of_week(&self, date: IsoDate) -> Result<u16, TemporalError> {
            self.0.day_of_week(&date.into()).map_err(Into::into)
        }
        pub fn day_of_year(&self, date: IsoDate) -> u16 {
            self.0.day_of_year(&date.into())
        }
        pub fn week_of_year(&self, date: IsoDate) -> Option<u8> {
            self.0.week_of_year(&date.into())
        }
        pub fn year_of_week(&self, date: IsoDate) -> Option<i32> {
            self.0.year_of_week(&date.into())
        }
        pub fn days_in_week(&self, date: IsoDate) -> Result<u16, TemporalError> {
            self.0.days_in_week(&date.into()).map_err(Into::into)
        }
        pub fn days_in_month(&self, date: IsoDate) -> u16 {
            self.0.days_in_month(&date.into())
        }
        pub fn days_in_year(&self, date: IsoDate) -> u16 {
            self.0.days_in_year(&date.into())
        }
        pub fn months_in_year(&self, date: IsoDate) -> u16 {
            self.0.months_in_year(&date.into())
        }
        pub fn in_leap_year(&self, date: IsoDate) -> bool {
            self.0.in_leap_year(&date.into())
        }

        // TODO .fields() (need to pick a convenient way to return vectors or iterators, depending on how the API gets used)
    }
}
