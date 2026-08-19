//! Common code for variation store builders.
use std::collections::HashMap;

/// The special index indicating no variation.
pub const NO_VARIATION_INDEX: u32 = 0xFFFFFFFF;

pub(crate) type TemporaryDeltaSetId = u32;

/// A map from the temporary delta set identifiers to the final values.
///
/// This is generated when the [ItemVariationStore](crate::tables::variations::ItemVariationStore) is built; afterwards
/// any tables or records that contain VariationIndex tables need to be remapped.
#[derive(Clone, Debug, Default)]
pub struct VarStoreRemapping<T> {
    pub(crate) map: HashMap<TemporaryDeltaSetId, T>,
}

/// Remapping temporary delta set identifiers to the final values.
///
/// This is called after the [ItemVariationStore](crate::tables::variations::ItemVariationStore) has been built, at which
/// point any table containing a delta set index needs to be updated to point
/// to the final value.
///
/// This trait should be implemented by any table that contains delta set indices,
/// as well as for any of table containing such a table, which should recursively
/// call it on the relevant subtables.
pub trait RemapVarStore<T> {
    /// Remap any `TemporaryDeltaSetId`s to their final index values
    fn remap_variation_indices(&mut self, key_map: &VarStoreRemapping<T>);
}

impl<T: Clone> VarStoreRemapping<T> {
    pub(crate) fn set(&mut self, from: TemporaryDeltaSetId, to: T) {
        self.map.insert(from, to);
    }

    pub fn get(&self, from: TemporaryDeltaSetId) -> Option<T> {
        self.map.get(&from).cloned()
    }
}
