use alloc::string::String;

use crate::TemporalResult;

use crate::TemporalError;
use alloc::string::ToString;
use web_time::{SystemTime, UNIX_EPOCH};

// TODO: Need to implement SystemTime handling for non_std.

#[inline]
pub(crate) fn get_system_timezone() -> TemporalResult<String> {
    iana_time_zone::get_timezone().map_err(|e| TemporalError::general(e.to_string()))
}

/// Returns the system time in nanoseconds.
#[cfg(feature = "sys")]
pub(crate) fn get_system_nanoseconds() -> TemporalResult<u128> {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|e| TemporalError::general(e.to_string()))
        .map(|d| d.as_nanos())
}
