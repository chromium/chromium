// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;
    use icu_calendar::Iso;

    use crate::unstable::calendar::ffi::Calendar;
    use crate::unstable::date::ffi::{Date, IsoDate};
    use crate::unstable::errors::ffi::Rfc9557ParseError;
    use crate::unstable::time::ffi::Time;

    /// An ICU4X `DateTime` object capable of containing a ISO-8601 date and time.
    #[diplomat::rust_link(icu::time::DateTime, Struct)]
    #[diplomat::out]
    pub struct IsoDateTime {
        pub date: Box<IsoDate>,
        pub time: Box<Time>,
    }

    impl IsoDateTime {
        /// Creates a new [`IsoDateTime`] from an IXDTF string.
        #[diplomat::rust_link(icu::time::DateTime::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::time::DateTime::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::time::DateTime::from_str, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_string(v: &DiplomatStr) -> Result<IsoDateTime, Rfc9557ParseError> {
            let icu_time::DateTime { date, time } = icu_time::DateTime::try_from_utf8(v, Iso)?;
            Ok(IsoDateTime {
                date: Box::new(IsoDate(date)),
                time: Box::new(Time(time)),
            })
        }
    }

    /// An ICU4X `DateTime` object capable of containing a date and time for any calendar.
    #[diplomat::rust_link(icu::time::DateTime, Struct)]
    #[diplomat::out]
    pub struct DateTime {
        pub date: Box<Date>,
        pub time: Box<Time>,
    }

    impl DateTime {
        /// Creates a new [`DateTime`] from an IXDTF string.
        #[diplomat::rust_link(icu::time::DateTime::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::time::DateTime::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::time::DateTime::from_str, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_string(
            v: &DiplomatStr,
            calendar: &Calendar,
        ) -> Result<DateTime, Rfc9557ParseError> {
            let icu_time::DateTime { date, time } =
                icu_time::DateTime::try_from_utf8(v, calendar.0.clone())?;
            Ok(DateTime {
                date: Box::new(Date(date)),
                time: Box::new(Time(time)),
            })
        }
    }
}
