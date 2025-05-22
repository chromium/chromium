use crate::error::ffi::TemporalError;

#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {
    use crate::error::ffi::TemporalError;
    use alloc::boxed::Box;
    use alloc::string::String;
    use core::str::FromStr;
    use diplomat_runtime::DiplomatOption;
    use diplomat_runtime::{DiplomatStr, DiplomatStr16};
    use num_traits::FromPrimitive;

    #[diplomat::opaque]
    pub struct Duration(pub(crate) temporal_rs::Duration);

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    pub struct TimeDuration(pub(crate) temporal_rs::TimeDuration);

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    pub struct DateDuration(pub(crate) temporal_rs::DateDuration);

    pub struct PartialDuration {
        pub years: DiplomatOption<i64>,
        pub months: DiplomatOption<i64>,
        pub weeks: DiplomatOption<i64>,
        pub days: DiplomatOption<i64>,
        pub hours: DiplomatOption<i64>,
        pub minutes: DiplomatOption<i64>,
        pub seconds: DiplomatOption<i64>,
        pub milliseconds: DiplomatOption<i64>,
        pub microseconds: DiplomatOption<f64>,
        pub nanoseconds: DiplomatOption<f64>,
    }

    #[diplomat::enum_convert(temporal_rs::Sign)]
    pub enum Sign {
        Positive = 1,
        Zero = 0,
        Negative = -1,
    }

    impl PartialDuration {
        pub fn is_empty(self) -> bool {
            temporal_rs::partial::PartialDuration::try_from(self)
                .map(|p| p.is_empty())
                .unwrap_or(false)
        }
    }

    impl TimeDuration {
        pub fn new(
            hours: i64,
            minutes: i64,
            seconds: i64,
            milliseconds: i64,
            microseconds: f64,
            nanoseconds: f64,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::TimeDuration::new(
                hours,
                minutes,
                seconds,
                milliseconds,
                i128::from_f64(microseconds).ok_or(TemporalError::range())?,
                i128::from_f64(nanoseconds).ok_or(TemporalError::range())?,
            )
            .map(|x| Box::new(TimeDuration(x)))
            .map_err(Into::into)
        }

        pub fn abs(&self) -> Box<Self> {
            Box::new(Self(self.0.abs()))
        }
        pub fn negated(&self) -> Box<Self> {
            Box::new(Self(self.0.negated()))
        }

        pub fn is_within_range(&self) -> bool {
            self.0.is_within_range()
        }
        pub fn sign(&self) -> Sign {
            self.0.sign().into()
        }
    }

    impl DateDuration {
        pub fn new(
            years: i64,
            months: i64,
            weeks: i64,
            days: i64,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::DateDuration::new(years, months, weeks, days)
                .map(|x| Box::new(DateDuration(x)))
                .map_err(Into::into)
        }

        pub fn abs(&self) -> Box<Self> {
            Box::new(Self(self.0.abs()))
        }
        pub fn negated(&self) -> Box<Self> {
            Box::new(Self(self.0.negated()))
        }

        pub fn sign(&self) -> Sign {
            self.0.sign().into()
        }
    }
    impl Duration {
        pub fn create(
            years: i64,
            months: i64,
            weeks: i64,
            days: i64,
            hours: i64,
            minutes: i64,
            seconds: i64,
            milliseconds: i64,
            microseconds: f64,
            nanoseconds: f64,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::Duration::new(
                years,
                months,
                weeks,
                days,
                hours,
                minutes,
                seconds,
                milliseconds,
                i128::from_f64(microseconds).ok_or(TemporalError::range())?,
                i128::from_f64(nanoseconds).ok_or(TemporalError::range())?,
            )
            .map(|x| Box::new(Duration(x)))
            .map_err(Into::into)
        }

        pub fn from_day_and_time(
            day: i64,
            time: &TimeDuration,
        ) -> Result<Box<Self>, TemporalError> {
            Ok(Box::new(Duration(
                temporal_rs::Duration::from_day_and_time(day, &time.0),
            )))
        }
        pub fn from_partial_duration(partial: PartialDuration) -> Result<Box<Self>, TemporalError> {
            temporal_rs::Duration::from_partial_duration(partial.try_into()?)
                .map(|x| Box::new(Duration(x)))
                .map_err(Into::into)
        }

        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::Duration::from_utf8(s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            temporal_rs::Duration::from_str(&s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn is_time_within_range(&self) -> bool {
            self.0.is_time_within_range()
        }

        pub fn time<'a>(&'a self) -> &'a TimeDuration {
            TimeDuration::transparent_convert(self.0.time())
        }
        pub fn date<'a>(&'a self) -> &'a DateDuration {
            DateDuration::transparent_convert(self.0.date())
        }

        // set_time_duration is NOT safe to expose over FFI if the date()/time() methods are available
        // Diplomat plans to make this a hard error.
        // If needed, implement it as with_time_duration(&self, TimeDuration) -> Self

        pub fn years(&self) -> i64 {
            self.0.years()
        }
        pub fn months(&self) -> i64 {
            self.0.months()
        }
        pub fn weeks(&self) -> i64 {
            self.0.weeks()
        }
        pub fn days(&self) -> i64 {
            self.0.days()
        }
        pub fn hours(&self) -> i64 {
            self.0.hours()
        }
        pub fn minutes(&self) -> i64 {
            self.0.minutes()
        }
        pub fn seconds(&self) -> i64 {
            self.0.seconds()
        }
        pub fn milliseconds(&self) -> i64 {
            self.0.milliseconds()
        }
        pub fn microseconds(&self) -> Option<f64> {
            f64::from_i128(self.0.microseconds())
        }
        pub fn nanoseconds(&self) -> Option<f64> {
            f64::from_i128(self.0.nanoseconds())
        }

        pub fn sign(&self) -> Sign {
            self.0.sign().into()
        }

        pub fn is_zero(&self) -> bool {
            self.0.is_zero()
        }

        pub fn abs(&self) -> Box<Self> {
            Box::new(Self(self.0.abs()))
        }
        pub fn negated(&self) -> Box<Self> {
            Box::new(Self(self.0.negated()))
        }

        pub fn add(&self, other: &Self) -> Result<Box<Self>, TemporalError> {
            self.0
                .add(&other.0)
                .map(|x| Box::new(Duration(x)))
                .map_err(Into::into)
        }

        pub fn subtract(&self, other: &Self) -> Result<Box<Self>, TemporalError> {
            self.0
                .subtract(&other.0)
                .map(|x| Box::new(Duration(x)))
                .map_err(Into::into)
        }

        // TODO round_with_provider (needs time zone stuff)
        // TODO total_with_provider (needs time zone stuff)
    }
}

impl TryFrom<ffi::PartialDuration> for temporal_rs::partial::PartialDuration {
    type Error = TemporalError;
    fn try_from(other: ffi::PartialDuration) -> Result<Self, TemporalError> {
        use num_traits::FromPrimitive;
        Ok(Self {
            years: other.years.into_option(),
            months: other.months.into_option(),
            weeks: other.weeks.into_option(),
            days: other.days.into_option(),
            hours: other.hours.into_option(),
            minutes: other.minutes.into_option(),
            seconds: other.seconds.into_option(),
            milliseconds: other.milliseconds.into_option(),
            microseconds: other
                .microseconds
                .into_option()
                .map(|v| i128::from_f64(v).ok_or(TemporalError::range()))
                .transpose()?,
            nanoseconds: other
                .nanoseconds
                .into_option()
                .map(|v| i128::from_f64(v).ok_or(TemporalError::range()))
                .transpose()?,
        })
    }
}
