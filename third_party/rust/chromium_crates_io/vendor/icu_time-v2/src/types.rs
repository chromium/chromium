// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_calendar::{AsCalendar, Date, RangeError};

/// This macro defines a struct for 0-based date fields: hours, minutes, seconds
/// and fractional seconds. Each unit is bounded by a range. The traits implemented
/// here will return a Result on whether or not the unit is in range from the given
/// input.
macro_rules! dt_unit {
    ($name:ident, $storage:ident, $value:expr, $(#[$docs:meta])+) => {
        $(#[$docs])+
        #[derive(Debug, Default, Clone, Copy, PartialEq, Eq, Ord, PartialOrd, Hash)]
        pub struct $name($storage);

        impl $name {
            /// Gets the numeric value for this component.
            pub const fn number(self) -> $storage {
                self.0
            }

            /// Creates a new value at 0.
            pub const fn zero() -> $name {
                Self(0)
            }

            /// Returns whether the value is zero.
            #[inline]
            pub fn is_zero(self) -> bool {
                self.0 == 0
            }
        }

        impl TryFrom<$storage> for $name {
            type Error = RangeError;

            fn try_from(input: $storage) -> Result<Self, Self::Error> {
                if input > $value {
                    Err(RangeError {
                        field: stringify!($name),
                        min: 0,
                        max: $value,
                        value: input as i32,
                    })
                } else {
                    Ok(Self(input))
                }
            }
        }

        impl TryFrom<usize> for $name {
            type Error = RangeError;

            fn try_from(input: usize) -> Result<Self, Self::Error> {
                if input > $value {
                    Err(RangeError {
                        field: "$name",
                        min: 0,
                        max: $value,
                        value: input as i32,
                    })
                } else {
                    Ok(Self(input as $storage))
                }
            }
        }

        impl From<$name> for $storage {
            fn from(input: $name) -> Self {
                input.0
            }
        }

        impl From<$name> for usize {
            fn from(input: $name) -> Self {
                input.0 as Self
            }
        }
    };
}

dt_unit!(
    Hour,
    u8,
    23,
    /// An ISO-8601 hour component, for use with ISO calendars.
    ///
    /// Must be within inclusive bounds `[0, 23]`.
);

dt_unit!(
    Minute,
    u8,
    59,
    /// An ISO-8601 minute component, for use with ISO calendars.
    ///
    /// Must be within inclusive bounds `[0, 59]`.
);

dt_unit!(
    Second,
    u8,
    60,
    /// An ISO-8601 second component, for use with ISO calendars.
    ///
    /// Must be within inclusive bounds `[0, 60]`. `60` accommodates for leap seconds.
);

dt_unit!(
    Nanosecond,
    u32,
    999_999_999,
    /// A fractional second component, stored as nanoseconds.
    ///
    /// Must be within inclusive bounds `[0, 999_999_999]`."
);

/// A representation of a time in hours, minutes, seconds, and nanoseconds
#[derive(Debug, Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Time {
    /// Hour
    pub hour: Hour,

    /// Minute
    pub minute: Minute,

    /// Second
    pub second: Second,

    /// Subsecond
    pub subsecond: Nanosecond,
}

impl Time {
    /// Construct a new [`Time`], without validating that all components are in range
    pub const fn new(hour: Hour, minute: Minute, second: Second, subsecond: Nanosecond) -> Self {
        Self {
            hour,
            minute,
            second,
            subsecond,
        }
    }

    /// Construct a new [`Time`] representing midnight (00:00.000)
    pub const fn midnight() -> Self {
        Self {
            hour: Hour::zero(),
            minute: Minute::zero(),
            second: Second::zero(),
            subsecond: Nanosecond::zero(),
        }
    }

    /// Construct a new [`Time`], whilst validating that all components are in range
    pub fn try_new(hour: u8, minute: u8, second: u8, nanosecond: u32) -> Result<Self, RangeError> {
        Ok(Self {
            hour: hour.try_into()?,
            minute: minute.try_into()?,
            second: second.try_into()?,
            subsecond: nanosecond.try_into()?,
        })
    }
}

/// A date and time for a given calendar.
#[derive(Debug, PartialEq, Eq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct DateTime<A: AsCalendar> {
    /// The date
    pub date: Date<A>,
    /// The time
    pub time: Time,
}

/// A date and time for a given calendar, local to a specified time zone.
#[derive(Debug, PartialEq, Eq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct ZonedDateTime<A: AsCalendar, Z> {
    /// The date, local to the time zone
    pub date: Date<A>,
    /// The time, local to the time zone
    pub time: Time,
    /// The time zone
    pub zone: Z,
}
