//! The Temporal Now component

use crate::provider::TimeZoneProvider;
use crate::unix_time::EpochNanoseconds;
use crate::TemporalResult;
use crate::{iso::IsoDateTime, TemporalError};

use super::{
    calendar::Calendar, timezone::TimeZone, Instant, PlainDate, PlainDateTime, PlainTime,
    ZonedDateTime,
};

#[derive(Debug, Default)]
pub struct NowBuilder {
    clock: Option<EpochNanoseconds>,
    zone: Option<TimeZone>,
}

impl NowBuilder {
    pub fn with_system_nanoseconds(mut self, nanoseconds: EpochNanoseconds) -> Self {
        self.clock = Some(nanoseconds);
        self
    }

    pub fn with_system_zone(mut self, zone: TimeZone) -> Self {
        self.zone = Some(zone);
        self
    }

    pub fn build(self) -> Now {
        Now {
            clock: self.clock,
            zone: self.zone.unwrap_or_default(),
        }
    }
}

#[derive(Debug)]
pub struct Now {
    clock: Option<EpochNanoseconds>,
    zone: TimeZone,
}

impl Now {
    pub(crate) fn clock(self) -> TemporalResult<EpochNanoseconds> {
        self.clock
            .ok_or(TemporalError::general("system clock unavailable"))
    }

    pub(crate) fn system_datetime_with_provider(
        self,
        time_zone: Option<TimeZone>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<IsoDateTime> {
        let Now { clock, zone } = self;
        let system_nanoseconds = clock.ok_or(TemporalError::general("system clock unavailable"))?;
        let time_zone = time_zone.unwrap_or(zone);
        time_zone.get_iso_datetime_for(&Instant::from(system_nanoseconds), provider)
    }
}

impl Now {
    /// Converts the current [`Now`] into a [`TimeZone`].
    pub fn time_zone(self) -> TimeZone {
        self.zone
    }

    /// Converts the current [`Now`] into an [`Instant`].
    pub fn instant(self) -> TemporalResult<Instant> {
        Ok(Instant::from(self.clock()?))
    }

    /// Converts the current [`Now`] into an [`ZonedDateTime`] with an ISO8601 calendar.
    pub fn zoned_date_time_iso(self, time_zone: Option<TimeZone>) -> TemporalResult<ZonedDateTime> {
        let Now { clock, zone } = self;
        let system_nanoseconds = clock.ok_or(TemporalError::general("system clock unavailable"))?;
        let time_zone = time_zone.unwrap_or(zone);
        let instant = Instant::from(system_nanoseconds);
        Ok(ZonedDateTime::new_unchecked(
            instant,
            Calendar::ISO,
            time_zone,
        ))
    }
}

impl Now {
    /// Converts `Now` into the current system [`PlainDateTime`] with an ISO8601 calendar.
    ///
    /// When `TimeZone` is `None`, the value will default to the
    /// system time zone or UTC if the system zone is unavailable.
    pub fn plain_date_time_iso_with_provider(
        self,
        time_zone: Option<TimeZone>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<PlainDateTime> {
        let iso = self.system_datetime_with_provider(time_zone, provider)?;
        Ok(PlainDateTime::new_unchecked(iso, Calendar::ISO))
    }

    /// Converts `Now` into the current system [`PlainDate`] with an ISO8601 calendar.
    ///
    /// When `TimeZone` is `None`, the value will default to the
    /// system time zone or UTC if the system zone is unavailable.
    pub fn plain_date_iso_with_provider(
        self,
        time_zone: Option<TimeZone>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<PlainDate> {
        let iso = self.system_datetime_with_provider(time_zone, provider)?;
        Ok(PlainDate::new_unchecked(iso.date, Calendar::ISO))
    }

    /// Converts `Now` into the current system [`PlainTime`].
    ///
    /// When `TimeZone` is `None`, the value will default to the
    /// system time zone or UTC if the system zone is unavailable.
    pub fn plain_time_with_provider(
        self,
        time_zone: Option<TimeZone>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<PlainTime> {
        let iso = self.system_datetime_with_provider(time_zone, provider)?;
        Ok(PlainTime::new_unchecked(iso.time))
    }
}

#[cfg(test)]
mod tests {

    #[cfg(feature = "tzdb")]
    use crate::options::DifferenceSettings;
    #[cfg(feature = "tzdb")]
    use crate::unix_time::EpochNanoseconds;

    #[cfg(feature = "tzdb")]
    #[test]
    fn mocked_datetime() {
        use crate::{now::NowBuilder, tzdb::FsTzdbProvider, TimeZone};
        let provider = FsTzdbProvider::default();

        // 2025-03-11T10:47-06:00
        const TIME_BASE: u128 = 1_741_751_188_077_363_694;

        let cdt = TimeZone::try_from_identifier_str("-05:00").unwrap();
        let uschi = TimeZone::try_from_identifier_str("America/Chicago").unwrap();

        let base = EpochNanoseconds::try_from(TIME_BASE).unwrap();
        let now = NowBuilder::default()
            .with_system_nanoseconds(base)
            .with_system_zone(cdt.clone())
            .build();
        let cdt_datetime = now
            .plain_date_time_iso_with_provider(None, &provider)
            .unwrap();
        assert_eq!(cdt_datetime.year(), 2025);
        assert_eq!(cdt_datetime.month(), 3);
        assert_eq!(cdt_datetime.month_code().as_str(), "M03");
        assert_eq!(cdt_datetime.day(), 11);
        assert_eq!(cdt_datetime.hour(), 22);
        assert_eq!(cdt_datetime.minute(), 46);
        assert_eq!(cdt_datetime.second(), 28);
        assert_eq!(cdt_datetime.millisecond(), 77);
        assert_eq!(cdt_datetime.microsecond(), 363);
        assert_eq!(cdt_datetime.nanosecond(), 694);

        let now_cdt = NowBuilder::default()
            .with_system_nanoseconds(base)
            .with_system_zone(cdt.clone())
            .build();
        let uschi_datetime = now_cdt
            .plain_date_time_iso_with_provider(Some(uschi), &provider)
            .unwrap();
        assert_eq!(cdt_datetime, uschi_datetime);

        let plus_5_secs = TIME_BASE + (5 * 1_000_000_000);
        let plus_5_epoch = EpochNanoseconds::try_from(plus_5_secs).unwrap();
        let plus_5_now = NowBuilder::default()
            .with_system_nanoseconds(plus_5_epoch)
            .with_system_zone(cdt)
            .build();
        let plus_5_pdt = plus_5_now
            .plain_date_time_iso_with_provider(None, &provider)
            .unwrap();
        assert_eq!(plus_5_pdt.second(), 33);

        let duration = cdt_datetime
            .until(&plus_5_pdt, DifferenceSettings::default())
            .unwrap();
        assert_eq!(duration.hours(), 0);
        assert_eq!(duration.minutes(), 0);
        assert_eq!(duration.seconds(), 5);
        assert_eq!(duration.milliseconds(), 0);
    }

    #[cfg(all(feature = "tzdb", feature = "sys", feature = "compiled_data"))]
    #[test]
    fn now_datetime_test() {
        use crate::Temporal;
        use std::thread;
        use std::time::Duration as StdDuration;

        let sleep = 2;

        let before = Temporal::now().plain_date_time_iso(None).unwrap();
        thread::sleep(StdDuration::from_secs(sleep));
        let after = Temporal::now().plain_date_time_iso(None).unwrap();

        let diff = after.since(&before, DifferenceSettings::default()).unwrap();

        let sleep_base = sleep as i64;
        let tolerable_range = sleep_base..=sleep_base + 5;

        // We assert a tolerable range of sleep + 5 because std::thread::sleep
        // is only guaranteed to be >= the value to sleep. So to prevent sporadic
        // errors, we only assert a range.
        assert!(tolerable_range.contains(&diff.seconds()));
        assert_eq!(diff.hours(), 0);
        assert_eq!(diff.minutes(), 0);
    }
}
