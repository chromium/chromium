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
