use std::fmt;

#[derive(Debug)]
#[cfg_attr(test, derive(PartialEq))]
pub(crate) enum Repr {
    Visit(Box<str>),

    OutOfRange,
    NaN,

    Empty,
    InvalidCharacter(usize),

    TrailingCharactersAfterMember(usize),
    TrailingComma(usize),
    TrailingCharactersAfterParsedValue(usize),

    ExpectedStartOfInnerList(usize),
    ExpectedInnerListDelimiter(usize),
    UnterminatedInnerList(usize),

    ExpectedStartOfBareItem(usize),

    ExpectedStartOfBoolean(usize),
    ExpectedBoolean(usize),

    ExpectedStartOfString(usize),
    InvalidStringCharacter(usize),
    UnterminatedString(usize),

    UnterminatedEscapeSequence(usize),
    InvalidEscapeSequence(usize),

    ExpectedStartOfToken(usize),

    ExpectedStartOfByteSequence(usize),
    UnterminatedByteSequence(usize),
    InvalidByteSequence(usize),

    ExpectedDigit(usize),
    TooManyDigits(usize),
    TooManyDigitsBeforeDecimalPoint(usize),
    TooManyDigitsAfterDecimalPoint(usize),
    TrailingDecimalPoint(usize),

    ExpectedStartOfDate(usize),
    Rfc8941Date(usize),
    NonIntegerDate(usize),

    ExpectedStartOfDisplayString(usize),
    Rfc8941DisplayString(usize),
    ExpectedQuote(usize),
    InvalidUtf8InDisplayString(usize),
    InvalidDisplayStringCharacter(usize),
    UnterminatedDisplayString(usize),

    ExpectedStartOfKey(usize),
}

impl<E: std::error::Error> From<E> for Repr {
    fn from(err: E) -> Self {
        Self::Visit(err.to_string().into())
    }
}

impl Repr {
    fn parts(&self) -> (&str, Option<usize>) {
        match *self {
            Self::Visit(ref msg) => (msg, None),

            Self::NaN => ("NaN", None),
            Self::OutOfRange => ("out of range", None),

            Self::Empty => ("cannot be empty", None),
            Self::InvalidCharacter(i) => ("invalid character", Some(i)),

            Self::TrailingCharactersAfterMember(i) => ("trailing characters after member", Some(i)),
            Self::TrailingComma(i) => ("trailing comma", Some(i)),
            Self::TrailingCharactersAfterParsedValue(i) => {
                ("trailing characters after parsed value", Some(i))
            }

            Self::ExpectedStartOfInnerList(i) => ("expected start of inner list", Some(i)),
            Self::ExpectedInnerListDelimiter(i) => {
                ("expected inner list delimiter (' ' or ')')", Some(i))
            }
            Self::UnterminatedInnerList(i) => ("unterminated inner list", Some(i)),

            Self::ExpectedStartOfBareItem(i) => ("expected start of bare item", Some(i)),

            Self::ExpectedStartOfBoolean(i) => ("expected start of boolean ('?')", Some(i)),
            Self::ExpectedBoolean(i) => ("expected boolean ('0' or '1')", Some(i)),

            Self::ExpectedStartOfString(i) => (r#"expected start of string ('"')"#, Some(i)),
            Self::InvalidStringCharacter(i) => ("invalid string character", Some(i)),
            Self::UnterminatedString(i) => ("unterminated string", Some(i)),

            Self::UnterminatedEscapeSequence(i) => ("unterminated escape sequence", Some(i)),
            Self::InvalidEscapeSequence(i) => ("invalid escape sequence", Some(i)),

            Self::ExpectedStartOfToken(i) => ("expected start of token", Some(i)),

            Self::ExpectedStartOfByteSequence(i) => {
                ("expected start of byte sequence (':')", Some(i))
            }
            Self::UnterminatedByteSequence(i) => ("unterminated byte sequence", Some(i)),
            Self::InvalidByteSequence(i) => ("invalid byte sequence", Some(i)),

            Self::ExpectedDigit(i) => ("expected digit", Some(i)),
            Self::TooManyDigits(i) => ("too many digits", Some(i)),
            Self::TooManyDigitsBeforeDecimalPoint(i) => {
                ("too many digits before decimal point", Some(i))
            }
            Self::TooManyDigitsAfterDecimalPoint(i) => {
                ("too many digits after decimal point", Some(i))
            }
            Self::TrailingDecimalPoint(i) => ("trailing decimal point", Some(i)),

            Self::ExpectedStartOfDate(i) => ("expected start of date ('@')", Some(i)),
            Self::Rfc8941Date(i) => ("RFC 8941 does not support dates", Some(i)),
            Self::NonIntegerDate(i) => ("date must be an integer number of seconds", Some(i)),

            Self::ExpectedStartOfDisplayString(i) => {
                ("expected start of display string ('%')", Some(i))
            }
            Self::Rfc8941DisplayString(i) => ("RFC 8941 does not support display strings", Some(i)),
            Self::ExpectedQuote(i) => (r#"expected '"'"#, Some(i)),
            Self::InvalidUtf8InDisplayString(i) => ("invalid UTF-8 in display string", Some(i)),
            Self::InvalidDisplayStringCharacter(i) => ("invalid display string character", Some(i)),
            Self::UnterminatedDisplayString(i) => ("unterminated display string", Some(i)),

            Self::ExpectedStartOfKey(i) => ("expected start of key ('a'-'z' or '*')", Some(i)),
        }
    }
}

impl fmt::Display for Repr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let (msg, index) = self.parts();
        match (f.alternate(), index) {
            (true, _) | (false, None) => f.write_str(msg),
            (false, Some(index)) => write!(f, "{msg} at index {index}"),
        }
    }
}

/// An error that can occur in this crate.
///
/// The most common type of error is invalid input during parsing, but others
/// exist as well:
///
/// - Conversion to or from bare-item types such as [`Integer`][crate::Integer]
/// - Attempting to serialize an empty [list][crate::ListSerializer::finish] or
///   [dictionary][crate::DictSerializer::finish]
///
/// By default, the `std::fmt::Display` implementation for this type includes
/// the index at which the error occurred, if any. To omit this, use the
/// alternate form:
///
/// ```
/// # use sfv::{visitor::Ignored, Parser};
/// let err = Parser::new("abc;0").parse_item_with_visitor(Ignored).unwrap_err();
///
/// assert_eq!(format!("{}", err), "expected start of key ('a'-'z' or '*') at index 4");
/// assert_eq!(format!("{:#}", err), "expected start of key ('a'-'z' or '*')");
/// assert_eq!(err.index(), Some(4));
/// ```
#[derive(Debug)]
#[cfg_attr(test, derive(PartialEq))]
pub struct Error {
    repr: Repr,
}

impl From<Repr> for Error {
    fn from(repr: Repr) -> Self {
        Self { repr }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&self.repr, f)
    }
}

impl std::error::Error for Error {}

impl Error {
    /// Returns the zero-based index in the input at which the error occurred,
    /// if any.
    #[must_use]
    pub fn index(&self) -> Option<usize> {
        self.repr.parts().1
    }
}

pub(crate) struct NonEmptyStringError {
    byte_index: Option<usize>,
}

impl NonEmptyStringError {
    pub(crate) const fn empty() -> Self {
        Self { byte_index: None }
    }

    pub(crate) const fn invalid_character(byte_index: usize) -> Self {
        Self {
            byte_index: Some(byte_index),
        }
    }

    pub(crate) const fn msg(&self) -> &'static str {
        match self.byte_index {
            None => "cannot be empty",
            Some(_) => "invalid character",
        }
    }
}

impl From<NonEmptyStringError> for Error {
    fn from(err: NonEmptyStringError) -> Self {
        match err.byte_index {
            None => Repr::Empty,
            Some(index) => Repr::InvalidCharacter(index),
        }
        .into()
    }
}
