//! The error type for reading glyphs and evaluating font programs.

use crate::ReadError;
use core::fmt;

/// An error from reading a glyph or evaluating a program in the font.
///
/// It identifies only the kind of failure. The caller already knows which
/// glyph or table was requested.
#[non_exhaustive]
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum Error {
    /// The font data is too short, inconsistent, or refers past the end of
    /// something that is present.
    Malformed,
    /// A required part of the font is absent, such as a glyph, a table, or a
    /// CFF dictionary, subroutine index or charset.
    MissingResource,
    /// A program in the font, a charstring or TrueType instructions, did
    /// something that is not valid, such as popping an empty stack.
    InvalidOperation,
    /// The font exceeds a limit this crate sets, such as a nesting depth or a
    /// count of points, components or paints. A cycle in a color glyph's
    /// paint graph exceeds the nesting depth.
    ///
    /// The limits are in [`limits`](crate::limits).
    LimitExceeded,
    /// The memory supplied by the caller is too small.
    InsufficientMemory,
}

impl From<ReadError> for Error {
    fn from(_: ReadError) -> Self {
        Self::Malformed
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Self::Malformed => "font data was malformed",
            Self::MissingResource => "a required part of the font was missing",
            Self::InvalidOperation => "a program in the font did something invalid",
            Self::LimitExceeded => "the font exceeded a processing limit",
            Self::InsufficientMemory => "the supplied memory was too small",
        })
    }
}

impl core::error::Error for Error {}
