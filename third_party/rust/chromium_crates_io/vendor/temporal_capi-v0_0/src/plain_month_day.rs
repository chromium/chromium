#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {
    use crate::calendar::ffi::Calendar;
    use crate::error::ffi::TemporalError;
    use alloc::boxed::Box;

    use crate::options::ffi::ArithmeticOverflow;
    use crate::plain_date::ffi::{PartialDate, PlainDate};

    use alloc::string::String;
    use core::fmt::Write;
    use core::str::FromStr;
    use diplomat_runtime::DiplomatWrite;
    use diplomat_runtime::{DiplomatStr, DiplomatStr16};

    #[diplomat::opaque]
    pub struct PlainMonthDay(pub(crate) temporal_rs::PlainMonthDay);

    impl PlainMonthDay {
        pub fn create_with_overflow(
            month: u8,
            day: u8,
            calendar: &Calendar,
            overflow: ArithmeticOverflow,
            ref_year: Option<i32>,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainMonthDay::new_with_overflow(
                month,
                day,
                calendar.0.clone(),
                overflow.into(),
                ref_year,
            )
            .map(|x| Box::new(PlainMonthDay(x)))
            .map_err(Into::into)
        }

        pub fn with(
            &self,
            partial: PartialDate,
            overflow: ArithmeticOverflow,
        ) -> Result<Box<PlainMonthDay>, TemporalError> {
            self.0
                .with(partial.try_into()?, overflow.into())
                .map(|x| Box::new(PlainMonthDay(x)))
                .map_err(Into::into)
        }

        pub fn equals(&self, other: &Self) -> bool {
            self.0 == other.0
        }

        pub fn compare(one: &Self, two: &Self) -> core::cmp::Ordering {
            (one.iso_year(), one.iso_month()).cmp(&(two.iso_year(), two.iso_month()))
        }

        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainMonthDay::from_utf8(s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            temporal_rs::PlainMonthDay::from_str(&s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn iso_year(&self) -> i32 {
            self.0.iso_year()
        }
        pub fn iso_month(&self) -> u8 {
            self.0.iso_month()
        }
        pub fn iso_day(&self) -> u8 {
            self.0.iso_day()
        }

        pub fn calendar<'a>(&'a self) -> &'a Calendar {
            Calendar::transparent_convert(self.0.calendar())
        }

        pub fn month_code(&self, write: &mut DiplomatWrite) {
            let code = self.0.month_code();
            // throw away the error, this should always succeed
            let _ = write.write_str(code.as_str());
        }

        pub fn to_plain_date(&self) -> Result<Box<PlainDate>, TemporalError> {
            self.0
                .to_plain_date()
                .map(|x| Box::new(PlainDate(x)))
                .map_err(Into::into)
        }
    }
}
