//! Where outline loading gets its data.

use crate::{
    tables::{
        glyf::{Glyf, PHANTOM_POINT_COUNT},
        gvar::{GlyphVariationData, Gvar},
        hmtx::Hmtx,
        hvar::Hvar,
        loca::{Loca, LocaGlyph},
        os2::Os2,
    },
    types::{F2Dot14, Fixed, GlyphId, Point},
    ReadError, TableProvider,
};

/// Everything [`OutlinePlan::load`](super::OutlinePlan::load) needs from a
/// font: glyphs, variation data, and the metrics behind the phantom points.
///
/// A caller that already holds these tables implements this trait and hands
/// them over. [`OutlineTables`] reads them from a font instead.
///
/// `'a` is the lifetime of the font data rather than of the context, so
/// glyphs outlive any borrow of the implementor.
pub trait OutlineContext<'a> {
    /// Returns the glyph with the given identifier, or `None` if it is empty.
    fn glyph(&self, glyph: GlyphId) -> Option<LocaGlyph<'a>>;

    /// Variation data for a glyph, or `None` if the font does not vary or the
    /// glyph has none.
    fn glyph_variation_data(&self, glyph: GlyphId) -> Option<GlyphVariationData<'a>>;

    /// True if the font has a `gvar` table, whatever any one glyph has in it.
    ///
    /// Answers for the font rather than per glyph, since it decides whether
    /// the delta buffers are needed at all.
    fn has_gvar(&self) -> bool;

    /// Variation deltas for a glyph's four phantom points.
    ///
    /// [`Gvar::phantom_point_deltas`] computes these from `glyf` and `loca`,
    /// including which glyph of a composite tree supplies the metrics.
    fn phantom_point_deltas(&self, glyph: GlyphId) -> Option<[Point<Fixed>; PHANTOM_POINT_COUNT]>;

    /// Normalized variation coordinates, empty for the default instance.
    ///
    /// The context states the location so an implementor that holds one is
    /// never handed a different one.
    fn coords(&self) -> &[F2Dot14] {
        &[]
    }

    /// Scalars for `gvar`'s shared tuples at [`Self::coords`], as
    /// `Gvar::compute_scalars` fills them.
    ///
    /// A scalar depends on the location and the tuple but never on the glyph,
    /// so an implementor reading many glyphs at one location computes these
    /// once. An empty or short slice is always correct; whatever is missing is
    /// computed per glyph.
    ///
    /// These must be the scalars for [`Self::coords`]. Scalars from another
    /// location produce wrong deltas and cannot be detected.
    fn gvar_scalars(&self) -> &[Fixed] {
        &[]
    }

    /// Design units per em.
    fn units_per_em(&self) -> u16;

    /// Left side bearing and advance width, in font units.
    ///
    /// These must be unvaried, straight out of `hmtx` and never adjusted by
    /// `HVAR`. The loader applies `gvar` phantom point deltas to whatever this
    /// returns, so varied metrics would be varied twice.
    ///
    /// The phantom deltas arrive in the tuple accumulation the outline needs
    /// anyway, which makes them cheaper than an `HVAR` lookup per glyph.
    fn h_metrics(&self, glyph: GlyphId) -> (i32, i32);

    /// Top side bearing and vertical advance for a glyph, in font units.
    ///
    /// `None`, the default, means the font states no per glyph vertical
    /// metrics. They are then derived from [`Self::h_line_metrics`] and the
    /// glyph's bounding box, as FreeType does for a font without `vmtx`.
    ///
    /// Like [`Self::h_metrics`], these must be unvaried. The vertical phantom
    /// points take their variation from `gvar` deltas.
    fn v_metrics(&self, glyph: GlyphId) -> Option<(i32, i32)> {
        let _ = glyph;
        None
    }

    /// The ascender and descender of the line horizontal text sits on, in
    /// font units.
    ///
    /// Only read when [`Self::v_metrics`] returns `None`, to derive that
    /// glyph's vertical metrics.
    fn h_line_metrics(&self) -> (i32, i32);

    /// True if the font has an `HVAR` table.
    ///
    /// Phantom point deltas apply either way, per [`Self::h_metrics`]. This
    /// only tells [`Scale26Dot6`](super::Scale26Dot6) to round them as
    /// FreeType rounds `HVAR` metric deltas, so the output matches a FreeType
    /// build that reads `HVAR`.
    fn has_hvar(&self) -> bool {
        false
    }
}

/// An [`OutlineContext`] that reads the tables from a font.
///
/// [`new`][Self::new] reads at the default instance and [`at`][Self::at]
/// selects a location. Nothing is cached between glyphs; a caller reading many
/// glyphs at one location is better served by a context that caches.
#[derive(Clone)]
pub struct OutlineTables<'a> {
    /// Glyph outlines.
    pub glyf: Glyf<'a>,
    /// The index into [`Self::glyf`].
    pub loca: Loca<'a>,
    /// Glyph variations, absent if the font does not vary.
    pub gvar: Option<Gvar<'a>>,
    /// Source of the metrics for the first two phantom points.
    pub hmtx: Option<Hmtx<'a>>,
    /// Supplies the ascender and descender for the last two.
    pub os2: Option<Os2<'a>>,
    /// Only its presence is read: it changes how deltas apply to phantom
    /// points.
    pub hvar: Option<Hvar<'a>>,
    /// Design units per em, from `head`.
    pub units_per_em: u16,
    /// Not public, with `gvar_scalars`: the two are only correct together,
    /// and [`at`][Self::at] is what keeps them so.
    pub(super) coords: &'a [F2Dot14],
    pub(super) gvar_scalars: &'a [Fixed],
}

impl<'a> OutlineTables<'a> {
    /// Reads the tables from a font.
    ///
    /// Fails if `glyf`, `loca` or `head` is missing or malformed. The rest are
    /// optional: without `gvar` the font is treated as non-variable, and
    /// without `hmtx` or `OS/2` the corresponding phantom point metrics are
    /// zero.
    pub fn new(font: &impl TableProvider<'a>) -> Result<Self, ReadError> {
        Ok(Self {
            glyf: font.glyf()?,
            loca: font.loca(None)?,
            gvar: font.gvar().ok(),
            hmtx: font.hmtx().ok(),
            os2: font.os2().ok(),
            hvar: font.hvar().ok(),
            units_per_em: font.head()?.units_per_em(),
            coords: &[],
            gvar_scalars: &[],
        })
    }

    /// Reads at `coords` instead of the default instance.
    ///
    /// `gvar_scalars` are the shared tuple scalars for `coords`, from
    /// `Gvar::compute_scalars`; pass `&[]` to compute them per glyph
    /// instead. They are taken together because scalars for another location
    /// produce wrong deltas and nothing can detect it.
    pub fn at(self, coords: &'a [F2Dot14], gvar_scalars: &'a [Fixed]) -> Self {
        Self {
            coords,
            gvar_scalars,
            ..self
        }
    }
}

impl<'a> OutlineContext<'a> for OutlineTables<'a> {
    fn glyph(&self, glyph: GlyphId) -> Option<LocaGlyph<'a>> {
        self.loca.get(glyph, &self.glyf)
    }

    fn glyph_variation_data(&self, glyph: GlyphId) -> Option<GlyphVariationData<'a>> {
        // Missing or malformed variation data for a glyph is not an error; it
        // simply does not vary.
        self.gvar.as_ref()?.glyph_variation_data(glyph).ok()?
    }

    fn has_gvar(&self) -> bool {
        self.gvar.is_some()
    }

    fn phantom_point_deltas(&self, glyph: GlyphId) -> Option<[Point<Fixed>; PHANTOM_POINT_COUNT]> {
        self.gvar
            .as_ref()?
            .phantom_point_deltas(&self.glyf, &self.loca, self.coords, glyph)
    }

    fn coords(&self) -> &[F2Dot14] {
        self.coords
    }

    fn gvar_scalars(&self) -> &[Fixed] {
        self.gvar_scalars
    }

    fn units_per_em(&self) -> u16 {
        self.units_per_em
    }

    fn h_metrics(&self, glyph: GlyphId) -> (i32, i32) {
        match &self.hmtx {
            Some(hmtx) => (
                hmtx.side_bearing(glyph).unwrap_or_default() as i32,
                hmtx.advance(glyph).unwrap_or_default() as i32,
            ),
            None => (0, 0),
        }
    }

    // `v_metrics` is left at its default on purpose, so vertical metrics are
    // derived from the ascender and descender below. Reading `vmtx` and
    // `VVAR` means two more tables for a type that caches nothing, and the
    // fallback chain a font missing them still needs is involved enough to
    // be worth writing once. A context that caches, and pays for both, is
    // where that belongs.

    fn h_line_metrics(&self) -> (i32, i32) {
        match &self.os2 {
            Some(os2) => (os2.s_typo_ascender() as i32, os2.s_typo_descender() as i32),
            None => (0, 0),
        }
    }

    fn has_hvar(&self) -> bool {
        self.hvar.is_some()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::tables::glyf::outline::{Outline, Scale26Dot6, Unscaled};
    use crate::{types::F26Dot6, FontRef};

    /// A context serving tables it already holds, and metrics from somewhere
    /// other than `hmtx`, the reason this is a trait.
    struct BorrowedContext<'a> {
        glyf: Glyf<'a>,
        loca: Loca<'a>,
        advance: i32,
        vertical: Option<(i32, i32)>,
    }

    impl<'a> OutlineContext<'a> for BorrowedContext<'a> {
        fn glyph(&self, glyph: GlyphId) -> Option<LocaGlyph<'a>> {
            self.loca.get(glyph, &self.glyf)
        }
        fn glyph_variation_data(&self, _glyph: GlyphId) -> Option<GlyphVariationData<'a>> {
            None
        }
        fn has_gvar(&self) -> bool {
            false
        }
        fn phantom_point_deltas(
            &self,
            _glyph: GlyphId,
        ) -> Option<[Point<Fixed>; PHANTOM_POINT_COUNT]> {
            None
        }
        fn units_per_em(&self) -> u16 {
            1000
        }
        fn h_metrics(&self, _glyph: GlyphId) -> (i32, i32) {
            (0, self.advance)
        }
        fn v_metrics(&self, _glyph: GlyphId) -> Option<(i32, i32)> {
            self.vertical
        }
        fn h_line_metrics(&self) -> (i32, i32) {
            (800, -200)
        }
    }

    /// An outside implementation drives loading, and its metrics reach the
    /// phantom points.
    #[test]
    fn a_custom_context_can_supply_everything() {
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        let context = BorrowedContext {
            glyf: font.glyf().unwrap(),
            loca: font.loca(None).unwrap(),
            advance: 1234,
            vertical: None,
        };
        let mut owned = Outline::<Scale26Dot6>::new();
        let outline = owned.load(&context, GlyphId::new(1), None).unwrap();
        assert!(!outline.points().is_empty());
        assert_eq!(
            outline.adjusted_advance_width(),
            F26Dot6::from_bits(1234 << 6)
        );
    }

    /// The default `glyph` reads through `loca`, so an implementation that
    /// leaves it alone agrees with one that owns the tables.
    #[test]
    fn the_default_glyph_lookup_matches() {
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        let borrowed = BorrowedContext {
            glyf: font.glyf().unwrap(),
            loca: font.loca(None).unwrap(),
            advance: 0,
            vertical: None,
        };
        let owned = OutlineTables::new(&font).unwrap();
        for gid in 0..font.maxp().unwrap().num_glyphs() {
            let gid = GlyphId::from(gid);
            assert_eq!(
                borrowed.glyph(gid).unwrap().into_glyph().is_some(),
                owned.glyph(gid).unwrap().into_glyph().is_some(),
                "gid {gid}"
            );
        }
    }

    /// Vertical phantom points come from `v_metrics` when it has an answer.
    #[test]
    fn per_glyph_vertical_metrics_win() {
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        let glyph = GlyphId::new(1);
        let y_max = font
            .loca(None)
            .unwrap()
            .get_glyf(glyph, &font.glyf().unwrap())
            .unwrap()
            .unwrap()
            .y_max() as i32;
        let vertical = (70, 900);
        let context = BorrowedContext {
            glyf: font.glyf().unwrap(),
            loca: font.loca(None).unwrap(),
            advance: 0,
            vertical: Some(vertical),
        };
        let mut owned = Outline::<Unscaled>::new();
        let outline = owned.load(&context, glyph).unwrap();
        let (tsb, vadvance) = vertical;
        assert_eq!(outline.phantom_points()[2].y, y_max + tsb);
        assert_eq!(outline.phantom_points()[3].y, y_max + tsb - vadvance);
    }

    /// Without them, they are derived from the ascender and descender, which
    /// is what FreeType does for a font with no `vmtx`.
    #[test]
    fn vertical_metrics_fall_back_to_ascent_and_descent() {
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        let glyph = GlyphId::new(1);
        let context = BorrowedContext {
            glyf: font.glyf().unwrap(),
            loca: font.loca(None).unwrap(),
            advance: 0,
            vertical: None,
        };
        let (ascent, descent) = context.h_line_metrics();
        let mut owned = Outline::<Unscaled>::new();
        let outline = owned.load(&context, glyph).unwrap();
        // tsb = ascent - y_max, so y_max + tsb is just the ascent.
        assert_eq!(outline.phantom_points()[2].y, ascent);
        assert_eq!(outline.phantom_points()[3].y, descent);
    }
}
