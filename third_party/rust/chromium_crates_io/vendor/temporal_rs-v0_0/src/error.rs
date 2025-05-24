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
    pub(crate) const fn assert() -> Self {
        Self::new(ErrorKind::Assert)
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
