// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use ffi::TimeZoneInfo;

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::unstable::{
        date::ffi::{Date, IsoDate},
        datetime::ffi::IsoDateTime,
        time::ffi::Time,
        variant_offset::ffi::UtcOffset,
    };

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::time::TimeZone, Struct)]
    pub struct TimeZone(pub(crate) icu_time::TimeZone);

    impl TimeZone {
        /// The unknown time zone.
        #[diplomat::rust_link(icu::time::TimeZoneInfo::unknown, FnInStruct)]
        #[diplomat::rust_link(icu::time::TimeZone::unknown, FnInStruct, hidden)]
        #[diplomat::attr(auto, named_constructor)]
        pub fn unknown() -> Box<TimeZone> {
            Box::new(TimeZone(icu_time::TimeZone::UNKNOWN))
        }

        /// Whether the time zone is the unknown zone.
        #[diplomat::rust_link(icu::time::TimeZone::is_unknown, FnInStruct)]
        pub fn is_unknown(&self) -> bool {
            self.0.is_unknown()
        }

        /// Construct a [`TimeZone`] from an IANA time zone ID.
        #[diplomat::rust_link(icu::time::TimeZone::from_iana_id, FnInStruct)]
        #[diplomat::demo(default_constructor)]
        #[diplomat::attr(auto, named_constructor = "from_iana_id")]
        #[cfg(feature = "compiled_data")]
        pub fn create_from_iana_id<'a>(iana_id: &'a DiplomatStr) -> Box<Self> {
            Box::new(Self(
                icu_time::zone::IanaParser::new().parse_from_utf8(iana_id),
            ))
        }

        /// Construct a [`TimeZone`] from a Windows time zone ID and region.
        #[diplomat::rust_link(icu::time::TimeZone::from_windows_id, FnInStruct)]
        #[diplomat::attr(auto, named_constructor = "from_windows_id")]
        #[cfg(feature = "compiled_data")]
        pub fn create_from_windows_id<'a>(
            windows_id: &'a DiplomatStr,
            region: &'a DiplomatStr,
        ) -> Box<Self> {
            Box::new(Self(
                icu_time::zone::WindowsParser::new()
                    .parse_from_utf8(
                        windows_id,
                        icu_locale_core::subtags::Region::try_from_utf8(region).ok(),
                    )
                    .unwrap_or(icu_time::TimeZone::UNKNOWN),
            ))
        }

        /// Construct a [`TimeZone`] from the platform-specific ID.
        #[diplomat::rust_link(icu::time::TimeZone::from_system_id, FnInStruct)]
        #[diplomat::attr(auto, named_constructor = "from_system_id")]
        #[cfg(feature = "compiled_data")]
        pub fn create_from_system_id<'a>(
            id: &'a DiplomatStr,
            _region: &'a DiplomatStr,
        ) -> Box<Self> {
            #[cfg(target_os = "windows")]
            return Self::create_from_windows_id(id, _region);
            #[cfg(not(target_os = "windows"))]
            return Self::create_from_iana_id(id);
        }

        /// Creates a time zone from a BCP-47 string.
        ///
        /// Returns the unknown time zone if the string is not a valid BCP-47 subtag.
        #[diplomat::rust_link(icu::time::TimeZone, Struct, compact)]
        #[diplomat::attr(auto, named_constructor = "from_bcp47")]
        pub fn create_from_bcp47(id: &DiplomatStr) -> Box<Self> {
            icu_locale_core::subtags::Subtag::try_from_utf8(id)
                .ok()
                .map(icu_time::TimeZone)
                .map(TimeZone)
                .map(Box::new)
                .unwrap_or_else(Self::unknown)
        }

        #[diplomat::rust_link(icu::time::TimeZone::with_offset, FnInStruct)]
        pub fn with_offset(&self, offset: &UtcOffset) -> Box<TimeZoneInfo> {
            Box::new(self.0.with_offset(Some(offset.0)).into())
        }

        #[diplomat::rust_link(icu::time::TimeZone::without_offset, FnInStruct)]
        pub fn without_offset(&self) -> Box<TimeZoneInfo> {
            Box::new(self.0.without_offset().into())
        }
    }

    #[diplomat::enum_convert(icu_time::zone::TimeZoneVariant, needs_wildcard)]
    #[diplomat::rust_link(icu::time::zone::TimeZoneVariant, Enum)]
    #[non_exhaustive]
    #[deprecated(note = "type not needed anymore")]
    #[diplomat::attr(dart, disable)]
    pub enum TimeZoneVariant {
        // TimeZoneVariant in Rust doesn't have a default, but it is useful to have one
        // here for consistent behavior.
        #[diplomat::attr(auto, default)]
        Standard,
        Daylight,
    }

    #[allow(deprecated)]
    impl TimeZoneVariant {
        #[diplomat::rust_link(icu::time::zone::TimeZoneVariant::from_rearguard_isdst, FnInEnum)]
        #[diplomat::rust_link(icu::time::TimeZoneInfo::with_variant, FnInStruct)]
        #[deprecated(note = "type not needed anymore")]
        #[allow(deprecated)] // remove in 3.0
        pub fn from_rearguard_isdst(isdst: bool) -> Self {
            icu_time::zone::TimeZoneVariant::from_rearguard_isdst(isdst).into()
        }
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::time::TimeZoneInfo, Struct)]
    #[diplomat::rust_link(icu::time::zone::models::AtTime, Struct, hidden)]
    #[diplomat::rust_link(icu::time::zone::models::Base, Struct, hidden)]
    #[diplomat::rust_link(icu::time::zone::models::Full, Struct, hidden)]
    #[derive(Clone, Copy)]
    pub struct TimeZoneInfo {
        pub(crate) id: icu_time::TimeZone,
        pub(crate) offset: Option<icu_time::zone::UtcOffset>,
        pub(crate) zone_name_timestamp: Option<icu_time::zone::ZoneNameTimestamp>,
    }

    impl TimeZoneInfo {
        /// Creates a time zone for UTC (Coordinated Universal Time).
        #[diplomat::rust_link(icu::time::TimeZoneInfo::utc, FnInStruct)]
        #[diplomat::rust_link(icu::time::zone::UtcOffset::zero, FnInStruct, hidden)]
        #[diplomat::attr(auto, named_constructor)]
        pub fn utc() -> Box<TimeZoneInfo> {
            Box::new(icu_time::TimeZoneInfo::utc().into())
        }

        /// Creates a time zone info from parts.
        ///
        /// `variant` is ignored.
        #[diplomat::attr(auto, constructor)]
        #[allow(deprecated)]
        #[diplomat::attr(dart, disable)]
        pub fn from_parts(
            id: &TimeZone,
            offset: Option<&UtcOffset>,
            _variant: Option<TimeZoneVariant>,
        ) -> Box<TimeZoneInfo> {
            Box::new(Self {
                id: id.0,
                offset: offset.map(|o| o.0),
                zone_name_timestamp: None,
            })
        }

        #[diplomat::rust_link(icu::time::TimeZoneInfo::id, FnInStruct)]
        #[diplomat::attr(demo_gen, disable)] // this just returns a constructor argument
        #[diplomat::attr(auto, getter)]
        pub fn id(&self) -> Box<TimeZone> {
            Box::new(TimeZone(self.id))
        }

        /// Sets the datetime at which to interpret the time zone
        /// for display name lookup.
        ///
        /// Notes:
        ///
        /// - If not set, the formatting datetime is used if possible.
        /// - If the offset is not set, the datetime is interpreted as UTC.
        /// - The constraints are the same as with `ZoneNameTimestamp` in Rust.
        /// - Set to year 1000 or 9999 for a reference far in the past or future.
        #[diplomat::rust_link(icu::time::TimeZoneInfo::at_date_time, FnInStruct)]
        #[diplomat::rust_link(icu::time::zone::ZoneNameTimestamp, Struct, compact)]
        #[diplomat::rust_link(
            icu::time::TimeZoneInfo::with_zone_name_timestamp,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::time::zone::ZoneNameTimestamp::from_date_time_iso,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::time::zone::ZoneNameTimestamp::from_zoned_date_time_iso,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::time::zone::ZoneNameTimestamp::far_in_future,
            FnInStruct,
            hidden
        )] // documented
        #[diplomat::rust_link(icu::time::zone::ZoneNameTimestamp::far_in_past, FnInStruct, hidden)] // documented
        pub fn at_date_time_iso(&self, date: &IsoDate, time: &Time) -> Box<Self> {
            Box::new(Self {
                zone_name_timestamp: Some(
                    self.id
                        .with_offset(self.offset)
                        .at_date_time(icu_time::DateTime {
                            date: date.0,
                            time: time.0,
                        })
                        .zone_name_timestamp(),
                ),
                ..*self
            })
        }

        /// Sets the datetime at which to interpret the time zone
        /// for display name lookup.
        ///
        /// Notes:
        ///
        /// - If not set, the formatting datetime is used if possible.
        /// - If the offset is not set, the datetime is interpreted as UTC.
        /// - The constraints are the same as with `ZoneNameTimestamp` in Rust.
        /// - Set to year 1000 or 9999 for a reference far in the past or future.
        #[diplomat::rust_link(icu::time::TimeZoneInfo::at_date_time, FnInStruct)]
        #[diplomat::rust_link(icu::time::zone::ZoneNameTimestamp, Struct, compact)]
        #[diplomat::rust_link(
            icu::time::TimeZoneInfo::with_zone_name_timestamp,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::time::zone::ZoneNameTimestamp::from_date_time,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::time::zone::ZoneNameTimestamp::from_zoned_date_time,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::time::zone::ZoneNameTimestamp::far_in_future,
            FnInStruct,
            hidden
        )] // documented
        #[diplomat::rust_link(icu::time::zone::ZoneNameTimestamp::far_in_past, FnInStruct, hidden)] // documented
        pub fn at_date_time(&self, date: &Date, time: &Time) -> Box<Self> {
            Box::new(Self {
                zone_name_timestamp: Some(
                    self.id
                        .with_offset(self.offset)
                        .at_date_time(icu_time::DateTime {
                            date: date.0.clone(),
                            time: time.0,
                        })
                        .zone_name_timestamp(),
                ),
                ..*self
            })
        }

        /// Sets the timestamp, in milliseconds since Unix epoch, at which to interpret the time zone
        /// for display name lookup.
        ///
        /// Notes:
        ///
        /// - If not set, the formatting datetime is used if possible.
        /// - The constraints are the same as with `ZoneNameTimestamp` in Rust.
        #[diplomat::rust_link(icu::time::TimeZoneInfo::with_zone_name_timestamp, FnInStruct)]
        #[diplomat::rust_link(
            icu::time::zone::ZoneNameTimestamp::from_epoch_seconds,
            FnInStruct,
            compact
        )]
        pub fn at_timestamp(&self, timestamp: i64) -> Box<Self> {
            Box::new(Self {
                zone_name_timestamp: Some(icu_time::zone::ZoneNameTimestamp::from_epoch_seconds(
                    timestamp / 1000,
                )),
                ..*self
            })
        }

        #[diplomat::rust_link(icu::time::TimeZoneInfo::zone_name_timestamp, FnInStruct)]
        #[diplomat::rust_link(
            icu::time::zone::ZoneNameTimestamp::to_zoned_date_time_iso,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::time::zone::ZoneNameTimestamp::to_date_time_iso,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, getter)]
        /// Returns the `DateTime` for the UTC zone name reference time
        pub fn zone_name_date_time(&self) -> Option<IsoDateTime> {
            let datetime = self.zone_name_timestamp?.to_zoned_date_time_iso();
            Some(IsoDateTime {
                date: Box::new(IsoDate(datetime.date)),
                time: Box::new(Time(datetime.time)),
            })
        }

        #[diplomat::rust_link(icu::time::TimeZoneInfo::with_variant, FnInStruct)]
        #[deprecated(note = "returns unmodified copy")]
        #[diplomat::attr(dart, disable)]
        #[allow(deprecated)]
        pub fn with_variant(&self, _time_variant: TimeZoneVariant) -> Box<Self> {
            Box::new(*self)
        }

        #[diplomat::attr(auto, getter)]
        #[diplomat::rust_link(icu::time::TimeZoneInfo::offset, FnInStruct)]
        pub fn offset(&self) -> Option<Box<UtcOffset>> {
            self.offset.map(UtcOffset).map(Box::new)
        }

        #[deprecated(note = "does nothing")]
        #[diplomat::attr(dart, disable)]
        #[diplomat::rust_link(icu::time::TimeZoneInfo::infer_variant, FnInStruct)]
        #[diplomat::rust_link(icu::time::zone::TimeZoneVariant, Enum, compact)]
        #[allow(deprecated)]
        pub fn infer_variant(
            &self,
            _offset_calculator: &crate::unstable::variant_offset::ffi::VariantOffsetsCalculator,
        ) -> Option<()> {
            Some(())
        }

        #[diplomat::rust_link(icu::time::TimeZoneInfo::variant, FnInStruct)]
        #[diplomat::attr(demo_gen, disable)] // this just returns a constructor argument
        #[deprecated(note = "always returns null")]
        #[diplomat::attr(dart, disable)]
        #[allow(deprecated)]
        pub fn variant(&self) -> Option<TimeZoneVariant> {
            None
        }
    }
}

impl ffi::TimeZoneInfo {
    pub(crate) fn as_rust_at_time<C: icu_calendar::Calendar>(
        &self,
        date: Option<icu_calendar::Date<C>>,
        time: Option<icu_time::Time>,
    ) -> icu_time::TimeZoneInfo<icu_time::zone::models::AtTime> {
        let base = self.id.with_offset(self.offset);
        if let Some(zone_name_timestamp) = self.zone_name_timestamp {
            base.with_zone_name_timestamp(zone_name_timestamp)
        } else if let Some(date) = date {
            base.at_date_time(icu_time::DateTime {
                date,
                time: time.unwrap_or(icu_time::Time::noon()),
            })
        } else {
            base.with_zone_name_timestamp(icu_time::zone::ZoneNameTimestamp::far_in_future())
        }
    }
}

impl From<icu_time::zone::UtcOffset> for TimeZoneInfo {
    fn from(other: icu_time::zone::UtcOffset) -> Self {
        Self {
            id: icu_time::TimeZone::UNKNOWN,
            offset: Some(other),
            zone_name_timestamp: None,
        }
    }
}

impl From<icu_time::TimeZoneInfo<icu_time::zone::models::Base>> for TimeZoneInfo {
    fn from(other: icu_time::TimeZoneInfo<icu_time::zone::models::Base>) -> Self {
        Self {
            id: other.id(),
            offset: other.offset(),
            zone_name_timestamp: None,
        }
    }
}

impl From<icu_time::TimeZoneInfo<icu_time::zone::models::AtTime>> for TimeZoneInfo {
    fn from(other: icu_time::TimeZoneInfo<icu_time::zone::models::AtTime>) -> Self {
        Self {
            id: other.id(),
            offset: other.offset(),
            zone_name_timestamp: Some(other.zone_name_timestamp()),
        }
    }
}

#[allow(deprecated)]
impl From<icu_time::TimeZoneInfo<icu_time::zone::models::Full>> for TimeZoneInfo {
    fn from(other: icu_time::TimeZoneInfo<icu_time::zone::models::Full>) -> Self {
        Self {
            id: other.id(),
            offset: other.offset(),
            zone_name_timestamp: Some(other.zone_name_timestamp()),
        }
    }
}
