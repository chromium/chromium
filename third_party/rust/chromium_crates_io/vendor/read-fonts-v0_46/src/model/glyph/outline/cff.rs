//! Outlines from `CFF` and `CFF2` charstrings.
//!
//! The charstrings are parsed and evaluated by [`crate::ps::cff`]. This
//! module pairs a charstring with the font's location and its subfont.

use super::{DrawOptions, DrawnMetrics, PathSink};
use crate::model::Error;
use crate::{
    model::glyph::outline::OutlineSource,
    model::variation::NormalizedCoord,
    ps::{
        cff::{CffFontRef, Subfont},
        charstring::{NopFilterSink, TransformSink},
    },
    types::{F2Dot14, GlyphId},
};

/// A `CFF` or `CFF2` charstring from a [`Font`](crate::model::Font), with
/// its subfont.
#[derive(Clone, Copy)]
pub struct CffGlyph<'a> {
    pub(super) cff: &'a CffFontRef<'a>,
    pub(super) coords: &'a [F2Dot14],
    pub(super) glyph: GlyphId,
    pub(super) subfont_index: u16,
    pub(super) subfont: Subfont,
    pub(super) source: OutlineSource,
}

impl<'a> CffGlyph<'a> {
    /// Returns the glyph identifier.
    pub fn glyph(&self) -> GlyphId {
        self.glyph
    }

    /// Returns the table that contains the charstring, either `CFF` or
    /// `CFF2`.
    pub fn source(&self) -> OutlineSource {
        self.source
    }

    /// Returns the `CFF` or `CFF2` font that evaluates the charstring.
    pub fn cff_font(&self) -> &'a CffFontRef<'a> {
        self.cff
    }

    /// Returns the normalized variation coordinates used to evaluate the
    /// charstring.
    pub fn normalized_coords(&self) -> &'a [NormalizedCoord] {
        self.coords
    }

    /// Returns the index of the glyph's subfont.
    pub fn subfont_index(&self) -> u32 {
        self.subfont_index as u32
    }

    /// Returns the glyph's subfont, parsed at [`normalized_coords`](Self::normalized_coords).
    ///
    /// For hinting, use [`CffFontRef::subfont_hinted`] with the same index
    /// to also read the hinting parameters.
    pub fn subfont(&self) -> &Subfont {
        &self.subfont
    }

    /// Draws the charstring and returns its width, which can be
    /// negative.
    pub(super) fn draw(
        &self,
        options: &DrawOptions,
        mut sink: &mut dyn PathSink,
    ) -> Result<DrawnMetrics, Error> {
        let mut filter = NopFilterSink::new(&mut sink);
        let transform = self.cff.transform(&self.subfont, options.ppem);
        let mut transformer = TransformSink::new(&mut filter, transform);
        let width = self.cff.evaluate_charstring(
            &self.subfont,
            self.glyph,
            self.coords,
            &mut transformer,
        )?;
        Ok(DrawnMetrics {
            h_advance: width.map(|width| transform.transform_h_metric(width).to_f32()),
            ..Default::default()
        })
    }
}
