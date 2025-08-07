//! This module implements `TemporalError`.

use alloc::borrow::Cow;
use alloc::format;
use core::fmt;

use icu_calendar::DateError;

/// `TemporalError`'s error type.
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub enum ErrorKind {
    /// Error.
    #[default]
    Generic,
    /// TypeError
    Type,
    /// RangeError
    Range,
    /// SyntaxError
    Syntax,
    /// Assert
    Assert,
}

impl fmt::Display for ErrorKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Generic => "Error",
            Self::Type => "TypeError",
            Self::Range => "RangeError",
            Self::Syntax => "SyntaxError",
            Self::Assert => "ImplementationError",
        }
        .fmt(f)
    }
}

/// The error type for `boa_temporal`.
#[derive(Debug, Clone, PartialEq)]
pub struct TemporalError {
    kind: ErrorKind,
    msg: Cow<'static, str>,
}

impl TemporalError {
    #[inline]
    #[must_use]
    const fn new(kind: ErrorKind) -> Self {
        Self {
            kind,
            msg: Cow::Borrowed(""),
        }
    }

    /// Create a generic error
    #[inline]
    #[must_use]
    pub fn general<S>(msg: S) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        Self::new(ErrorKind::Generic).with_message(msg)
    }

    /// Create a range error.
    #[inline]
    #[must_use]
    pub const fn range() -> Self {
        Self::new(ErrorKind::Range)
    }

    /// Create a type error.
    #[inline]
    #[must_use]
    pub const fn r#type() -> Self {
        Self::new(ErrorKind::Type)
    }

    /// Create a syntax error.
    #[inline]
    #[must_use]
    pub const fn syntax() -> Self {
        Self::new(ErrorKind::Syntax)
    }

    /// Creates an assertion error
    #[inline]
    #[must_use]
    #[cfg_attr(debug_assertions, track_caller)]
    pub(crate) const fn assert() -> Self {
        #[cfg(not(debug_assertions))]
        {
            Self::new(ErrorKind::Assert)
        }
        #[cfg(debug_assertions)]
        Self {
            kind: ErrorKind::Assert,
            msg: Cow::Borrowed(core::panic::Location::caller().file()),
        }
    }

    /// Create an abrupt end error.
    #[inline]
    #[must_use]
    pub fn abrupt_end() -> Self {
        Self::syntax().with_message("Abrupt end to parsing target.")
    }

    /// Add a message to the error.
    #[inline]
    #[must_use]
    pub fn with_message<S>(mut self, msg: S) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        self.msg = msg.into();
        self
    }

    /// Add a message enum to the error.
    #[inline]
    #[must_use]
    pub(crate) fn with_enum(mut self, msg: ErrorMessage) -> Self {
        self.msg = msg.to_string().into();
        self
    }

    /// Returns this error's kind.
    #[inline]
    #[must_use]
    pub const fn kind(&self) -> ErrorKind {
        self.kind
    }

    /// Returns the error message.
    #[inline]
    #[must_use]
    pub fn message(&self) -> &str {
        &self.msg
    }

    /// Extracts the error message.
    #[inline]
    #[must_use]
    pub fn into_message(self) -> Cow<'static, str> {
        self.msg
    }

    pub fn from_icu4x(error: DateError) -> Self {
        TemporalError::range().with_message(format!("{error}"))
    }
}

impl fmt::Display for TemporalError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.kind)?;

        let msg = self.msg.trim();
        if !msg.is_empty() {
            write!(f, ": {msg}")?;
        }

        Ok(())
    }
}

/// The error message
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) enum ErrorMessage {
    // Range
    InstantOutOfRange,
    IntermediateDateTimeOutOfRange,
    ZDTOutOfDayBounds,

    // Numerical errors
    NumberNotFinite,
    NumberNotIntegral,
    NumberNotPositive,
    NumberOutOfRange,
    FractionalDigitsPrecisionInvalid,

    // Options validity
    SmallestUnitNotTimeUnit,
    SmallestUnitLargerThanLargestUnit,
    UnitNotDate,
    UnitNotTime,
    UnitRequired,
    UnitNoAutoDuringComparison,
    RoundToUnitInvalid,
    RoundingModeInvalid,
    CalendarNameInvalid,
    OffsetOptionInvalid,
    TimeZoneNameInvalid,

    // Field mismatches
    CalendarMismatch,
    TzMismatch,

    // Parsing
    ParserNeedsDate,
    FractionalTimeMoreThanNineDigits,

    // Other
    OffsetNeedsDisambiguation,
}

impl ErrorMessage {
    pub fn to_string(self) -> &'static str {
        match self {
            Self::InstantOutOfRange => "Instant nanoseconds are not within a valid epoch range.",
            Self::IntermediateDateTimeOutOfRange => {
                "Intermediate ISO datetime was not within a valid range."
            }
            Self::ZDTOutOfDayBounds => "ZonedDateTime is outside the expected day bounds",
            Self::NumberNotFinite => "number value is not a finite value.",
            Self::NumberNotIntegral => "value must be integral.",
            Self::NumberNotPositive => "integer must be positive.",
            Self::NumberOutOfRange => "number exceeded a valid range.",
            Self::FractionalDigitsPrecisionInvalid => "Invalid fractionalDigits precision value",
            Self::SmallestUnitNotTimeUnit => "smallestUnit must be a valid time unit.",
            Self::SmallestUnitLargerThanLargestUnit => {
                "smallestUnit was larger than largestunit in DifferenceeSettings"
            }
            Self::UnitNotDate => "Unit was not part of the date unit group.",
            Self::UnitNotTime => "Unit was not part of the time unit group.",
            Self::UnitRequired => "Unit is required",
            Self::UnitNoAutoDuringComparison => "'auto' units are not allowed during comparison",
            Self::RoundToUnitInvalid => "Invalid roundTo unit provided.",
            Self::RoundingModeInvalid => "Invalid roundingMode option provided",
            Self::CalendarNameInvalid => "Invalid calendarName option provided",
            Self::OffsetOptionInvalid => "Invalid offsetOption option provided",
            Self::TimeZoneNameInvalid => "Invalid timeZoneName option provided",
            Self::CalendarMismatch => {
                "Calendar must be the same for operations involving two calendared types."
            }
            Self::TzMismatch => "Timezones must be the same if unit is a day unit.",

            Self::ParserNeedsDate => "Could not find a valid DateRecord node during parsing.",
            Self::FractionalTimeMoreThanNineDigits => "Fractional time exceeds nine digits.",
            Self::OffsetNeedsDisambiguation => {
                "Offsets could not be determined without disambiguation"
            }
        }
    }
}
