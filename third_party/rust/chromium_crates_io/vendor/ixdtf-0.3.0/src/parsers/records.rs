// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! The records that `ixdtf`'s contain the resulting values of parsing.

/// An `IxdtfParseRecord` is an intermediary record returned by `IxdtfParser`.
#[non_exhaustive]
#[derive(Default, Debug, PartialEq)]
pub struct IxdtfParseRecord<'a> {
    /// Parsed `DateRecord`
    pub date: Option<DateRecord>,
    /// Parsed `TimeRecord`
    pub time: Option<TimeRecord>,
    /// Parsed UtcOffset
    pub offset: Option<UtcOffsetRecordOrZ>,
    /// Parsed `TimeZone` annotation with critical flag and data (UTCOffset | IANA name)
    pub tz: Option<TimeZoneAnnotation<'a>>,
    /// The parsed calendar value.
    pub calendar: Option<&'a [u8]>,
}

#[non_exhaustive]
#[derive(Debug, Clone, PartialEq)]
/// A record of an annotation.
pub struct Annotation<'a> {
    /// Whether this annotation is flagged as critical
    pub critical: bool,
    /// The parsed key value of the annotation
    pub key: &'a [u8],
    /// The parsed value of the annotation
    pub value: &'a [u8],
}

#[allow(clippy::exhaustive_structs)] // DateRecord only allows for a year, month, and day value.
#[derive(Default, Debug, Clone, Copy, PartialEq)]
/// The record of a parsed date.
pub struct DateRecord {
    /// Date Year
    pub year: i32,
    /// Date Month
    pub month: u8,
    /// Date Day
    pub day: u8,
}

/// Parsed Time info
#[allow(clippy::exhaustive_structs)] // TimeRecord only allows for a hour, minute, second, and sub-second value.
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct TimeRecord {
    /// An hour
    pub hour: u8,
    /// A minute value
    pub minute: u8,
    /// A second value.
    pub second: u8,
    /// A nanosecond value representing all sub-second components.
    pub nanosecond: u32,
}

/// A `TimeZoneAnnotation` that represents a parsed `TimeZoneRecord` and its critical flag.
#[non_exhaustive]
#[derive(Debug, Clone, PartialEq)]
pub struct TimeZoneAnnotation<'a> {
    /// Critical flag for the `TimeZoneAnnotation`.
    pub critical: bool,
    /// The parsed `TimeZoneRecord` for the annotation.
    pub tz: TimeZoneRecord<'a>,
}

/// Parsed `TimeZone` data, which can be either a UTC Offset value or IANA Time Zone Name value.
#[non_exhaustive]
#[derive(Debug, Clone, PartialEq)]
pub enum TimeZoneRecord<'a> {
    /// TimeZoneIANAName
    Name(&'a [u8]),
    /// TimeZoneOffset
    Offset(UtcOffsetRecord),
}

/// The parsed sign value, representing whether its struct is positive or negative.
#[repr(i8)]
#[allow(clippy::exhaustive_enums)] // Sign can only be positive or negative.
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Sign {
    /// A negative value sign, representable as either -1 or false.
    Negative = -1,
    /// A positive value sign, representable as either 1 or true.
    Positive = 1,
}

impl From<bool> for Sign {
    fn from(value: bool) -> Self {
        match value {
            true => Self::Positive,
            false => Self::Negative,
        }
    }
}

/// A full precision `UtcOffsetRecord`
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct UtcOffsetRecord {
    /// The `Sign` value of the `UtcOffsetRecord`.
    pub sign: Sign,
    /// The hour value of the `UtcOffsetRecord`.
    pub hour: u8,
    /// The minute value of the `UtcOffsetRecord`.
    pub minute: u8,
    /// The second value of the `UtcOffsetRecord`.
    pub second: u8,
    /// Any nanosecond value of the `UTCOffsetRecord`.
    pub nanosecond: u32,
}

impl UtcOffsetRecord {
    /// +0000
    pub const fn zero() -> Self {
        Self {
            sign: Sign::Positive,
            hour: 0,
            minute: 0,
            second: 0,
            nanosecond: 0,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
#[allow(clippy::exhaustive_enums)] // explicitly A or B
pub enum UtcOffsetRecordOrZ {
    Offset(UtcOffsetRecord),
    Z,
}

impl UtcOffsetRecordOrZ {
    /// Resolves to a [`UtcOffsetRecord`] according to RFC9557: "Z" == "-00:00"
    pub fn resolve_rfc_9557(self) -> UtcOffsetRecord {
        match self {
            UtcOffsetRecordOrZ::Offset(o) => o,
            UtcOffsetRecordOrZ::Z => UtcOffsetRecord {
                sign: Sign::Negative,
                hour: 0,
                minute: 0,
                second: 0,
                nanosecond: 0,
            },
        }
    }
}

/// The resulting record of parsing a `Duration` string.
#[allow(clippy::exhaustive_structs)]
// A duration can only be a Sign, a DateDuration part, and a TimeDuration part that users need to match on.
#[cfg(feature = "duration")]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct DurationParseRecord {
    /// Duration Sign
    pub sign: Sign,
    /// The parsed `DateDurationRecord` if present.
    pub date: Option<DateDurationRecord>,
    /// The parsed `TimeDurationRecord` if present.
    pub time: Option<TimeDurationRecord>,
}

/// A `DateDurationRecord` represents the result of parsing the date component of a Duration string.
#[allow(clippy::exhaustive_structs)]
// A `DateDurationRecord` by spec can only be made up of years, months, weeks, and days parts that users need to match on.
#[cfg(feature = "duration")]
#[derive(Default, Debug, Clone, Copy, PartialEq)]
pub struct DateDurationRecord {
    /// Years value.
    pub years: u32,
    /// Months value.
    pub months: u32,
    /// Weeks value.
    pub weeks: u32,
    /// Days value.
    pub days: u32,
}

/// A `TimeDurationRecord` represents the result of parsing the time component of a Duration string.
#[allow(clippy::exhaustive_enums)]
// A `TimeDurationRecord` by spec can only be made up of the valid parts up to a present fraction that users need to match on.
#[cfg(feature = "duration")]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum TimeDurationRecord {
    // An hours Time duration record.
    Hours {
        /// Hours value.
        hours: u32,
        /// The parsed fraction value in nanoseconds.
        fraction: u64,
    },
    // A Minutes Time duration record.
    Minutes {
        /// Hours value.
        hours: u32,
        /// Minutes value.
        minutes: u32,
        /// The parsed fraction value in nanoseconds.
        fraction: u64,
    },
    // A Seconds Time duration record.
    Seconds {
        /// Hours value.
        hours: u32,
        /// Minutes value.
        minutes: u32,
        /// Seconds value.
        seconds: u32,
        /// The parsed fraction value in nanoseconds.
        fraction: u32,
    },
}
