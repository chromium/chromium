//! Retention and remapping of lookup-oriented feature variations.

use super::*;
use read_fonts::{
    tables::layout::{
        FeatureLookups, LookupConditionRecord, LookupIndexList, LookupVariationRecord,
    },
    types::MajorMinor,
};

impl<'a> SubsetTable<'a> for LookupVariationRecord {
    type ArgsForSubset = (
        FontData<'a>,
        &'a FastHashMap<u16, u16>,
        &'a FastHashMap<u16, u16>,
    );
    type Output = ();

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let (data, features, lookups) = args;
        let Some(index) = features.get(&self.feature_index()) else {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        };
        if self.feature_lookups_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let table = self
            .feature_lookups(data)
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        s.embed(*index)?;
        let pos = s.embed(0u32)?;
        Offset32::serialize_subset(&table, s, plan, lookups, pos)
    }
}

impl<'a> SubsetTable<'a> for FeatureLookups<'_> {
    type ArgsForSubset = &'a FastHashMap<u16, u16>;
    type Output = ();

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookups: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        if self.version() != MajorMinor::VERSION_1_0 {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
        }
        if u64::from(self.lookup_condition_count()) * 8
            > self.offset_data().len().saturating_sub(10) as u64
        {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        }
        s.embed(self.version())?;
        s.embed(self.flags())?;
        let pos = s.embed(0u32)?;
        let mut count = 0u32;
        for record in self.lookup_condition_records() {
            if !record
                .subset(plan, s, (self.offset_data(), lookups))
                .is_empty()?
            {
                count += 1;
            }
        }
        s.copy_assign(pos, count);
        // In replacement mode even an empty table must suppress defaults.
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for LookupConditionRecord {
    type ArgsForSubset = (FontData<'a>, &'a FastHashMap<u16, u16>);
    type Output = ();

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let (data, lookups) = args;
        if self.lookup_index_list_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let list = self
            .lookup_index_list(data)
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        if list.min_table_bytes().is_empty() {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        }
        if !list
            .lookup_indices()
            .iter()
            .any(|index| lookups.contains_key(&index.get()))
        {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let condition_pos = s.embed(0u32)?;
        let list_pos = s.embed(0u32)?;
        if let Some(condition) = self
            .condition(data)
            .transpose()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
        {
            Offset32::serialize_subset(&condition, s, plan, (), condition_pos)?;
        }
        Offset32::serialize_subset(&list, s, plan, lookups, list_pos)
    }
}

impl<'a> SubsetTable<'a> for LookupIndexList<'_> {
    type ArgsForSubset = &'a FastHashMap<u16, u16>;
    type Output = ();

    fn subset(
        &self,
        _: &Plan,
        s: &mut Serializer,
        lookups: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        if self.min_table_bytes().is_empty() {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        }
        let pos = s.embed(0u16)?;
        let mut count = 0u16;
        for index in self
            .lookup_indices()
            .iter()
            .filter_map(|index| lookups.get(&index.get()))
        {
            s.embed(*index)?;
            count += 1;
        }
        s.copy_assign(pos, count);
        Ok(())
    }
}

pub(super) fn features_with_retained_lookups(
    variations: &FeatureVariations<'_>,
    retained: &IntSet<u16>,
) -> IntSet<u16> {
    let mut features = IntSet::empty();
    for record in variations.lookup_variation_records().unwrap_or_default() {
        if record.feature_lookups_offset().is_null() {
            continue;
        }
        let Ok(table) = record.feature_lookups(variations.offset_data()) else {
            return IntSet::empty();
        };
        if u64::from(table.lookup_condition_count()) * 8
            > table.offset_data().len().saturating_sub(10) as u64
        {
            return IntSet::empty();
        }
        for condition in table.lookup_condition_records() {
            if condition.lookup_index_list_offset().is_null() {
                continue;
            }
            let Ok(indices) = condition.lookup_index_list(table.offset_data()) else {
                return IntSet::empty();
            };
            if indices.min_table_bytes().is_empty() {
                return IntSet::empty();
            }
            if indices
                .lookup_indices()
                .iter()
                .any(|i| retained.contains(i.get()))
            {
                features.insert(record.feature_index());
                break;
            }
        }
    }
    features
}

#[cfg(test)]
mod tests;
