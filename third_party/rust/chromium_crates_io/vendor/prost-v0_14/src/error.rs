//! Protobuf encoding and decoding errors.

use crate::encoding::WireType;
use alloc::borrow::Cow;
#[cfg(not(feature = "std"))]
use alloc::boxed::Box;
#[cfg(not(feature = "std"))]
use alloc::string::String;
#[cfg(not(feature = "std"))]
use alloc::vec::Vec;
use core::fmt;

/// A Protobuf message decoding error.
///
/// `DecodeError` indicates that the input buffer does not contain a valid
/// Protobuf message. The error details should be considered 'best effort': in
/// general it is not possible to exactly pinpoint why data is malformed.
#[derive(Clone, PartialEq, Eq)]
pub struct DecodeError {
    inner: Box<Inner>,
}

#[derive(Clone, PartialEq, Eq)]
struct Inner {
    /// A 'best effort' root cause description.
    description: DecodeErrorKind,
    /// A stack of (message, field) name pairs, which identify the specific
    /// message type and field where decoding failed. The stack contains an
    /// entry per level of nesting.
    stack: Vec<(&'static str, &'static str)>,
}

impl DecodeError {
    /// Creates a new `DecodeError` with a 'best effort' root cause description.
    ///
    /// Meant to be used only by `Message` implementations.
    #[deprecated(
        since = "0.14.2",
        note = "This function was meant for internal use only. Because of `doc(hidden)` it was publicly available and it is actually used by users. The prost project intents to remove this function in the next breaking release."
    )]
    #[cold]
    #[doc(hidden)]
    pub fn new(description: impl Into<Cow<'static, str>>) -> DecodeError {
        DecodeErrorKind::Other { description: description.into() }.into()
    }

    /// Creates a new `DecodeError` with a DecodeErrorKind::UnexpectedTypeUrl.
    ///
    /// Must only be used by `prost_types::Any` implementation.
    #[doc(hidden)]
    #[cold]
    pub fn new_unexpected_type_url(
        actual: impl Into<String>,
        expected: impl Into<String>,
    ) -> DecodeError {
        DecodeErrorKind::UnexpectedTypeUrl { actual: actual.into(), expected: expected.into() }
            .into()
    }

    /// Pushes a (message, field) name location pair on to the location stack.
    ///
    /// Meant to be used only by `Message` implementations.
    #[doc(hidden)]
    pub fn push(&mut self, message: &'static str, field: &'static str) {
        self.inner.stack.push((message, field));
    }
}

impl fmt::Debug for DecodeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("DecodeError")
            .field("description", &self.inner.description)
            .field("stack", &self.inner.stack)
            .finish()
    }
}

impl fmt::Display for DecodeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("failed to decode Protobuf message: ")?;
        for &(message, field) in &self.inner.stack {
            write!(f, "{message}.{field}: ")?;
        }
        write!(f, "{}", self.inner.description)
    }
}

impl From<DecodeErrorKind> for DecodeError {
    fn from(description: DecodeErrorKind) -> Self {
        DecodeError { inner: Box::new(Inner { description, stack: Vec::new() }) }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) enum DecodeErrorKind {
    /// Length delimiter exceeds maximum usize value
    LengthDelimiterTooLarge,
    /// Invalid varint
    InvalidVarint,
    #[cfg(not(feature = "no-recursion-limit"))]
    /// Recursion limit reached
    RecursionLimitReached,
    /// Invalid wire type value
    InvalidWireType { value: u64 },
    /// Invalid key value
    InvalidKey { key: u64 },
    /// Invalid tag value: 0
    InvalidTag,
    /// Invalid wire type
    UnexpectedWireType { actual: WireType, expected: WireType },
    /// Buffer underflow
    BufferUnderflow,
    /// Delimited length exceeded
    DelimitedLengthExceeded,
    /// Unexpected end group tag
    UnexpectedEndGroupTag,
    /// Invalid string value: data is not UTF-8 encoded
    InvalidString,
    /// Unexpected type URL
    UnexpectedTypeUrl { actual: String, expected: String },
    /// A textual description of a problem
    Other { description: Cow<'static, str> },
}

impl fmt::Display for DecodeErrorKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::LengthDelimiterTooLarge => {
                write!(f, "length delimiter exceeds maximum usize value")
            }
            Self::InvalidVarint => write!(f, "invalid varint"),
            #[cfg(not(feature = "no-recursion-limit"))]
            Self::RecursionLimitReached => write!(f, "recursion limit reached"),
            Self::InvalidWireType { value } => write!(f, "invalid wire type value: {value}"),
            Self::InvalidKey { key } => write!(f, "invalid key value: {key}"),
            Self::InvalidTag => write!(f, "invalid tag value: 0"),
            Self::UnexpectedWireType { actual, expected } => {
                write!(f, "invalid wire type: {actual:?} (expected {expected:?})")
            }
            Self::BufferUnderflow => write!(f, "buffer underflow"),
            Self::DelimitedLengthExceeded => write!(f, "delimited length exceeded"),
            Self::UnexpectedEndGroupTag => write!(f, "unexpected end group tag"),
            Self::InvalidString => {
                write!(f, "invalid string value: data is not UTF-8 encoded")
            }
            Self::UnexpectedTypeUrl { actual, expected } => {
                write!(f, "unexpected type URL.type_url: expected type URL: \"{expected}\" (got: \"{actual}\")")
            }
            Self::Other { description } => {
                write!(f, "{description}")
            }
        }
    }
}

impl core::error::Error for DecodeError {}

#[cfg(feature = "std")]
impl From<DecodeError> for std::io::Error {
    fn from(error: DecodeError) -> std::io::Error {
        std::io::Error::new(std::io::ErrorKind::InvalidData, error)
    }
}

/// A Protobuf message encoding error.
///
/// `EncodeError` always indicates that a message failed to encode because the
/// provided buffer had insufficient capacity. Message encoding is otherwise
/// infallible.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct EncodeError {
    required: usize,
    remaining: usize,
}

impl EncodeError {
    /// Creates a new `EncodeError`.
    pub(crate) fn new(required: usize, remaining: usize) -> EncodeError {
        EncodeError { required, remaining }
    }

    /// Returns the required buffer capacity to encode the message.
    pub fn required_capacity(&self) -> usize {
        self.required
    }

    /// Returns the remaining length in the provided buffer at the time of
    /// encoding.
    pub fn remaining(&self) -> usize {
        self.remaining
    }
}

impl fmt::Display for EncodeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "failed to encode Protobuf message; insufficient buffer capacity (required: {}, remaining: {})",
            self.required, self.remaining
        )
    }
}

impl core::error::Error for EncodeError {}

#[cfg(feature = "std")]
impl From<EncodeError> for std::io::Error {
    fn from(error: EncodeError) -> std::io::Error {
        std::io::Error::new(std::io::ErrorKind::InvalidInput, error)
    }
}

/// An error indicating that an unknown enumeration value was encountered.
///
/// The Protobuf spec mandates that enumeration value sets are ‘open’, so this
/// error's value represents an integer value unrecognized by the
/// presently used enum definition.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct UnknownEnumValue(pub i32);

impl fmt::Display for UnknownEnumValue {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "unknown enumeration value {}", self.0)
    }
}

impl core::error::Error for UnknownEnumValue {}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_push() {
        let mut decode_error = DecodeError::from(DecodeErrorKind::InvalidVarint);
        decode_error.push("Foo bad", "bar.foo");
        decode_error.push("Baz bad", "bar.baz");

        assert_eq!(
            decode_error.to_string(),
            "failed to decode Protobuf message: Foo bad.bar.foo: Baz bad.bar.baz: invalid varint"
        );
    }

    #[cfg(feature = "std")]
    #[test]
    fn test_into_std_io_error() {
        let decode_error = DecodeError::from(DecodeErrorKind::InvalidVarint);
        let std_io_error = std::io::Error::from(decode_error);

        assert_eq!(std_io_error.kind(), std::io::ErrorKind::InvalidData);
        assert_eq!(std_io_error.to_string(), "failed to decode Protobuf message: invalid varint");
    }
}
