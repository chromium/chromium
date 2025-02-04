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
    #[diplomat::rust_link(icu::calendar::Time, Struct)]
    pub struct Time(pub icu_calendar::Time);

    impl Time {
        /// Creates a new [`Time`] given field values
        #[diplomat::rust_link(icu::calendar::Time::try_new, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Time::new, FnInStruct, hidden)]
        #[diplomat::attr(supports = fallible_constructors, constructor)]
        pub fn create(
            hour: u8,
            minute: u8,
            second: u8,
            nanosecond: u32,
        ) -> Result<Box<Time>, CalendarError> {
            let hour = hour.try_into()?;
            let minute = minute.try_into()?;
            let second = second.try_into()?;
            let nanosecond = nanosecond.try_into()?;
            let time = icu_calendar::Time {
                hour,
                minute,
                second,
                nanosecond,
            };
            Ok(Box::new(Time(time)))
        }

        /// Creates a new [`Time`] from an IXDTF string.
        #[diplomat::rust_link(icu::calendar::Time::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Time::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::Time::from_str, FnInStruct, hidden)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_string(v: &DiplomatStr) -> Result<Box<Time>, CalendarParseError> {
            Ok(Box::new(Time(icu_calendar::Time::try_from_utf8(v)?)))
        }

        /// Creates a new [`Time`] representing midnight (00:00.000).
        #[diplomat::rust_link(icu::calendar::Time::midnight, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn midnight() -> Result<Box<Time>, CalendarError> {
            let time = icu_calendar::Time::midnight();
            Ok(Box::new(Time(time)))
        }

        /// Returns the hour in this time
        #[diplomat::rust_link(icu::calendar::Time::hour, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn hour(&self) -> u8 {
            self.0.hour.into()
        }
        /// Returns the minute in this time
        #[diplomat::rust_link(icu::calendar::Time::minute, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn minute(&self) -> u8 {
            self.0.minute.into()
        }
        /// Returns the second in this time
        #[diplomat::rust_link(icu::calendar::Time::second, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn second(&self) -> u8 {
            self.0.second.into()
        }
        /// Returns the nanosecond in this time
        #[diplomat::rust_link(icu::calendar::Time::nanosecond, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn nanosecond(&self) -> u32 {
            self.0.nanosecond.into()
        }
    }
}
