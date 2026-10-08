//! What can go wrong while building an outline.

use super::super::bytecode::HintError;
use crate::ReadError;

/// An error from inspecting or building an outline.
///
/// The variants carry no payload. The caller already named the glyph, and
/// anything finer would need a path through the composite tree to be
/// actionable.
#[derive(Clone, PartialEq, Debug)]
pub enum OutlineError {
    /// A requested glyph was not found in the font.
    MissingGlyph,
    /// A composite glyph nests more deeply than
    /// [`MAX_RECURSION_DEPTH`](crate::limits::MAX_RECURSION_DEPTH).
    RecursionLimitExceeded,
    /// A composite glyph references more components than
    /// [`MAX_COMPOSITE_EDGES`](crate::limits::MAX_COMPOSITE_EDGES).
    TooManyComponents,
    /// An outline has more points than
    /// [`MAX_OUTLINE_POINTS`](crate::limits::MAX_OUTLINE_POINTS).
    TooManyPoints,
    /// A component referenced a point that does not exist.
    InvalidAnchorPoint,
    /// A caller supplied buffer was too small.
    InsufficientMemory,
    /// The [`Hinter`](super::Hinter) reported a failure.
    Hinting(HintError),
    /// The font data was malformed.
    ///
    /// Covers every failure to read the underlying tables.
    Malformed,
}

impl From<ReadError> for OutlineError {
    fn from(_: ReadError) -> Self {
        Self::Malformed
    }
}

impl From<HintError> for OutlineError {
    fn from(value: HintError) -> Self {
        Self::Hinting(value)
    }
}

impl core::fmt::Display for OutlineError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::MissingGlyph => f.write_str("requested glyph was not found in the font"),
            Self::RecursionLimitExceeded => {
                f.write_str("glyph exceeded the composite recursion limit")
            }
            Self::TooManyComponents => f.write_str("glyph referenced too many components"),
            Self::TooManyPoints => f.write_str("glyph has too many points"),
            Self::InvalidAnchorPoint => f.write_str("glyph referenced a nonexistent anchor point"),
            Self::InsufficientMemory => f.write_str("outline buffers were too small"),
            Self::Hinting(err) => write!(f, "{err}"),
            Self::Malformed => f.write_str("font data was malformed"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for OutlineError {}
