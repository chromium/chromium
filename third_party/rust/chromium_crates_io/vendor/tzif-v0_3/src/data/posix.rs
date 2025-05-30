// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::time::Seconds;

/// A struct to hold a time-zone variant name and its offset.
/// The offset is how many hours must be added to the time to reach UTC.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ZoneVariantInfo {
    /// The name of the time-zone variant.
    pub name: String,
    /// The offset time in seconds that must be added to reach UTC.
    pub offset: Seconds,
}

/// A struct to hold a DST transition date.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TransitionDate {
    /// The day on which the transition occurrs.
    pub day: TransitionDay,
    /// The time in seconds in which the transition occurrs.
    pub time: Seconds,
}

/// A struct for defining a DST transition day.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TransitionDay {
    /// The day of the year, ignoring Feb. 29 on leap years. Designated by `Jn`.
    ///
    /// Ranges from [1, 365]
    NoLeap(u16),

    /// The day of the year, accounting for Feb. 29 on leap years. Designated by `n`.
    ///
    /// Ranges from [0, 365]
    WithLeap(u16),

    /// The month, week, day value. Designated by `M.w.d`.
    ///
    /// `M` ranges from [1, 12].
    ///
    /// `w` ranges from [1, 5].
    ///
    /// `d` ranges from [0, 6].
    Mwd(u16, u16, u16),
}

/// A struct for holding DST transition info.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DstTransitionInfo {
    /// The zone variant info including name and offset.
    pub variant_info: ZoneVariantInfo,

    /// The DST transition start date.
    pub start_date: TransitionDate,

    /// The DST transition end date.
    pub end_date: TransitionDate,
}

/// A struct for holding data encoded by a POSIX time-zone string.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PosixTzString {
    /// The variant info of the STD time-zone variant.
    pub std_info: ZoneVariantInfo,

    /// The variant info of the DST time-zone variant if present.
    pub dst_info: Option<DstTransitionInfo>,
}
