#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {
    use crate::calendar::ffi::Calendar;
    use crate::duration::ffi::Duration;
    use crate::error::ffi::TemporalError;
    use alloc::boxed::Box;

    use crate::options::ffi::{ArithmeticOverflow, DifferenceSettings};
    use crate::plain_date::ffi::{PartialDate, PlainDate};
    use alloc::string::String;
    use core::fmt::Write;
    use diplomat_runtime::DiplomatWrite;
    use diplomat_runtime::{DiplomatStr, DiplomatStr16};

    use core::str::FromStr;

    #[diplomat::opaque]
    pub struct PlainYearMonth(pub(crate) temporal_rs::PlainYearMonth);

    impl PlainYearMonth {
        pub fn create_with_overflow(
            year: i32,
            month: u8,
            reference_day: Option<u8>,
            calendar: &Calendar,
            overflow: ArithmeticOverflow,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainYearMonth::new_with_overflow(
                year,
                month,
                reference_day,
                calendar.0.clone(),
                overflow.into(),
            )
            .map(|x| Box::new(PlainYearMonth(x)))
            .map_err(Into::into)
        }

        pub fn with(
            &self,
            partial: PartialDate,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .with(partial.try_into()?, overflow.map(Into::into))
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }

        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainYearMonth::from_utf8(s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            temporal_rs::PlainYearMonth::from_str(&s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn iso_year(&self) -> i32 {
            self.0.iso_year()
        }

        pub fn padded_iso_year_string(&self, write: &mut DiplomatWrite) {
            // TODO this double-allocates, an API returning a Writeable or impl Write would be better
            let string = self.0.padded_iso_year_string();
            // throw away the error, the write itself should always succeed
            let _ = write.write_str(&string);
        }

        pub fn iso_month(&self) -> u8 {
            self.0.iso_month()
        }

        pub fn year(&self) -> i32 {
            self.0.year()
        }
        pub fn month(&self) -> u8 {
            self.0.month()
        }
        pub fn month_code(&self, write: &mut DiplomatWrite) {
            let code = self.0.month_code();
            // throw away the error, this should always succeed
            let _ = write.write_str(code.as_str());
        }

        pub fn in_leap_year(&self) -> bool {
            self.0.in_leap_year()
        }
        pub fn days_in_month(&self) -> u16 {
            self.0.days_in_month()
        }
        pub fn days_in_year(&self) -> u16 {
            self.0.days_in_year()
        }
        pub fn months_in_year(&self) -> u16 {
            self.0.months_in_year()
        }
        // Writes an empty string for no era
        pub fn era(&self, write: &mut DiplomatWrite) {
            let era = self.0.era();
            if let Some(era) = era {
                // throw away the error, this should always succeed
                let _ = write.write_str(&era);
            }
        }

        pub fn era_year(&self) -> Option<i32> {
            self.0.era_year()
        }

        pub fn calendar<'a>(&'a self) -> &'a Calendar {
            Calendar::transparent_convert(self.0.calendar())
        }
        pub fn add(
            &self,
            duration: &Duration,
            overflow: ArithmeticOverflow,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .add(&duration.0, overflow.into())
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }
        pub fn subtract(
            &self,
            duration: &Duration,
            overflow: ArithmeticOverflow,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .subtract(&duration.0, overflow.into())
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }
        pub fn until(
            &self,
            other: &Self,
            settings: DifferenceSettings,
        ) -> Result<Box<Duration>, TemporalError> {
            self.0
                .until(&other.0, settings.try_into()?)
                .map(|x| Box::new(Duration(x)))
                .map_err(Into::into)
        }
        pub fn since(
            &self,
            other: &Self,
            settings: DifferenceSettings,
        ) -> Result<Box<Duration>, TemporalError> {
            self.0
                .since(&other.0, settings.try_into()?)
                .map(|x| Box::new(Duration(x)))
                .map_err(Into::into)
        }
        pub fn equals(&self, other: &Self) -> bool {
            self.0 == other.0
        }
        pub fn compare(one: &Self, two: &Self) -> core::cmp::Ordering {
            (one.iso_year(), one.iso_month()).cmp(&(two.iso_year(), two.iso_month()))
        }
        pub fn to_plain_date(&self) -> Result<Box<PlainDate>, TemporalError> {
            self.0
                .to_plain_date()
                .map(|x| Box::new(PlainDate(x)))
                .map_err(Into::into)
        }
    }
}
