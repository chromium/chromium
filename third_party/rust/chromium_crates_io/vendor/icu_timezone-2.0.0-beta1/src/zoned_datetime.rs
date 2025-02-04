// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_calendar::{AsCalendar, Date, Iso, Time};

/// A date and time local to a specified custom time zone.
#[derive(Debug)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct CustomZonedDateTime<A: AsCalendar, Z> {
    /// The date, local to the time zone
    pub date: Date<A>,
    /// The time, local to the time zone
    pub time: Time,
    /// The time zone
    pub zone: Z,
}

impl<A: AsCalendar, Z: Copy> CustomZonedDateTime<A, Z> {
    /// Convert the CustomZonedDateTime to an ISO CustomZonedDateTime
    #[inline]
    pub fn to_iso(&self) -> CustomZonedDateTime<Iso, Z> {
        CustomZonedDateTime {
            date: self.date.to_iso(),
            time: self.time,
            zone: self.zone,
        }
    }

    /// Convert the CustomZonedDateTime to a CustomZonedDateTime in a different calendar
    #[inline]
    pub fn to_calendar<A2: AsCalendar>(&self, calendar: A2) -> CustomZonedDateTime<A2, Z> {
        CustomZonedDateTime {
            date: self.date.to_calendar(calendar),
            time: self.time,
            zone: self.zone,
        }
    }
}
