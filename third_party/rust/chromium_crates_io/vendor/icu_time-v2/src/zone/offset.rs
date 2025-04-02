// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::str::FromStr;

use crate::provider::{MinutesSinceEpoch, TimeZoneOffsetsV1};
use crate::{Time, TimeZone};
use icu_calendar::Date;
use icu_calendar::Iso;
use icu_provider::prelude::*;

use displaydoc::Display;

/// The time zone offset was invalid. Must be within ±18:00:00.
#[derive(Display, Debug, Copy, Clone, PartialEq)]
#[allow(clippy::exhaustive_structs)]
pub struct InvalidOffsetError;

/// An offset from Coordinated Universal Time (UTC)
#[derive(Copy, Clone, Debug, PartialEq, Eq, Default, PartialOrd, Ord)]
pub struct UtcOffset(i32);

impl UtcOffset {
    /// Attempt to create a [`UtcOffset`] from a seconds input.
    ///
    /// Returns [`InvalidOffsetError`] if the seconds are out of bounds.
    pub fn try_from_seconds(seconds: i32) -> Result<Self, InvalidOffsetError> {
        if seconds.unsigned_abs() > 18 * 60 * 60 {
            Err(InvalidOffsetError)
        } else {
            Ok(Self(seconds))
        }
    }

    /// Creates a [`UtcOffset`] from eighths of an hour.
    ///
    /// This is chosen because eighths of an hour cover all current time zones
    /// and all values of i8 are within range of this type.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::time::zone::UtcOffset;
    ///
    /// assert_eq!(
    ///     UtcOffset::try_from_str("-0600").unwrap(),
    ///     UtcOffset::from_eighths_of_hour(-6 * 8),
    /// );
    /// ```
    pub const fn from_eighths_of_hour(eighths_of_hour: i8) -> Self {
        Self(eighths_of_hour as i32 * 450)
    }

    /// Creates a [`UtcOffset`] of zero.
    pub const fn zero() -> Self {
        Self(0)
    }

    /// Parse a [`UtcOffset`] from bytes.
    ///
    /// The offset must range from UTC-12 to UTC+14.
    ///
    /// The string must be an ISO-8601 time zone designator:
    /// e.g. Z
    /// e.g. +05
    /// e.g. +0500
    /// e.g. +05:00
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::time::zone::UtcOffset;
    ///
    /// let offset0: UtcOffset = UtcOffset::try_from_str("Z").unwrap();
    /// let offset1: UtcOffset = UtcOffset::try_from_str("+05").unwrap();
    /// let offset2: UtcOffset = UtcOffset::try_from_str("+0500").unwrap();
    /// let offset3: UtcOffset = UtcOffset::try_from_str("-05:00").unwrap();
    ///
    /// let offset_err0 =
    ///     UtcOffset::try_from_str("0500").expect_err("Invalid input");
    /// let offset_err1 =
    ///     UtcOffset::try_from_str("+05000").expect_err("Invalid input");
    ///
    /// assert_eq!(offset0.to_seconds(), 0);
    /// assert_eq!(offset1.to_seconds(), 18000);
    /// assert_eq!(offset2.to_seconds(), 18000);
    /// assert_eq!(offset3.to_seconds(), -18000);
    /// ```
    #[inline]
    pub fn try_from_str(s: &str) -> Result<Self, InvalidOffsetError> {
        Self::try_from_utf8(s.as_bytes())
    }

    /// See [`Self::try_from_str`]
    pub fn try_from_utf8(mut code_units: &[u8]) -> Result<Self, InvalidOffsetError> {
        fn try_get_time_component([tens, ones]: [u8; 2]) -> Option<i32> {
            Some(((tens as char).to_digit(10)? * 10 + (ones as char).to_digit(10)?) as i32)
        }

        let offset_sign = match code_units {
            [b'+', rest @ ..] => {
                code_units = rest;
                1
            }
            [b'-', rest @ ..] => {
                code_units = rest;
                -1
            }
            // Unicode minus ("\u{2212}" == [226, 136, 146])
            [226, 136, 146, rest @ ..] => {
                code_units = rest;
                -1
            }
            [b'Z'] => return Ok(Self(0)),
            _ => return Err(InvalidOffsetError),
        };

        let hours = match code_units {
            &[h1, h2, ..] => try_get_time_component([h1, h2]),
            _ => None,
        }
        .ok_or(InvalidOffsetError)?;

        let minutes = match code_units {
            /* ±hh */
            &[_, _] => Some(0),
            /* ±hhmm, ±hh:mm */
            &[_, _, m1, m2] | &[_, _, b':', m1, m2] => {
                try_get_time_component([m1, m2]).filter(|&m| m < 60)
            }
            _ => None,
        }
        .ok_or(InvalidOffsetError)?;

        Self::try_from_seconds(offset_sign * (hours * 60 + minutes) * 60)
    }

    /// Create a [`UtcOffset`] from a seconds input without checking bounds.
    #[inline]
    pub fn from_seconds_unchecked(seconds: i32) -> Self {
        Self(seconds)
    }

    /// Returns the raw offset value in seconds.
    pub fn to_seconds(self) -> i32 {
        self.0
    }

    /// Returns the raw offset value in eights of an hour (7.5 minute units).
    pub fn to_eighths_of_hour(self) -> i8 {
        (self.0 / 450) as i8
    }

    /// Whether the [`UtcOffset`] is non-negative.
    pub fn is_non_negative(self) -> bool {
        self.0 >= 0
    }

    /// Whether the [`UtcOffset`] is zero.
    pub fn is_zero(self) -> bool {
        self.0 == 0
    }

    /// Returns the hours part of if the [`UtcOffset`]
    pub fn hours_part(self) -> i32 {
        self.0 / 3600
    }

    /// Returns the minutes part of if the [`UtcOffset`].
    pub fn minutes_part(self) -> u32 {
        (self.0 % 3600 / 60).unsigned_abs()
    }

    /// Returns the seconds part of if the [`UtcOffset`].
    pub fn seconds_part(self) -> u32 {
        (self.0 % 60).unsigned_abs()
    }
}

impl FromStr for UtcOffset {
    type Err = InvalidOffsetError;

    #[inline]
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::try_from_str(s)
    }
}

/// [`UtcOffsetCalculator`] uses data from the [data provider] to calculate time zone offsets.
///
/// [data provider]: icu_provider
#[derive(Debug)]
pub struct UtcOffsetCalculator {
    pub(super) offset_period: DataPayload<TimeZoneOffsetsV1>,
}

#[cfg(feature = "compiled_data")]
impl Default for UtcOffsetCalculator {
    fn default() -> Self {
        Self::new()
    }
}

impl UtcOffsetCalculator {
    /// Constructs a `UtcOffsetCalculator` using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    #[inline]
    pub const fn new() -> Self {
        UtcOffsetCalculator {
            offset_period: DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_TIME_ZONE_OFFSETS_V1,
            ),
        }
    }

    icu_provider::gen_buffer_data_constructors!(() -> error: DataError,
        functions: [
            new: skip,
                        try_new_with_buffer_provider,
            try_new_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable(
        provider: &(impl DataProvider<TimeZoneOffsetsV1> + ?Sized),
    ) -> Result<Self, DataError> {
        let metazone_period = provider.load(Default::default())?.payload;
        Ok(Self {
            offset_period: metazone_period,
        })
    }

    /// Calculate zone offsets from timezone and local datetime.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::Date;
    /// use icu::time::zone::UtcOffset;
    /// use icu::time::zone::UtcOffsetCalculator;
    /// use icu::time::Time;
    /// use icu::time::TimeZone;
    /// use tinystr::tinystr;
    ///
    /// let zoc = UtcOffsetCalculator::new();
    ///
    /// // America/Denver observes DST
    /// let offsets = zoc
    ///     .compute_offsets_from_time_zone(
    ///         TimeZone(tinystr!(8, "usden")),
    ///         (Date::try_new_iso(2024, 1, 1).unwrap(), Time::midnight()),
    ///     )
    ///     .unwrap();
    /// assert_eq!(
    ///     offsets.standard,
    ///     UtcOffset::try_from_seconds(-7 * 3600).unwrap()
    /// );
    /// assert_eq!(
    ///     offsets.daylight,
    ///     Some(UtcOffset::try_from_seconds(-6 * 3600).unwrap())
    /// );
    ///
    /// // America/Phoenix does not
    /// let offsets = zoc
    ///     .compute_offsets_from_time_zone(
    ///         TimeZone(tinystr!(8, "usphx")),
    ///         (Date::try_new_iso(2024, 1, 1).unwrap(), Time::midnight()),
    ///     )
    ///     .unwrap();
    /// assert_eq!(
    ///     offsets.standard,
    ///     UtcOffset::try_from_seconds(-7 * 3600).unwrap()
    /// );
    /// assert_eq!(offsets.daylight, None);
    /// ```
    pub fn compute_offsets_from_time_zone(
        &self,
        time_zone_id: TimeZone,
        dt: (Date<Iso>, Time),
    ) -> Option<UtcOffsets> {
        use zerovec::ule::AsULE;
        match self.offset_period.get().get0(&time_zone_id) {
            Some(cursor) => {
                let mut offsets = None;
                let minutes_since_epoch_walltime = MinutesSinceEpoch::from(dt);
                for (minutes, id) in cursor.iter1_copied() {
                    if minutes_since_epoch_walltime >= MinutesSinceEpoch::from_unaligned(*minutes) {
                        offsets = Some(id);
                    } else {
                        break;
                    }
                }
                let offsets = offsets?;
                Some(UtcOffsets {
                    standard: UtcOffset::from_eighths_of_hour(offsets.0),
                    daylight: (offsets.1 != 0)
                        .then_some(UtcOffset::from_eighths_of_hour(offsets.0 + offsets.1)),
                })
            }
            None => None,
        }
    }
}

/// Represents the different offsets in use for a time zone
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, PartialOrd, Eq, Ord)]
pub struct UtcOffsets {
    /// The standard offset.
    pub standard: UtcOffset,
    /// The daylight-saving offset, if used.
    pub daylight: Option<UtcOffset>,
}
