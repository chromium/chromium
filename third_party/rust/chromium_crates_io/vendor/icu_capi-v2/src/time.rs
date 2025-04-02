// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::errors::ffi::{CalendarError, CalendarParseError};

    #[diplomat::opaque]
    /// An ICU4X Time object representing a time in terms of hour, minute, second, nanosecond
    #[diplomat::rust_link(icu::time::Time, Struct)]
    pub struct Time(pub icu_time::Time);

    impl Time {
        /// Creates a new [`Time`] given field values
        #[diplomat::rust_link(icu::time::Time::try_new, FnInStruct)]
        #[diplomat::rust_link(icu::time::Time::new, FnInStruct, hidden)]
        #[diplomat::attr(supports = fallible_constructors, constructor)]
        pub fn create(
            hour: u8,
            minute: u8,
            second: u8,
            subsecond: u32,
        ) -> Result<Box<Time>, CalendarError> {
            let hour = hour.try_into()?;
            let minute = minute.try_into()?;
            let second = second.try_into()?;
            let subsecond = subsecond.try_into()?;
            let time = icu_time::Time {
                hour,
                minute,
                second,
                subsecond,
            };
            Ok(Box::new(Time(time)))
        }

        /// Creates a new [`Time`] from an IXDTF string.
        #[diplomat::rust_link(icu::time::Time::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::time::Time::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::time::Time::from_str, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_string(v: &DiplomatStr) -> Result<Box<Time>, CalendarParseError> {
            Ok(Box::new(Time(icu_time::Time::try_from_utf8(v)?)))
        }

        /// Creates a new [`Time`] representing midnight (00:00.000).
        #[diplomat::rust_link(icu::time::Time::midnight, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn midnight() -> Result<Box<Time>, CalendarError> {
            let time = icu_time::Time::midnight();
            Ok(Box::new(Time(time)))
        }

        /// Returns the hour in this time
        #[diplomat::rust_link(icu::time::Time::hour, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn hour(&self) -> u8 {
            self.0.hour.into()
        }
        /// Returns the minute in this time
        #[diplomat::rust_link(icu::time::Time::minute, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn minute(&self) -> u8 {
            self.0.minute.into()
        }
        /// Returns the second in this time
        #[diplomat::rust_link(icu::time::Time::second, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn second(&self) -> u8 {
            self.0.second.into()
        }
        /// Returns the subsecond in this time as nanoseconds
        #[diplomat::rust_link(icu::time::Time::subsecond, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn subsecond(&self) -> u32 {
            self.0.subsecond.into()
        }
    }
}
