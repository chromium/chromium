//! The [COLR](https://docs.microsoft.com/en-us/typography/opentype/spec/colr) table

#[cfg(feature = "std")]
mod closure;

use super::variations::{DeltaSetIndexMap, ItemVariationStore};

include!("../../generated/generated_colr.rs");

/// Unique paint identifier used for detecting cycles in the paint graph.
pub type PaintId = usize;

impl<'a> Colr<'a> {
    /// Returns the COLRv0 base glyph for the given glyph identifier.
    ///
    /// The return value is a range of layer indices that can be passed to
    /// [`v0_layer`](Self::v0_layer) to retrieve the layer glyph identifiers
    /// and palette color indices.
    pub fn v0_base_glyph(&self, glyph_id: GlyphId) -> Option<Range<usize>> {
        let records = self.base_glyph_records()?.ok()?;
        let glyph_id = glyph_id.try_into().ok()?;
        let ix = records
            .binary_search_by(|rec| rec.glyph_id().cmp(&glyph_id))
            .ok()?;
        let record = records.get(ix)?;
        let start = record.first_layer_index() as usize;
        Some(start..start.checked_add(record.num_layers() as usize)?)
    }

    /// Returns the COLRv0 layer at the given index.
    ///
    /// The layer is represented by a tuple containing the glyph identifier of
    /// the associated outline and the palette color index.
    pub fn v0_layer(&self, index: usize) -> Option<(GlyphId16, u16)> {
        let layer = self.layer_records()?.ok()?.get(index)?;
        Some((layer.glyph_id(), layer.palette_index()))
    }

    /// Returns the COLRv1 base glyph for the given glyph identifier.
    ///
    /// The second value in the tuple is a unique identifier for the paint that
    /// may be used to detect recursion in the paint graph.
    pub fn v1_base_glyph(&self, glyph_id: GlyphId) -> Option<(Paint<'a>, PaintId)> {
        let glyph_id = glyph_id.try_into().ok()?;
        let list = self.base_glyph_list()?.ok()?;
        let records = list.base_glyph_paint_records();
        let ix = records
            .binary_search_by(|rec| rec.glyph_id().cmp(&glyph_id))
            .ok()?;
        let record = records.get(ix)?;
        let offset_data = list.offset_data();
        // Use the address of the paint as an identifier for the recursion
        // blacklist.
        let id = record.paint_offset().to_u32() as usize + offset_data.as_ref().as_ptr() as usize;
        Some((record.paint(offset_data).ok()?, id))
    }

    /// Returns the COLRv1 layer at the given index.
    ///
    /// The second value in the tuple is a unique identifier for the paint that
    /// may be used to detect recursion in the paint graph.
    pub fn v1_layer(&self, index: usize) -> Option<(Paint<'a>, PaintId)> {
        let list = self.layer_list()?.ok()?;
        let offset = list.paint_offsets().get(index)?.get();
        let offset_data = list.offset_data();
        // Use the address of the paint as an identifier for the recursion
        // blacklist.
        let id = offset.to_u32() as usize + offset_data.as_ref().as_ptr() as usize;
        Some((offset.resolve(offset_data).ok()?, id))
    }

    /// Returns the COLRv1 clip box for the given glyph identifier.
    pub fn v1_clip_box(&self, glyph_id: GlyphId) -> Option<ClipBox<'a>> {
        use core::cmp::Ordering;
        let glyph_id: GlyphId16 = glyph_id.try_into().ok()?;
        let list = self.clip_list()?.ok()?;
        let clips = list.clips();
        let ix = clips
            .binary_search_by(|clip| {
                if glyph_id < clip.start_glyph_id() {
                    Ordering::Greater
                } else if glyph_id > clip.end_glyph_id() {
                    Ordering::Less
                } else {
                    Ordering::Equal
                }
            })
            .ok()?;
        let clip = clips.get(ix)?;
        clip.clip_box(list.offset_data()).ok()
    }
}
