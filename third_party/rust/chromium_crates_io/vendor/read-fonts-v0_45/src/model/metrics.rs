//! Metrics for a font and its glyphs.

use crate::tables::mvar::MvarInstance;
use types::{F48Dot16, Tag};

/// Returns a design-unit metric with its location's delta applied.
fn metric(value: i32, deltas: Option<&MvarInstance>, tag: Tag) -> F48Dot16 {
    F48Dot16::from_i32(value)
        + deltas
            .and_then(|deltas| deltas.get(tag))
            .unwrap_or_default()
}

mod global;
mod glyph;
mod scaled;
mod style;

pub use global::{LineBox, LineExtents, Metrics};
pub(crate) use glyph::{empty as empty_glyph_metrics, RawGlyphMetrics};
pub use glyph::{GlyphExtents, GlyphMetrics};
pub use scaled::{
    Scale, Scale26Dot6, ScaleF32, ScaledGlyphMetrics, ScaledMetrics, ScaledStyleMetrics,
};
pub use style::{Decoration, ScriptMetrics, StyleMetrics};
