use crate::{builtins::TZ_PROVIDER, TemporalError, TemporalResult, PlainDate, PlainTime};

impl PlainDate {

    /// Converts a `Date` to a `ZonedDateTime` in the UTC time zone.
    pub fn to_zoned_date_time(
        &self, 
        time_zone: TimeZone,
        plain_time: Option<PlainTime>
    ) -> TemporalResult<crate::ZonedDateTime> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.to_zoned_date_time_with_provider(time_zone, plain_time, &*provider)
    }
}