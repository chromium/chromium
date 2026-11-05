//! PostScript hinting for a [`ScalerFont`](super::ScalerFont).
//!
//! read-fonts runs the charstrings, and this crate's PostScript hinter hints
//! them before the font matrix is applied.

use super::{DrawnMetrics, ScaleError};
use crate::{
    font::Font,
    outline::cff::{scale_for_hinting, HintParams, HintState, HintedTransformingSink, HintingSink},
};
use alloc::vec::Vec;
use raw::model::Error;
use raw::{
    model::glyph::outline::{cff::CffGlyph, PathSink},
    ps::{
        cff::{CffFontRef, Subfont},
        charstring::NopFilterSink,
        FontMatrix, HintingParams, Transform,
    },
    types::{F2Dot14, Fixed},
};

/// The number of subfonts whose hinting state is kept for a size. A
/// subfont past them is read again for each glyph that needs it, so a font
/// with thousands of subfonts costs no more to configure than one with eight.
const CACHED_SUBFONTS: u16 = 8;

/// The PostScript hinter, configured for one font at one size.
#[derive(Default)]
pub(super) struct PostScriptHinter {
    /// The first [`CACHED_SUBFONTS`] subfonts. `None` marks one that cannot
    /// be read, which fails only its own glyphs.
    subfonts: Vec<Option<HintedSubfont>>,
    ppem: f32,
}

/// A subfont and its hinting state at the size.
struct HintedSubfont {
    subfont: Subfont,
    transform: Transform,
    hint_state: HintState,
}

impl HintedSubfont {
    /// Reads subfont `index` at `coords` and sets up its hints for `ppem`.
    fn new(cff: &CffFontRef<'_>, index: u16, coords: &[F2Dot14], ppem: f32) -> Result<Self, Error> {
        let (subfont, params) = cff.subfont_hinted(index, coords)?;
        let transform = cff.transform(&subfont, Some(ppem));
        let hint_state = HintState::new(&hint_params(params), scale_for_hinting(transform.scale));
        Ok(Self {
            subfont,
            transform,
            hint_state,
        })
    }
}

impl PostScriptHinter {
    /// Reads the first subfonts at the font's location and `ppem`.
    pub(super) fn reconfigure(&mut self, cff: &CffFontRef<'_>, coords: &[F2Dot14], ppem: f32) {
        self.ppem = ppem;
        self.subfonts.clear();
        self.subfonts.extend(
            (0..cff.num_subfonts().min(CACHED_SUBFONTS))
                .map(|index| HintedSubfont::new(cff, index, coords, ppem).ok()),
        );
    }

    pub(super) fn draw(
        &self,
        font: &Font,
        outline: &CffGlyph<'_>,
        sink: &mut dyn PathSink,
    ) -> Result<DrawnMetrics, ScaleError> {
        let cff = outline.cff_font();
        let glyph = outline.glyph();
        let coords = outline.normalized_coords();
        let index = u16::try_from(outline.subfont_index()).map_err(|_| Error::Malformed)?;
        let read;
        let hinted = match self.subfonts.get(usize::from(index)) {
            Some(hinted) => hinted.as_ref().ok_or(Error::Malformed)?,
            None => {
                read = HintedSubfont::new(cff, index, coords, self.ppem)?;
                &read
            }
        };
        let mut sink = sink;
        let mut filter = NopFilterSink::new(&mut sink);
        let width = if hinted.transform.matrix != FontMatrix::IDENTITY {
            // Hinting runs at scale, so only the matrix is left to apply.
            let mut transform = HintedTransformingSink::new(&mut filter, hinted.transform.matrix);
            let mut hinting = HintingSink::new(&hinted.hint_state, &mut transform);
            cff.evaluate_charstring(&hinted.subfont, glyph, coords, &mut hinting)?
        } else {
            let mut hinting = HintingSink::new(&hinted.hint_state, &mut filter);
            cff.evaluate_charstring(&hinted.subfont, glyph, coords, &mut hinting)?
        };
        let advance_width = width
            // A subfont with no default width leaves the metrics tables,
            // read in whole units as FreeType rounds them.
            .or_else(|| {
                let advance = font.glyph_metrics().h_advance_exact(glyph).round();
                Some(Fixed::from_i32(advance.to_i32()))
            })
            // Hinting was asked for, so the advance is rounded as FreeType
            // rounds it.
            .map(|width| hinted.transform.transform_h_metric(width).round().to_f32())
            // A charstring can state a width below zero. Report none and
            // leave the caller to its linear metrics.
            .filter(|width| *width >= 0.0);
        let mut metrics = DrawnMetrics::default();
        metrics.h_advance = advance_width;
        Ok(metrics)
    }
}

/// Converts the parameters read by read-fonts into the hinter's parameters.
///
/// The two types have the same fields.
fn hint_params(params: HintingParams) -> HintParams {
    HintParams {
        blues: params.blues,
        family_blues: params.family_blues,
        other_blues: params.other_blues,
        family_other_blues: params.family_other_blues,
        blue_scale: params.blue_scale,
        blue_shift: params.blue_shift,
        blue_fuzz: params.blue_fuzz,
        language_group: params.language_group,
    }
}
