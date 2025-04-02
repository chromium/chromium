// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Types for resolving and manipulating time zones.
//!
//! # Fields
//!
//! In ICU4X, a [formattable time zone] consists of up to four different fields:
//!
//! 1. The time zone ID
//! 2. The offset from UTC
//! 3. A timestamp, as time zone names can change over time
//! 4. The zone variant, representing concepts such as Standard, Summer, Daylight, and Ramadan time
//!
//! ## Time Zone
//!
//! The time zone ID corresponds to a time zone from the time zone database. The time zone ID
//! usually corresponds to the largest city in the time zone.
//!
//! There are two mostly-interchangeable standards for time zone IDs:
//!
//! 1. IANA time zone IDs, like `"America/Chicago"`
//! 2. BCP-47 time zone IDs, like `"uschi"`
//!
//! ICU4X uses BCP-47 time zone IDs for all of its APIs. To get a BCP-47 time zone from an
//! IANA time zone, use [`IanaParser`].
//!
//! ## UTC Offset
//!
//! The UTC offset precisely states the time difference between the time zone in question and
//! Coordinated Universal Time (UTC).
//!
//! In localized strings, it is often rendered as "UTC-6", meaning 6 hours less than UTC (some locales
//! use the term "GMT" instead of "UTC").
//!
//! ## Timestamp
//!
//! Some time zones change names over time, such as when changing "metazone". For example, Portugal changed from
//! "Western European Time" to "Central European Time" and back in the 1990s, without changing time zone id
//! (`Europe/Lisbon`/`ptlis`). Therefore, a timestamp is needed to resolve such generic time zone names.
//!
//! It is not required to set the timestamp on [`TimeZoneInfo`]. If it is not set, some string
//! formats may be unsupported.
//!
//! ## Zone Variant
//!
//! Many zones use different names and offsets in the summer than in the winter. In ICU4X,
//! this is called the _zone variant_.
//!
//! CLDR has two zone variants, named `"standard"` and `"daylight"`. However, the mapping of these
//! variants to specific observed offsets varies from time zone to time zone, and they may not
//! consistently represent winter versus summer time.
//!
//! Note: It is not required to set the zone variant on [`TimeZoneInfo`]. If it is not set, some string
//! formats may be unsupported.
//!
//! # Examples
//!
//! ```
//! use icu::calendar::Date;
//! use icu::time::zone::IanaParser;
//! use icu::time::zone::TimeZoneVariant;
//! use icu::time::Time;
//! use icu::time::TimeZone;
//! use tinystr::tinystr;
//!
//! // Parse the IANA ID
//! let id = IanaParser::new().parse("America/Chicago");
//!
//! // Alternatively, use the BCP47 ID directly
//! let id = TimeZone(tinystr!(8, "uschi"));
//!
//! // Create a TimeZoneInfo<Base> by associating the ID with an offset
//! let time_zone = id.with_offset("-0600".parse().ok());
//!
//! // Extend to a TimeZoneInfo<AtTime> by adding a local time
//! let time_zone_at_time = time_zone
//!     .at_time((Date::try_new_iso(2023, 12, 2).unwrap(), Time::midnight()));
//!
//! // Extend to a TimeZoneInfo<Full> by adding a zone variant
//! let time_zone_with_variant =
//!     time_zone_at_time.with_zone_variant(TimeZoneVariant::Standard);
//! ```

pub mod iana;
mod offset;
pub mod windows;

#[doc(inline)]
pub use crate::provider::{TimeZone, TimeZoneVariant};
pub use offset::InvalidOffsetError;
pub use offset::UtcOffset;
pub use offset::UtcOffsetCalculator;
pub use offset::UtcOffsets;

#[doc(no_inline)]
pub use iana::IanaParser;
#[doc(no_inline)]
pub use windows::WindowsParser;

use crate::{scaffold::IntoOption, Time};
use core::fmt;
use icu_calendar::{Date, Iso};

/// Time zone data model choices.
pub mod models {
    use super::*;
    mod private {
        pub trait Sealed {}
    }

    /// Trait encoding a particular data model for time zones.
    ///
    /// <div class="stab unstable">
    /// 🚫 This trait is sealed; it cannot be implemented by user code. If an API requests an item that implements this
    /// trait, please consider using a type from the implementors listed below.
    /// </div>
    pub trait TimeZoneModel: private::Sealed {
        /// The zone variant, if required for this time zone model.
        type TimeZoneVariant: IntoOption<TimeZoneVariant> + fmt::Debug + Copy;
        /// The local time, if required for this time zone model.
        type LocalTime: IntoOption<(Date<Iso>, Time)> + fmt::Debug + Copy;
    }

    /// A time zone containing a time zone ID and optional offset.
    #[derive(Debug, PartialEq, Eq)]
    #[non_exhaustive]
    pub enum Base {}

    impl private::Sealed for Base {}
    impl TimeZoneModel for Base {
        type TimeZoneVariant = ();
        type LocalTime = ();
    }

    /// A time zone containing a time zone ID, optional offset, and local time.
    #[derive(Debug, PartialEq, Eq)]
    #[non_exhaustive]
    pub enum AtTime {}

    impl private::Sealed for AtTime {}
    impl TimeZoneModel for AtTime {
        type TimeZoneVariant = ();
        type LocalTime = (Date<Iso>, Time);
    }

    /// A time zone containing a time zone ID, optional offset, local time, and zone variant.
    #[derive(Debug, PartialEq, Eq)]
    #[non_exhaustive]
    pub enum Full {}

    impl private::Sealed for Full {}
    impl TimeZoneModel for Full {
        type TimeZoneVariant = TimeZoneVariant;
        type LocalTime = (Date<Iso>, Time);
    }
}

/// A utility type that can hold time zone information.
#[derive(Debug, PartialEq, Eq)]
#[allow(clippy::exhaustive_structs)] // these four fields fully cover the needs of UTS 35
pub struct TimeZoneInfo<Model: models::TimeZoneModel> {
    time_zone_id: TimeZone,
    offset: Option<UtcOffset>,
    local_time: Model::LocalTime,
    zone_variant: Model::TimeZoneVariant,
}

impl<Model: models::TimeZoneModel> Clone for TimeZoneInfo<Model> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<Model: models::TimeZoneModel> Copy for TimeZoneInfo<Model> {}

impl<Model: models::TimeZoneModel> TimeZoneInfo<Model> {
    /// The BCP47 time-zone identifier.
    pub fn time_zone_id(self) -> TimeZone {
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
    Model: models::TimeZoneModel<LocalTime = (Date<Iso>, Time)>,
{
    /// The time at which to interpret the time zone.
    pub fn local_time(self) -> (Date<Iso>, Time) {
        self.local_time
    }
}

impl<Model> TimeZoneInfo<Model>
where
    Model: models::TimeZoneModel<TimeZoneVariant = TimeZoneVariant>,
{
    /// The time variant e.g. daylight or standard, if known.
    ///
    /// This field is not enforced to be consistent with the time zone id and offset.
    pub fn zone_variant(self) -> TimeZoneVariant {
        self.zone_variant
    }
}

impl TimeZone {
    /// Associates this [`TimeZone`] with a UTC offset, returning a [`TimeZoneInfo`].
    pub const fn with_offset(self, offset: Option<UtcOffset>) -> TimeZoneInfo<models::Base> {
        TimeZoneInfo {
            offset,
            time_zone_id: self,
            local_time: (),
            zone_variant: (),
        }
    }

    /// Converts this [`TimeZone`] into a [`TimeZoneInfo`] without an offset.
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
        TimeZone::unknown().with_offset(None)
    }

    /// Creates a new [`TimeZoneInfo`] for the UTC time zone.
    pub const fn utc() -> Self {
        TimeZone(tinystr::tinystr!(8, "utc")).with_offset(Some(UtcOffset::zero()))
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
    pub const fn with_zone_variant(
        self,
        zone_variant: TimeZoneVariant,
    ) -> TimeZoneInfo<models::Full> {
        TimeZoneInfo {
            offset: self.offset,
            time_zone_id: self.time_zone_id,
            local_time: self.local_time,
            zone_variant,
        }
    }

    /// Sets the zone variant by calculating it using a [`UtcOffsetCalculator`].
    ///
    /// If `offset()` is `None`, or if it doesn't match either of the
    /// timezone's standard or daylight offset around `local_time()`,
    /// the variant will be set to [`TimeZoneVariant::Standard`] and the time zone
    /// to [`TimeZone::unknown()`].
    pub fn infer_zone_variant(
        self,
        calculator: &UtcOffsetCalculator,
    ) -> TimeZoneInfo<models::Full> {
        let Some(offset) = self.offset else {
            return TimeZone::unknown()
                .with_offset(self.offset)
                .at_time(self.local_time)
                .with_zone_variant(TimeZoneVariant::Standard);
        };
        let Some(zone_variant) = calculator
            .compute_offsets_from_time_zone(self.time_zone_id, self.local_time)
            .and_then(|os| {
                if os.standard == offset {
                    Some(TimeZoneVariant::Standard)
                } else if os.daylight == Some(offset) {
                    Some(TimeZoneVariant::Daylight)
                } else {
                    None
                }
            })
        else {
            return TimeZone::unknown()
                .with_offset(self.offset)
                .at_time(self.local_time)
                .with_zone_variant(TimeZoneVariant::Standard);
        };
        self.with_zone_variant(zone_variant)
    }
}

impl TimeZoneVariant {
    /// Creates a zone variant from a TZDB `isdst` flag, if it is known that the TZDB was built with
    /// `DATAFORM=rearguard`.
    ///
    /// If it is known that the database was *not* built with `rearguard`, a caller can try to adjust
    /// for the differences. This is a moving target, for example the known differences for 2025a are:
    ///
    /// * `Europe/Dublin` since 1968-10-27
    /// * `Africa/Windhoek` between 1994-03-20 and 2017-10-24
    /// * `Africa/Casablanca` and `Africa/El_Aaiun` since 2018-10-28
    ///
    /// If the TZDB build mode is unknown or variable, use [`TimeZoneInfo::infer_zone_variant`].
    pub const fn from_rearguard_isdst(isdst: bool) -> Self {
        if isdst {
            TimeZoneVariant::Daylight
        } else {
            TimeZoneVariant::Standard
        }
    }
}
