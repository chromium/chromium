// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::unstable::errors::ffi::Rfc9557ParseError;
    use crate::unstable::iana_parser::ffi::IanaParser;
    use crate::unstable::time::ffi::Time;
    use crate::unstable::timezone::ffi::TimeZoneInfo;

    /// An ICU4X `ZonedTime` object capable of containing a ISO-8601 time, and zone.
    #[diplomat::rust_link(icu::time::ZonedTime, Struct)]
    #[diplomat::out]
    pub struct ZonedTime {
        pub time: Box<Time>,
        pub zone: Box<TimeZoneInfo>,
    }

    impl ZonedTime {
        /// Creates a new [`ZonedTime`] from an IXDTF string.
        #[diplomat::rust_link(icu::time::ZonedTime::try_strict_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::time::ZonedTime::try_strict_from_utf8, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = named_constructors, supports = fallible_constructors), named_constructor = "strict_from_string")]
        pub fn strict_from_string(
            v: &DiplomatStr,
            iana_parser: &IanaParser,
        ) -> Result<ZonedTime, Rfc9557ParseError> {
            let icu_time::ZonedTime { time, zone } =
                icu_time::ZonedTime::try_strict_from_utf8(v, iana_parser.0.as_borrowed())?;
            Ok(ZonedTime {
                time: Box::new(Time(time)),
                zone: Box::new(TimeZoneInfo::from(zone)),
            })
        }

        /// Creates a new [`ZonedTime`] from a location-only IXDTF string.
        #[diplomat::rust_link(icu::time::ZonedTime::try_location_only_from_str, FnInStruct)]
        #[diplomat::rust_link(
            icu::time::ZonedTime::try_location_only_from_utf8,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = named_constructors, supports = fallible_constructors), named_constructor = "location_only_from_string")]
        pub fn location_only_from_string(
            v: &DiplomatStr,
            iana_parser: &IanaParser,
        ) -> Result<ZonedTime, Rfc9557ParseError> {
            let icu_time::ZonedTime { time, zone } =
                icu_time::ZonedTime::try_location_only_from_utf8(v, iana_parser.0.as_borrowed())?;
            Ok(ZonedTime {
                time: Box::new(Time(time)),
                zone: Box::new(TimeZoneInfo::from(zone)),
            })
        }

        /// Creates a new [`ZonedTime`] from an offset-only IXDTF string.
        #[diplomat::rust_link(icu::time::ZonedTime::try_offset_only_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::time::ZonedTime::try_offset_only_from_utf8, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = named_constructors, supports = fallible_constructors), named_constructor = "offset_only_from_string")]
        pub fn offset_only_from_string(v: &DiplomatStr) -> Result<ZonedTime, Rfc9557ParseError> {
            let icu_time::ZonedTime { time, zone } =
                icu_time::ZonedTime::try_offset_only_from_utf8(v)?;
            Ok(ZonedTime {
                time: Box::new(Time(time)),
                zone: Box::new(TimeZoneInfo::from(zone)),
            })
        }

        /// Creates a new [`ZonedTime`] from an IXDTF string, without requiring the offset.
        #[diplomat::rust_link(icu::time::ZonedTime::try_lenient_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::time::ZonedTime::try_lenient_from_utf8, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = named_constructors, supports = fallible_constructors), named_constructor = "lenient_from_string")]
        pub fn lenient_from_string(
            v: &DiplomatStr,
            iana_parser: &IanaParser,
        ) -> Result<ZonedTime, Rfc9557ParseError> {
            let icu_time::ZonedTime { time, zone } =
                icu_time::ZonedTime::try_lenient_from_utf8(v, iana_parser.0.as_borrowed())?;
            Ok(ZonedTime {
                time: Box::new(Time(time)),
                zone: Box::new(TimeZoneInfo::from(zone)),
            })
        }
    }
}
