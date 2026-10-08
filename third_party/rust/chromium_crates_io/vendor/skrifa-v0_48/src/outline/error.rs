//! Error types associated with outlines.

use core::fmt;
use read_fonts::types::GlyphId;

pub use read_fonts::ps::error::Error as CffError;

pub use super::glyf::HintError;

/// Errors that may occur when drawing glyphs.
#[derive(Clone, Debug)]
pub enum DrawError {
    /// No viable sources were available.
    NoSources,
    /// The requested glyph was not present in the font.
    GlyphNotFound(GlyphId),
    /// Exceeded memory limits when loading a glyph.
    InsufficientMemory,
    /// Exceeded a recursion limit when loading a glyph.
    RecursionLimitExceeded(GlyphId),
    /// Glyph outline contains too many points.
    TooManyPoints(GlyphId),
    /// Error occurred during hinting.
    HintingFailed(HintError),
    /// An anchor point had invalid indices.
    InvalidAnchorPoint(GlyphId, u16),
    /// Error occurred while loading a PostScript (CFF/CFF2) glyph.
    PostScript(CffError),
    /// The font data does not make sense: absent where it was required, too
    /// short, or self-inconsistent.
    ///
    /// Carries nothing. A table that is missing, one that is truncated and one
    /// whose offsets do not resolve are the same answer to "can this glyph be
    /// drawn", and there is nothing a caller can do differently between them.
    Malformed,
    /// HarfBuzz style drawing with hints is not supported
    // Error rather than silently returning unhinted per f2f discussion.
    HarfBuzzHintingUnsupported,
}

impl From<HintError> for DrawError {
    fn from(value: HintError) -> Self {
        Self::HintingFailed(value)
    }
}

impl From<CffError> for DrawError {
    fn from(value: CffError) -> Self {
        Self::PostScript(value)
    }
}

impl fmt::Display for DrawError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Self::NoSources => write!(f, "No glyph sources are available for the given font"),
            Self::GlyphNotFound(gid) => write!(f, "Glyph {gid} was not found in the given font"),
            Self::InsufficientMemory => write!(f, "exceeded memory limits"),
            Self::RecursionLimitExceeded(gid) => write!(
                f,
                "Recursion limit ({}) exceeded when loading composite component {gid}",
                read_fonts::limits::MAX_RECURSION_DEPTH,
            ),
            Self::TooManyPoints(gid) => write!(f, "Glyph {gid} contains more than 64k points"),
            Self::HintingFailed(e) => write!(f, "{e}"),
            Self::InvalidAnchorPoint(gid, index) => write!(
                f,
                "Invalid anchor point index ({index}) for composite glyph {gid}",
            ),
            Self::PostScript(e) => write!(f, "{e}"),
            Self::Malformed => write!(f, "font data was absent or malformed"),
            Self::HarfBuzzHintingUnsupported => write!(
                f,
                "HarfBuzz style paths with hinting is not (yet?) supported"
            ),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for DrawError {}
