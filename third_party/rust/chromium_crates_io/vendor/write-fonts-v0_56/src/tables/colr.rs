//! The [colr](https://learn.microsoft.com/en-us/typography/opentype/spec/colr) table

include!("../../generated/generated_colr.rs");

use super::variations::{DeltaSetIndexMap, ItemVariationStore};

impl Colr {
    fn compute_version(&self) -> u16 {
        // Using v1-only fields?
        if self.base_glyph_list.is_some()
            || self.layer_list.is_some()
            || self.clip_list.is_some()
            || self.var_index_map.is_some()
            || self.item_variation_store.is_some()
        {
            return 1;
        }
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::write::dump_table;
    use read_fonts::FontRead;

    #[test]
    fn paint_glyph2_writes_iso_bytes_and_roundtrips() {
        let paint: Paint = PaintGlyph2::new(
            PaintSolid::new(7, F2Dot14::ONE).into(),
            GlyphId24::new(0x123456),
        )
        .into();
        let data = dump_table(&paint).unwrap();
        assert_eq!(data, font_test_data::colr::PAINT_GLYPH2);
        let read = read_fonts::tables::colr::Paint::read(FontData::new(&data)).unwrap();
        let roundtrip: Paint = read.to_owned_table();
        assert_eq!(roundtrip, paint);
        assert_eq!(dump_table(&roundtrip).unwrap(), data);
    }

    #[test]
    fn colr_paint_glyph2_roundtrips() {
        let data = font_test_data::colr::paint_glyph2_colr();
        let read = read_fonts::tables::colr::Colr::read(FontData::new(&data)).unwrap();
        let owned: Colr = read.to_owned_table();
        let compiled = dump_table(&owned).unwrap();
        assert_eq!(compiled, data.as_slice());
    }
}
