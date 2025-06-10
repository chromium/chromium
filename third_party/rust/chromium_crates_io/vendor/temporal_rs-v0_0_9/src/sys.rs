use crate::builtins::Now;
use crate::builtins::NowBuilder;
use crate::TemporalResult;

use crate::unix_time::EpochNanoseconds;
use crate::TemporalError;
use crate::TimeZone;
use alloc::string::ToString;
use web_time::{SystemTime, UNIX_EPOCH};

// TODO: Need to implement SystemTime handling for non_std.

// TODO: Look into and potentially implement a `SystemTime` struct allows
// providing closures or trait implementations that can then
// be used to construct [`Now`]. Basically `Temporal` but with
// traits or closures.
//
// Temporal could then be something like:
//
// pub struct Temporal(SystemTime<DefaultSystemClock, DefaultSystemTimeZone>)
//

#[cfg(feature = "sys")]
pub struct Temporal;

#[cfg(feature = "sys")]
impl Temporal {
    /// Returns a [`Now`] with the default system time and time zone.
    ///
    /// ## Panics
    ///
    /// This API can panic if reading the values from the system
    /// fails or the retreived values are not valid.
    ///
    /// For the non-panicking version of this API, see [`Self::try_now`].
    pub fn now() -> Now {
        Self::try_now().expect("failed to retrieve and validate system values.")
    }

    /// Returns a [`Now`] with the default system time and time zone.
    pub fn try_now() -> TemporalResult<Now> {
        Ok(NowBuilder::default()
            .with_system_zone(get_system_timezone()?)
            .with_system_nanoseconds(get_system_nanoseconds()?)
            .build())
    }
}

#[cfg(feature = "sys")]
#[inline]
pub(crate) fn get_system_timezone() -> TemporalResult<TimeZone> {
    iana_time_zone::get_timezone()
        .map(|s| TimeZone::try_from_identifier_str(&s))
        .map_err(|e| TemporalError::general(e.to_string()))?
}

/// Returns the system time in nanoseconds.
#[cfg(feature = "sys")]
pub(crate) fn get_system_nanoseconds() -> TemporalResult<EpochNanoseconds> {
    use crate::unix_time::EpochNanoseconds;

    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|e| TemporalError::general(e.to_string()))
        .map(|d| EpochNanoseconds::try_from(d.as_nanos()))?
}
