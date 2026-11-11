use std::fmt;

use crate::Integer;

/// A structured field value [date].
///
/// Dates represent an integer number of seconds from the Unix epoch.
///
/// [`Version::Rfc9651`][`crate::Version::Rfc9651`] supports bare items of this
/// type; [`Version::Rfc8941`][`crate::Version::Rfc8941`] does not.
///
/// [date]: <https://httpwg.org/specs/rfc9651.html#date>
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
pub struct Date(Integer);

impl Date {
    /// The minimum value for a parsed or serialized date, corresponding to
    /// [`Integer::MIN`] seconds from the Unix epoch.
    pub const MIN: Self = Self::from_unix_seconds(Integer::MIN);

    /// The maximum value for a parsed or serialized date, corresponding to
    /// [`Integer::MAX`] seconds from the Unix epoch.
    pub const MAX: Self = Self::from_unix_seconds(Integer::MAX);

    /// The Unix epoch: `1970-01-01T00:00:00Z`.
    pub const UNIX_EPOCH: Self = Self::from_unix_seconds(Integer::ZERO);

    /// Returns the date as an integer number of seconds from the Unix epoch.
    #[must_use]
    pub fn unix_seconds(&self) -> Integer {
        self.0
    }

    /// Creates a date from an integer number of seconds from the Unix epoch.
    #[must_use]
    pub const fn from_unix_seconds(v: Integer) -> Self {
        Self(v)
    }
}

impl fmt::Display for Date {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "@{}", self.unix_seconds())
    }
}
