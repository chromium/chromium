use crate::builtins::{
    core::{Now, PlainDate, PlainDateTime, PlainTime},
    TZ_PROVIDER,
};
use crate::{TemporalError, TemporalResult, TimeZone};

impl Now {
    /// Returns the current system time as a [`PlainDateTime`] with an optional
    /// [`TimeZone`].
    ///
    /// Enable with the `compiled_data` and `sys` feature flags.
    pub fn plain_date_time_iso(self, time_zone: Option<TimeZone>) -> TemporalResult<PlainDateTime> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.plain_date_time_iso_with_provider(time_zone, &*provider)
    }

    /// Returns the current system time as a [`PlainDate`] with an optional
    /// [`TimeZone`].
    ///
    /// Enable with the `compiled_data` and `sys` feature flags.
    pub fn plain_date_iso(self, time_zone: Option<TimeZone>) -> TemporalResult<PlainDate> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.plain_date_iso_with_provider(time_zone, &*provider)
    }

    /// Returns the current system time as a [`PlainTime`] with an optional
    /// [`TimeZone`].
    ///
    /// Enable with the `compiled_data` and `sys` feature flags.
    pub fn plain_time_iso(self, time_zone: Option<TimeZone>) -> TemporalResult<PlainTime> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.plain_time_with_provider(time_zone, &*provider)
    }
}
