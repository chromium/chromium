//! Lookup variations (ISO/IEC 14496-22:2026, 6.2.11).

use super::condition::Evaluator;
use super::{
    FeatureLookups, FeatureLookupsFlags, FeatureVariations, LookupConditionRecord,
    LookupVariationRecord,
};
use crate::{
    types::{Compatible, F2Dot14, FixedSize},
    FontData,
};
use alloc::{collections::BTreeSet, vec::Vec};

impl<'a> FeatureVariations<'a> {
    /// Lookup variations, sorted by feature index; absent in version 1.0.
    pub fn lookup_variation_records(&self) -> Option<&'a [LookupVariationRecord]> {
        if !self.version().compatible((1, 1)) {
            return None;
        }
        let data = self.offset_data();
        if u64::from(self.feature_variation_record_count()) * 8
            > data.len().saturating_sub(8) as u64
        {
            return None;
        }
        // A present, empty version-1.1 array is distinct from an absent field.
        let count = usize::try_from(self.lookup_variation_record_count()?).ok()?;
        let start = self.lookup_variation_record_count_byte_range().end;
        let end = start.checked_add(count.checked_mul(LookupVariationRecord::RAW_BYTE_LEN)?)?;
        data.read_array(start..end).ok()
    }

    /// Returns the table's lookup variations, or `None` if it has none.
    ///
    /// Only version 1.1 and later tables carry lookup variations, so for
    /// other tables this is a version check. Records that are not strictly
    /// sorted by feature index are ignored as a whole.
    pub fn lookup_variations(&self) -> Option<LookupVariations<'a>> {
        let records = self.lookup_variation_records()?;
        let sorted = records
            .windows(2)
            .all(|pair| pair[0].feature_index() < pair[1].feature_index());
        (!records.is_empty() && sorted).then(|| LookupVariations {
            data: self.offset_data(),
            records,
        })
    }
}

/// The lookup variations of a [`FeatureVariations`] table, which change the
/// lookups of a feature depending on the location in design space.
#[derive(Clone)]
pub struct LookupVariations<'a> {
    data: FontData<'a>,
    records: &'a [LookupVariationRecord],
}

impl<'a> LookupVariations<'a> {
    /// Applies the lookup variations of a feature at a location.
    ///
    /// `lookups` holds the feature's lookup indices. They are replaced,
    /// unless the variations only add to them, by the lookups of each
    /// condition record whose condition holds at `coords`. Format 2
    /// conditions resolve their deltas through `delta`; see
    /// [`Condition::evaluate`](super::Condition::evaluate).
    ///
    /// The added lookups may be unsorted and repeat ones already present.
    /// All conditions of the feature share one evaluation budget.
    ///
    /// Returns `false`, leaving `lookups` unchanged, if the feature has no
    /// lookup variations.
    pub fn apply<E>(
        &self,
        feature_index: u16,
        coords: &[F2Dot14],
        delta: impl FnMut(u32) -> Result<f64, E>,
        lookups: &mut Vec<u16>,
    ) -> bool {
        let Some(table) = self.feature_lookups(feature_index) else {
            return false;
        };
        if !table
            .flags()
            .contains(FeatureLookupsFlags::ADD_DEFAULT_LOOKUPS)
        {
            lookups.clear();
        }
        // Only whether a condition holds is used, so a failed delta is simply
        // a condition that does not.
        let mut delta = delta;
        let mut evaluator = Evaluator::new(coords, |index| {
            delta(index).map_err(|_| crate::model::Error::Malformed)
        });
        let mut lists = LookupLists::new(&table);
        for record in table.lookup_condition_records() {
            if evaluator.evaluate_record(table.offset_data(), record).ok() == Some(true) {
                lists.extend(record, lookups);
            }
        }
        true
    }

    /// Adds the lookups of every condition record of a feature, as if each
    /// condition held: the lookups the feature can use anywhere in design
    /// space.
    ///
    /// Returns `false` if the feature has no lookup variations.
    pub fn add_all(&self, feature_index: u16, lookups: &mut Vec<u16>) -> bool {
        let Some(table) = self.feature_lookups(feature_index) else {
            return false;
        };
        let mut lists = LookupLists::new(&table);
        for record in table.lookup_condition_records() {
            lists.extend(record, lookups);
        }
        true
    }

    /// The conditional lookups of a feature, if it has readable ones.
    fn feature_lookups(&self, feature_index: u16) -> Option<FeatureLookups<'a>> {
        let index = self
            .records
            .binary_search_by_key(&feature_index, LookupVariationRecord::feature_index)
            .ok()?;
        let record = &self.records[index];
        if record.feature_lookups_offset().to_u32() == 0 {
            return None;
        }
        let table = record.feature_lookups(self.data).ok()?;
        // Checked before the records are read, whose range the generated
        // code computes without checking for overflow.
        let records_end = usize::try_from(table.lookup_condition_count())
            .ok()?
            .checked_mul(LookupConditionRecord::RAW_BYTE_LEN)?
            .checked_add(table.lookup_condition_count_byte_range().end)?;
        (table.version().major == 1 && records_end <= table.offset_data().len()).then_some(table)
    }
}

/// The lookup index lists of a FeatureLookups table, each added at most once.
///
/// Condition records can share a list, so adding each one per record would
/// let a small table produce an arbitrarily long result.
struct LookupLists<'a> {
    data: FontData<'a>,
    added: BTreeSet<u32>,
}

impl<'a> LookupLists<'a> {
    fn new(table: &FeatureLookups<'a>) -> Self {
        Self {
            data: table.offset_data(),
            added: BTreeSet::new(),
        }
    }

    /// Appends the lookups of a condition record, skipping a missing,
    /// truncated or already added list.
    fn extend(&mut self, record: &LookupConditionRecord, lookups: &mut Vec<u16>) {
        let offset = record.lookup_index_list_offset().to_u32();
        if offset == 0 || !self.added.insert(offset) {
            return;
        }
        if let Ok(list) = record.lookup_index_list(self.data) {
            let indices = list.lookup_indices();
            if indices.len() == usize::from(list.lookup_index_count()) {
                lookups.extend(indices.iter().map(|index| index.get()));
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{types::MajorMinor, FontRead, ReadError};

    /// The conditional lookups of a feature, found through the raw records.
    fn feature_lookups<'a>(
        table: &FeatureVariations<'a>,
        feature_index: u16,
    ) -> Option<Result<FeatureLookups<'a>, ReadError>> {
        let records = table.lookup_variation_records()?;
        let record = records
            .iter()
            .find(|record| record.feature_index() == feature_index)?;
        Some(record.feature_lookups(table.offset_data()))
    }

    /// A version 1.1 FeatureVariations table whose feature 2 has two
    /// condition records: lookups 7 and 8 when axis 0 is at least 0.5, and
    /// lookup 9 always (a null condition).
    fn lookup_variation_table(flags: u16) -> alloc::vec::Vec<u8> {
        let mut bytes = alloc::vec::Vec::new();
        // FeatureVariations 1.1: no feature variation records, one lookup
        // variation record.
        bytes.extend([0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1]);
        // LookupVariationRecord: feature 2, FeatureLookups at offset 18.
        bytes.extend([0, 2, 0, 0, 0, 18]);
        assert_eq!(bytes.len(), 18);
        // FeatureLookups 1.0: flags, two condition records.
        bytes.extend([0, 1, 0, 0]);
        bytes.extend(flags.to_be_bytes());
        bytes.extend([0, 0, 0, 2]);
        // Record 1: condition at 26, lookup list at 34 (relative to
        // FeatureLookups). Record 2: null condition, lookup list at 40.
        bytes.extend([0, 0, 0, 26, 0, 0, 0, 34]);
        bytes.extend([0, 0, 0, 0, 0, 0, 0, 40]);
        assert_eq!(bytes.len() - 18, 26);
        // ConditionFormat1: axis 0, min 0.5, max 1.0.
        bytes.extend([0, 1, 0, 0, 0x20, 0, 0x40, 0]);
        assert_eq!(bytes.len() - 18, 34);
        // LookupIndexList [7, 8], then [9].
        bytes.extend([0, 2, 0, 7, 0, 8]);
        bytes.extend([0, 1, 0, 9]);
        bytes
    }

    fn no_delta(_: u32) -> Result<f64, core::convert::Infallible> {
        Ok(0.0)
    }

    #[test]
    fn lookup_variations_replace_feature_lookups_by_location() {
        let bytes = lookup_variation_table(0);
        let table = FeatureVariations::read(bytes.as_slice().into()).unwrap();
        let variations = table.lookup_variations().unwrap();
        let mut lookups = alloc::vec![1, 2];
        assert!(variations.apply(2, &[F2Dot14::from_f32(0.75)], no_delta, &mut lookups));
        assert_eq!(lookups, [7, 8, 9]);
        let mut lookups = alloc::vec![1, 2];
        assert!(variations.apply(2, &[F2Dot14::from_f32(0.25)], no_delta, &mut lookups));
        assert_eq!(lookups, [9]);
        let mut lookups = alloc::vec![1, 2];
        assert!(!variations.apply(3, &[F2Dot14::from_f32(0.75)], no_delta, &mut lookups));
        assert_eq!(lookups, [1, 2]);
        let mut lookups = alloc::vec![1];
        assert!(variations.add_all(2, &mut lookups));
        assert_eq!(lookups, [1, 7, 8, 9]);
    }

    #[test]
    fn lookup_variations_can_add_to_default_lookups() {
        let bytes = lookup_variation_table(FeatureLookupsFlags::ADD_DEFAULT_LOOKUPS.bits());
        let table = FeatureVariations::read(bytes.as_slice().into()).unwrap();
        let mut lookups = alloc::vec![1, 2];
        assert!(table.lookup_variations().unwrap().apply(
            2,
            &[F2Dot14::from_f32(0.75)],
            no_delta,
            &mut lookups
        ));
        assert_eq!(lookups, [1, 2, 7, 8, 9]);
    }

    #[test]
    fn lookup_variations_add_a_shared_list_once() {
        let mut bytes = alloc::vec::Vec::new();
        // FeatureVariations 1.1 with one lookup variation record for
        // feature 2, whose FeatureLookups table is at 18.
        bytes.extend([0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 0, 18]);
        // FeatureLookups 1.0: three unconditional records sharing the list
        // at 34.
        bytes.extend([0, 1, 0, 0, 0, 0, 0, 0, 0, 3]);
        for _ in 0..3 {
            bytes.extend([0, 0, 0, 0, 0, 0, 0, 34]);
        }
        bytes.extend([0, 2, 0, 7, 0, 8]);
        let table = FeatureVariations::read(bytes.as_slice().into()).unwrap();
        let variations = table.lookup_variations().unwrap();
        let mut lookups = alloc::vec![];
        assert!(variations.add_all(2, &mut lookups));
        assert_eq!(lookups, [7, 8]);
        let mut lookups = alloc::vec![];
        assert!(variations.apply(2, &[], no_delta, &mut lookups));
        assert_eq!(lookups, [7, 8]);
    }

    #[test]
    fn lookup_variations_absent_or_unsorted() {
        // Version 1.0 has none.
        let v1_0 = [0, 1, 0, 0, 0, 0, 0, 0];
        let table = FeatureVariations::read(v1_0.as_slice().into()).unwrap();
        assert!(table.lookup_variations().is_none());
        // Version 1.1 without records has none.
        let v1_1 = [0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0];
        let table = FeatureVariations::read(v1_1.as_slice().into()).unwrap();
        assert!(table.lookup_variations().is_none());
        // Records out of order are ignored.
        let unsorted = [
            0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0,
        ];
        let table = FeatureVariations::read(unsorted.as_slice().into()).unwrap();
        assert!(table.lookup_variations().is_none());
    }

    #[test]
    fn lookup_variations_versioned_empty_arrays() {
        let v1_0 = [0, 1, 0, 0, 0, 0, 0, 0];
        let table = FeatureVariations::read(v1_0.as_slice().into()).unwrap();
        assert_eq!(table.version(), MajorMinor::VERSION_1_0);
        assert_eq!(table.lookup_variation_record_count(), None);
        assert!(table.lookup_variation_records().is_none());
        assert!(feature_lookups(&table, 0).is_none());

        let v1_1 = [0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0];
        let table = FeatureVariations::read(v1_1.as_slice().into()).unwrap();
        assert_eq!(table.version(), MajorMinor::VERSION_1_1);
        assert_eq!(table.lookup_variation_record_count(), Some(0));
        assert_eq!(table.lookup_variation_records().unwrap().len(), 0);
        assert!(feature_lookups(&table, 0).is_none());
    }

    #[test]
    fn lookup_variations_truncated_arrays_and_invalid_offsets() {
        let bytes = [
            0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 3, 0xff, 0xff, 0xff, 0xff,
        ];
        for len in 8..bytes.len() {
            let table = FeatureVariations::read(bytes[..len].into()).unwrap();
            assert!(table.lookup_variation_records().is_none());
            assert!(feature_lookups(&table, 3).is_none());
        }
        let table = FeatureVariations::read(bytes.as_slice().into()).unwrap();
        assert_eq!(table.lookup_variation_records().unwrap().len(), 1);
        assert_eq!(
            feature_lookups(&table, 3).unwrap().err(),
            Some(ReadError::OutOfBounds)
        );
        assert!(feature_lookups(&table, 2).is_none());
        assert!(feature_lookups(&table, 4).is_none());
    }
}
