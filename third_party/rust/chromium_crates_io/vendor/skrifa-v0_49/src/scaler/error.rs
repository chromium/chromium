//! The error from scaling a glyph.

use core::fmt;
use raw::{model::Error, tables::glyf::bytecode::HintError, types::GlyphId};

use crate::outline::DrawError;

/// An error from drawing a glyph with a [`ScalerFont`](super::ScalerFont).
///
/// Each variant names the step that failed and carries what that step knows.
/// [`kind`](Self::kind) gives the same failure as a read-fonts [`Error`], for
/// a caller that only sorts errors.
#[non_exhaustive]
#[derive(Clone, PartialEq, Debug)]
pub enum ScaleError {
    /// The font has no outline for the glyph.
    GlyphNotFound(GlyphId),
    /// The memory supplied for the draw is smaller than the glyph needs.
    InsufficientMemory {
        /// The size in bytes the glyph needs, as
        /// [`ScalerOutline::required_memory_size`](super::ScalerOutline::required_memory_size)
        /// reports it.
        required: usize,
        /// The size in bytes that was supplied.
        supplied: usize,
    },
    /// Reading or evaluating the outline failed.
    Outline(Error),
    /// A TrueType program failed.
    ///
    /// This is the font or control value program when the scaler was
    /// configured, or a glyph program when hinting is pedantic.
    Hinting(HintError),
}

impl ScaleError {
    /// Returns the kind of failure.
    pub fn kind(&self) -> Error {
        match self {
            Self::GlyphNotFound(_) => Error::MissingResource,
            Self::InsufficientMemory { .. } => Error::InsufficientMemory,
            Self::Outline(error) => *error,
            Self::Hinting(_) => Error::InvalidOperation,
        }
    }

    /// Converts an error from drawing an outline into `supplied` bytes of a
    /// `required` size.
    pub(super) fn from_outline(error: Error, required: usize, supplied: usize) -> Self {
        match error {
            Error::InsufficientMemory => Self::InsufficientMemory { required, supplied },
            error => Self::Outline(error),
        }
    }

    /// Converts an error from skrifa's automatic hinter, which drew into
    /// `supplied` bytes of a `required` size.
    pub(super) fn from_draw_error(error: DrawError, required: usize, supplied: usize) -> Self {
        match error {
            DrawError::GlyphNotFound(glyph) => Self::GlyphNotFound(glyph),
            DrawError::HintingFailed(error) => Self::Hinting(error),
            DrawError::PostScript(error) => Self::Outline(error),
            DrawError::InsufficientMemory => Self::InsufficientMemory { required, supplied },
            DrawError::RecursionLimitExceeded(_) | DrawError::TooManyPoints(_) => {
                Self::Outline(Error::LimitExceeded)
            }
            DrawError::NoSources => Self::Outline(Error::MissingResource),
            _ => Self::Outline(Error::Malformed),
        }
    }
}

impl From<Error> for ScaleError {
    fn from(error: Error) -> Self {
        Self::Outline(error)
    }
}

impl From<HintError> for ScaleError {
    fn from(error: HintError) -> Self {
        Self::Hinting(error)
    }
}

impl fmt::Display for ScaleError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::GlyphNotFound(glyph) => write!(f, "glyph {glyph} has no outline"),
            Self::InsufficientMemory { required, supplied } => write!(
                f,
                "the glyph needs {required} bytes of memory but {supplied} were supplied"
            ),
            Self::Outline(error) => write!(f, "{error}"),
            Self::Hinting(error) => write!(f, "{error}"),
        }
    }
}

impl core::error::Error for ScaleError {}

#[cfg(test)]
mod tests {
    use super::*;
    use raw::tables::glyf::bytecode::{HintErrorKind, Program};

    #[test]
    fn each_variant_has_a_kind() {
        let hinting = HintError {
            program: Program::Font,
            glyph_id: None,
            pc: 0,
            opcode: None,
            kind: HintErrorKind::ValueStackUnderflow,
        };
        for (error, kind) in [
            (
                ScaleError::GlyphNotFound(GlyphId::new(3)),
                Error::MissingResource,
            ),
            (
                ScaleError::InsufficientMemory {
                    required: 8,
                    supplied: 4,
                },
                Error::InsufficientMemory,
            ),
            (
                ScaleError::Outline(Error::LimitExceeded),
                Error::LimitExceeded,
            ),
            (ScaleError::Hinting(hinting), Error::InvalidOperation),
        ] {
            assert_eq!(error.kind(), kind, "{error:?}");
        }
    }

    // The automatic hinter's memory error is the documented variant.
    #[test]
    fn autohint_memory_errors_gain_the_memory_sizes() {
        assert_eq!(
            ScaleError::from_draw_error(DrawError::InsufficientMemory, 0, 4),
            ScaleError::InsufficientMemory {
                required: 0,
                supplied: 4
            }
        );
    }

    // A short buffer reports the sizes, and anything else keeps its kind.
    #[test]
    fn outline_errors_gain_the_memory_sizes() {
        assert_eq!(
            ScaleError::from_outline(Error::InsufficientMemory, 64, 16),
            ScaleError::InsufficientMemory {
                required: 64,
                supplied: 16,
            }
        );
        assert_eq!(
            ScaleError::from_outline(Error::Malformed, 64, 16),
            ScaleError::Outline(Error::Malformed)
        );
    }
}
