// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use std::ops::{Add, Sub};

/// The seconds unit of time.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct Seconds(pub i64);

/// The minutes unit of time.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct Minutes(pub i64);

/// The hours unit of time.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct Hours(pub i64);

impl Add for Seconds {
    type Output = Seconds;

    fn add(self, rhs: Self) -> Self::Output {
        Seconds(self.0 + rhs.0)
    }
}

impl Sub for Seconds {
    type Output = Seconds;

    fn sub(self, rhs: Self) -> Self::Output {
        Seconds(self.0 - rhs.0)
    }
}

impl Minutes {
    /// Returns the number of minutes in seconds.
    pub fn as_seconds(self) -> Seconds {
        Seconds(self.0 * 60)
    }
}

impl Hours {
    /// Returns the number of hours in seconds.
    pub fn as_seconds(self) -> Seconds {
        Seconds(self.0 * 60 * 60)
    }
}
