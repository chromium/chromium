//! Errors that may occur when processing PostScript fonts.

use crate::ReadError;
use core::fmt;

/// Errors that are specific to PostScript processing.
#[derive(Clone, Debug)]
pub enum Error {
    InvalidVariationStoreIndex,
    StackOverflow,
    StackUnderflow,
    ExpectedInt,
    InvalidNumber,
    CharstringNestingDepthLimitExceeded,
    MissingSubroutines,
    MissingBlendState,
    MissingFdArray,
    MissingPrivateDict,
    MissingCharstrings,
    MissingCharset,
    InvalidSeacCode(i32),
    /// The data does not make sense: absent where it was required, too short,
    /// or self-inconsistent.
    ///
    /// Carries nothing. This module answers a malformed read with `None`
    /// wherever it can; this is what it says where a `Result` is required.
    Malformed,
}

/// Kept so the `?` inside this module still reads well. It is `read-fonts`
/// converting one of its own error types into another, which is why it can
/// stay: nothing outside the crate has to name `ReadError` to use `Error`.
impl From<ReadError> for Error {
    fn from(_: ReadError) -> Self {
        Self::Malformed
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidVariationStoreIndex => {
                write!(f, "variation store index referenced an invalid region")
            }
            Self::StackOverflow => {
                write!(f, "attempted to push a value to a full stack")
            }
            Self::StackUnderflow => {
                write!(f, "attempted to pop a value from an empty stack")
            }
            Self::ExpectedInt => {
                write!(
                    f,
                    "expected an integer on the stack, found a fixed point value"
                )
            }
            Self::InvalidNumber => {
                write!(f, "number is in an invalid format")
            }
            Self::CharstringNestingDepthLimitExceeded => {
                write!(
                    f,
                    "exceeded subroutine nesting depth limit {} while evaluating a charstring",
                    crate::ps::cs::NESTING_DEPTH_LIMIT
                )
            }
            Self::MissingSubroutines => {
                write!(
                    f,
                    "encountered a callsubr operator but no subroutine index was provided"
                )
            }
            Self::MissingBlendState => {
                write!(
                    f,
                    "encountered a blend operator but no blend state was provided"
                )
            }
            Self::MissingFdArray => {
                write!(f, "CFF table does not contain a font dictionary index")
            }
            Self::MissingPrivateDict => {
                write!(f, "CFF table does not contain a private dictionary")
            }
            Self::MissingCharstrings => {
                write!(f, "CFF table does not contain a charstrings index")
            }
            Self::MissingCharset => {
                write!(f, "CFF table does not contain a valid charset")
            }
            Self::InvalidSeacCode(code) => {
                write!(f, "seac code {code} is not valid")
            }
            Self::Malformed => write!(f, "font data was absent or malformed"),
        }
    }
}

impl core::error::Error for Error {}
