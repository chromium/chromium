//! Scaling support for TrueType outlines.

mod deltas;
mod glyph;
mod hint;
mod mem;
mod scaler;

pub use glyph::{ScalerGlyph, ScalerOutline};
pub use hint::HinterOutline;
pub use mem::ScalerMemory;
pub use scaler::Scaler;

use super::Error;

/// Recursion limit for processing composite outlines.
///
/// In reality, most fonts contain shallow composite graphs with a nesting
/// depth of 1 or 2. This is set as a hard limit to avoid stack overflow
/// and infinite recursion.
pub const COMPOSITE_RECURSION_LIMIT: usize = 32;

/// Number of phantom points generated at the end of an outline.
pub const PHANTOM_POINT_COUNT: usize = 4;
