// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::provider::{ZoneOffsetPeriodV1Marker, EPOCH};
use crate::{TimeZoneBcp47Id, UtcOffset};
use icu_calendar::Iso;
use icu_calendar::{Date, Time};
use icu_provider::prelude::*;

/// [`ZoneOffsetCalculator`] uses data from the [data provider] to calculate time zone offsets.
///
/// [data provider]: icu_provider
#[derive(Debug)]
pub struct ZoneOffsetCalculator {
    pub(super) offset_period: DataPayload<ZoneOffsetPeriodV1Marker>,
}

#[cfg(feature = "compiled_data")]
impl Default for ZoneOffsetCalculator {
    fn default() -> Self {
        Self::new()
    }
}

impl ZoneOffsetCalculator {
    /// Constructs a `ZoneOffsetCalculator` using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    #[inline]
    pub const fn new() -> Self {
        ZoneOffsetCalculator {
            offset_period: DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_ZONE_OFFSET_PERIOD_V1_MARKER,
            ),
        }
    }

    icu_provider::gen_any_buffer_data_constructors!(() -> error: DataError,
        functions: [
            new: skip,
            try_new_with_any_provider,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable(
        provider: &(impl DataProvider<ZoneOffsetPeriodV1Marker> + ?Sized),
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
    /// use icu::calendar::{Date, Time};
    /// use icu::timezone::TimeZoneBcp47Id;
    /// use icu::timezone::UtcOffset;
    /// use icu::timezone::ZoneOffsetCalculator;
    /// use tinystr::tinystr;
    ///
    /// let zoc = ZoneOffsetCalculator::new();
    ///
    /// // America/Denver observes DST
    /// let offsets = zoc
    ///     .compute_offsets_from_time_zone(
    ///         TimeZoneBcp47Id(tinystr!(8, "usden")),
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
    ///         TimeZoneBcp47Id(tinystr!(8, "usphx")),
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
        time_zone_id: TimeZoneBcp47Id,
        (date, time): (Date<Iso>, Time),
    ) -> Option<ZoneOffsets> {
        use zerovec::ule::AsULE;
        match self.offset_period.get().0.get0(&time_zone_id) {
            Some(cursor) => {
                let mut offsets = None;
                let minutes_since_epoch_walltime = (date.to_fixed() - EPOCH) as i32 * 24 * 60
                    + (time.hour.number() as i32 * 60 + time.minute.number() as i32);
                for (minutes, id) in cursor.iter1_copied().rev() {
                    if minutes_since_epoch_walltime <= i32::from_unaligned(*minutes) {
                        offsets = Some(id);
                    } else {
                        break;
                    }
                }
                let offsets = offsets?;
                Some(ZoneOffsets {
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
#[derive(Debug, Clone, Copy)]
pub struct ZoneOffsets {
    /// The standard offset.
    pub standard: UtcOffset,
    /// The daylight-saving offset, if used.
    pub daylight: Option<UtcOffset>,
}
