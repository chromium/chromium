use crate::error::ffi::TemporalError;

#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {
    use crate::calendar::ffi::Calendar;
    use crate::duration::ffi::Duration;
    use crate::error::ffi::TemporalError;
    use alloc::boxed::Box;

    use crate::options::ffi::{
        ArithmeticOverflow, DifferenceSettings, DisplayCalendar, RoundingOptions,
        ToStringRoundingOptions,
    };
    use crate::plain_date::ffi::{PartialDate, PlainDate};
    use crate::plain_time::ffi::{PartialTime, PlainTime};
    use alloc::string::String;
    use core::fmt::Write;
    use core::str::FromStr;
    use diplomat_runtime::DiplomatWrite;
    use diplomat_runtime::{DiplomatStr, DiplomatStr16};

    #[diplomat::opaque]
    pub struct PlainDateTime(pub(crate) temporal_rs::PlainDateTime);

    pub struct PartialDateTime<'a> {
        pub date: PartialDate<'a>,
        pub time: PartialTime,
    }

    impl PlainDateTime {
        pub fn create(
            year: i32,
            month: u8,
            day: u8,
            hour: u8,
            minute: u8,
            second: u8,
            millisecond: u16,
            microsecond: u16,
            nanosecond: u16,
            calendar: &Calendar,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDateTime::new(
                year,
                month,
                day,
                hour,
                minute,
                second,
                millisecond,
                microsecond,
                nanosecond,
                calendar.0.clone(),
            )
            .map(|x| Box::new(PlainDateTime(x)))
            .map_err(Into::into)
        }
        pub fn try_create(
            year: i32,
            month: u8,
            day: u8,
            hour: u8,
            minute: u8,
            second: u8,
            millisecond: u16,
            microsecond: u16,
            nanosecond: u16,
            calendar: &Calendar,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDateTime::try_new(
                year,
                month,
                day,
                hour,
                minute,
                second,
                millisecond,
                microsecond,
                nanosecond,
                calendar.0.clone(),
            )
            .map(|x| Box::new(PlainDateTime(x)))
            .map_err(Into::into)
        }

        pub fn from_partial(
            partial: PartialDateTime,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDateTime::from_partial(partial.try_into()?, overflow.map(Into::into))
                .map(|x| Box::new(PlainDateTime(x)))
                .map_err(Into::into)
        }
        pub fn with(
            &self,
            partial: PartialDateTime,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .with(partial.try_into()?, overflow.map(Into::into))
                .map(|x| Box::new(PlainDateTime(x)))
                .map_err(Into::into)
        }

        pub fn with_time(&self, time: &PlainTime) -> Result<Box<Self>, TemporalError> {
            self.0
                .with_time(time.0)
                .map(|x| Box::new(PlainDateTime(x)))
                .map_err(Into::into)
        }

        pub fn with_calendar(&self, calendar: &Calendar) -> Result<Box<Self>, TemporalError> {
            self.0
                .with_calendar(calendar.0.clone())
                .map(|x| Box::new(PlainDateTime(x)))
                .map_err(Into::into)
        }

        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDateTime::from_utf8(s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            temporal_rs::PlainDateTime::from_str(&s)
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

        pub fn hour(&self) -> u8 {
            self.0.hour()
        }
        pub fn minute(&self) -> u8 {
            self.0.minute()
        }
        pub fn second(&self) -> u8 {
            self.0.second()
        }
        pub fn millisecond(&self) -> u16 {
            self.0.millisecond()
        }
        pub fn microsecond(&self) -> u16 {
            self.0.microsecond()
        }
        pub fn nanosecond(&self) -> u16 {
            self.0.nanosecond()
        }

        pub fn calendar<'a>(&'a self) -> &'a Calendar {
            Calendar::transparent_convert(self.0.calendar())
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
        pub fn day(&self) -> u8 {
            self.0.day()
        }
        pub fn day_of_week(&self) -> Result<u16, TemporalError> {
            self.0.day_of_week().map_err(Into::into)
        }
        pub fn day_of_year(&self) -> u16 {
            self.0.day_of_year()
        }
        pub fn week_of_year(&self) -> Option<u8> {
            self.0.week_of_year()
        }
        pub fn year_of_week(&self) -> Option<i32> {
            self.0.year_of_week()
        }
        pub fn days_in_week(&self) -> Result<u16, TemporalError> {
            self.0.days_in_week().map_err(Into::into)
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
        pub fn in_leap_year(&self) -> bool {
            self.0.in_leap_year()
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

        pub fn add(
            &self,
            duration: &Duration,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .add(&duration.0, overflow.map(Into::into))
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }
        pub fn subtract(
            &self,
            duration: &Duration,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .subtract(&duration.0, overflow.map(Into::into))
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
            let tuple1 = (
                one.iso_year(),
                one.iso_month(),
                one.iso_day(),
                one.hour(),
                one.minute(),
                one.second(),
                one.millisecond(),
                one.microsecond(),
                one.nanosecond(),
            );
            let tuple2 = (
                two.iso_year(),
                two.iso_month(),
                two.iso_day(),
                two.hour(),
                two.minute(),
                two.second(),
                two.millisecond(),
                two.microsecond(),
                two.nanosecond(),
            );

            tuple1.cmp(&tuple2)
        }

        pub fn round(&self, options: RoundingOptions) -> Result<Box<Self>, TemporalError> {
            self.0
                .round(options.try_into()?)
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }

        pub fn to_plain_date(&self) -> Result<Box<PlainDate>, TemporalError> {
            self.0
                .to_plain_date()
                .map(|x| Box::new(PlainDate(x)))
                .map_err(Into::into)
        }

        pub fn to_plain_time(&self) -> Result<Box<PlainTime>, TemporalError> {
            self.0
                .to_plain_time()
                .map(|x| Box::new(PlainTime(x)))
                .map_err(Into::into)
        }

        pub fn to_ixdtf_string(
            &self,
            options: ToStringRoundingOptions,

            display_calendar: DisplayCalendar,
            write: &mut DiplomatWrite,
        ) -> Result<(), TemporalError> {
            // TODO this double-allocates, an API returning a Writeable or impl Write would be better
            let string = self
                .0
                .to_ixdtf_string(options.into(), display_calendar.into())?;
            // throw away the error, this should always succeed
            let _ = write.write_str(&string);
            Ok(())
        }
    }
}

impl TryFrom<ffi::PartialDateTime<'_>> for temporal_rs::partial::PartialDateTime {
    type Error = TemporalError;
    fn try_from(other: ffi::PartialDateTime<'_>) -> Result<Self, TemporalError> {
        Ok(Self {
            date: other.date.try_into()?,
            time: other.time.into(),
        })
    }
}
