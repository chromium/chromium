//! The Temporal Now component

use crate::iso::IsoDateTime;
use crate::provider::TimeZoneProvider;
use crate::time::EpochNanoseconds;
use crate::TemporalResult;

#[cfg(feature = "sys")]
use alloc::string::String;

use super::{
    calendar::Calendar, timezone::TimeZone, Instant, PlainDate, PlainDateTime, PlainTime,
    ZonedDateTime,
};

/// The Temporal Now object.
pub struct Now;

impl Now {
    /// Returns the current system `DateTime` based off the provided system args
    ///
    /// ## Order of operations
    ///
    /// The order of operations for this method requires the `GetSystemTimeZone` call
    /// to occur prior to calling system time and resolving the `EpochNanoseconds`
    /// value.
    ///
    /// A correct implementation will follow the following steps:
    ///
    ///   1. Resolve user input `TimeZone` with the `SystemTimeZone`.
    ///   2. Get the `SystemNanoseconds`
    pub(crate) fn system_datetime_with_provider(
        system_epoch_nanoseconds: EpochNanoseconds,
        system_timezone: TimeZone,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<IsoDateTime> {
        // 1. If temporalTimeZoneLike is undefined, then
        // a. Let timeZone be SystemTimeZoneIdentifier().
        // 2. Else,
        // a. Let timeZone be ? ToTemporalTimeZoneIdentifier(temporalTimeZoneLike).
        // 3. Let epochNs be SystemUTCEpochNanoseconds().
        // 4. Return GetISODateTimeFor(timeZone, epochNs).
        system_timezone.get_iso_datetime_for(&Instant::from(system_epoch_nanoseconds), provider)
    }

    /// Returns the current system time as a `ZonedDateTime` with an ISO8601 calendar.
    ///
    /// The time zone will be set to either the `TimeZone` if a value is provided, or
    /// according to the system timezone if no value is provided.
    ///
    /// ## Order of operations
    ///
    /// The order of operations for this method requires the `GetSystemTimeZone` call
    /// to occur prior to calling system time and resolving the `EpochNanoseconds`
    /// value.
    ///
    /// A correct implementation will follow the following steps:
    ///
    ///   1. Resolve user input `TimeZone` with the `SystemTimeZone`.
    ///   2. Get the `SystemNanoseconds`
    ///
    /// For an example implementation, see `Now::zoneddatetime_iso`; available with
    /// the `compiled_data` feature flag.
    pub fn zoneddatetime_iso_with_system_info(
        sys_epoch_nanos: EpochNanoseconds,
        sys_timezone: TimeZone,
    ) -> TemporalResult<ZonedDateTime> {
        let instant = Instant::from(sys_epoch_nanos);
        Ok(ZonedDateTime::new_unchecked(
            instant,
            Calendar::default(),
            sys_timezone,
        ))
    }
}

#[cfg(feature = "sys")]
impl Now {
    /// Returns the current instant
    ///
    /// Enable with the `sys` feature flag.
    pub fn instant() -> TemporalResult<Instant> {
        let system_nanos = crate::sys::get_system_nanoseconds()?;
        let epoch_nanos = EpochNanoseconds::try_from(system_nanos)?;
        Ok(Instant::from(epoch_nanos))
    }

    /// Returns the current time zone.
    ///
    /// Enable with the `sys` feature flag.
    pub fn time_zone_identifier() -> TemporalResult<String> {
        crate::sys::get_system_timezone()
    }

    /// Returns the current system time as a [`PlainDateTime`] with an optional
    /// [`TimeZone`].
    ///
    /// Enable with the `sys` feature flag.
    pub fn zoneddatetime_iso(timezone: Option<TimeZone>) -> TemporalResult<ZonedDateTime> {
        let timezone =
            timezone.unwrap_or(TimeZone::IanaIdentifier(crate::sys::get_system_timezone()?));
        let system_nanos = crate::sys::get_system_nanoseconds()?;
        let epoch_nanos = EpochNanoseconds::try_from(system_nanos)?;
        Now::zoneddatetime_iso_with_system_info(epoch_nanos, timezone)
    }
}

impl Now {
    /// Returns the current system time as a `PlainDateTime` with an ISO8601 calendar.
    ///
    /// ## Order of operations
    ///
    /// The order of operations for this method requires the `GetSystemTimeZone` call
    /// to occur prior to calling system time and resolving the `EpochNanoseconds`
    /// value.
    ///
    /// A correct implementation will follow the following steps:
    ///
    ///   1. Resolve user input `TimeZone` with the `SystemTimeZone`.
    ///   2. Get the `SystemNanoseconds`
    ///
    /// For an example implementation, see `Now::plain_datetime_iso`; available with the
    /// `compiled_data` feature flag.
    pub fn plain_datetime_iso_with_provider_and_system_info(
        sys_epoch_nanos: EpochNanoseconds,
        sys_timezone: TimeZone,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<PlainDateTime> {
        let iso = Self::system_datetime_with_provider(sys_epoch_nanos, sys_timezone, provider)?;
        Ok(PlainDateTime::new_unchecked(iso, Calendar::default()))
    }

    /// Returns the current system time as a `PlainDate` with an ISO8601 calendar.
    ///
    /// ## Order of operations
    ///
    /// The order of operations for this method requires the `GetSystemTimeZone` call
    /// to occur prior to calling system time and resolving the `EpochNanoseconds`
    /// value.
    ///
    /// A correct implementation will follow the following steps:
    ///
    ///   1. Resolve user input `TimeZone` with the `SystemTimeZone`.
    ///   2. Get the `SystemNanoseconds`
    ///
    /// For an example implementation, see `Now::plain_date_iso`; available
    /// with the `compiled_data` feature flag.
    pub fn plain_date_iso_with_provider_and_system_info(
        sys_epoch_nanos: EpochNanoseconds,
        sys_timezone: TimeZone,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<PlainDate> {
        let iso = Self::system_datetime_with_provider(sys_epoch_nanos, sys_timezone, provider)?;
        Ok(PlainDate::new_unchecked(iso.date, Calendar::default()))
    }

    /// Returns the current system time as a `PlainTime` according to an ISO8601 calendar.
    ///
    /// ## Order of operations
    ///
    /// The order of operations for this method requires the `GetSystemTimeZone` call
    /// to occur prior to calling system time and resolving the `EpochNanoseconds`
    /// value.
    ///
    /// A correct implementation will follow the following steps:
    ///
    ///   1. Resolve user input `TimeZone` with the `SystemTimeZone`.
    ///   2. Get the `SystemNanoseconds`
    ///
    /// For an example implementation, see `Now::plain_time_iso`; available with the
    /// `compiled_data` feature flag.
    pub fn plain_time_iso_with_provider_and_system_info(
        sys_epoch_nanos: EpochNanoseconds,
        sys_timezone: TimeZone,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<PlainTime> {
        let iso = Self::system_datetime_with_provider(sys_epoch_nanos, sys_timezone, provider)?;
        Ok(PlainTime::new_unchecked(iso.time))
    }
}

#[cfg(test)]
mod tests {

    #[cfg(feature = "tzdb")]
    use crate::builtins::core::Now;
    #[cfg(feature = "tzdb")]
    use crate::options::DifferenceSettings;
    #[cfg(feature = "tzdb")]
    use crate::time::EpochNanoseconds;

    #[cfg(feature = "tzdb")]
    #[test]
    fn mocked_datetime() {
        use crate::{tzdb::FsTzdbProvider, TimeZone};
        let provider = FsTzdbProvider::default();

        // 2025-03-11T10:47-06:00
        const TIME_BASE: u128 = 1_741_751_188_077_363_694;

        let cdt = TimeZone::try_from_identifier_str("-05:00").unwrap();
        let uschi = TimeZone::try_from_identifier_str("America/Chicago").unwrap();

        let base = EpochNanoseconds::try_from(TIME_BASE).unwrap();
        let now =
            Now::plain_datetime_iso_with_provider_and_system_info(base, cdt.clone(), &provider)
                .unwrap();
        assert_eq!(now.year(), 2025);
        assert_eq!(now.month(), 3);
        assert_eq!(now.month_code().as_str(), "M03");
        assert_eq!(now.day(), 11);
        assert_eq!(now.hour(), 22);
        assert_eq!(now.minute(), 46);
        assert_eq!(now.second(), 28);
        assert_eq!(now.millisecond(), 77);
        assert_eq!(now.microsecond(), 363);
        assert_eq!(now.nanosecond(), 694);

        let now_iana =
            Now::plain_datetime_iso_with_provider_and_system_info(base, uschi.clone(), &provider)
                .unwrap();
        assert_eq!(now, now_iana);

        let plus_5_secs = TIME_BASE + (5 * 1_000_000_000);
        let plus_5_epoch = EpochNanoseconds::try_from(plus_5_secs).unwrap();
        let now_plus_5 =
            Now::plain_datetime_iso_with_provider_and_system_info(plus_5_epoch, cdt, &provider)
                .unwrap();
        assert_eq!(now_plus_5.second(), 33);

        let duration = now
            .until(&now_plus_5, DifferenceSettings::default())
            .unwrap();
        assert!(duration.hours().is_zero());
        assert!(duration.minutes().is_zero());
        assert_eq!(duration.seconds().as_inner(), 5.0);
        assert!(duration.milliseconds().is_zero());
    }

    #[cfg(all(feature = "tzdb", feature = "sys", feature = "compiled_data"))]
    #[test]
    fn now_datetime_test() {
        use std::thread;
        use std::time::Duration as StdDuration;

        let sleep = 2;

        let before = Now::plain_datetime_iso(None).unwrap();
        thread::sleep(StdDuration::from_secs(sleep));
        let after = Now::plain_datetime_iso(None).unwrap();

        let diff = after.since(&before, DifferenceSettings::default()).unwrap();

        let sleep_base = sleep as f64;
        let tolerable_range = sleep_base..=sleep_base + 5.0;

        // We assert a tolerable range of sleep + 5 because std::thread::sleep
        // is only guaranteed to be >= the value to sleep. So to prevent sporadic
        // errors, we only assert a range.
        assert!(tolerable_range.contains(&diff.seconds().as_inner()));
        assert!(diff.hours().is_zero());
        assert!(diff.minutes().is_zero());
    }
}
