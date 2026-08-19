//! the [GDEF] table
//!
//! [GDEF]: https://docs.microsoft.com/en-us/typography/opentype/spec/gdef

use types::MajorMinor;

use crate::tables::layout::VariationIndex;

use super::{
    layout::{ClassDef, CoverageTable, DeviceOrVariationIndex},
    variations::{
        common_builder::RemapVarStore, ivs_builder::VariationIndexRemapping, ItemVariationStore,
    },
};

include!("../../generated/generated_gdef.rs");

impl Gdef {
    fn compute_version(&self) -> MajorMinor {
        if self.item_var_store.is_some() {
            MajorMinor::VERSION_1_3
        } else if self.mark_glyph_sets_def.is_some() {
            MajorMinor::VERSION_1_2
        } else {
            MajorMinor::VERSION_1_0
        }
    }
}

impl RemapVarStore<VariationIndex> for Gdef {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        if let Some(ligs) = self.lig_caret_list.as_mut() {
            ligs.remap_variation_indices(key_map);
        }
    }
}

impl RemapVarStore<VariationIndex> for LigCaretList {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        self.lig_glyphs.iter_mut().for_each(|lig| {
            lig.caret_values
                .iter_mut()
                .for_each(|caret| caret.remap_variation_indices(key_map))
        })
    }
}

impl RemapVarStore<VariationIndex> for CaretValue {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        if let CaretValue::Format3(table) = self {
            table.remap_variation_indices(key_map)
        }
    }
}

impl RemapVarStore<VariationIndex> for CaretValueFormat3 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        self.device.remap_variation_indices(key_map)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn var_store_without_glyph_sets() {
        // this should compile, and version should be 1.3
        let gdef = Gdef {
            item_var_store: ItemVariationStore::default().into(),
            ..Default::default()
        };

        assert_eq!(gdef.compute_version(), MajorMinor::VERSION_1_3);
        let _dumped = crate::write::dump_table(&gdef).unwrap();
        let data = FontData::new(&_dumped);
        let loaded = read_fonts::tables::gdef::Gdef::read(data).unwrap();

        assert_eq!(loaded.version(), MajorMinor::VERSION_1_3);
        assert!(!loaded.item_var_store_offset().unwrap().is_null());
    }
}
