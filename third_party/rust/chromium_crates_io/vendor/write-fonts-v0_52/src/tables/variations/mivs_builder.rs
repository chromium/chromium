//! Building the MultiItemVariationStore for VARC
//!
//! See also [`VariationStoreBuilder`]. Where Item Variation Stores stores a
//! single scalar delta per region, [`MultiItemVariationStore`] stores a tuple
//! of deltas per region, and uses a sparse representation of regions
//! (only active axes are stored).
//!
//! [`VariationStoreBuilder`]: crate::tables::variations::ivs_builder::VariationStoreBuilder

use std::collections::HashMap;

use indexmap::IndexMap;
use types::F2Dot14;

use crate::{
    error::Error,
    ps::cff::v2::Index as Index2,
    tables::{
        varc::{
            MultiItemVariationData, MultiItemVariationStore, SparseRegionAxisCoordinates,
            SparseVariationRegion, SparseVariationRegionList,
        },
        variations::{
            common_builder::{TemporaryDeltaSetId, VarStoreRemapping, NO_VARIATION_INDEX},
            PackedDeltas,
        },
    },
};

pub type MultiVariationIndexRemapping = VarStoreRemapping<u32>;

/// A sparse region definition, containing only axes with non-zero peaks.
///
/// This is used as a key for deduplicating regions in the builder.
/// Each entry is (axis_index, start, peak, end).
#[derive(Clone, Debug, Hash, PartialEq, Eq)]
pub struct SparseRegion(Vec<(u16, F2Dot14, F2Dot14, F2Dot14)>);

impl SparseRegion {
    /// Create a new sparse region from axis coordinates.
    ///
    /// The coordinates should be in the form (axis_index, start, peak, end).
    /// The region will be sorted by axis index for consistent hashing.
    pub fn new(mut axes: Vec<(u16, F2Dot14, F2Dot14, F2Dot14)>) -> Self {
        // Sort by axis index for consistent hashing/equality
        axes.sort_by_key(|(idx, _, _, _)| *idx);
        // Filter out axes with zero peak (they don't contribute)
        axes.retain(|(_, _, peak, _)| peak.to_f32() != 0.0);
        Self(axes)
    }

    /// Returns true if this region has no active axes.
    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }

    fn to_sparse_variation_region(&self) -> SparseVariationRegion {
        let region_axis_offsets = self
            .0
            .iter()
            .map(|(axis_index, start, peak, end)| {
                SparseRegionAxisCoordinates::new(*axis_index, *start, *peak, *end)
            })
            .collect::<Vec<_>>();
        SparseVariationRegion::new(region_axis_offsets.len() as u16, region_axis_offsets)
    }
}

/// A delta set for MIVS: tuples of deltas for each region.
///
/// Unlike IVS where each region has a single scalar delta, MIVS has a tuple
/// of N values per region (e.g., for x,y coordinates or transform values).
#[derive(Clone, Debug, Hash, PartialEq, Eq)]
struct MultiDeltaSet {
    /// The tuple length (number of values per region).
    tuple_len: usize,
    /// Per-region delta tuples, stored as (region_index, [delta0, delta1, ...]).
    /// Sorted by region index.
    deltas: Vec<(u16, Vec<i32>)>,
}

impl MultiDeltaSet {
    fn new(tuple_len: usize, mut deltas: Vec<(u16, Vec<i32>)>) -> Result<Self, Error> {
        // Sort by region index
        deltas.sort_by_key(|(idx, _)| *idx);
        // Verify all tuples have the expected length
        if !deltas.iter().all(|(_, tuple)| tuple.len() == tuple_len) {
            return Err(Error::InvalidInput(
                "all delta tuples in MultiDeltaSet must have the same length",
            ));
        };
        // Filter out all-zero tuples
        deltas.retain(|(_, tuple)| tuple.iter().any(|v| *v != 0));
        Ok(Self { tuple_len, deltas })
    }

    fn is_empty(&self) -> bool {
        self.deltas.is_empty()
    }
}

/// A builder for the [`MultiItemVariationStore`]
///
/// This handles assigning VariationIndex values to unique sets of tuple deltas
/// and grouping delta sets into [`MultiItemVariationData`] subtables.
#[derive(Clone, Debug, Default)]
pub struct MultiItemVariationStoreBuilder {
    /// Maps SparseRegion to its index in the region list.
    all_regions: HashMap<SparseRegion, usize>,
    /// Deduplicates identical delta sets.
    /// Maps delta set -> temporary ID.
    delta_sets: IndexMap<MultiDeltaSet, TemporaryDeltaSetId>,
    /// Counter for generating temporary IDs.
    next_id: TemporaryDeltaSetId,
}

impl MultiItemVariationStoreBuilder {
    /// Create a new builder.
    pub fn new() -> Self {
        Self::default()
    }

    /// Returns `true` if no deltas have been added to this builder.
    pub fn is_empty(&self) -> bool {
        self.delta_sets.is_empty()
    }

    /// Add a set of tuple deltas and return a temporary ID.
    ///
    /// # Arguments
    ///
    /// * `deltas` - Vec of (SparseRegion, delta_tuple) pairs.
    ///   Each delta_tuple must have the same length.
    ///
    /// # Returns
    ///
    /// A temporary ID that can be used to retrieve the final VarIdx after
    /// calling [`build`](Self::build).
    ///
    /// Returns an error if the delta tuples have inconsistent lengths.
    pub fn add_deltas<T: Into<i32>>(
        &mut self,
        deltas: Vec<(SparseRegion, Vec<T>)>,
    ) -> Result<TemporaryDeltaSetId, Error> {
        // Determine tuple length from first non-empty entry
        let tuple_len = deltas
            .iter()
            .map(|(_, tuple)| tuple.len())
            .next()
            .unwrap_or(0);

        // Convert regions to indices and collect delta tuples
        let mut indexed_deltas = Vec::with_capacity(deltas.len());
        for (region, tuple) in deltas {
            assert_eq!(
                tuple.len(),
                tuple_len,
                "all delta tuples must have the same length"
            );
            if region.is_empty() {
                continue;
            }
            let region_idx = self.canonical_index_for_region(region) as u16;
            let converted_tuple: Vec<i32> = tuple.into_iter().map(|v| v.into()).collect();
            indexed_deltas.push((region_idx, converted_tuple));
        }

        let delta_set = MultiDeltaSet::new(tuple_len, indexed_deltas)?;

        // Return NO_VARIATION_INDEX for empty delta sets
        if delta_set.is_empty() {
            return Ok(NO_VARIATION_INDEX);
        }

        // Deduplicate
        if let Some(&existing_id) = self.delta_sets.get(&delta_set) {
            return Ok(existing_id);
        }

        let id = self.next_id;
        self.next_id += 1;
        self.delta_sets.insert(delta_set, id);
        Ok(id)
    }

    fn canonical_index_for_region(&mut self, region: SparseRegion) -> usize {
        let next_idx = self.all_regions.len();
        *self.all_regions.entry(region).or_insert(next_idx)
    }

    /// Build the [`MultiItemVariationStore`] table.
    ///
    /// This also returns a structure that can be used to remap the temporarily
    /// assigned delta set IDs to their final `VarIdx` values.
    pub fn build(self) -> (MultiItemVariationStore, MultiVariationIndexRemapping) {
        if self.delta_sets.is_empty() {
            // Return an empty store
            let region_list = SparseVariationRegionList::new(0, vec![]);
            let store = MultiItemVariationStore::new(region_list, 0, vec![]);
            return (store, MultiVariationIndexRemapping::default());
        }

        // Group delta sets by their region indices (like Python's _varDataIndices)
        let mut var_data_groups: IndexMap<Vec<u16>, Vec<(&MultiDeltaSet, TemporaryDeltaSetId)>> =
            IndexMap::new();

        for (delta_set, temp_id) in &self.delta_sets {
            let region_indices: Vec<u16> = delta_set.deltas.iter().map(|(idx, _)| *idx).collect();
            var_data_groups
                .entry(region_indices)
                .or_default()
                .push((delta_set, *temp_id));
        }

        // Build region list
        let region_list = self.build_region_list();

        // Build MultiVarData subtables
        let mut key_map = MultiVariationIndexRemapping::default();
        let mut var_data_tables = Vec::new();

        for (outer, (region_indices, delta_sets)) in var_data_groups.into_iter().enumerate() {
            // Split into chunks of 0xFFFF if needed
            for chunk in delta_sets.chunks(0xFFFF) {
                let subtable =
                    self.build_var_data(&region_indices, chunk, outer as u16, &mut key_map);
                var_data_tables.push(subtable);
            }
        }

        let store = MultiItemVariationStore::new(
            region_list,
            var_data_tables.len() as u16,
            var_data_tables,
        );

        (store, key_map)
    }

    fn build_region_list(&self) -> SparseVariationRegionList {
        // Sort regions by their canonical index
        let mut regions: Vec<_> = self.all_regions.iter().collect();
        regions.sort_by_key(|(_, idx)| *idx);

        let sparse_regions: Vec<SparseVariationRegion> = regions
            .into_iter()
            .map(|(region, _)| region.to_sparse_variation_region())
            .collect();

        SparseVariationRegionList::new(sparse_regions.len() as u16, sparse_regions)
    }

    fn build_var_data(
        &self,
        region_indices: &[u16],
        delta_sets: &[(&MultiDeltaSet, TemporaryDeltaSetId)],
        outer: u16,
        key_map: &mut MultiVariationIndexRemapping,
    ) -> MultiItemVariationData {
        // Build the CFF2 Index containing packed delta sets
        let mut items = Vec::new();

        for (inner, (delta_set, temp_id)) in delta_sets.iter().enumerate() {
            // Flatten the delta tuples in region order
            let flattened = self.flatten_deltas(delta_set, region_indices);

            // Encode as PackedDeltas (TupleValues)
            let packed = PackedDeltas::new(flattened);
            let mut encoded = Vec::new();
            // We need to manually encode since PackedDeltas uses TableWriter
            encode_packed_deltas(&packed, &mut encoded);

            items.push(encoded);

            // Record the mapping
            let var_idx = ((outer as u32) << 16) | (inner as u32);
            key_map.set(*temp_id, var_idx);
        }

        let index2 = Index2::from_items(items);
        let raw_delta_sets = crate::dump_table(&index2).expect("Index2 serialization failed");

        MultiItemVariationData::new(
            region_indices.len() as u16,
            region_indices.to_vec(),
            raw_delta_sets,
        )
    }

    /// Flatten delta tuples into a single vector in region order.
    ///
    /// For each region in `region_indices`, we output the corresponding tuple's
    /// values (or zeros if no delta exists for that region).
    fn flatten_deltas(&self, delta_set: &MultiDeltaSet, region_indices: &[u16]) -> Vec<i32> {
        let tuple_len = delta_set.tuple_len;
        let mut result = Vec::with_capacity(region_indices.len() * tuple_len);

        // Build a map from region index to delta tuple
        let delta_map: HashMap<u16, &Vec<i32>> = delta_set
            .deltas
            .iter()
            .map(|(idx, tuple)| (*idx, tuple))
            .collect();

        for &region_idx in region_indices {
            if let Some(tuple) = delta_map.get(&region_idx) {
                result.extend(tuple.iter().copied());
            } else {
                // No delta for this region - output zeros
                result.extend(std::iter::repeat_n(0, tuple_len));
            }
        }

        result
    }
}

/// Encode PackedDeltas to bytes.
///
/// This is a helper since PackedDeltas normally writes via TableWriter.
fn encode_packed_deltas(packed: &PackedDeltas, output: &mut Vec<u8>) {
    use crate::write::FontWrite;

    // Use a TableWriter to get the bytes
    let mut writer = crate::write::TableWriter::default();
    packed.write_into(&mut writer);

    // Extract the bytes from the writer's internal data
    let data = writer.into_data();
    output.extend(&data.bytes);
}

#[cfg(test)]
mod tests {
    use super::*;

    fn f2dot14(val: f32) -> F2Dot14 {
        F2Dot14::from_f32(val)
    }

    #[test]
    fn empty_builder() {
        let builder = MultiItemVariationStoreBuilder::new();
        assert!(builder.is_empty());

        let (store, _remap) = builder.build();
        assert_eq!(store.variation_data_count, 0);
    }

    #[test]
    fn single_delta_set() {
        let mut builder = MultiItemVariationStoreBuilder::new();

        let region = SparseRegion::new(vec![(0, f2dot14(0.0), f2dot14(1.0), f2dot14(1.0))]);

        // Add a 2-tuple delta
        let temp_id = builder.add_deltas(vec![(region, vec![10, 20])]).unwrap();

        assert!(temp_id != NO_VARIATION_INDEX);
        assert!(!builder.is_empty());

        let (store, remap) = builder.build();

        // Should have 1 region
        assert_eq!(store.region_list.region_count, 1);

        // Should have 1 var data subtable
        assert_eq!(store.variation_data_count, 1);

        // The temp_id should map to (0, 0)
        let var_idx = remap.get(temp_id).unwrap();
        assert_eq!(var_idx >> 16, 0); // outer = 0
        assert_eq!(var_idx & 0xFFFF, 0); // inner = 0
    }

    #[test]
    fn deduplication() {
        let mut builder = MultiItemVariationStoreBuilder::new();

        let region = SparseRegion::new(vec![(0, f2dot14(0.0), f2dot14(1.0), f2dot14(1.0))]);

        // Add the same delta set twice
        let id1 = builder
            .add_deltas(vec![(region.clone(), vec![10, 20])])
            .unwrap();
        let id2 = builder.add_deltas(vec![(region, vec![10, 20])]).unwrap();

        // Should get the same ID
        assert_eq!(id1, id2);
    }

    #[test]
    fn empty_delta_returns_no_variation_index() {
        let mut builder = MultiItemVariationStoreBuilder::new();

        // Empty deltas
        let id = builder.add_deltas::<i32>(vec![]).unwrap();
        assert_eq!(id, NO_VARIATION_INDEX);

        // All-zero deltas
        let region = SparseRegion::new(vec![(0, f2dot14(0.0), f2dot14(1.0), f2dot14(1.0))]);
        let id = builder.add_deltas(vec![(region, vec![0, 0])]).unwrap();
        assert_eq!(id, NO_VARIATION_INDEX);
    }

    #[test]
    fn multiple_regions() {
        let mut builder = MultiItemVariationStoreBuilder::new();

        let region1 = SparseRegion::new(vec![(0, f2dot14(0.0), f2dot14(1.0), f2dot14(1.0))]);
        let region2 = SparseRegion::new(vec![(1, f2dot14(0.0), f2dot14(1.0), f2dot14(1.0))]);

        let temp_id = builder
            .add_deltas(vec![(region1, vec![10, 20]), (region2, vec![30, 40])])
            .unwrap();

        assert!(temp_id != NO_VARIATION_INDEX);

        let (store, _remap) = builder.build();

        // Should have 2 regions
        assert_eq!(store.region_list.region_count, 2);
    }
}
