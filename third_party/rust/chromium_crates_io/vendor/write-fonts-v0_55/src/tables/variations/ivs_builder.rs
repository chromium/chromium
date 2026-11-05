//! Building the ItemVariationStore

use std::{
    cmp::Reverse,
    collections::{BinaryHeap, HashMap, HashSet},
    fmt::{Debug, Display},
};

use crate::tables::{
    layout::VariationIndex,
    variations::{
        common_builder::{TemporaryDeltaSetId, VarStoreRemapping},
        ItemVariationData, ItemVariationStore, VariationRegion, VariationRegionList,
    },
};
use indexmap::IndexMap;

pub type VariationIndexRemapping = VarStoreRemapping<VariationIndex>;

/// A builder for the [ItemVariationStore].
///
/// This handles assigning VariationIndex values to unique sets of deltas and
/// grouping delta sets into [ItemVariationData] subtables.
#[derive(Clone, Debug)]
pub struct VariationStoreBuilder {
    // region -> index map
    all_regions: HashMap<VariationRegion, usize>,
    delta_sets: DeltaSetStorage,
    // must match fvar. We require the user to pass this in because we cannot
    // infer it in the case where no deltas are added to the builder.
    axis_count: u16,
}

/// A collection of delta sets.
#[derive(Clone, Debug)]
enum DeltaSetStorage {
    // only for hvar: we do not deduplicate deltas, and store one per glyph id
    Direct(Vec<DeltaSet>),
    // the general case, where each delta gets a unique id
    Deduplicated(IndexMap<DeltaSet, TemporaryDeltaSetId>),
}

/// Always sorted, so we can ensure equality
///
/// Each tuple is (region index, delta value)
#[derive(Clone, Debug, Default, Hash, PartialEq, Eq)]
struct DeltaSet(Vec<(u16, i32)>);

impl VariationStoreBuilder {
    /// Create a builder that will optimize delta storage.
    ///
    /// This is the general case. For HVAR, it is also possible to use the
    /// glyph ids as implicit indices, which may be more efficient for some
    /// data. To use implicit indices, use [`new_with_implicit_indices`] instead.
    ///
    /// [`new_with_implicit_indices`]: VariationStoreBuilder::new_with_implicit_indices
    pub fn new(axis_count: u16) -> Self {
        Self {
            axis_count,
            delta_sets: DeltaSetStorage::Deduplicated(Default::default()),
            all_regions: Default::default(),
        }
    }

    /// Returns `true` if no deltas have been added to this builder
    pub fn is_empty(&self) -> bool {
        match &self.delta_sets {
            DeltaSetStorage::Direct(val) => val.is_empty(),
            DeltaSetStorage::Deduplicated(val) => val.is_empty(),
        }
    }

    /// Create a builder that does not share deltas between entries.
    ///
    /// This is used in HVAR, where it is possible to use glyph ids as the
    /// 'inner index', and to generate a single ItemVariationData subtable
    /// with one entry per item.
    pub fn new_with_implicit_indices(axis_count: u16) -> Self {
        VariationStoreBuilder {
            axis_count,
            all_regions: Default::default(),
            delta_sets: DeltaSetStorage::Direct(Default::default()),
        }
    }

    pub fn add_deltas<T: Into<i32>>(
        &mut self,
        deltas: Vec<(VariationRegion, T)>,
    ) -> TemporaryDeltaSetId {
        let mut delta_set = Vec::with_capacity(deltas.len());
        for (region, delta) in deltas {
            let region_idx = self.canonical_index_for_region(region) as u16;
            delta_set.push((region_idx, delta.into()));
        }
        // Strip zero-valued entries before deduplication. A zero delta for a region
        // is semantically equivalent to not specifying that region at all (the
        // interpolation engine defaults to 0), so two delta sets that differ only by
        // explicit zeros must be treated as identical.
        //
        // A concrete case: a glyph with an intermediate master whose advance equals
        // the interpolated value produces an explicit zero for that region; without
        // stripping it would not deduplicate against an otherwise-identical glyph that
        // simply lacks that intermediate master.
        //
        // fonttools achieves the same result via VarStore_optimize, which expands every
        // row to the full region width (padding absent regions with 0) before
        // deduplication:
        // https://github.com/fonttools/fonttools/blob/772918952/Lib/fontTools/varLib/varStore.py#L575-L598
        delta_set.retain(|(_, delta)| *delta != 0);
        delta_set.sort_unstable();
        self.delta_sets.add(DeltaSet(delta_set))
    }

    fn canonical_index_for_region(&mut self, region: VariationRegion) -> usize {
        let next_idx = self.all_regions.len();
        *self.all_regions.entry(region).or_insert(next_idx)
    }

    fn make_region_list(&self, subtables: &mut [Option<ItemVariationData>]) -> VariationRegionList {
        // collect the set of region indices actually used by each ItemVariationData
        let used_regions = subtables
            .iter()
            .flatten()
            .flat_map(|var_data| var_data.region_indexes.iter())
            .map(|idx| *idx as usize)
            .collect::<HashSet<_>>();
        // prune unused regions and keep track of old index to new index
        let mut region_list = self
            .all_regions
            .iter()
            .filter_map(|(reg, idx)| {
                if used_regions.contains(idx) {
                    Some((idx, reg.to_owned()))
                } else {
                    None
                }
            })
            .collect::<Vec<_>>();
        region_list.sort_unstable();
        let mut new_regions = Vec::new();
        let mut region_map = HashMap::new();
        for (old_idx, reg) in region_list.into_iter() {
            region_map.insert(*old_idx as u16, new_regions.len() as u16);
            new_regions.push(reg);
        }
        // remap the region indexes in each subtable
        for var_data in subtables.iter_mut().flatten() {
            var_data.region_indexes = var_data
                .region_indexes
                .iter()
                .map(|idx| region_map[idx])
                .collect();
        }
        VariationRegionList::new(self.axis_count, new_regions)
    }

    fn encoder(&self) -> Encoder<'_> {
        Encoder::new(&self.delta_sets, self.all_regions.len() as u16)
    }

    /// Build the `ItemVariationStore` table
    ///
    /// This also returns a structure that can be used to remap the temporarily
    /// assigned delta set Ids to their final `VariationIndex` values.
    pub fn build(self) -> (ItemVariationStore, VariationIndexRemapping) {
        let mut key_map = VariationIndexRemapping::default();
        let mut subtables = if matches!(self.delta_sets, DeltaSetStorage::Direct(_)) {
            vec![self.build_unoptimized(&mut key_map)]
        } else {
            let mut encoder = self.encoder();
            encoder.optimize();
            encoder.encode(&mut key_map)
        };
        let region_list = self.make_region_list(&mut subtables);
        (ItemVariationStore::new(region_list, subtables), key_map)
    }

    /// Build a single ItemVariationData subtable
    fn build_unoptimized(
        &self,
        key_map: &mut VariationIndexRemapping,
    ) -> Option<ItemVariationData> {
        // first pick an encoding capable of representing all items:
        let n_regions = self.all_regions.len() as u16;
        let mut shape = RowShape(vec![ColumnBits::None; n_regions as usize]);
        let mut temp = RowShape::default();

        for (delta, _) in self.delta_sets.iter() {
            temp.reuse(delta, n_regions);
            if !shape.can_cover(&temp) {
                shape = shape.merge(&temp);
            }
        }

        // then encode everything with that encoding.
        let encoding = Encoding {
            shape,
            deltas: self.delta_sets.iter().collect(),
        };
        debug_assert!(
            encoding.deltas.len() <= u16::MAX as usize,
            "unmapped variation store supports at most u16::MAX items"
        );
        encoding.encode(key_map, 0)
    }
}

/// A context for encoding deltas into the final [`ItemVariationStore`].
///
/// This mostly exists so that we can write better tests.
struct Encoder<'a> {
    encodings: Vec<Encoding<'a>>,
}

/// A set of deltas that share a shape.
struct Encoding<'a> {
    shape: RowShape,
    deltas: Vec<(&'a DeltaSet, TemporaryDeltaSetId)>,
}

/// A type for remapping delta sets during encoding.
///
/// This mapping applies to a single ItemVariationData table, with the regions
/// defined in the VariationRegionList in the parent table.
struct RegionMap {
    /// A map from the canonical region indices (represented in the sorted
    /// order of the map) to the ordering of the deltas in a particular
    /// ItemVariationData table.
    ///
    /// For each canonical index, we store the local (column) index and the bits
    /// required to store that column.
    regions_to_columns: Vec<(u16, ColumnBits)>,
    n_active_regions: u16,
    n_long_regions: u16,
    long_words: bool,
}

/// Describes the compressability of a row of deltas across all variation regions.
///
/// fonttools calls this the 'characteristic' of a row.
///
/// We could get much fancier about how we represent this type, and avoid
/// allocation in most cases; but this is simple and works, so :shrug:
#[derive(Debug, Clone, Default, PartialEq, Eq, Hash, PartialOrd, Ord)]
struct RowShape(Vec<ColumnBits>);

//NOTE: we could do fancier bit packing here (fonttools uses four bits per
//column but I think the gains will be marginal)
/// The number of bits required to represent a given delta column.
#[repr(u8)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
enum ColumnBits {
    /// i.e. the value is zero
    None = 0,
    /// an i8
    One = 1,
    /// an i16
    Two = 2,
    /// an i32
    Four = 4,
}

impl<'a> Encoder<'a> {
    fn new(delta_map: &'a DeltaSetStorage, total_regions: u16) -> Self {
        let mut shape = RowShape::default();
        let mut encodings: IndexMap<_, Vec<_>> = Default::default();

        for (delta, idx) in delta_map.iter() {
            shape.reuse(delta, total_regions);
            match encodings.get_mut(&shape) {
                Some(items) => items.push((delta, idx)),
                None => {
                    encodings.insert(shape.clone(), vec![(delta, idx)]);
                }
            }
        }
        let encodings = encodings
            .into_iter()
            .map(|(shape, deltas)| Encoding { shape, deltas })
            .collect();

        Encoder { encodings }
    }

    fn cost(&self) -> usize {
        self.encodings.iter().map(Encoding::cost).sum()
    }

    /// Recursively combine encodings where doing so provides space savings.
    ///
    /// This is a reimplementation of the [VarStore_optimize][fonttools] function
    /// in fonttools, although it is not a direct port.
    ///
    /// [fonttools]: https://github.com/fonttools/fonttools/blob/fb56e7b7c9715895b81708904c840875008adb9c/Lib/fontTools/varLib/varStore.py#L471
    fn optimize(&mut self) {
        let cost = self.cost();
        log::trace!("optimizing {} encodings, {cost}B", self.encodings.len(),);
        // a little helper for pretty-printing our todo list
        struct DebugTodoList<'a>(&'a [Option<Encoding<'a>>]);
        impl Debug for DebugTodoList<'_> {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                write!(f, "Todo({} items", self.0.len())?;
                for (i, enc) in self.0.iter().enumerate() {
                    if let Some(enc) = enc {
                        write!(f, "\n    {i:>4}: {enc:?}")?;
                    }
                }
                writeln!(f, ")")
            }
        }
        // temporarily take ownership of all the encodings
        let mut to_process = std::mem::take(&mut self.encodings);
        // pre-sort them like fonttools does, for stability
        to_process.sort_unstable_by(Encoding::ord_matching_fonttools);
        // convert to a vec of Option<Encoding>;
        // we will replace items with None as they are combined
        let mut to_process = to_process.into_iter().map(Option::Some).collect::<Vec<_>>();

        // build up a priority list of the space savings from combining each pair
        // of encodings
        let mut queue = BinaryHeap::with_capacity(to_process.len());

        for (i, red) in to_process.iter().enumerate() {
            for (j, blue) in to_process.iter().enumerate().skip(i + 1) {
                let gain = red.as_ref().unwrap().compute_gain(blue.as_ref().unwrap());
                if gain > 0 {
                    log::trace!("adding ({i}, {j} ({gain})) to queue");
                    // negate gain to match fonttools and use std::cmp::Reverse to
                    // mimic Python heapq's "min heap"
                    queue.push(Reverse((-gain, i, j)));
                }
            }
        }

        // iteratively process each item in the queue
        while let Some(Reverse((_gain, i, j))) = queue.pop() {
            if to_process[i].is_none() || to_process[j].is_none() {
                continue;
            }
            // as items are combined, we leave `None` in the to_process list.
            // This ensures that indices are stable.
            let (Some(mut to_update), Some(to_add)) = (
                to_process.get_mut(i).and_then(Option::take),
                to_process.get_mut(j).and_then(Option::take),
            ) else {
                unreachable!("checked above")
            };
            log::trace!(
                "combining {i}+{j} ({}, {} {_gain})",
                to_update.shape,
                to_add.shape
            );

            //NOTE: it is now possible that we have duplicate data. I'm not sure
            //how likely this is? Not very likely? it would require one deltaset's
            //regions to be a subset of another, with zeros for the missing axes?
            to_update.merge_with(to_add);
            let n = to_process.len(); // index we assign the combined encoding
            let mut maybe_existing_encoding = None;
            for (ii, opt_encoding) in to_process.iter_mut().enumerate() {
                // does two things: skips empty indices, and also temporarily
                // removes the item (we'll put it back unless we merge, below)
                let Some(encoding) = opt_encoding.take() else {
                    continue;
                };

                if encoding.shape == to_update.shape {
                    // if an identical encoding exists in the list, we will just
                    // merge it with the newly created one. We do this after
                    // calculating the new gains, though, so we aren't changing
                    // anything mid-stream
                    maybe_existing_encoding = Some(encoding);
                    continue;
                }
                let gain = to_update.compute_gain(&encoding);
                if gain > 0 {
                    log::trace!("adding ({n}, {ii} ({gain})) to queue");
                    queue.push(Reverse((-gain, ii, n)));
                }
                *opt_encoding = Some(encoding);
            }
            if let Some(existing) = maybe_existing_encoding.take() {
                to_update.deltas.extend(existing.deltas);
            }
            to_process.push(Some(to_update));
            log::trace!("{:?}", DebugTodoList(&to_process));
        }
        self.encodings = to_process.into_iter().flatten().collect();
        // now sort the items in each individual encoding
        self.encodings
            .iter_mut()
            .for_each(|enc| enc.deltas.sort_unstable());
        // and then sort the encodings themselves; order doesn't matter,
        // but we want to match fonttools output when comparing ttx
        self.encodings
            .sort_unstable_by(Encoding::ord_matching_fonttools);
        log::trace!(
            "optimized {} encodings, {}B, ({}B saved)",
            self.encodings.len(),
            self.cost(),
            cost.saturating_sub(self.cost()),
        );
    }

    /// Encode the `Encoding` sets into [`ItemVariationData`] subtables.
    ///
    /// In general, each encoding ends up being one subtable, except:
    /// - if the encoding is empty, we get a `NULL` subtable (aka None)
    /// - if an encoding contains more than 0xFFFF rows, it is split into
    ///   multiple subtables.
    fn encode(self, key_map: &mut VariationIndexRemapping) -> Vec<Option<ItemVariationData>> {
        self.encodings
            .into_iter()
            .flat_map(Encoding::iter_split_into_table_size_chunks)
            .enumerate()
            .map(|(i, encoding)| encoding.encode(key_map, i as u16))
            .collect()
    }
}

impl ColumnBits {
    fn for_val(val: i32) -> Self {
        if val == 0 {
            Self::None
        } else if i8::try_from(val).is_ok() {
            Self::One
        } else if i16::try_from(val).is_ok() {
            Self::Two
        } else {
            Self::Four
        }
    }

    /// The number of bytes required to store this column
    fn cost(self) -> usize {
        self as u8 as _
    }
}

impl RowShape {
    /// Reuse this types storage for a new delta set.
    ///
    /// This might be premature optimization.
    ///
    /// The rationale is that many of these are identical, so this saves us
    /// from constantly allocating and throwing away.
    fn reuse(&mut self, deltas: &DeltaSet, n_regions: u16) {
        self.0.clear();
        self.0.resize(n_regions as _, ColumnBits::None);
        for (region, delta) in &deltas.0 {
            self.0[*region as usize] = ColumnBits::for_val(*delta);
        }
    }

    /// Returns a shape that can fit both self and other.
    ///
    /// In practice this means taking the max of each column.
    fn merge(&self, other: &Self) -> Self {
        Self(
            self.0
                .iter()
                .zip(other.0.iter())
                .map(|(us, them)| *us.max(them))
                .collect(),
        )
    }

    /// `true` if each value in this shape is >= the same value in `other`.
    fn can_cover(&self, other: &Self) -> bool {
        debug_assert_eq!(self.0.len(), other.0.len());
        self.0
            .iter()
            .zip(other.0.iter())
            .all(|(us, them)| us >= them)
    }

    /// the cost in bytes of a row in this encoding
    fn row_cost(&self) -> usize {
        self.0.iter().copied().map(ColumnBits::cost).sum()
    }

    fn overhead(&self) -> usize {
        /// the minimum number of bytes in an ItemVariationData table
        const SUBTABLE_FIXED_COST: usize = 10;
        const COST_PER_REGION: usize = 2;
        SUBTABLE_FIXED_COST + (self.n_non_zero_regions() * COST_PER_REGION)
    }

    fn n_non_zero_regions(&self) -> usize {
        self.0.iter().map(|x| (*x as u8).min(1) as usize).sum()
    }

    /// return a tuple for the counts of (1, 2, 3) byte-encoded items in self
    fn count_lengths(&self) -> (u16, u16, u16) {
        self.0
            .iter()
            .fold((0, 0, 0), |(byte, short, long), this| match this {
                ColumnBits::One => (byte + 1, short, long),
                ColumnBits::Two => (byte, short + 1, long),
                ColumnBits::Four => (byte, short, long + 1),
                _ => (byte, short, long),
            })
    }

    /// Returns a struct that maps the canonical regions to the column indices
    /// used in this ItemVariationData.
    fn region_map(&self) -> RegionMap {
        let mut with_idx = self.0.iter().copied().enumerate().collect::<Vec<_>>();
        // sort in descending order of bit size, e.g. big first
        with_idx.sort_unstable_by_key(|(idx, bit)| (std::cmp::Reverse(*bit), *idx));
        // now build a map of indexes from the original positions to the new ones.
        let mut map = vec![(0u16, ColumnBits::None); with_idx.len()];
        for (new_idx, (canonical_idx, bits)) in with_idx.iter().enumerate() {
            map[*canonical_idx] = (new_idx as _, *bits);
        }

        let (count_8, count_16, count_32) = self.count_lengths();
        let long_words = count_32 > 0;
        let n_long_regions = if long_words { count_32 } else { count_16 };
        let n_active_regions = count_8 + count_16 + count_32;
        RegionMap {
            regions_to_columns: map,
            n_active_regions,
            n_long_regions,
            long_words,
        }
    }

    // for verifying our sorting behaviour.
    // ported from https://github.com/fonttools/fonttools/blob/ec9986d3b863d/Lib/fontTools/varLib/varStore.py#L441
    #[cfg(test)]
    fn to_fonttools_repr(&self) -> u128 {
        assert!(
            self.0.len() <= u128::BITS as usize / 4,
            "we can only pack 128 bits"
        );

        let has_long_word = self.0.contains(&ColumnBits::Four);
        let mut chars = 0;
        let mut i = 1;

        if !has_long_word {
            for v in &self.0 {
                if *v != ColumnBits::None {
                    chars += i;
                }
                if *v == ColumnBits::Two {
                    chars += i * 0b0010;
                }
                i <<= 4;
            }
        } else {
            for v in &self.0 {
                if *v != ColumnBits::None {
                    chars += i * 0b0011;
                }
                if *v == ColumnBits::Four {
                    chars += i * 0b1100;
                }
                i <<= 4;
            }
        }
        chars
    }
}

impl<'a> Encoding<'a> {
    fn cost(&self) -> usize {
        self.shape.overhead() + (self.shape.row_cost() * self.deltas.len())
    }

    fn compute_gain(&self, other: &Encoding) -> i64 {
        let current_cost = self.cost() + other.cost();

        let combined = self.shape.merge(&other.shape);
        let combined_cost =
            combined.overhead() + (combined.row_cost() * (self.deltas.len() + other.deltas.len()));
        current_cost as i64 - combined_cost as i64
    }

    fn merge_with(&mut self, other: Encoding<'a>) {
        self.shape = self.shape.merge(&other.shape);
        self.deltas.extend(other.deltas);
    }

    /// Split this item into chunks that fit in an ItemVariationData subtable.
    ///
    /// we can only encode up to u16::MAX items in a single subtable, so if we
    /// have more items than that we split them off now.
    fn iter_split_into_table_size_chunks(self) -> impl Iterator<Item = Encoding<'a>> {
        let mut next = Some(self);
        std::iter::from_fn(move || {
            let mut this = next.take()?;
            next = this.split_off_back();
            Some(this)
        })
    }

    /// If we contain more than the max allowed items, split the extra items off
    ///
    /// This ensures `self` can be encoded.
    fn split_off_back(&mut self) -> Option<Self> {
        const MAX_ITEMS: usize = 0xFFFF;
        if self.deltas.len() <= MAX_ITEMS {
            return None;
        }
        let deltas = self.deltas.split_off(MAX_ITEMS);
        Some(Self {
            shape: self.shape.clone(),
            deltas,
        })
    }

    fn encode(
        self,
        key_map: &mut VariationIndexRemapping,
        subtable_idx: u16,
    ) -> Option<ItemVariationData> {
        log::trace!(
            "encoding subtable {subtable_idx} ({} rows, {}B)",
            self.deltas.len(),
            self.cost()
        );
        assert!(self.deltas.len() <= 0xffff, "call split_off_back first");
        let item_count = self.deltas.len() as u16;
        if item_count == 0 {
            //TODO: figure out when a null subtable is useful?
            return None;
        }

        let region_map = self.shape.region_map();
        let n_regions = self.shape.n_non_zero_regions();
        let total_n_delta_values = self.deltas.len() * n_regions;
        let mut raw_deltas = vec![0i32; total_n_delta_values];

        // first we generate a vec of i32s, which represents an uncompressed
        // 2d array where rows are items and columns are per-region values.
        for (i, (delta, raw_key)) in self.deltas.iter().enumerate() {
            let pos = i * n_regions;
            for (region, val) in &delta.0 {
                let Some(column_idx) = region_map.column_index_for_region(*region) else {
                    continue;
                };
                let idx = pos + column_idx as usize;
                raw_deltas[idx] = *val;
            }
            let final_key = VariationIndex::new(subtable_idx, i as u16);
            key_map.set(*raw_key, final_key);
        }

        // then we convert the correctly-ordered i32s into the final compressed
        // representation.
        let delta_sets = region_map.encode_raw_delta_values(raw_deltas);
        let word_delta_count = region_map.word_delta_count();
        let region_indexes = region_map.indices();

        Some(ItemVariationData::new(
            item_count,
            word_delta_count,
            region_indexes,
            delta_sets,
        ))
    }

    /// match the sorting behaviour that fonttools uses for the final sorting.
    ///
    /// fonttools's behaviour is particular, because they store the 'rowshape' as
    /// a packed bitvec with the least significant bits storing the first item,
    /// e.g. it's the inverse of our default order. Also we don't want to include
    /// our temporary ids.
    fn ord_matching_fonttools(&self, other: &Self) -> std::cmp::Ordering {
        // first just compare the cost
        let cost_ord = self.shape.row_cost().cmp(&other.shape.row_cost());
        if cost_ord != std::cmp::Ordering::Equal {
            return cost_ord;
        }

        debug_assert_eq!(
            self.shape.0.len(),
            other.shape.0.len(),
            "all shapes have same # of regions"
        );

        // if cost is equal, compare each column, in reverse
        for (a, b) in self.shape.0.iter().rev().zip(other.shape.0.iter().rev()) {
            match a.cmp(b) {
                std::cmp::Ordering::Equal => (), // continue
                not_eq => return not_eq,
            }
        }
        std::cmp::Ordering::Equal
    }
}

impl RegionMap {
    /// Takes the delta data as a vec of i32s, writes a vec of BigEndian bytes.
    ///
    /// This is mostly boilerplate around whether we are writing i16 and i8, or
    /// i32 and i16.
    ///
    /// Invariant: the raw deltas are sorted based on the region ordering of this
    /// RegionMap.
    fn encode_raw_delta_values(&self, raw_deltas: Vec<i32>) -> Vec<u8> {
        // handles the branching logic of whether long words are 32 or 16 bits.
        fn encode_words<'a>(
            long: &'a [i32],
            short: &'a [i32],
            long_words: bool,
        ) -> impl Iterator<Item = u8> + 'a {
            // dumb trick: the two branches have different concrete types,
            // so we need to unify them
            let left = long_words.then(|| {
                long.iter()
                    .flat_map(|x| x.to_be_bytes().into_iter())
                    .chain(short.iter().flat_map(|x| (*x as i16).to_be_bytes()))
            });
            let right = (!long_words).then(|| {
                long.iter()
                    .flat_map(|x| (*x as i16).to_be_bytes().into_iter())
                    .chain(short.iter().flat_map(|x| (*x as i8).to_be_bytes()))
            });

            // combine the two branches into a single type
            left.into_iter()
                .flatten()
                .chain(right.into_iter().flatten())
        }

        if self.n_active_regions == 0 {
            return Default::default();
        }

        raw_deltas
            .chunks(self.n_active_regions as usize)
            .flat_map(|delta_set| {
                let (long, short) = delta_set.split_at(self.n_long_regions as usize);
                encode_words(long, short, self.long_words)
            })
            .collect()
    }

    /// Compute the 'wordDeltaCount' field
    ///
    /// This is a packed field, with the high bit indicating if we have 2-or-4-bit
    /// words, and the low 15 bits indicating the number of 'long' types
    fn word_delta_count(&self) -> u16 {
        let long_flag = if self.long_words { 0x8000 } else { 0 };
        self.n_long_regions | long_flag
    }

    /// For the provided canonical region index, returns the column index used
    /// in this encoding, or None if the region is ignored.
    fn column_index_for_region(&self, region: u16) -> Option<u16> {
        let (column, bits) = self.regions_to_columns[region as usize];
        (bits != ColumnBits::None).then_some(column)
    }

    /// the indexes into the canonical region list of the active columns
    fn indices(&self) -> Vec<u16> {
        let mut result: Vec<_> = self
            .regions_to_columns
            .iter()
            .enumerate()
            .filter_map(|(i, (_, bits))| (*bits as u8 > 0).then_some(i as _))
            .collect();
        // we need this result to be sorted based on the local order:
        result.sort_unstable_by_key(|region_idx| {
            self.regions_to_columns
                .get(*region_idx as usize)
                .map(|(column, _)| *column)
                // this can't fail since we got the indexes from this array
                // immediately previously, but this probably generates better
                // code than an unwrap
                .unwrap_or(u16::MAX)
        });
        result
    }
}

impl VariationIndexRemapping {
    /// convert to tuple for easier comparisons in tests
    #[cfg(test)]
    fn get_raw(&self, from: TemporaryDeltaSetId) -> Option<(u16, u16)> {
        self.map
            .get(&from)
            .map(|var| (var.delta_set_outer_index, var.delta_set_inner_index))
    }
}

impl DeltaSetStorage {
    fn add(&mut self, delta_set: DeltaSet) -> TemporaryDeltaSetId {
        match self {
            DeltaSetStorage::Direct(deltas) => {
                let next_id = deltas.len() as u32;
                deltas.push(delta_set);
                next_id
            }
            DeltaSetStorage::Deduplicated(deltas) => {
                let next_id = deltas.len() as u32;
                *deltas.entry(delta_set).or_insert(next_id)
            }
        }
    }

    fn iter(&self) -> impl Iterator<Item = (&DeltaSet, TemporaryDeltaSetId)> + '_ {
        // a dumb trick so that we are returning a single concrete type regardless
        // of which variant this is (which is required when returning impl Trait)
        let (a_vec, a_map) = match self {
            DeltaSetStorage::Direct(deltas) => (Some(deltas), None),
            DeltaSetStorage::Deduplicated(deltas) => (None, Some(deltas)),
        };
        a_vec
            .into_iter()
            .flat_map(|x| x.iter().enumerate().map(|(i, val)| (val, i as u32)))
            .chain(
                a_map
                    .into_iter()
                    .flat_map(|map| map.iter().map(|(val, idx)| (val, *idx))),
            )
    }
}

// a custom impl so that we match the behaviour of fonttools:
//
// - fonttools stores this densely, as just a tuple of deltas in region-order.
// - we store this sparsely, with explicit region indices.
// - this means that we need to handle the case where we are eliding a delta,
//   in one deltaset where we have a negative value in the other.
//   For example:
//   # fonttools rep (1, 5, -10), (1, 5, 0)
//   # fontations   [(0, 1), (1, 5), (2, -10), (0, 1), (1, 5)]
//
// in this case fonttools will sort the first set before the second, and we would
// do the opposite.
impl PartialOrd for DeltaSet {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for DeltaSet {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        let max_region_idx = self
            .0
            .iter()
            .chain(other.0.iter())
            .map(|(idx, _)| *idx)
            .max()
            .unwrap_or(0);

        let left = DenseDeltaIter::new(&self.0, max_region_idx);
        let right = DenseDeltaIter::new(&other.0, max_region_idx);

        for (l, r) in left.zip(right) {
            match l.cmp(&r) {
                std::cmp::Ordering::Equal => (),
                non_eq => return non_eq,
            }
        }
        std::cmp::Ordering::Equal
    }
}

// a helper that iterates our sparse deltas, inserting explicit 0s for any missing
// regions.
//
// // this is only used in our partial ord impl
struct DenseDeltaIter<'a> {
    total_len: u16,
    cur_pos: u16,
    deltas: &'a [(u16, i32)],
}

impl<'a> DenseDeltaIter<'a> {
    fn new(deltas: &'a [(u16, i32)], max_idx: u16) -> Self {
        DenseDeltaIter {
            total_len: max_idx,
            deltas,
            cur_pos: 0,
        }
    }
}

impl Iterator for DenseDeltaIter<'_> {
    type Item = i32;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur_pos > self.total_len {
            return None;
        }
        let result = if self.deltas.first().map(|(idx, _)| *idx) == Some(self.cur_pos) {
            let result = self.deltas.first().unwrap().1;
            self.deltas = &self.deltas[1..];
            result
        } else {
            0
        };
        self.cur_pos += 1;
        Some(result)
    }
}

impl Default for DeltaSetStorage {
    fn default() -> Self {
        Self::Deduplicated(Default::default())
    }
}

impl Display for RowShape {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for col in &self.0 {
            match col {
                ColumnBits::None => write!(f, "-"),
                ColumnBits::One => write!(f, "B"),
                ColumnBits::Two => write!(f, "S"),
                ColumnBits::Four => write!(f, "L"),
            }?
        }
        Ok(())
    }
}

impl Debug for Encoding<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Encoding({}, {} items {} bytes)",
            self.shape,
            self.deltas.len(),
            self.cost()
        )
    }
}

impl Debug for Encoder<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Encoder")
            .field("encodings", &self.encodings)
            .finish_non_exhaustive()
    }
}

#[cfg(test)]
mod tests {
    use crate::tables::variations::RegionAxisCoordinates;
    use font_types::F2Dot14;
    use read_fonts::{FontData, FontRead};

    use super::*;

    fn reg_coords(min: f32, default: f32, max: f32) -> RegionAxisCoordinates {
        RegionAxisCoordinates {
            start_coord: F2Dot14::from_f32(min),
            peak_coord: F2Dot14::from_f32(default),
            end_coord: F2Dot14::from_f32(max),
        }
    }

    fn test_regions() -> [VariationRegion; 3] {
        [
            VariationRegion::new(vec![reg_coords(0.0, 0.2, 1.0), reg_coords(0.0, 0.0, 1.0)]),
            VariationRegion::new(vec![reg_coords(0.0, 0.1, 0.3), reg_coords(0.0, 0.1, 0.3)]),
            VariationRegion::new(vec![reg_coords(0.0, 0.1, 0.5), reg_coords(0.0, 0.1, 0.3)]),
        ]
    }

    #[test]
    #[allow(clippy::redundant_clone)]
    fn smoke_test() {
        let [r1, r2, r3] = test_regions();

        let mut builder = VariationStoreBuilder::new(2);
        builder.add_deltas(vec![(r1.clone(), 512), (r2, 266), (r3.clone(), 1115)]);
        builder.add_deltas(vec![(r3.clone(), 20)]);
        builder.add_deltas(vec![(r3.clone(), 21)]);
        builder.add_deltas(vec![(r3, 22)]);

        // we should have three regions, and two subtables
        let (store, _) = builder.build();
        assert_eq!(store.variation_region_list.variation_regions.len(), 3);
        assert_eq!(store.item_variation_data.len(), 2);
        assert_eq!(
            store.item_variation_data[0]
                .as_ref()
                .unwrap()
                .region_indexes,
            vec![2]
        );
        assert_eq!(
            store.item_variation_data[1]
                .as_ref()
                .unwrap()
                .region_indexes,
            vec![0, 1, 2]
        );
    }

    #[test]
    fn key_mapping() {
        let [r1, r2, r3] = test_regions();

        let mut builder = VariationStoreBuilder::new(2);
        let k1 = builder.add_deltas(vec![(r1.clone(), 5), (r2, 1000), (r3.clone(), 1500)]);
        let k2 = builder.add_deltas(vec![(r1.clone(), -3), (r3.clone(), 20)]);
        let k3 = builder.add_deltas(vec![(r1.clone(), -12), (r3.clone(), 7)]);
        // add enough items so that the optimizer doesn't merge these two encodings
        let _ = builder.add_deltas(vec![(r1.clone(), -10), (r3.clone(), 7)]);
        let _ = builder.add_deltas(vec![(r1.clone(), -9), (r3.clone(), 7)]);
        let _ = builder.add_deltas(vec![(r1, -11), (r3, 7)]);

        // let encoder = builder.encoder();
        // eprintln!("{encoder:?}");
        // we should have three regions, and two subtables
        let (_, key_lookup) = builder.build();

        // first subtable has only one item
        // first item gets mapped into second subtable, because of how we sort
        assert_eq!(key_lookup.get_raw(k1).unwrap(), (1, 0),);
        // next two items are in the same (first) subtable
        // inner indexes are based on sort order within the subtable:
        assert_eq!(key_lookup.get_raw(k2).unwrap(), (0, 4),); // largest r1 value
        assert_eq!(key_lookup.get_raw(k3).unwrap(), (0, 0),); // smallest r1 value

        assert_eq!(key_lookup.map.len(), 6);
    }

    // really just here to check my own understanding of what's going on
    #[test]
    fn fontools_rowshape_repr() {
        use ColumnBits as C;
        let shape1 = RowShape(vec![C::None, C::One, C::One, C::Two]);
        assert_eq!(shape1.to_fonttools_repr(), 0b0011_0001_0001_0000);
        let shape2 = RowShape(vec![C::Two, C::One, C::One, C::None]);
        assert_eq!(shape2.to_fonttools_repr(), 0b0000_0001_0001_0011);

        assert!(shape1.to_fonttools_repr() > shape2.to_fonttools_repr());
    }

    #[test]
    fn encoding_sort_order() {
        let _ = env_logger::builder().is_test(true).try_init();
        let [r1, r2, r3] = test_regions();

        // make two encodings that have the same total cost, but different shape

        let mut builder = VariationStoreBuilder::new(2);
        // shape (2, 1, 0)
        builder.add_deltas(vec![(r1.clone(), 1000), (r2.clone(), 5)]);
        builder.add_deltas(vec![(r1.clone(), 1013), (r2.clone(), 20)]);
        builder.add_deltas(vec![(r1.clone(), 1014), (r2.clone(), 21)]);
        // shape (0, 2, 1)
        builder.add_deltas(vec![(r2.clone(), 1212), (r3.clone(), 7)]);
        builder.add_deltas(vec![(r2.clone(), 1213), (r3.clone(), 8)]);
        builder.add_deltas(vec![(r2.clone(), 1214), (r3.clone(), 8)]);

        //shape (1, 0, 1)
        builder.add_deltas(vec![(r1.clone(), 12), (r3.clone(), 7)]);
        builder.add_deltas(vec![(r1.clone(), 13), (r3.clone(), 9)]);
        builder.add_deltas(vec![(r1.clone(), 14), (r3.clone(), 10)]);
        builder.add_deltas(vec![(r1.clone(), 15), (r3.clone(), 11)]);
        builder.add_deltas(vec![(r1.clone(), 16), (r3.clone(), 12)]);

        let (var_store, key_lookup) = builder.build();
        assert_eq!(var_store.item_variation_data.len(), 3);
        assert_eq!(key_lookup.map.len(), 11);

        // encoding (1, 0, 1) will be sorted first, since it has the lowest cost
        assert_eq!(
            var_store.item_variation_data[0]
                .as_ref()
                .unwrap()
                .region_indexes,
            vec![0, 2]
        );

        // then encoding with shape (2, 1, 0) since the costs are equal and we
        // compare backwards, to match fonttools
        assert_eq!(
            var_store.item_variation_data[1]
                .as_ref()
                .unwrap()
                .region_indexes,
            vec![0, 1]
        );
    }

    #[test]
    #[allow(clippy::redundant_clone)]
    fn to_binary() {
        let [r1, r2, r3] = test_regions();

        let mut builder = VariationStoreBuilder::new(2);
        builder.add_deltas(vec![(r1.clone(), 512), (r2, 1000), (r3.clone(), 265)]);
        builder.add_deltas(vec![(r1.clone(), -3), (r3.clone(), 20)]);
        builder.add_deltas(vec![(r1.clone(), -12), (r3.clone(), 7)]);
        builder.add_deltas(vec![(r1.clone(), -11), (r3.clone(), 8)]);
        builder.add_deltas(vec![(r1.clone(), -10), (r3.clone(), 9)]);
        let (table, _) = builder.build();
        let bytes = crate::dump_table(&table).unwrap();
        let data = FontData::new(&bytes);

        let reloaded = read_fonts::tables::variations::ItemVariationStore::read(data).unwrap();

        assert_eq!(reloaded.item_variation_data_count(), 2);
        let var_data_array = reloaded.item_variation_data();

        let var_data = var_data_array.get(0).unwrap().unwrap();
        assert_eq!(var_data.region_indexes(), &[0, 2]);
        assert_eq!(var_data.item_count(), 4);
        assert_eq!(var_data.delta_set(0).collect::<Vec<_>>(), vec![-12, 7]);
        assert_eq!(var_data.delta_set(1).collect::<Vec<_>>(), vec![-11, 8]);
        assert_eq!(var_data.delta_set(2).collect::<Vec<_>>(), vec![-10, 9]);
        assert_eq!(var_data.delta_set(3).collect::<Vec<_>>(), vec![-3, 20]);

        let var_data = var_data_array.get(1).unwrap().unwrap();
        assert_eq!(var_data.region_indexes(), &[0, 1, 2]);
        assert_eq!(var_data.item_count(), 1);
        assert_eq!(
            var_data.delta_set(0).collect::<Vec<_>>(),
            vec![512, 1000, 265]
        );
    }

    #[test]
    fn reuse_identical_variation_data() {
        let _ = env_logger::builder().is_test(true).try_init();
        let [r1, r2, r3] = test_regions();

        let mut builder = VariationStoreBuilder::new(2);
        let k1 = builder.add_deltas(vec![(r1.clone(), 5), (r2, 10), (r3.clone(), 15)]);
        let k2 = builder.add_deltas(vec![(r1.clone(), -12), (r3.clone(), 7)]);
        let k3 = builder.add_deltas(vec![(r1.clone(), -12), (r3.clone(), 7)]);
        let k4 = builder.add_deltas(vec![(r1, 322), (r3, 532)]);

        // we should have three regions, and two subtables
        let (_, key_lookup) = builder.build();
        assert_eq!(k2, k3);
        assert_ne!(k1, k2);
        assert_ne!(k1, k4);
        assert_eq!(key_lookup.map.len(), 3);
    }

    /// if we have a single region set, where some deltas are 32-bit, the
    /// smaller deltas should get their own subtable IFF we save enough bytes
    /// to justify this
    #[test]
    #[allow(clippy::redundant_clone)]
    fn long_deltas_split() {
        let [r1, r2, _] = test_regions();
        let mut builder = VariationStoreBuilder::new(2);
        // short
        builder.add_deltas(vec![(r1.clone(), 1), (r2.clone(), 2)]);
        builder.add_deltas(vec![(r1.clone(), 3), (r2.clone(), 4)]);
        builder.add_deltas(vec![(r1.clone(), 5), (r2.clone(), 6)]);
        // long
        builder.add_deltas(vec![(r1.clone(), 0xffff + 1), (r2.clone(), 0xffff + 2)]);
        let mut encoder = builder.encoder();
        assert_eq!(encoder.encodings.len(), 2);
        encoder.optimize();
        assert_eq!(encoder.encodings.len(), 2);
    }

    /// combine smaller deltas into larger when there aren't many of them
    #[test]
    #[allow(clippy::redundant_clone)]
    fn long_deltas_combine() {
        let [r1, r2, _] = test_regions();
        let mut builder = VariationStoreBuilder::new(2);
        // short
        builder.add_deltas(vec![(r1.clone(), 1), (r2.clone(), 2)]);
        builder.add_deltas(vec![(r1.clone(), 3), (r2.clone(), 4)]);
        // long
        builder.add_deltas(vec![(r1.clone(), 0xffff + 1), (r2.clone(), 0xffff + 2)]);

        let mut encoder = builder.encoder();
        assert_eq!(encoder.encodings.len(), 2);
        assert_eq!(encoder.encodings[0].shape.overhead(), 14); // 10 base, 2 * 2 columns
        assert_eq!(encoder.encodings[0].cost(), 14 + 4); // overhead + 2 * 2 bytes/row
        assert_eq!(encoder.encodings[1].shape.overhead(), 14);
        assert_eq!(encoder.encodings[1].cost(), 14 + 8); // overhead + 1 * 8 bytes/rows
        encoder.optimize();
        assert_eq!(encoder.encodings.len(), 1);
    }

    // ensure that we are merging as expected
    #[test]
    #[allow(clippy::redundant_clone)]
    fn combine_many_shapes() {
        let _ = env_logger::builder().is_test(true).try_init();
        let [r1, r2, r3] = test_regions();
        let mut builder = VariationStoreBuilder::new(2);
        // orchestrate a failure case:
        // - we want to combine
        builder.add_deltas(vec![(r1.clone(), 0xffff + 5)]); // (L--)
        builder.add_deltas(vec![(r1.clone(), 2)]); // (B--)
        builder.add_deltas(vec![(r1.clone(), 300)]); // (S--)
        builder.add_deltas(vec![(r2.clone(), 0xffff + 5)]); // (-L-)
        builder.add_deltas(vec![(r2.clone(), 2)]); // (-B-)
        builder.add_deltas(vec![(r2.clone(), 300)]); // (-S-)
        builder.add_deltas(vec![(r3.clone(), 0xffff + 5)]); // (--L)
        builder.add_deltas(vec![(r3.clone(), 2)]); // (--B)
        builder.add_deltas(vec![(r3.clone(), 300)]); // (--S)
        let mut encoder = builder.encoder();
        encoder.optimize();
        // we compile down to three subtables, each with one column
        assert_eq!(encoder.encodings.len(), 3);
        assert!(encoder.encodings[0]
            .compute_gain(&encoder.encodings[1])
            .is_negative());
    }

    #[test]
    #[allow(clippy::redundant_clone)]
    fn combine_two_big_fellas() {
        let _ = env_logger::builder().is_test(true).try_init();
        let [r1, r2, r3] = test_regions();
        let mut builder = VariationStoreBuilder::new(2);
        // we only combine two of these, since that saves 2 bytes, but adding
        // the third is too expensive
        builder.add_deltas(vec![(r1.clone(), 0xffff + 5)]); // (L--)
        builder.add_deltas(vec![(r2.clone(), 0xffff + 5)]); // (-L-)
        builder.add_deltas(vec![(r3.clone(), 0xffff + 5)]); // (--L)

        let mut encoder = builder.encoder();
        assert_eq!(encoder.encodings[0].cost(), 16);
        let merge_cost = 2 // extra column
            + 4 // existing encoding gets extra column
            + 8; // two columns for new row
        assert_eq!(
            encoder.encodings[0].compute_gain(&encoder.encodings[1]),
            16 - merge_cost
        );
        encoder.optimize();

        // we didn't merge any further because it's too expensive
        let next_merge_cost = 2
            + 2 * 4 // two existing rows get extra column
            + 12; // three columns for new row
        assert_eq!(encoder.encodings.len(), 2);
        assert_eq!(encoder.encodings[0].cost(), 16);
        assert_eq!(
            encoder.encodings[0].compute_gain(&encoder.encodings[1]),
            16 - next_merge_cost
        );
    }

    /// we had a crash here where we were trying to write zeros when they should
    /// be getting ignored.
    #[test]
    fn ensure_zero_deltas_dont_write() {
        let _ = env_logger::builder().is_test(true).try_init();
        let [r1, r2, _] = test_regions();
        let mut builder = VariationStoreBuilder::new(2);
        builder.add_deltas(vec![(r1.clone(), 0), (r2.clone(), 4)]);
        let _ = builder.build();
    }

    // we had another crash here where when *all* deltas were zero we would
    // call 'slice.chunks()' with '0', which is not allowed
    #[test]
    fn ensure_all_zeros_dont_write() {
        let _ = env_logger::builder().is_test(true).try_init();
        let [r1, r2, _] = test_regions();
        let mut builder = VariationStoreBuilder::new(2);
        builder.add_deltas(vec![(r1.clone(), 0), (r2.clone(), 0)]);
        let _ = builder.build();
    }

    #[test]
    fn vardata_region_indices_order() {
        let r0 = VariationRegion::new(vec![reg_coords(0.0, 0.5, 1.0)]);
        let r1 = VariationRegion::new(vec![reg_coords(0.5, 1.0, 1.0)]);

        let mut builder = VariationStoreBuilder::new(1);
        builder.add_deltas(vec![(r0.clone(), 1), (r1.clone(), 2)]);
        // 256 won't fit in a u8 thus we expect the deltas for the column corresponding
        // to r1 will be packed as u16
        builder.add_deltas(vec![(r0.clone(), 3), (r1.clone(), 256)]);

        let (store, _varidx_map) = builder.build();

        assert_eq!(store.variation_region_list.variation_regions.len(), 2);
        assert_eq!(store.item_variation_data.len(), 1);

        let var_data = store.item_variation_data[0].as_ref().unwrap();

        assert_eq!(var_data.item_count, 2);
        assert_eq!(var_data.word_delta_count, 1);
        // this should be [1, 0] and not [0, 1] because the regions with wider
        // deltas should be packed first.
        // var_data.region_indexes is an array of indices into the variation region list
        // in the order of the columns of the variation data. So it maps from column index
        // to region index, not the other way around.
        assert_eq!(var_data.region_indexes, vec![1, 0]);
        assert_eq!(
            var_data.delta_sets,
            // ItemVariationData packs deltas as two-dimensional [u8] array
            // with item_count rows and region_index_count columns.
            // In this particular case (word_count=1) the first column contains 'words'
            // with 2-byte deltas, followed by the second column with 1-byte deltas.
            vec![
                // item[0]
                0, 2, // 2: delta for r1
                1, //    1: delta for r0
                // item[1]
                1, 0, // 256: delta for r1
                3, //    3: delta for r0
            ],
        );
    }

    #[test]
    fn unoptimized_version() {
        let r0 = VariationRegion::new(vec![reg_coords(0.0, 0.5, 1.0)]);
        let r1 = VariationRegion::new(vec![reg_coords(0.5, 1.0, 1.0)]);

        let mut builder = VariationStoreBuilder::new_with_implicit_indices(1);
        builder.add_deltas(vec![(r0.clone(), 1), (r1.clone(), 2)]);
        // 256 won't fit in a u8 thus we expect the deltas for the column corresponding
        // to r1 will be packed as u16
        builder.add_deltas(vec![(r0.clone(), 1), (r1.clone(), 2)]);
        builder.add_deltas(vec![(r0.clone(), 3), (r1.clone(), 256)]);
        builder.add_deltas(vec![(r0.clone(), 3), (r1.clone(), 256)]);

        let (ivs, key_map) = builder.build();
        // we should get an ivs with one subtable, containing four deltas
        assert_eq!(ivs.item_variation_data.len(), 1);

        let var_data = ivs.item_variation_data[0].as_ref().unwrap();
        assert_eq!(var_data.item_count, 4);
        assert_eq!(var_data.region_indexes, vec![1, 0]);

        assert_eq!(
            var_data.delta_sets,
            &[
                0x0, 0x2, // item 1, region 2
                0x1, // item 1, region 1
                0x0, 0x2, // item 2, region 2
                0x1, // item 2, region 1
                0x1, 0x0, // item 3, region 2
                0x3, // item 3, region 1
                0x1, 0x0, // item 4, region 2
                0x3, // item 4, region 1
            ]
        );

        // assert that keymap entries are identity mapping
        assert_eq!(key_map.map.len(), 4);
        assert!(key_map
            .map
            .iter()
            .all(|(key, idx)| *key == idx.delta_set_inner_index as u32))
    }

    #[test]
    fn delta_set_ordering() {
        let left = DeltaSet(vec![(0, 1), (1, 2), (2, -11)]);
        let right = DeltaSet(vec![(0, 1), (1, 2)]);

        // although the vec ord impl thinks that the left is 'bigger'
        // (it having more items):
        assert!(left.0 > right.0);
        // our custom impl treats it as smaller, matching fonttools
        assert!(left < right);

        // but this is only the case because the delta is negative
        let left = DeltaSet(vec![(0, 1), (1, 2), (2, 11)]);
        let right = DeltaSet(vec![(0, 1), (1, 2)]);

        assert!(left > right);
        let left = DeltaSet(vec![(0, 1), (1, 2), (2, -11)]);
        let right = DeltaSet(vec![(0, 1), (1, 2), (3, 0)]);

        assert!(left < right);

        // also true in the middle
        let left = DeltaSet(vec![(0, 1), (1, -2), (2, -11)]);
        let right = DeltaSet(vec![(0, 1), (2, -11)]);
        assert!(left < right)
    }

    #[test]
    fn no_duplicate_zero_delta_sets() {
        let r0 = VariationRegion::new(vec![reg_coords(0.0, 5.0, 1.0)]);
        let r1 = VariationRegion::new(vec![reg_coords(0.5, 1.0, 1.0)]);
        let mut builder = VariationStoreBuilder::new(1);
        let varidxes = vec![
            // first glyph has no variations (e.g. .notdef only defined at default location)
            // but we still need to add it to the variation store to reserve an index so
            // we add an empty delta set
            builder.add_deltas::<i32>(Vec::new()),
            builder.add_deltas(vec![(r0.clone(), 50), (r1.clone(), 100)]),
            // this glyph has explicit masters that are *all* the same as the default (delta is 0);
            // we expect the builder to reuse the same no-op delta set as the first glyph
            builder.add_deltas(vec![(r0.clone(), 0), (r1.clone(), 0)]),
            // this glyph repeats the same delta set as the second glyph, thus we expect
            // the builder to map it to the same delta set index
            builder.add_deltas(vec![(r0.clone(), 50), (r1.clone(), 100)]),
            // this glyph happens to have one master that's the same as the default (delta is 0);
            // nothing special here, we expect a new delta set to be created
            builder.add_deltas(vec![(r0.clone(), 0), (r1.clone(), 100)]),
        ];
        let (store, key_map) = builder.build();

        let varidx_map: Vec<u32> = varidxes
            .into_iter()
            .map(|idx| key_map.get(idx).unwrap().into())
            .collect::<Vec<_>>();

        assert_eq!(store.variation_region_list.variation_regions.len(), 2);
        assert_eq!(store.item_variation_data.len(), 1);

        let var_data = store.item_variation_data[0].as_ref().unwrap();

        assert_eq!(var_data.item_count, 3);
        assert_eq!(var_data.word_delta_count, 0);
        assert_eq!(var_data.region_indexes, vec![0, 1]);
        assert_eq!(var_data.delta_sets, vec![0, 0, 0, 100, 50, 100],);
        // glyph 0 and 2 should map to the same no-op [0, 0] deltaset, while
        // glyph 1 and 3 should map to deltaset [50, 100];
        // glyph 4 should map to deltaset [0, 100]
        assert_eq!(varidx_map, vec![0, 2, 0, 2, 1]);
    }

    #[test]
    fn zero_deltas_deduplicate_with_absent_regions() {
        // A glyph with an intermediate master whose advance equals the interpolated
        // value produces an explicit zero delta for that region. It must deduplicate
        // against a glyph that simply lacks that intermediate master but has identical
        // non-zero deltas. This matches the behaviour of fonttools' VarStore_optimize,
        // which expands every row to the full region width (padding absent regions with 0)
        // before deduplication:
        // https://github.com/fonttools/fonttools/blob/772918952/Lib/fontTools/varLib/varStore.py#L575-L598
        let r0 = VariationRegion::new(vec![reg_coords(0.0, 0.5, 1.0)]);
        let r1 = VariationRegion::new(vec![reg_coords(0.5, 1.0, 1.0)]);
        // An intermediate master region (e.g. SemiBold) that glyph A has but glyph B lacks.
        let r_intermediate = VariationRegion::new(vec![reg_coords(0.0, 0.7, 1.0)]);

        let mut builder = VariationStoreBuilder::new(1);

        // Glyph A: built from a model that includes the intermediate master; its
        // advance at the intermediate is the same as the interpolated value, so the
        // delta for r_intermediate is 0.
        let idx_a = builder.add_deltas(vec![
            (r0.clone(), -87_i16),
            (r1.clone(), 20_i16),
            (r_intermediate.clone(), 0_i16),
        ]);
        // Glyph B: no intermediate master at all; same non-zero deltas for r0 and r1.
        let idx_b = builder.add_deltas(vec![(r0.clone(), -87_i16), (r1.clone(), 20_i16)]);

        let (store, key_map) = builder.build();

        // Both glyphs must resolve to the same (outer, inner) delta-set index.
        assert_eq!(
            key_map.get(idx_a),
            key_map.get(idx_b),
            "explicit zero delta should deduplicate with absent region"
        );

        // Only the two non-zero regions should appear in the final store; r_intermediate
        // is registered internally but must be pruned because no item references it.
        assert_eq!(store.variation_region_list.variation_regions.len(), 2);
    }

    #[test]
    fn prune_unused_regions() {
        // https://github.com/googlefonts/fontations/issues/733
        let r0 = VariationRegion::new(vec![reg_coords(-1.0, -0.5, 0.0)]);
        let r1 = VariationRegion::new(vec![reg_coords(-1.0, -1.0, 0.0)]);
        let r2 = VariationRegion::new(vec![reg_coords(0.0, 0.5, 1.0)]);
        let r3 = VariationRegion::new(vec![reg_coords(0.0, 1.0, 1.0)]);
        let mut builder = VariationStoreBuilder::new(1);
        builder.add_deltas(vec![
            (r0.clone(), 0),
            (r1.clone(), 50),
            (r2.clone(), 0),
            (r3.clone(), 100),
        ]);
        let (store, _) = builder.build();

        // not 4 regions, since only 2 are actually used
        assert_eq!(store.variation_region_list.variation_regions.len(), 2);
        assert_eq!(store.item_variation_data.len(), 1);

        let var_data = store.item_variation_data[0].as_ref().unwrap();
        assert_eq!(var_data.item_count, 1);
        assert_eq!(var_data.word_delta_count, 0);
        assert_eq!(var_data.region_indexes, vec![0, 1]); // not 1, 3
        assert_eq!(var_data.delta_sets, vec![50, 100]);
    }

    #[test]
    fn we_match_fonttools_stable_order() {
        use rand::seq::SliceRandom;

        let mut builder = VariationStoreBuilder::new(1);
        let r1 = VariationRegion::new(vec![reg_coords(-1.0, -1.0, 0.0)]);
        let r2 = VariationRegion::new(vec![reg_coords(0.0, 1.0, 1.0)]);

        let mut delta_sets = vec![
            // 7 delta sets with row shape "BB"
            vec![(r1.clone(), 1), (r2.clone(), 2)],
            vec![(r1.clone(), 3), (r2.clone(), 4)],
            vec![(r1.clone(), 5), (r2.clone(), 6)],
            vec![(r1.clone(), 7), (r2.clone(), 8)],
            vec![(r1.clone(), 9), (r2.clone(), 10)],
            vec![(r1.clone(), 11), (r2.clone(), 12)],
            vec![(r1.clone(), 13), (r2.clone(), 14)],
            // 4 delta sets with row shape "-S"
            vec![(r1.clone(), 0), (r2.clone(), -130)],
            vec![(r1.clone(), 0), (r2.clone(), -129)],
            vec![(r1.clone(), 0), (r2.clone(), 128)],
            vec![(r1.clone(), 0), (r2.clone(), 129)],
            // 1 delta set with row shape "-B".
            // The gain from merging the following into either one of the previous
            // encodings happens to be the same so the order in which the winning pair
            // gets popped from the heap (sorted by relative gain) depends on the order
            // in which the delta sets were pushed; the sort key that fonttools uses to
            // sort the inputs (for stability) is such that the encoding with row shape
            // "-B" will be merged with the first encoding with row shape "BB" and not
            // with the second one with row shape "-S".
            vec![(r1.clone(), 0), (r2.clone(), -1)],
        ];

        // Add delta sets in random order and test that the algorithm is stable
        let mut rng = rand::rng();
        delta_sets.shuffle(&mut rng);
        for deltas in delta_sets {
            builder.add_deltas(deltas);
        }

        let (store, _) = builder.build();

        let bytes = crate::dump_table(&store).unwrap();
        let data = FontData::new(&bytes);
        let reloaded = read_fonts::tables::variations::ItemVariationStore::read(data).unwrap();

        assert_eq!(reloaded.item_variation_data_count(), 2);
        let var_data_array = reloaded.item_variation_data();

        let var_data = var_data_array.get(0).unwrap().unwrap();
        assert_eq!(var_data.region_indexes(), &[0, 1]);
        // count must be 8, not 7, because [0, -1] should be in here
        assert_eq!(var_data.item_count(), 8);
        assert_eq!(var_data.delta_set(0).collect::<Vec<_>>(), vec![0, -1]);
        assert_eq!(var_data.delta_set(1).collect::<Vec<_>>(), vec![1, 2]);
        assert_eq!(var_data.delta_set(2).collect::<Vec<_>>(), vec![3, 4]);
        assert_eq!(var_data.delta_set(3).collect::<Vec<_>>(), vec![5, 6]);
        assert_eq!(var_data.delta_set(4).collect::<Vec<_>>(), vec![7, 8]);
        assert_eq!(var_data.delta_set(5).collect::<Vec<_>>(), vec![9, 10]);
        assert_eq!(var_data.delta_set(6).collect::<Vec<_>>(), vec![11, 12]);
        assert_eq!(var_data.delta_set(7).collect::<Vec<_>>(), vec![13, 14]);

        let var_data = var_data_array.get(1).unwrap().unwrap();
        assert_eq!(var_data.region_indexes(), &[1]);
        assert_eq!(var_data.item_count(), 4);
        // ... and not in here
        assert_eq!(var_data.delta_set(0).collect::<Vec<_>>(), vec![-130]);
        assert_eq!(var_data.delta_set(1).collect::<Vec<_>>(), vec![-129]);
        assert_eq!(var_data.delta_set(2).collect::<Vec<_>>(), vec![128]);
        assert_eq!(var_data.delta_set(3).collect::<Vec<_>>(), vec![129]);
    }
}
