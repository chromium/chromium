// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::fmt;

use crate::{scaffold::IntoOption, TimeZoneBcp47Id, UtcOffset, ZoneVariant};
use icu_calendar::{Date, Iso, Time};

mod private {
    pub trait Sealed {}
}

/// Trait encoding a particular data model for time zones.
pub trait TimeZoneModel: private::Sealed {
    /// The zone variant, if required for this time zone model.
    type ZoneVariant: IntoOption<ZoneVariant> + fmt::Debug + Copy;
    /// The local time, if required for this time zone model.
    type LocalTime: IntoOption<(Date<Iso>, Time)> + fmt::Debug + Copy;
}

/// Time zone data model choices.
pub mod models {
    use super::*;

    /// A time zone containing a time zone ID and optional offset.
    #[derive(Debug)]
    #[non_exhaustive]
    pub enum Base {}

    impl super::private::Sealed for Base {}
    impl TimeZoneModel for Base {
        type ZoneVariant = ();
        type LocalTime = ();
    }

    /// A time zone containing a time zone ID, optional offset, and local time.
    #[derive(Debug)]
    #[non_exhaustive]
    pub enum AtTime {}

    impl super::private::Sealed for AtTime {}
    impl TimeZoneModel for AtTime {
        type ZoneVariant = ();
        type LocalTime = (Date<Iso>, Time);
    }

    /// A time zone containing a time zone ID, optional offset, local time, and zone variant.
    #[derive(Debug)]
    #[non_exhaustive]
    pub enum Full {}

    impl super::private::Sealed for Full {}
    impl TimeZoneModel for Full {
        type ZoneVariant = ZoneVariant;
        type LocalTime = (Date<Iso>, Time);
    }
}

/// A utility type that can hold time zone information.
#[derive(Debug, PartialEq, Eq)]
#[allow(clippy::exhaustive_structs)] // these four fields fully cover the needs of UTS 35
pub struct TimeZoneInfo<Model: TimeZoneModel> {
    time_zone_id: TimeZoneBcp47Id,
    offset: Option<UtcOffset>,
    local_time: Model::LocalTime,
    zone_variant: Model::ZoneVariant,
}

impl<Model: TimeZoneModel> Clone for TimeZoneInfo<Model> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<Model: TimeZoneModel> Copy for TimeZoneInfo<Model> {}

impl<Model: TimeZoneModel> TimeZoneInfo<Model> {
    /// The BCP47 time-zone identifier.
    pub fn time_zone_id(self) -> TimeZoneBcp47Id {
        self.time_zone_id
    }

    /// The UTC offset, if known.
    ///
    /// This field is not enforced to be consistent with the time zone id.
    pub fn offset(self) -> Option<UtcOffset> {
        self.offset
    }
}

impl<Model> TimeZoneInfo<Model>
where
    Model: TimeZoneModel<LocalTime = (Date<Iso>, Time)>,
{
    /// The time at which to interpret the time zone.
    pub fn local_time(self) -> (Date<Iso>, Time) {
        self.local_time
    }
}

impl<Model> TimeZoneInfo<Model>
where
    Model: TimeZoneModel<ZoneVariant = ZoneVariant>,
{
    /// The time variant e.g. daylight or standard, if known.
    ///
    /// This field is not enforced to be consistent with the time zone id and offset.
    pub fn zone_variant(self) -> ZoneVariant {
        self.zone_variant
    }
}

impl TimeZoneBcp47Id {
    /// Associates this [`TimeZoneBcp47Id`] with a UTC offset, returning a [`TimeZoneInfo`].
    pub const fn with_offset(self, offset: Option<UtcOffset>) -> TimeZoneInfo<models::Base> {
        TimeZoneInfo {
            offset,
            time_zone_id: self,
            local_time: (),
            zone_variant: (),
        }
    }

    /// Converts this [`TimeZoneBcp47Id`] into a [`TimeZoneInfo`] without an offset.
    pub const fn without_offset(self) -> TimeZoneInfo<models::Base> {
        TimeZoneInfo {
            offset: None,
            time_zone_id: self,
            local_time: (),
            zone_variant: (),
        }
    }
}

impl TimeZoneInfo<models::Base> {
    /// Creates a time zone info with no information.
    pub const fn unknown() -> Self {
        TimeZoneBcp47Id::unknown().with_offset(None)
    }

    /// Creates a new [`TimeZoneInfo`] for the UTC time zone.
    pub const fn utc() -> Self {
        TimeZoneBcp47Id(tinystr::tinystr!(8, "utc")).with_offset(Some(UtcOffset::zero()))
    }

    /// Sets a local time on this time zone.
    pub const fn at_time(self, local_time: (Date<Iso>, Time)) -> TimeZoneInfo<models::AtTime> {
        TimeZoneInfo {
            offset: self.offset,
            time_zone_id: self.time_zone_id,
            local_time,
            zone_variant: (),
        }
    }
}

impl TimeZoneInfo<models::AtTime> {
    /// Sets a zone variant on this time zone.
    pub const fn with_zone_variant(self, zone_variant: ZoneVariant) -> TimeZoneInfo<models::Full> {
        TimeZoneInfo {
            offset: self.offset,
            time_zone_id: self.time_zone_id,
            local_time: self.local_time,
            zone_variant,
        }
    }
}
