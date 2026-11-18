//! The [BASE](https://learn.microsoft.com/en-us/typography/opentype/spec/base) table

use super::layout::DeviceOrVariationIndex;
use super::variations::ItemVariationStore;

include!("../../generated/generated_base.rs");

impl Base {
    fn compute_version(&self) -> MajorMinor {
        if self.item_var_store.is_some() {
            MajorMinor::VERSION_1_1
        } else {
            MajorMinor::VERSION_1_0
        }
    }
}
