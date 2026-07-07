//! Types for collecting the output when drawing a glyph outline.

pub use read_fonts::model::pen::{ControlBoundsPen, NullPen, OutlinePen, PathElement, SvgPen};

/// Style for path conversion.
///
/// The order to process points in a glyf point stream is ambiguous when the
/// first point is off-curve. Major implementations differ. Which one would
/// you like to match?
///
/// **If you add a new one make sure to update the fuzzer.**
#[derive(Debug, Default, Copy, Clone)]
pub enum PathStyle {
    /// If the first point is off-curve, check if the last is on-curve
    /// If it is, start there. If it isn't, start at the implied midpoint
    /// between first and last.
    #[default]
    FreeType,
    /// If the first point is off-curve, check if the second is on-curve.
    /// If it is, start there. If it isn't, start at the implied midpoint
    /// between first and second.
    ///
    /// Matches hb-draw's interpretation of a point stream.
    HarfBuzz,
}
