use super::*;
use font_test_data::bebuffer::BeBuffer;
use read_fonts::{
    tables::{gpos::Gpos, layout::FeatureLookupsFlags},
    types::Uint24,
};

type ConditionalLookups = (Option<Vec<u8>>, Vec<u16>);

fn feature_lookups(flags: u16, records: &[ConditionalLookups]) -> Vec<u8> {
    let mut bytes = BeBuffer::new()
        .push(1u16)
        .push(0u16)
        .push(flags)
        .push(records.len() as u32)
        .to_vec();
    bytes.resize(10 + records.len() * 8, 0);
    for (i, (condition, lookups)) in records.iter().enumerate() {
        let pos = 10 + i * 8;
        if let Some(condition) = condition {
            let offset = bytes.len() as u32;
            bytes[pos..pos + 4].copy_from_slice(&offset.to_be_bytes());
            bytes.extend(condition);
        }
        let offset = bytes.len() as u32;
        bytes[pos + 4..pos + 8].copy_from_slice(&offset.to_be_bytes());
        bytes.extend(
            BeBuffer::new()
                .push(lookups.len() as u16)
                .extend(lookups.iter().copied())
                .to_vec(),
        );
    }
    bytes
}

fn lookup_variations(records: &[(u16, Vec<u8>)]) -> Vec<u8> {
    let mut bytes = BeBuffer::new()
        .push(1u16)
        .push(1u16)
        .push(0u32)
        .push(records.len() as u32)
        .to_vec();
    bytes.resize(12 + records.len() * 6, 0);
    for (i, (feature, table)) in records.iter().enumerate() {
        let pos = 12 + i * 6;
        bytes[pos..pos + 2].copy_from_slice(&feature.to_be_bytes());
        let offset = bytes.len() as u32;
        bytes[pos + 2..pos + 6].copy_from_slice(&offset.to_be_bytes());
        bytes.extend(table);
    }
    bytes
}

fn subset(input: &[u8], plan: &Plan, gpos: bool) -> Result<Vec<u8>, SerializeErrorFlags> {
    let table = FeatureVariations::read(FontData::new(input)).unwrap();
    let mut s = Serializer::new(4 * 1024 * 1024);
    s.start_serialize().unwrap();
    let mut context = SubsetLayoutContext::new(if gpos { Gpos::TAG } else { Gsub::TAG });
    table.subset(plan, &mut s, &mut context)?;
    s.end_serialize();
    if s.in_error() {
        return Err(s.error());
    }
    Ok(s.copy_bytes())
}

fn plan(gpos: bool) -> Plan {
    let mut plan = Plan::default();
    let features = [(0, 0), (2, 1)].into_iter().collect();
    let lookups = [(3, 0), (7, 1)].into_iter().collect();
    if gpos {
        plan.gpos_features_w_duplicates = features;
        plan.gpos_lookups = lookups;
    } else {
        plan.gsub_features_w_duplicates = features;
        plan.gsub_lookups = lookups;
    }
    plan.layout_varidx_delta_map.insert(0x10002, (0x20003, 0));
    plan
}

#[test]
fn lookup_variations_subset_features_conditions_and_indices_in_both_tables() {
    let condition = BeBuffer::new()
        .push(5u16)
        .push(Uint24::new(5))
        .push(2u16)
        .push(-1i16)
        .push(0x10002u32)
        .to_vec();
    let bytes = lookup_variations(&[
        (
            0,
            feature_lookups(0, &[(Some(condition), vec![3, 5, 7]), (None, vec![7])]),
        ),
        (1, vec![0xff; 10]),
        (2, feature_lookups(1, &[(None, vec![5]), (None, vec![3])])),
    ]);
    for gpos in [false, true] {
        let output = subset(&bytes, &plan(gpos), gpos).unwrap();
        let table = FeatureVariations::read(FontData::new(&output)).unwrap();
        assert_eq!(table.version(), MajorMinor::new(1, 1));
        assert_eq!(table.feature_variation_record_count(), 0);
        assert_eq!(table.lookup_variation_record_count(), Some(2));
        let records = table.lookup_variation_records().unwrap();
        assert_eq!(
            records
                .iter()
                .map(|record| record.feature_index())
                .collect::<Vec<_>>(),
            [0, 1]
        );
        let first = records[0].feature_lookups(table.offset_data()).unwrap();
        assert_eq!(first.flags(), FeatureLookupsFlags::empty());
        assert_eq!(first.lookup_condition_count(), 2);
        for (i, expected) in [vec![0, 1], vec![1]].into_iter().enumerate() {
            let record = &first.lookup_condition_records()[i];
            let list = record.lookup_index_list(first.offset_data()).unwrap();
            assert_eq!(
                list.lookup_indices()
                    .iter()
                    .map(|index| index.get())
                    .collect::<Vec<_>>(),
                expected
            );
        }
        let Condition::Format5Negate(negate) = first.lookup_condition_records()[0]
            .condition(first.offset_data())
            .unwrap()
            .unwrap()
        else {
            panic!()
        };
        let Condition::Format2VariableValue(value) = negate.condition().unwrap() else {
            panic!()
        };
        assert_eq!(value.var_index(), 0x20003);
        assert!(first.lookup_condition_records()[1]
            .condition_offset()
            .is_null());
        let second = records[1].feature_lookups(table.offset_data()).unwrap();
        assert_eq!(second.flags(), FeatureLookupsFlags::ADD_DEFAULT_LOOKUPS);
        assert_eq!(second.lookup_condition_count(), 1);
        assert_eq!(
            second.lookup_condition_records()[0]
                .lookup_index_list(second.offset_data())
                .unwrap()
                .lookup_indices()[0]
                .get(),
            0
        );
    }
}

#[test]
fn lookup_variations_preserve_empty_overrides_and_nullable_lists() {
    for flags in [0, 1] {
        let mut bytes = lookup_variations(&[(
            0,
            feature_lookups(flags, &[(None, vec![5]), (None, vec![]), (None, vec![7])]),
        )]);
        // Null lookup list is discarded, not confused with a null condition.
        bytes[48..52].fill(0);
        let output = subset(&bytes, &plan(false), false).unwrap();
        let table = FeatureVariations::read(FontData::new(&output)).unwrap();
        assert_eq!(table.lookup_variation_record_count(), Some(1));
        let feature = table.lookup_variation_records().unwrap()[0]
            .feature_lookups(table.offset_data())
            .unwrap();
        assert_eq!(feature.flags().bits(), flags);
        assert_eq!(feature.lookup_condition_count(), 0);
    }
    assert_eq!(
        subset(&input(), &Plan::default(), false),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
    );
}

#[test]
fn lookup_variations_keep_legacy_first_match_barriers_and_version_suffix() {
    let mut bytes = BeBuffer::new()
        .push(1u16)
        .push(1u16)
        .push(2u32)
        .push(40u32)
        .push(0u32)
        .push(0u32)
        .push(54u32)
        .push(2u32)
        .push(0u16)
        .push(72u32)
        .push(2u16)
        .push(94u32)
        .push(1u16)
        .push(6u32)
        .push(2u16)
        .push(-1i16)
        .push(0x10002u32)
        .push(1u16)
        .push(0u16)
        .push(1u16)
        .push(0u16)
        .push(12u32)
        .push(0u16)
        .push(1u16)
        .push(7u16)
        .to_vec();
    bytes.extend(feature_lookups(0, &[(None, vec![7])]));
    bytes.extend(feature_lookups(1, &[]));
    let output = subset(&bytes, &plan(false), false).unwrap();
    let table = FeatureVariations::read(FontData::new(&output)).unwrap();
    assert_eq!(table.feature_variation_record_count(), 2);
    assert_eq!(table.lookup_variation_record_count(), Some(2));
    let records = table.feature_variation_records();
    assert!(records[0].feature_table_substitution_offset().is_null());
    let conditions = records[0]
        .condition_set(table.offset_data())
        .unwrap()
        .unwrap();
    let Condition::Format2VariableValue(condition) = conditions.conditions().get(0).unwrap() else {
        panic!()
    };
    assert_eq!(condition.var_index(), 0x20003);
    let substitutions = records[1]
        .feature_table_substitution(table.offset_data())
        .unwrap()
        .unwrap();
    let feature = substitutions.substitutions()[0]
        .alternate_feature(substitutions.offset_data())
        .unwrap();
    assert_eq!(feature.lookup_list_indices()[0].get(), 1);

    // A retained legacy prefix still needs the version-1.1 zero-count suffix.
    bytes[24..28].fill(0);
    let output = subset(&bytes, &plan(false), false).unwrap();
    let table = FeatureVariations::read(FontData::new(&output)).unwrap();
    assert_eq!(table.version(), MajorMinor::new(1, 1));
    assert!(table.lookup_variation_records().unwrap().is_empty());
}

#[test]
fn lookup_variations_reject_bad_counts_offsets_and_versions() {
    for range in [4..8, 8..12, 14..18, 24..28, 28..32, 32..36] {
        let mut bytes = input();
        bytes[range].fill(0xff);
        assert_eq!(
            subset(&bytes, &plan(false), false),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
    }
    for length in [8, 11, 17, 37, 39] {
        let bytes = input();
        assert_eq!(
            subset(&bytes[..length], &plan(false), false),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
    }
    for range in [2..4, 20..22] {
        let mut bytes = input();
        bytes[range].copy_from_slice(&2u16.to_be_bytes());
        assert_eq!(
            subset(&bytes, &plan(false), false),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER)
        );
    }
}

#[test]
fn lookup_variations_keep_32_bit_condition_counts_and_16_bit_lookup_indices() {
    let bytes = lookup_variations(&[(0, feature_lookups(0, &vec![(None, vec![7]); 65537]))]);
    let output = subset(&bytes, &plan(false), false).unwrap();
    let table = FeatureVariations::read(FontData::new(&output)).unwrap();
    let feature = table.lookup_variation_records().unwrap()[0]
        .feature_lookups(table.offset_data())
        .unwrap();
    assert_eq!(feature.lookup_condition_count(), 65537);
    assert!(feature
        .lookup_condition_records()
        .iter()
        .all(|record| record
            .lookup_index_list(feature.offset_data())
            .unwrap()
            .lookup_indices()[0]
            .get()
            == 1));
    let bytes = lookup_variations(&[(0, feature_lookups(0, &[(None, (0..u16::MAX).collect())]))]);
    let mut plan = plan(false);
    plan.gsub_lookups = (0..u16::MAX).map(|index| (index, index)).collect();
    let output = subset(&bytes, &plan, false).unwrap();
    let table = FeatureVariations::read(FontData::new(&output)).unwrap();
    let feature = table.lookup_variation_records().unwrap()[0]
        .feature_lookups(table.offset_data())
        .unwrap();
    let list = feature.lookup_condition_records()[0]
        .lookup_index_list(feature.offset_data())
        .unwrap();
    assert_eq!(list.lookup_index_count(), u16::MAX);
    assert_eq!(list.lookup_indices().last().unwrap().get(), u16::MAX - 1);
}

#[test]
fn lookup_variation_conditions_retain_and_remap_gdef_store_rows() {
    use crate::{Subset, SubsetState};
    use read_fonts::{tables::gdef::Gdef, TableProvider};
    use write_fonts::FontBuilder;

    for gpos in [false, true] {
        let condition = BeBuffer::new().push(2u16).push(-1i16).push(1u32).to_vec();
        let variations =
            lookup_variations(&[(0, feature_lookups(0, &[(Some(condition), vec![7])]))]);
        let mut layout = BeBuffer::new()
            .push(1u16)
            .push(1u16)
            .push(0u16)
            .push(0u16)
            .push(0u16)
            .push(14u32)
            .to_vec();
        layout.extend(&variations);
        let mut gdef = BeBuffer::new()
            .push(1u16)
            .push(3u16)
            .push(0u16)
            .push(0u16)
            .push(0u16)
            .push(0u16)
            .push(0u16)
            .push(18u32)
            .to_vec();
        gdef.extend(
            BeBuffer::new()
                .push(1u16)
                .push(12u32)
                .push(1u16)
                .push(22u32)
                .push(1u16)
                .push(1u16)
                .push(0u16)
                .push(0x4000u16)
                .push(0x4000u16)
                .push(2u16)
                .push(1u16)
                .push(1u16)
                .push(0u16)
                .push(10i16)
                .push(20i16)
                .to_vec(),
        );
        let mut builder = FontBuilder::new();
        builder.add_raw(if gpos { Gpos::TAG } else { Gsub::TAG }, layout);
        builder.add_raw(Gdef::TAG, gdef);
        let bytes = builder.build();
        let font = FontRef::new(&bytes).unwrap();
        let mut plan = plan(gpos);
        plan.layout_varidx_delta_map.clear();
        plan.collect_layout_var_indices(&font);
        assert_eq!(plan.layout_varidx_delta_map.get(&1), Some(&(0, 0)));
        assert_eq!(plan.layout_varidx_delta_map.len(), 1);
        let output = subset(&variations, &plan, gpos).unwrap();
        let table = FeatureVariations::read(FontData::new(&output)).unwrap();
        let lookups = table.lookup_variation_records().unwrap()[0]
            .feature_lookups(table.offset_data())
            .unwrap();
        let Condition::Format2VariableValue(condition) = lookups.lookup_condition_records()[0]
            .condition(lookups.offset_data())
            .unwrap()
            .unwrap()
        else {
            panic!()
        };
        assert_eq!(condition.var_index(), 0);
        let mut state = SubsetState::default();
        let mut s = Serializer::new(1024);
        s.start_serialize().unwrap();
        font.gdef()
            .unwrap()
            .subset_with_state(&plan, &font, &mut state, &mut s, &mut FontBuilder::new())
            .unwrap();
        s.end_serialize();
        assert!(!s.in_error());
        assert!(state.has_gdef_varstore);
        let output = s.copy_bytes();
        let gdef = Gdef::read(FontData::new(&output)).unwrap();
        let store = gdef.item_var_store().unwrap().unwrap();
        let data = store.item_variation_data().get(0).unwrap().unwrap();
        assert_eq!(data.item_count(), 1);
        assert_eq!(data.delta_set(0).collect::<Vec<_>>(), [20]);
    }
}

fn input() -> Vec<u8> {
    BeBuffer::new()
        .push(1u16)
        .push(1u16)
        .push(0u32)
        .push(1u32)
        .push(0u16)
        .push(18u32)
        .push(1u16)
        .push(0u16)
        .push(0u16)
        .push(1u32)
        .push(0u32)
        .push(18u32)
        .push(1u16)
        .push(7u16)
        .to_vec()
}

fn features(defaults: &[&[u16]]) -> Vec<u8> {
    let mut bytes = BeBuffer::new().push(defaults.len() as u16);
    let mut offset = 2 + defaults.len() * 6;
    for default in defaults {
        bytes = bytes.push(Tag::new(b"liga")).push(offset as u16);
        offset += 4 + default.len() * 2;
    }
    for default in defaults {
        bytes = bytes
            .push(0u16)
            .push(default.len() as u16)
            .extend(default.iter().copied());
    }
    bytes.to_vec()
}

#[test]
fn lookup_variations_keep_features_with_no_retained_default_lookups() {
    let input = input();
    let variations = FeatureVariations::read(FontData::new(&input)).unwrap();
    let retained = [7u16].into_iter().collect();
    let alternate = crate::layout::collect_features_with_retained_subs(&variations, &retained);
    assert_eq!(alternate.iter().collect::<Vec<_>>(), [0]);
    let features = features(&[&[], &[1], &[1]]);
    let table = FeatureList::read(FontData::new(&features)).unwrap();
    let pruned = crate::layout::prune_features(
        &table,
        &alternate,
        &retained,
        [0u16, 1, 2].into_iter().collect(),
    );
    assert_eq!(pruned.iter().collect::<Vec<_>>(), [0]);
    assert!(features_with_retained_lookups(&variations, &[1u16].into_iter().collect()).is_empty());
}

#[test]
fn lookup_variations_prevent_default_only_feature_deduplication() {
    let mut input = input();
    let features = features(&[&[1], &[1], &[1]]);
    let table = FeatureList::read(FontData::new(&features)).unwrap();
    let retained = [1u16, 7].into_iter().collect();
    let selected: IntSet<u16> = [0u16, 1, 2].into_iter().collect();
    let duplicates = crate::layout::find_duplicate_features(
        &table,
        &retained,
        selected.clone(),
        &IntSet::empty(),
    );
    assert_eq!(duplicates.get(&1), Some(&0));
    assert_eq!(duplicates.get(&2), Some(&0));

    // feature 2 has a lookup variation: it is never merged.
    input[12..14].copy_from_slice(&2u16.to_be_bytes());
    let variations = FeatureVariations::read(FontData::new(&input)).unwrap();
    let variable = crate::layout::collect_variable_feature_indices(&variations);
    let duplicates =
        crate::layout::find_duplicate_features(&table, &retained, selected.clone(), &variable);
    assert_eq!(duplicates.get(&0), Some(&0));
    assert_eq!(duplicates.get(&1), Some(&0));
    assert_eq!(duplicates.get(&2), Some(&2));

    // feature 0 has a lookup variation: 1 cannot merge into 0, but 2 can
    // still merge into 1, matching harfbuzz.
    input[12..14].fill(0);
    let variations = FeatureVariations::read(FontData::new(&input)).unwrap();
    let variable = crate::layout::collect_variable_feature_indices(&variations);
    let duplicates = crate::layout::find_duplicate_features(&table, &retained, selected, &variable);
    assert_eq!(duplicates.get(&0), Some(&0));
    assert_eq!(duplicates.get(&1), Some(&1));
    assert_eq!(duplicates.get(&2), Some(&1));
}

#[test]
fn lookup_variations_feature_retention_handles_empty_and_invalid_lists() {
    let retained = [7u16].into_iter().collect();
    let mut bytes = input();
    bytes[36..38].fill(0);
    let table = FeatureVariations::read(FontData::new(&bytes)).unwrap();
    assert!(features_with_retained_lookups(&table, &retained).is_empty());
    for range in [14..18, 32..36, 24..28] {
        let mut bytes = input();
        bytes[range].fill(0xff);
        let table = FeatureVariations::read(FontData::new(&bytes)).unwrap();
        assert!(features_with_retained_lookups(&table, &retained).is_empty());
    }
}
