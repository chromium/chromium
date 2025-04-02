#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {

    use crate::duration::ffi::{Duration, TimeDuration};
    use crate::error::ffi::TemporalError;
    use crate::options::ffi::{
        ArithmeticOverflow, DifferenceSettings, TemporalRoundingMode, TemporalUnit,
        ToStringRoundingOptions,
    };
    use diplomat_runtime::{DiplomatOption, DiplomatWrite};
    use std::fmt::Write;

    #[diplomat::opaque]
    pub struct PlainTime(pub(crate) temporal_rs::PlainTime);

    pub struct PartialTime {
        pub hour: DiplomatOption<u8>,
        pub minute: DiplomatOption<u8>,
        pub second: DiplomatOption<u8>,
        pub millisecond: DiplomatOption<u16>,
        pub microsecond: DiplomatOption<u16>,
        pub nanosecond: DiplomatOption<u16>,
    }

    impl PlainTime {
        pub fn create(
            hour: u8,
            minute: u8,
            second: u8,
            millisecond: u16,
            microsecond: u16,
            nanosecond: u16,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainTime::new(hour, minute, second, millisecond, microsecond, nanosecond)
                .map(|x| Box::new(PlainTime(x)))
                .map_err(Into::into)
        }
        pub fn try_create(
            hour: u8,
            minute: u8,
            second: u8,
            millisecond: u16,
            microsecond: u16,
            nanosecond: u16,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainTime::try_new(
                hour,
                minute,
                second,
                millisecond,
                microsecond,
                nanosecond,
            )
            .map(|x| Box::new(PlainTime(x)))
            .map_err(Into::into)
        }

        pub fn from_partial(
            partial: PartialTime,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainTime::from_partial(partial.into(), overflow.map(Into::into))
                .map(|x| Box::new(PlainTime(x)))
                .map_err(Into::into)
        }
        pub fn with(
            &self,
            partial: PartialTime,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .with(partial.into(), overflow.map(Into::into))
                .map(|x| Box::new(PlainTime(x)))
                .map_err(Into::into)
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

        pub fn add(&self, duration: &Duration) -> Result<Box<Self>, TemporalError> {
            self.0
                .add(&duration.0)
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }
        pub fn subtract(&self, duration: &Duration) -> Result<Box<Self>, TemporalError> {
            self.0
                .subtract(&duration.0)
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }
        pub fn add_time_duration(
            &self,
            duration: &TimeDuration,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .add_time_duration(&duration.0)
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }
        pub fn subtract_time_duration(
            &self,
            duration: &TimeDuration,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .subtract_time_duration(&duration.0)
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
        pub fn round(
            &self,
            smallest_unit: TemporalUnit,
            rounding_increment: Option<f64>,
            rounding_mode: Option<TemporalRoundingMode>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .round(
                    smallest_unit.into(),
                    rounding_increment,
                    rounding_mode.map(Into::into),
                )
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }
        pub fn to_ixdtf_string(
            &self,
            options: ToStringRoundingOptions,
            write: &mut DiplomatWrite,
        ) -> Result<(), TemporalError> {
            // TODO this double-allocates, an API returning a Writeable or impl Write would be better
            let string = self.0.to_ixdtf_string(options.into())?;
            // throw away the error, the write itself should always succeed
            let _ = write.write_str(&string);

            Ok(())
        }
    }
}

impl From<ffi::PartialTime> for temporal_rs::partial::PartialTime {
    fn from(other: ffi::PartialTime) -> Self {
        Self {
            hour: other.hour.into(),
            minute: other.minute.into(),
            second: other.second.into(),
            millisecond: other.millisecond.into(),
            microsecond: other.microsecond.into(),
            nanosecond: other.nanosecond.into(),
        }
    }
}
