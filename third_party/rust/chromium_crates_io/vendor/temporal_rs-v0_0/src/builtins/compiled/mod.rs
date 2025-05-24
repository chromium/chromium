//! This module implements native Rust wrappers for the Temporal builtins.

mod duration;
mod instant;
mod now;
mod plain_date_time;
mod zoneddatetime;

mod options {
    use crate::{builtins::TZ_PROVIDER, options::RelativeTo, TemporalError, TemporalResult};

    impl RelativeTo {
        pub fn try_from_str(source: &str) -> TemporalResult<Self> {
            let provider = TZ_PROVIDER
                .lock()
                .map_err(|_| TemporalError::general("Unable to acquire lock"))?;

            Self::try_from_str_with_provider(source, &*provider)
        }
    }
}
