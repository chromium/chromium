// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(feature = "buffer_provider")]
    use crate::errors::ffi::DataError;
    #[cfg(feature = "buffer_provider")]
    use crate::provider::ffi::DataProvider;
    use crate::{
        date::ffi::IsoDate, errors::ffi::TimeZoneInvalidOffsetError, time::ffi::Time,
        timezone::ffi::TimeZone,
    };

    #[diplomat::rust_link(icu::time::zone::UtcOffsetCalculator, Struct)]
    #[diplomat::opaque]
    pub struct UtcOffsetCalculator(pub icu_time::zone::UtcOffsetCalculator);

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::time::zone::UtcOffset, Struct)]
    pub struct UtcOffset(pub(crate) icu_time::zone::UtcOffset);

    #[diplomat::out]
    #[diplomat::rust_link(icu::time::zone::UtcOffsets, Struct)]
    pub struct UtcOffsets {
        pub standard: Box<UtcOffset>,
        pub daylight: Option<Box<UtcOffset>>,
    }

    impl UtcOffset {
        /// Creates an offset from seconds.
        ///
        /// Errors if the offset seconds are out of range.
        #[diplomat::rust_link(icu::time::zone::UtcOffset::try_from_seconds, FnInStruct)]
        #[diplomat::rust_link(
            icu::time::zone::UtcOffset::from_seconds_unchecked,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, named_constructor = "from_seconds")]
        pub fn from_seconds(seconds: i32) -> Result<Box<UtcOffset>, TimeZoneInvalidOffsetError> {
            Ok(Box::new(Self(icu_time::zone::UtcOffset::try_from_seconds(
                seconds,
            )?)))
        }

        /// Creates an offset from eighths of an hour.
        #[diplomat::rust_link(icu::time::zone::UtcOffset, Struct, compact)]
        #[diplomat::rust_link(icu::time::zone::UtcOffset::from_eighths_of_hour, FnInStruct)]
        #[diplomat::attr(auto, named_constructor = "from_eights_of_hour")]
        pub fn from_eighths_of_hour(eighths_of_hour: i8) -> Box<Self> {
            Box::new(Self(icu_time::zone::UtcOffset::from_eighths_of_hour(
                eighths_of_hour,
            )))
        }

        /// Creates an offset from a string.
        #[diplomat::rust_link(icu::time::zone::UtcOffset, Struct, compact)]
        #[diplomat::rust_link(icu::time::zone::UtcOffset::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::time::zone::UtcOffset::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::time::zone::UtcOffset::from_str, FnInStruct, hidden)]
        #[diplomat::attr(auto, named_constructor = "from_string")]
        #[diplomat::demo(default_constructor)]
        pub fn from_string(offset: &DiplomatStr) -> Result<Box<Self>, TimeZoneInvalidOffsetError> {
            icu_time::zone::UtcOffset::try_from_utf8(offset)
                .map_err(|_| TimeZoneInvalidOffsetError)
                .map(Self)
                .map(Box::new)
        }

        /// Gets the offset as eighths of an hour.
        #[diplomat::rust_link(icu::time::zone::UtcOffset::to_eighths_of_hour, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn eighths_of_hour(&self) -> i8 {
            self.0.to_eighths_of_hour()
        }

        /// Returns the value as offset seconds.
        #[diplomat::rust_link(icu::time::TimeZoneInfo::offset, FnInStruct)]
        #[diplomat::rust_link(icu::time::zone::UtcOffset::to_seconds, FnInStruct)]
        #[diplomat::rust_link(icu::time::zone::UtcOffset, Struct, compact)]
        #[diplomat::attr(auto, getter)]
        pub fn seconds(&self) -> i32 {
            self.0.to_seconds()
        }

        /// Returns whether the offset is positive.
        #[diplomat::rust_link(icu::time::zone::UtcOffset::is_non_negative, FnInStruct)]
        #[diplomat::rust_link(icu::time::zone::UtcOffset, Struct, compact)]
        #[diplomat::attr(auto, getter)]
        pub fn is_non_negative(&self) -> bool {
            self.0.is_non_negative()
        }

        /// Returns whether the offset is zero.
        #[diplomat::rust_link(icu::time::zone::UtcOffset::is_zero, FnInStruct)]
        #[diplomat::rust_link(icu::time::zone::UtcOffset, Struct, compact)]
        #[diplomat::attr(auto, getter)]
        pub fn is_zero(&self) -> bool {
            self.0.is_zero()
        }

        /// Returns the hours part of the offset.
        #[diplomat::rust_link(icu::time::zone::UtcOffset::hours_part, FnInStruct)]
        #[diplomat::rust_link(icu::time::zone::UtcOffset, Struct, compact)]
        #[diplomat::attr(auto, getter)]
        pub fn hours_part(&self) -> i32 {
            self.0.hours_part()
        }

        /// Returns the minutes part of the offset.
        #[diplomat::rust_link(icu::time::zone::UtcOffset::minutes_part, FnInStruct)]
        #[diplomat::rust_link(icu::time::zone::UtcOffset, Struct, compact)]
        #[diplomat::attr(auto, getter)]
        pub fn minutes_part(&self) -> u32 {
            self.0.minutes_part()
        }

        /// Returns the seconds part of the offset.
        #[diplomat::rust_link(icu::time::zone::UtcOffset::seconds_part, FnInStruct)]
        #[diplomat::rust_link(icu::time::zone::UtcOffset, Struct, compact)]
        #[diplomat::attr(auto, getter)]
        pub fn seconds_part(&self) -> u32 {
            self.0.seconds_part()
        }
    }

    impl UtcOffsetCalculator {
        /// Construct a new [`UtcOffsetCalculator`] instance using compiled data.
        #[diplomat::rust_link(icu::time::zone::UtcOffsetCalculator::new, FnInStruct)]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<UtcOffsetCalculator> {
            Box::new(UtcOffsetCalculator(
                icu_time::zone::UtcOffsetCalculator::new(),
            ))
        }
        /// Construct a new [`UtcOffsetCalculator`] instance using a particular data source.
        #[diplomat::rust_link(icu::time::zone::UtcOffsetCalculator::new, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<UtcOffsetCalculator>, DataError> {
            Ok(Box::new(UtcOffsetCalculator(
                icu_time::zone::UtcOffsetCalculator::try_new_with_buffer_provider(provider.get()?)?,
            )))
        }

        #[diplomat::rust_link(
            icu::time::zone::UtcOffsetCalculator::compute_offsets_from_time_zone,
            FnInStruct
        )]
        pub fn compute_offsets_from_time_zone(
            &self,
            time_zone: &TimeZone,
            local_date: &IsoDate,
            local_time: &Time,
        ) -> Option<UtcOffsets> {
            let icu_time::zone::UtcOffsets {
                standard, daylight, ..
            } = self
                .0
                .compute_offsets_from_time_zone(time_zone.0, (local_date.0, local_time.0))?;

            Some(UtcOffsets {
                standard: Box::new(UtcOffset(standard)),
                daylight: daylight.map(UtcOffset).map(Box::new),
            })
        }
    }
}
