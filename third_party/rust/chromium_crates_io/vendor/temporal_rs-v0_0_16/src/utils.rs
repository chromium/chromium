//! Utility date and time equations for Temporal

use alloc::format;
use alloc::string::String;

pub(crate) use timezone_provider::utils::epoch_days_from_gregorian_date;

// NOTE: Potentially add more of tests.

// ==== Begin Date Equations ====

pub(crate) const MS_PER_HOUR: i64 = 3_600_000;
pub(crate) const MS_PER_MINUTE: i64 = 60_000;

pub(crate) use timezone_provider::utils::{
    epoch_days_to_epoch_ms, iso_days_in_month, ymd_from_epoch_milliseconds,
};

/// 3.5.11 PadISOYear ( y )
///
/// returns a String representation of y suitable for inclusion in an ISO 8601 string
pub(crate) fn pad_iso_year(year: i32) -> String {
    if (0..9999).contains(&year) {
        return format!("{year:04}");
    }
    let year_sign = if year > 0 { "+" } else { "-" };
    let year_string = format!("{:06}", year.abs());
    format!("{year_sign}{year_string}",)
}

// ==== End Calendar Equations ====
