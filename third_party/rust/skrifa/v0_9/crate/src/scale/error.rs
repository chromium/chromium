use read_fonts::{tables::glyf::ToPathError, types::GlyphId, ReadError};

use std::fmt;

/// Errors that may occur when scaling glyphs.
#[derive(Clone, Debug)]
pub enum Error {
    /// No viable sources were available.
    NoSources,
    /// The requested glyph was not present in the font.
    GlyphNotFound(GlyphId),
    /// Exceeded a recursion limit when loading a glyph.
    RecursionLimitExceeded(GlyphId),
    /// Error occured during hinting.
    #[cfg(feature = "hinting")]
    HintingFailed(GlyphId),
    /// An anchor point had invalid indices.
    InvalidAnchorPoint(GlyphId, u16),
    /// Conversion from outline to path failed.
    ToPath(ToPathError),
    /// Error occured when reading font data.
    Read(ReadError),
}

impl From<ToPathError> for Error {
    fn from(e: ToPathError) -> Self {
        Self::ToPath(e)
    }
}

impl From<ReadError> for Error {
    fn from(e: ReadError) -> Self {
        Self::Read(e)
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Self::NoSources => write!(f, "No glyph sources are available for the given font"),
            Self::GlyphNotFound(gid) => write!(f, "Glyph {gid} was not found in the given font"),
            Self::RecursionLimitExceeded(gid) => write!(
                f,
                "Recursion limit ({}) exceeded when loading composite component {gid}",
                super::GLYF_COMPOSITE_RECURSION_LIMIT,
            ),
            #[cfg(feature = "hinting")]
            Self::HintingFailed(gid) => write!(f, "Bad hinting bytecode for glyph {gid}"),
            Self::InvalidAnchorPoint(gid, index) => write!(
                f,
                "Invalid anchor point index ({index}) for composite glyph {gid}",
            ),
            Self::ToPath(e) => write!(f, "{e}"),
            Self::Read(e) => write!(f, "{e}"),
        }
    }
}

impl std::error::Error for Error {}

/// Result type for errors that may occur when loading glyphs.
pub type Result<T> = core::result::Result<T, Error>;
