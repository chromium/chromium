#![cfg(feature = "sys")]

use crate::builtins::{
    core::{Now, PlainDate, PlainDateTime, PlainTime},
    TZ_PROVIDER,
};
use crate::sys;
use crate::{time::EpochNanoseconds, TemporalError, TemporalResult, TimeZone};

impl Now {
    /// Returns the current system time as a [`PlainDateTime`] with an optional
    /// [`TimeZone`].
    ///
    /// Enable with the `compiled_data` and `sys` feature flags.
    pub fn plain_datetime_iso(timezone: Option<TimeZone>) -> TemporalResult<PlainDateTime> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        let timezone = timezone.unwrap_or(TimeZone::IanaIdentifier(sys::get_system_timezone()?));
        let system_nanos = sys::get_system_nanoseconds()?;
        let epoch_nanos = EpochNanoseconds::try_from(system_nanos)?;
        Now::plain_datetime_iso_with_provider_and_system_info(epoch_nanos, timezone, &*provider)
    }

    /// Returns the current system time as a [`PlainDate`] with an optional
    /// [`TimeZone`].
    ///
    /// Enable with the `compiled_data` and `sys` feature flags.
    pub fn plain_date_iso(timezone: Option<TimeZone>) -> TemporalResult<PlainDate> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        let timezone = timezone.unwrap_or(TimeZone::IanaIdentifier(sys::get_system_timezone()?));
        let system_nanos = sys::get_system_nanoseconds()?;
        let epoch_nanos = EpochNanoseconds::try_from(system_nanos)?;
        Now::plain_date_iso_with_provider_and_system_info(epoch_nanos, timezone, &*provider)
    }

    /// Returns the current system time as a [`PlainTime`] with an optional
    /// [`TimeZone`].
    ///
    /// Enable with the `compiled_data` and `sys` feature flags.
    pub fn plain_time_iso(timezone: Option<TimeZone>) -> TemporalResult<PlainTime> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        let timezone = timezone.unwrap_or(TimeZone::IanaIdentifier(sys::get_system_timezone()?));
        let system_nanos = sys::get_system_nanoseconds()?;
        let epoch_nanos = EpochNanoseconds::try_from(system_nanos)?;
        Now::plain_time_iso_with_provider_and_system_info(epoch_nanos, timezone, &*provider)
    }
}
