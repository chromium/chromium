use crate::{
    builtins::core::{PlainDateTime, ZonedDateTime},
    builtins::TZ_PROVIDER,
    options::Disambiguation,
    TemporalError, TemporalResult, TimeZone,
};

impl PlainDateTime {
    /// Returns a `ZonedDateTime` with the provided `PlainDateTime`, TimeZone` and
    /// `Disambiguation`
    /// Enable with the `compiled_data` feature flag.
    pub fn to_zoned_date_time(
        &self,
        time_zone: &TimeZone,
        disambiguation: Disambiguation,
    ) -> TemporalResult<ZonedDateTime> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;

        self.to_zoned_date_time_with_provider(time_zone, disambiguation, &*provider)
    }
}
