//! Autohinting state for a font instance.

use super::{
    super::{
        pen::PathStyle, AdjustedMetrics, DrawError, OutlineGlyphCollection, OutlinePen, Target,
    },
    context::{AutohintContext, FontRefContext},
    metrics::{fixed_mul, pix_round, Scale, UnscaledStyleMetricsSet},
    outline::Outline,
    shape::{Shaper, ShaperMode},
    style::{GlyphStyle, GlyphStyleMap},
    ScaleFlags,
};
use crate::{attribute::Style, font::Font, prelude::Size};
use alloc::{boxed::Box, sync::Arc};
use raw::{
    model::{
        _font_interop,
        glyph::{
            outline::{OutlineGlyphs, OutlineSource},
            GlyphSource, GlyphSourceOrder,
        },
    },
    types::{F26Dot6, GlyphId},
};

/// We enable "best effort" mode by default but allow it to be disabled with
/// a feature for testing.
const SHAPER_MODE: ShaperMode = if cfg!(feature = "autohint_shaping") {
    ShaperMode::BestEffort
} else {
    ShaperMode::Nominal
};

/// Set of derived glyph styles that are used for automatic hinting.
///
/// These are invariant per font so can be precomputed and reused for multiple
/// instances when requesting automatic hinting with [`Engine::Auto`](super::super::hint::Engine::Auto).
/// A [`ScalerFont`](crate::ScalerFont) stores them with its font and shares
/// them automatically.
#[derive(Clone, Debug)]
pub struct GlyphStyles(Arc<GlyphStyleMap>);

impl GlyphStyles {
    /// Precomputes the full set of glyph styles for the given outlines.
    pub fn new(outlines: &OutlineGlyphCollection) -> Self {
        if let Some(font) = outlines.font() {
            // The style table is sized by the glyph count, which a font can
            // overstate; it never needs more entries than there are outlines.
            Self::from_context(&FontRefContext::with_outlines(font, outlines, &[]))
        } else {
            Self(Default::default())
        }
    }

    /// Precomputes the full set of glyph styles for the font behind
    /// `context`.
    pub(crate) fn from_context(context: &dyn AutohintContext) -> Self {
        let shaper = Shaper::new(context, SHAPER_MODE);
        Self(Arc::new(GlyphStyleMap::new(context.glyph_count(), &shaper)))
    }

    /// Returns the style for the given glyph.
    pub fn get(&self, gid: GlyphId) -> Option<GlyphStyle> {
        self.0.style(gid)
    }
}

/// The data the automatic hinter derives from a font at one location.
///
/// It does not depend on the hinting target.
pub(crate) struct LocatedState {
    styles: GlyphStyles,
    /// Measured from the outlines at the location. Each style is measured
    /// when first drawn, or all at once without `std`.
    metrics: UnscaledStyleMetricsSet,
    is_fixed_width: bool,
    style: Style,
}

impl LocatedState {
    #[cfg_attr(not(feature = "std"), allow(unused_variables))]
    fn new(context: &dyn AutohintContext, styles: GlyphStyles, lazy_metrics: bool) -> Self {
        #[cfg(feature = "std")]
        let metrics = if lazy_metrics {
            UnscaledStyleMetricsSet::lazy(&styles.0)
        } else {
            UnscaledStyleMetricsSet::precomputed(context, SHAPER_MODE, &styles.0)
        };
        #[cfg(not(feature = "std"))]
        let metrics = UnscaledStyleMetricsSet::precomputed(context, SHAPER_MODE, &styles.0);
        Self {
            styles,
            metrics,
            is_fixed_width: context.is_fixed_pitch(),
            style: context.style(),
        }
    }

    /// Returns true if both states share one allocation of glyph styles.
    #[cfg(test)]
    pub(crate) fn shares_styles_with(&self, other: &Self) -> bool {
        Arc::ptr_eq(&self.styles.0, &other.styles.0)
    }

    /// Returns the state of `font` at its location, measured from
    /// `outlines`.
    ///
    /// The state is computed once and stored with the font, so every scaler
    /// that draws the font from the same sources shares one copy. One copy is
    /// kept for each combination of base source and `VARC` that a scaler
    /// draws from.
    ///
    /// The metrics are measured from outlines at the location, so a variable
    /// font keeps one copy per location. A font without variations has only
    /// its default location, so it keeps a single copy. The glyph styles are
    /// the same at every location because style coverage reads the
    /// substitutions for all locations. A font at a non-default location takes
    /// them from the default location's copy instead of computing its own.
    pub(crate) fn of_outlines(font: &Font, outlines: OutlineGlyphs<'_>) -> Arc<Self> {
        let key = SourceKey::of(&outlines);
        let build = || {
            let context = super::context::FontContext::new(font, outlines);
            let styles = if font.normalized_coords().is_empty() {
                GlyphStyles::from_context(&context)
            } else {
                let default = font.default_instance();
                let outlines = default.outline_glyphs_with(key.order());
                Self::of_outlines(&default, outlines).styles.clone()
            };
            Arc::new(Self::new(&context, styles, true))
        };
        let data =
            _font_interop::_get_or_init_scaling_data(font, || Box::new(ScalingData::default()));
        match data.downcast_ref::<ScalingData>() {
            Some(data) => data.autohint[key.0].get_or_init(build).clone(),
            // Another version of this crate filled the slot, so this state
            // is this scaler's alone. That rebuilds the styles and metrics,
            // the bulk of a first draw, for every scaler of the font. Only a
            // build linking two versions of the crate gets here.
            None => build(),
        }
    }
}

/// The outline sources a [`LocatedState`] is measured from: the base source
/// and whether `VARC` composites are drawn over it.
#[derive(Copy, Clone)]
struct SourceKey(usize);

impl SourceKey {
    /// The base sources, in the order of the keys.
    const BASES: [Option<GlyphSource>; 5] = [
        None,
        Some(GlyphSource::Glyf),
        Some(GlyphSource::Cff2),
        Some(GlyphSource::Cff),
        Some(GlyphSource::Type1),
    ];
    const COUNT: usize = Self::BASES.len() * 2;

    fn of(outlines: &OutlineGlyphs<'_>) -> Self {
        let base = match outlines.underlying_source() {
            Some(OutlineSource::Glyf) => 1,
            Some(OutlineSource::Cff2) => 2,
            Some(OutlineSource::Cff) => 3,
            Some(OutlineSource::Type1) => 4,
            Some(OutlineSource::Varc) | None => 0,
        };
        let varc = outlines.source() == Some(OutlineSource::Varc);
        Self(base * 2 + varc as usize)
    }

    /// Returns an order that resolves to the same sources.
    fn order(self) -> GlyphSourceOrder {
        let varc = (self.0 % 2 == 1).then_some(GlyphSource::Varc);
        let sources: alloc::vec::Vec<_> = varc.into_iter().chain(Self::BASES[self.0 / 2]).collect();
        GlyphSourceOrder::new(&sources)
    }
}

/// The data Skrifa stores with a font at one location.
#[derive(Default)]
struct ScalingData {
    /// The automatic hinter's state for each [`SourceKey`].
    autohint: [Once<Arc<LocatedState>>; SourceKey::COUNT],
}

#[cfg(feature = "std")]
type Once<T> = std::sync::OnceLock<T>;

/// A cell written once, for builds without `std`.
#[cfg(not(feature = "std"))]
struct Once<T>(once_cell::race::OnceBox<T>);

#[cfg(not(feature = "std"))]
impl<T> Default for Once<T> {
    fn default() -> Self {
        Self(once_cell::race::OnceBox::new())
    }
}

#[cfg(not(feature = "std"))]
impl<T> Once<T> {
    fn get_or_init(&self, f: impl FnOnce() -> T) -> &T {
        self.0.get_or_init(|| Box::new(f()))
    }
}

#[derive(Clone)]
pub(crate) struct Instance {
    state: Arc<LocatedState>,
    target: Target,
}

impl Instance {
    /// Returns the state used by this instance.
    ///
    /// All instances for one font at one location share this state.
    #[cfg(test)]
    pub fn state(&self) -> &Arc<LocatedState> {
        &self.state
    }

    pub fn new(
        context: &dyn AutohintContext,
        target: Target,
        styles: Option<GlyphStyles>,
        lazy_metrics: bool,
    ) -> Self {
        let styles = styles.unwrap_or_else(|| GlyphStyles::from_context(context));
        Self {
            state: Arc::new(LocatedState::new(context, styles, lazy_metrics)),
            target,
        }
    }

    /// Creates an instance that hints for `target` using precomputed state.
    pub fn from_state(state: Arc<LocatedState>, target: Target) -> Self {
        Self { state, target }
    }

    pub fn draw(
        &self,
        context: &dyn AutohintContext,
        size: Size,
        glyph_id: GlyphId,
        path_style: PathStyle,
        pen: &mut impl OutlinePen,
    ) -> Result<AdjustedMetrics, DrawError> {
        let state = &*self.state;
        let style = state
            .styles
            .0
            .style(glyph_id)
            .ok_or(DrawError::GlyphNotFound(glyph_id))?;
        let metrics = state
            .metrics
            .get(
                |compute| compute(context),
                SHAPER_MODE,
                &state.styles.0,
                glyph_id,
            )
            .ok_or(DrawError::GlyphNotFound(glyph_id))?;
        let mut outline = Outline::default();
        let has_overlaps = context
            .fill_outline(glyph_id, &mut outline, super::QuirksMode::default())
            .ok_or(DrawError::GlyphNotFound(glyph_id))??;
        let units_per_em = outline.units_per_em;
        let scale = Scale::new(
            size.ppem().unwrap_or(units_per_em as f32),
            units_per_em,
            state.style,
            self.target,
            metrics.style_class().script.group,
        );
        let hinted_metrics = super::hint::hint_outline(&mut outline, &metrics, &scale, Some(style));
        let h_advance = outline.advance;
        let mut pp1x = 0;
        let mut pp2x = fixed_mul(h_advance, hinted_metrics.x_scale);
        let is_light = self.target.is_light() || self.target.preserve_linear_metrics();
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afloader.c#L422>
        if !is_light {
            if let (true, Some(edge_metrics)) = (
                !scale.flags.contains(ScaleFlags::NO_ADVANCE),
                hinted_metrics.edge_metrics,
            ) {
                let old_rsb = pp2x - edge_metrics.right_opos;
                let old_lsb = edge_metrics.left_opos;
                let new_lsb = edge_metrics.left_pos;
                let mut pp1x_uh = new_lsb - old_lsb;
                let mut pp2x_uh = edge_metrics.right_pos + old_rsb;
                if old_lsb < 24 {
                    pp1x_uh -= 8;
                }
                if old_rsb < 24 {
                    pp2x_uh += 8;
                }
                pp1x = pix_round(pp1x_uh);
                pp2x = pix_round(pp2x_uh);
                if pp1x >= new_lsb && old_lsb > 0 {
                    pp1x -= 64;
                }
                if pp2x <= edge_metrics.right_pos && old_rsb > 0 {
                    pp2x += 64;
                }
            } else {
                pp1x = pix_round(pp1x);
                pp2x = pix_round(pp2x);
            }
        } else {
            pp1x = pix_round(pp1x);
            pp2x = pix_round(pp2x);
        }
        if pp1x != 0 {
            for point in &mut outline.points {
                point.x -= pp1x;
            }
        }
        let advance = if !is_light
            && (state.is_fixed_width || (metrics.digits_have_same_width && style.is_digit()))
        {
            fixed_mul(h_advance, scale.x_scale)
        } else if h_advance != 0 {
            pp2x - pp1x
        } else {
            0
        };
        outline.to_path(path_style, pen)?;
        Ok(AdjustedMetrics {
            has_overlaps,
            lsb: None,
            advance_width: Some(F26Dot6::from_bits(pix_round(advance)).to_f32()),
        })
    }
}
