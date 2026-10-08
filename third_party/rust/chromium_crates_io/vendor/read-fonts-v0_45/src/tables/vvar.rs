//! The [VVAR (Vertical Metrics Variation)](https://docs.microsoft.com/en-us/typography/opentype/spec/vvar) table

use super::variations::{self, DeltaSetIndexMap, ItemVariationStore};
use types::F48Dot16;

include!("../../generated/generated_vvar.rs");

impl Vvar<'_> {
    /// Computes the scalar for each variation region at `coords`, in table
    /// order, and returns how many were written.
    ///
    /// See [`ItemVariationStore::compute_scalars`] for what `out` receives.
    ///
    /// [`ItemVariationStore::compute_scalars`]: crate::tables::variations::ItemVariationStore::compute_scalars
    pub fn compute_scalars(&self, coords: &[F2Dot14], out: &mut [Fixed]) -> usize {
        match self.item_variation_store() {
            Ok(store) => store.compute_scalars(coords, out),
            Err(_) => 0,
        }
    }

    /// Returns the change a location makes to the advance height of a glyph.
    ///
    /// The value carries every bit the item variation store computed. It is
    /// a caller that decides how to round it into a whole design unit, and
    /// implementations differ on that.
    ///
    /// Returns `None` where the table says nothing readable about the glyph.
    pub fn advance_delta(&self, glyph_id: GlyphId, coords: &[F2Dot14]) -> Option<F48Dot16> {
        self.advance_delta_with_scalars(glyph_id, coords, &[])
    }

    /// Returns the change a location makes to the advance height of a glyph, reusing
    /// `scalars`.
    ///
    /// `scalars` holds one entry per variation region, from
    /// [`compute_scalars`](Self::compute_scalars).
    pub fn advance_delta_with_scalars(
        &self,
        glyph_id: GlyphId,
        coords: &[F2Dot14],
        scalars: &[Fixed],
    ) -> Option<F48Dot16> {
        variations::advance_delta_with_scalars(
            self.advance_height_mapping(),
            self.item_variation_store(),
            glyph_id,
            coords,
            scalars,
        )
    }

    /// Returns the change a location makes to the top side bearing of a glyph.
    ///
    /// The value carries every bit the item variation store computed. It is
    /// a caller that decides how to round it into a whole design unit, and
    /// implementations differ on that.
    ///
    /// Returns `None` where the table says nothing readable about the glyph.
    pub fn tsb_delta(&self, glyph_id: GlyphId, coords: &[F2Dot14]) -> Option<F48Dot16> {
        self.tsb_delta_with_scalars(glyph_id, coords, &[])
    }

    /// Returns the change a location makes to the top side bearing of a glyph, reusing
    /// `scalars`.
    ///
    /// `scalars` holds one entry per variation region, from
    /// [`compute_scalars`](Self::compute_scalars).
    pub fn tsb_delta_with_scalars(
        &self,
        glyph_id: GlyphId,
        coords: &[F2Dot14],
        scalars: &[Fixed],
    ) -> Option<F48Dot16> {
        variations::item_delta_with_scalars(
            self.tsb_mapping(),
            self.item_variation_store(),
            glyph_id,
            coords,
            scalars,
        )
    }

    /// Returns the change a location makes to the bottom side bearing of a glyph.
    ///
    /// The value carries every bit the item variation store computed. It is
    /// a caller that decides how to round it into a whole design unit, and
    /// implementations differ on that.
    ///
    /// Returns `None` where the table says nothing readable about the glyph.
    pub fn bsb_delta(&self, glyph_id: GlyphId, coords: &[F2Dot14]) -> Option<F48Dot16> {
        self.bsb_delta_with_scalars(glyph_id, coords, &[])
    }

    /// Returns the change a location makes to the bottom side bearing of a glyph, reusing
    /// `scalars`.
    ///
    /// `scalars` holds one entry per variation region, from
    /// [`compute_scalars`](Self::compute_scalars).
    pub fn bsb_delta_with_scalars(
        &self,
        glyph_id: GlyphId,
        coords: &[F2Dot14],
        scalars: &[Fixed],
    ) -> Option<F48Dot16> {
        variations::item_delta_with_scalars(
            self.bsb_mapping(),
            self.item_variation_store(),
            glyph_id,
            coords,
            scalars,
        )
    }

    /// Returns the change a location makes to the y coordinate of the vertical origin of a glyph.
    ///
    /// The value carries every bit the item variation store computed. It is
    /// a caller that decides how to round it into a whole design unit, and
    /// implementations differ on that.
    ///
    /// Returns `None` where the table says nothing readable about the glyph.
    pub fn v_origin_y_delta(&self, glyph_id: GlyphId, coords: &[F2Dot14]) -> Option<F48Dot16> {
        self.v_origin_y_delta_with_scalars(glyph_id, coords, &[])
    }

    /// Returns the change a location makes to the y coordinate of the vertical origin of a glyph, reusing
    /// `scalars`.
    ///
    /// `scalars` holds one entry per variation region, from
    /// [`compute_scalars`](Self::compute_scalars).
    pub fn v_origin_y_delta_with_scalars(
        &self,
        glyph_id: GlyphId,
        coords: &[F2Dot14],
        scalars: &[Fixed],
    ) -> Option<F48Dot16> {
        variations::item_delta_with_scalars(
            self.v_org_mapping(),
            self.item_variation_store(),
            glyph_id,
            coords,
            scalars,
        )
    }
}

#[cfg(test)]
mod tests {
    use crate::{FontRef, TableProvider};
    use types::{F2Dot14, F48Dot16, GlyphId};

    /// The only fixture stating `VVAR`. Every one of its mappings resolves to
    /// zero at every location, so this covers the shape of the accessors
    /// rather than their arithmetic; `HVAR` carries that, over the same code
    /// in `variations`.
    const VAR: &[u8] = font_test_data::ift::CFF2_FONT;

    #[test]
    fn a_default_location_moves_nothing() {
        let font = FontRef::new(VAR).unwrap();
        let vvar = font.vvar().unwrap();
        let gid = GlyphId::new(1);
        assert_eq!(vvar.advance_delta(gid, &[]), Some(F48Dot16::ZERO));
    }

    #[test]
    fn every_glyph_answers_at_a_location() {
        let font = FontRef::new(VAR).unwrap();
        let vvar = font.vvar().unwrap();
        let num_glyphs = font.maxp().unwrap().num_glyphs() as u32;
        for coord in [-1.0, -0.4, 0.25, 1.0] {
            let coords = [F2Dot14::from_f32(coord)];
            for gid in (0..num_glyphs).map(GlyphId::new) {
                assert!(
                    vvar.advance_delta(gid, &coords).is_some(),
                    "glyph {gid} at {coord}"
                );
            }
        }
    }

    #[test]
    fn a_mapping_the_font_does_not_state_is_absent() {
        // This font maps advance heights and vertical origins, but neither
        // side bearing.
        let font = FontRef::new(VAR).unwrap();
        let vvar = font.vvar().unwrap();
        let gid = GlyphId::new(1);
        let coords = [F2Dot14::from_f32(-1.0)];
        assert!(vvar.advance_delta(gid, &coords).is_some());
        assert!(vvar.v_origin_y_delta(gid, &coords).is_some());
        assert_eq!(vvar.tsb_delta(gid, &coords), None);
        assert_eq!(vvar.bsb_delta(gid, &coords), None);
    }
}
