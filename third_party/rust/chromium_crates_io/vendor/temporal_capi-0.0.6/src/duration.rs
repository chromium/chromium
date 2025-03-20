use crate::error::ffi::TemporalError;

#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {
    use crate::error::ffi::TemporalError;
    use diplomat_runtime::DiplomatOption;

    #[diplomat::opaque]
    pub struct Duration(pub(crate) temporal_rs::Duration);

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    pub struct TimeDuration(pub(crate) temporal_rs::TimeDuration);

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    pub struct DateDuration(pub(crate) temporal_rs::DateDuration);

    pub struct PartialDuration {
        pub years: DiplomatOption<f64>,
        pub months: DiplomatOption<f64>,
        pub weeks: DiplomatOption<f64>,
        pub days: DiplomatOption<f64>,
        pub hours: DiplomatOption<f64>,
        pub minutes: DiplomatOption<f64>,
        pub seconds: DiplomatOption<f64>,
        pub milliseconds: DiplomatOption<f64>,
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
            hours: f64,
            minutes: f64,
            seconds: f64,
            milliseconds: f64,
            microseconds: f64,
            nanoseconds: f64,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::TimeDuration::new(
                hours.try_into()?,
                minutes.try_into()?,
                seconds.try_into()?,
                milliseconds.try_into()?,
                microseconds.try_into()?,
                nanoseconds.try_into()?,
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
            years: f64,
            months: f64,
            weeks: f64,
            days: f64,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::DateDuration::new(
                years.try_into()?,
                months.try_into()?,
                weeks.try_into()?,
                days.try_into()?,
            )
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
            years: f64,
            months: f64,
            weeks: f64,
            days: f64,
            hours: f64,
            minutes: f64,
            seconds: f64,
            milliseconds: f64,
            microseconds: f64,
            nanoseconds: f64,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::Duration::new(
                years.try_into()?,
                months.try_into()?,
                weeks.try_into()?,
                days.try_into()?,
                hours.try_into()?,
                minutes.try_into()?,
                seconds.try_into()?,
                milliseconds.try_into()?,
                microseconds.try_into()?,
                nanoseconds.try_into()?,
            )
            .map(|x| Box::new(Duration(x)))
            .map_err(Into::into)
        }

        pub fn from_day_and_time(
            day: f64,
            time: &TimeDuration,
        ) -> Result<Box<Self>, TemporalError> {
            Ok(Box::new(Duration(
                temporal_rs::Duration::from_day_and_time(day.try_into()?, &time.0),
            )))
        }
        pub fn from_partial_duration(partial: PartialDuration) -> Result<Box<Self>, TemporalError> {
            temporal_rs::Duration::from_partial_duration(partial.try_into()?)
                .map(|x| Box::new(Duration(x)))
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

        pub fn years(&self) -> f64 {
            self.0.years().as_inner()
        }
        pub fn months(&self) -> f64 {
            self.0.months().as_inner()
        }
        pub fn weeks(&self) -> f64 {
            self.0.weeks().as_inner()
        }
        pub fn days(&self) -> f64 {
            self.0.days().as_inner()
        }
        pub fn hours(&self) -> f64 {
            self.0.hours().as_inner()
        }
        pub fn minutes(&self) -> f64 {
            self.0.minutes().as_inner()
        }
        pub fn seconds(&self) -> f64 {
            self.0.seconds().as_inner()
        }
        pub fn milliseconds(&self) -> f64 {
            self.0.milliseconds().as_inner()
        }
        pub fn microseconds(&self) -> f64 {
            self.0.microseconds().as_inner()
        }
        pub fn nanoseconds(&self) -> f64 {
            self.0.nanoseconds().as_inner()
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
        Ok(Self {
            years: other
                .years
                .into_option()
                .map(TryFrom::try_from)
                .transpose()?,
            months: other
                .months
                .into_option()
                .map(TryFrom::try_from)
                .transpose()?,
            weeks: other
                .weeks
                .into_option()
                .map(TryFrom::try_from)
                .transpose()?,
            days: other
                .days
                .into_option()
                .map(TryFrom::try_from)
                .transpose()?,
            hours: other
                .hours
                .into_option()
                .map(TryFrom::try_from)
                .transpose()?,
            minutes: other
                .minutes
                .into_option()
                .map(TryFrom::try_from)
                .transpose()?,
            seconds: other
                .seconds
                .into_option()
                .map(TryFrom::try_from)
                .transpose()?,
            milliseconds: other
                .milliseconds
                .into_option()
                .map(TryFrom::try_from)
                .transpose()?,
            microseconds: other
                .microseconds
                .into_option()
                .map(TryFrom::try_from)
                .transpose()?,
            nanoseconds: other
                .nanoseconds
                .into_option()
                .map(TryFrom::try_from)
                .transpose()?,
        })
    }
}
