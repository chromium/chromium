// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 🚧 \[Unstable\] Data provider struct definitions for chinese-based calendars.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]

use crate::calendar_arithmetic::ArithmeticDate;
use crate::chinese_based::ChineseBasedYearInfo;
use crate::Iso;
use calendrical_calculations::chinese_based::ChineseBased;
use calendrical_calculations::rata_die::RataDie;
use core::num::NonZeroU8;
use icu_provider::prelude::*;
use zerovec::ule::{AsULE, ULE};
use zerovec::ZeroVec;

icu_provider::data_marker!(
    /// Precomputed data for the Chinese calendar
    CalendarChineseV1,
    "calendar/chinese/v1",
    ChineseBasedCache<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// Precomputed data for the Dangi calendar
    CalendarDangiV1,
    "calendar/dangi/v1",
    ChineseBasedCache<'static>,
    is_singleton = true
);

/// Cached/precompiled data for a certain range of years for a chinese-based
/// calendar. Avoids the need to perform lunar calendar arithmetic for most calendrical
/// operations.
#[derive(Debug, PartialEq, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_calendar::provider::chinese_based))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct ChineseBasedCache<'data> {
    /// The extended year corresponding to the first data entry for this year
    pub first_extended_year: i32,
    /// A list of precomputed data for each year beginning with first_extended_year
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub data: ZeroVec<'data, PackedChineseBasedYearInfo>,
}

icu_provider::data_struct!(
    ChineseBasedCache<'_>,
    #[cfg(feature = "datagen")]
);

impl ChineseBasedCache<'_> {
    /// Compute this data for a range of years
    #[cfg(feature = "datagen")]
    pub fn compute_for<CB: ChineseBased>(extended_years: core::ops::Range<i32>) -> Self {
        let data = crate::chinese_based::compute_many_packed::<CB>(extended_years.clone());
        ChineseBasedCache {
            first_extended_year: extended_years.start,
            data: data.into(),
        }
    }

    /// Get the cached data for a given extended year
    pub(crate) fn get_for_extended_year(&self, extended_year: i32) -> Option<ChineseBasedYearInfo> {
        let delta = extended_year - self.first_extended_year;
        let delta = usize::try_from(delta).ok()?;

        if delta == 0 {
            return None;
        }

        let (Some(this_packed), Some(prev_packed)) =
            (self.data.get(delta), self.data.get(delta - 1))
        else {
            return None;
        };

        let days_in_prev_year = prev_packed.days_in_year();

        Some(ChineseBasedYearInfo::new(days_in_prev_year, this_packed))
    }
    /// Get the cached data for the Chinese Year corresponding to a given day.
    ///
    /// Also returns the corresponding extended year.
    pub(crate) fn get_for_iso<CB: ChineseBased>(
        &self,
        iso: ArithmeticDate<Iso>,
    ) -> Option<(ChineseBasedYearInfo, i32)> {
        let extended_year = CB::extended_from_iso(iso.year);
        let delta = extended_year - self.first_extended_year;
        let delta = usize::try_from(delta).ok()?;
        if delta <= 1 {
            return None;
        }

        let this_packed = self.data.get(delta)?;
        let prev_packed = self.data.get(delta - 1)?;

        let iso_in_year = iso.day_of_year();
        let fetched_data_ny_in_iso = u16::from(this_packed.ny_day_of_iso_year());

        if iso_in_year >= fetched_data_ny_in_iso {
            Some((
                ChineseBasedYearInfo::new(prev_packed.days_in_year(), this_packed),
                extended_year,
            ))
        } else {
            // We're dealing with an ISO day in the beginning of the year, before Chinese New Year.
            // Return data for the previous Chinese year instead.
            if delta <= 2 {
                return None;
            }
            let prev2_packed = self.data.get(delta - 2)?;

            let days_in_prev_year = prev2_packed.days_in_year();

            Some((
                ChineseBasedYearInfo::new(days_in_prev_year, prev_packed),
                extended_year - 1,
            ))
        }
    }
}

/// The struct containing compiled ChineseData
///
/// Bit structure (little endian: note that shifts go in the opposite direction!)
///
/// ```text
/// Bit:             0   1   2   3   4   5   6   7
/// Byte 0:          [  month lengths .............
/// Byte 1:         .. month lengths ] | [ leap month index ..
/// Byte 2:          ] | [   NY offset       ] | unused
/// ```
///
/// Where the New Year Offset is the offset from ISO Jan 21 of that year for Chinese New Year,
/// the month lengths are stored as 1 = 30, 0 = 29 for each month including the leap month.
/// The largest possible offset is 33, which requires 6 bits of storage.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, ULE)]
#[cfg_attr(feature = "datagen", derive(databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_calendar::provider))]
#[repr(C, packed)]
pub struct PackedChineseBasedYearInfo(pub u8, pub u8, pub u8);

impl PackedChineseBasedYearInfo {
    /// The first day of the ISO year on which Chinese New Year may occur
    ///
    /// According to Reingold & Dershowitz, ch 19.6, Chinese New Year occurs on Jan 21 - Feb 21 inclusive.
    ///
    /// Chinese New Year in the year 30 AD is January 20 (30-01-20).
    ///
    /// We allow it to occur as early as January 19 which is the earliest the second new moon
    /// could occur after the Winter Solstice if the solstice is pinned to December 20.
    pub(crate) const FIRST_NY: u8 = 19;

    pub(crate) fn new(
        month_lengths: [bool; 13],
        leap_month_idx: Option<NonZeroU8>,
        ny_offset: u8,
    ) -> Self {
        debug_assert!(
            !month_lengths[12] || leap_month_idx.is_some(),
            "Last month length should not be set for non-leap years"
        );
        debug_assert!(ny_offset < 34, "Year offset too big to store");
        debug_assert!(
            leap_month_idx.map(|l| l.get() <= 13).unwrap_or(true),
            "Leap month indices must be 1 <= i <= 13"
        );
        let mut all = 0u32; // last byte unused

        for (month, length_30) in month_lengths.iter().enumerate() {
            #[allow(clippy::indexing_slicing)]
            if *length_30 {
                all |= 1 << month as u32;
            }
        }
        let leap_month_idx = leap_month_idx.map(|x| x.get()).unwrap_or(0);
        all |= (leap_month_idx as u32) << (8 + 5);
        all |= (ny_offset as u32) << (16 + 1);
        let le = all.to_le_bytes();
        Self(le[0], le[1], le[2])
    }

    // Get the new year offset from January 21
    pub(crate) fn ny_offset(self) -> u8 {
        self.2 >> 1
    }

    /// The day of the year (1-indexed) that this is in the ISO year
    fn ny_day_of_iso_year(self) -> u8 {
        let ny_offset = self.ny_offset();
        // FIRST_NY is one-indexed, offset is an offset, we can just add
        Self::FIRST_NY + ny_offset
    }

    pub(crate) fn ny_rd(self, related_iso: i32) -> RataDie {
        let iso_ny = calendrical_calculations::iso::fixed_from_iso(related_iso, 1, 1);
        // -1 because `iso_ny` is itself in the year, and ny_day_of_iso_year
        iso_ny + i64::from(self.ny_day_of_iso_year()) - 1
    }

    pub(crate) fn leap_month_idx(self) -> Option<NonZeroU8> {
        let low_bits = self.1 >> 5;
        let high_bits = (self.2 & 0b1) << 3;

        NonZeroU8::new(low_bits + high_bits)
    }

    // Whether a particular month has 30 days (month is 1-indexed)
    #[cfg(any(test, feature = "datagen"))]
    pub(crate) fn month_has_30_days(self, month: u8) -> bool {
        let months = u16::from_le_bytes([self.0, self.1]);
        months & (1 << (month - 1) as u16) != 0
    }

    // Which day of year is the last day of a month (month is 1-indexed)
    pub(crate) fn last_day_of_month(self, month: u8) -> u16 {
        let months = u16::from_le_bytes([self.0, self.1]);
        // month is 1-indexed, so `29 * month` includes the current month
        let mut prev_month_lengths = 29 * month as u16;
        // month is 1-indexed, so `1 << month` is a mask with all zeroes except
        // for a 1 at the bit index at the next month. Subtracting 1 from it gets us
        // a bitmask for all months up to now
        let long_month_bits = months & ((1 << month as u16) - 1);
        prev_month_lengths += long_month_bits.count_ones().try_into().unwrap_or(0);
        prev_month_lengths
    }

    pub(crate) fn days_in_year(self) -> u16 {
        if self.leap_month_idx().is_some() {
            self.last_day_of_month(13)
        } else {
            self.last_day_of_month(12)
        }
    }
}

impl AsULE for PackedChineseBasedYearInfo {
    type ULE = Self;
    fn to_unaligned(self) -> Self {
        self
    }
    fn from_unaligned(other: Self) -> Self {
        other
    }
}

#[cfg(feature = "serde")]
mod serialization {
    use super::*;

    #[cfg(feature = "datagen")]
    use serde::{ser, Serialize};
    use serde::{Deserialize, Deserializer};

    #[derive(Deserialize)]
    #[cfg_attr(feature = "datagen", derive(Serialize))]
    struct SerdePackedChineseBasedYearInfo {
        ny_offset: u8,
        month_has_30_days: [bool; 13],
        leap_month_idx: Option<NonZeroU8>,
    }

    impl<'de> Deserialize<'de> for PackedChineseBasedYearInfo {
        fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where
            D: Deserializer<'de>,
        {
            if deserializer.is_human_readable() {
                SerdePackedChineseBasedYearInfo::deserialize(deserializer).map(Into::into)
            } else {
                let data = <(u8, u8, u8)>::deserialize(deserializer)?;
                Ok(PackedChineseBasedYearInfo(data.0, data.1, data.2))
            }
        }
    }

    #[cfg(feature = "datagen")]
    impl Serialize for PackedChineseBasedYearInfo {
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: ser::Serializer,
        {
            if serializer.is_human_readable() {
                SerdePackedChineseBasedYearInfo::from(*self).serialize(serializer)
            } else {
                (self.0, self.1, self.2).serialize(serializer)
            }
        }
    }

    #[cfg(feature = "datagen")]
    impl From<PackedChineseBasedYearInfo> for SerdePackedChineseBasedYearInfo {
        fn from(other: PackedChineseBasedYearInfo) -> Self {
            let mut month_has_30_days = [false; 13];
            for (i, month) in month_has_30_days.iter_mut().enumerate() {
                *month = other.month_has_30_days(i as u8 + 1)
            }
            Self {
                ny_offset: other.ny_offset(),
                month_has_30_days,
                leap_month_idx: other.leap_month_idx(),
            }
        }
    }

    impl From<SerdePackedChineseBasedYearInfo> for PackedChineseBasedYearInfo {
        fn from(other: SerdePackedChineseBasedYearInfo) -> Self {
            Self::new(
                other.month_has_30_days,
                other.leap_month_idx,
                other.ny_offset,
            )
        }
    }
}
