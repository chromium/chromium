use crate::{
    builtins::TZ_PROVIDER,
    options::{RelativeTo, RoundingOptions, Unit},
    primitive::FiniteF64,
    Duration, TemporalError, TemporalResult,
};

use core::cmp::Ordering;

#[cfg(test)]
mod tests;

impl Duration {
    /// Rounds the current [`Duration`] according to the provided [`RoundingOptions`] and an optional
    /// [`RelativeTo`]
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn round(
        &self,
        options: RoundingOptions,
        relative_to: Option<RelativeTo>,
    ) -> TemporalResult<Self> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.round_with_provider(options, relative_to, &*provider)
    }

    /// Returns the ordering between two [`Duration`], takes an optional
    /// [`RelativeTo`]
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn compare(
        &self,
        two: &Duration,
        relative_to: Option<RelativeTo>,
    ) -> TemporalResult<Ordering> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.compare_with_provider(two, relative_to, &*provider)
    }

    pub fn total(&self, unit: Unit, relative_to: Option<RelativeTo>) -> TemporalResult<FiniteF64> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.total_with_provider(unit, relative_to, &*provider)
    }
}
