//! Runtime autohinting support.

mod hint;
mod instance;
mod metrics;
mod outline;
mod recorder;
mod shape;
mod style;
mod topo;

pub use instance::GlyphStyles;
pub(crate) use instance::Instance;
pub use metrics::{
    BlueZones, Scale, ScaleFlags, ScaledAxisMetrics, ScaledBlue, ScaledStyleMetrics, ScaledWidth,
    UnscaledAxisMetrics, UnscaledBlue, UnscaledStyleMetrics, WidthMetrics,
};
pub use outline::Direction;
pub use recorder::{EdgeAction, EdgeHint, HintAction, PointAction, PointHint};
pub use style::{GlyphStyle, ScriptClass, ScriptGroup, StyleClass};
pub use style::{SCRIPT_CLASSES, STYLE_CLASSES};
pub use topo::{Axis, BlueProvenance, Dimension, Edge, Segment, TopoFlags};

use crate::outline::{DrawError, Target};
use crate::{FontRef, MetadataProvider};
use alloc::vec::Vec;
use raw::types::{F2Dot14, GlyphId};

/// Controls quirks for the different autohinters.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
enum QuirksMode {
    /// Just in time hinter; matches FreeType.
    #[default]
    Jit,
    /// Ahead of time hinter; matches ttfautohint.
    Aot,
}

/// Plan for hinting an outline.
#[derive(Clone, Debug, Default)]
pub struct HintPlan {
    hints: Vec<HintAction>,
    axes: [Option<Axis>; 2],
}

impl HintPlan {
    /// Creates a new hint plan.
    pub fn new(
        font: &FontRef,
        coords: &[F2Dot14],
        ppem: f32,
        target: Target,
        glyph_id: GlyphId,
        glyph_style: GlyphStyle,
    ) -> Result<Self, DrawError> {
        let style_class = glyph_style
            .style_class()
            .ok_or(DrawError::GlyphNotFound(glyph_id))?;
        let outline_glyph = font
            .outline_glyphs()
            .get(glyph_id)
            .ok_or(DrawError::GlyphNotFound(glyph_id))?;

        let shaper_mode = if cfg!(feature = "autohint_shaping") {
            shape::ShaperMode::BestEffort
        } else {
            shape::ShaperMode::Nominal
        };
        let shaper = shape::Shaper::new(font, shaper_mode);
        let metrics =
            metrics::compute_unscaled_style_metrics(&shaper, coords, style_class, QuirksMode::Aot);

        let mut outline = outline::Outline::default();
        outline.fill(&outline_glyph, coords, QuirksMode::Aot)?;

        let scale = metrics::Scale::new(
            ppem,
            outline_glyph.units_per_em() as i32,
            font.attributes().style,
            target,
            metrics.style_class().script.group,
        );

        let mut recorder = recorder::HintsRecorder::default();
        let hinted_plan = hint::hint_outline_with_recorder(
            &mut outline,
            &metrics,
            &scale,
            Some(glyph_style),
            &mut recorder,
        );

        Ok(Self {
            hints: recorder.actions,
            axes: hinted_plan.axes,
        })
    }

    /// Returns the collection of hinting actions.
    pub fn actions(&self) -> &[HintAction] {
        &self.hints
    }

    /// Returns the topological analysis of the horizontal axis.
    pub fn horizontal_axis(&self) -> Option<&Axis> {
        self.axes[Dimension::Horizontal].as_ref()
    }

    /// Returns the topological analysis of the vertical axis.
    pub fn vertical_axis(&self) -> Option<&Axis> {
        self.axes[Dimension::Vertical].as_ref()
    }
}

/// All constants are defined based on a UPEM of 2048.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/aflatin.h#L34>
fn derived_constant(units_per_em: i32, value: i32) -> i32 {
    value * units_per_em / 2048
}
