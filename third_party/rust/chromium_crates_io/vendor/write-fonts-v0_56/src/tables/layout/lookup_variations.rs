//! Tests for ISO OFF fifth-edition lookup variations.

use super::*;
use crate::{
    dump_table,
    from_obj::ToOwnedTable,
    tables::{gpos::Gpos, gsub::Gsub},
};
use read_fonts::{tables::layout as read_layout, FontRead};

/// The conditional lookups of a feature, found through the raw records.
fn feature_lookups<'a>(
    table: &read_layout::FeatureVariations<'a>,
    feature_index: u16,
) -> Option<Result<read_layout::FeatureLookups<'a>, read_fonts::ReadError>> {
    let records = table.lookup_variation_records()?;
    let record = records
        .iter()
        .find(|record| record.feature_index() == feature_index)?;
    Some(record.feature_lookups(table.offset_data()))
}

fn lookup_variations() -> FeatureVariations {
    let range = ConditionFormat1::new(1, F2Dot14::ZERO, F2Dot14::ONE);
    let value = ConditionFormat2::new(-10, 0x0002_0003);
    let conditions = vec![
        LookupConditionRecord::new(None, LookupIndexList::new(vec![1, 2])),
        LookupConditionRecord::new(Some(range.clone().into()), LookupIndexList::new(vec![3])),
        LookupConditionRecord::new(Some(value.clone().into()), LookupIndexList::new(vec![4])),
        LookupConditionRecord::new(
            Some(ConditionFormat3::new(2, vec![range.clone().into(), value.into()]).into()),
            LookupIndexList::new(vec![5]),
        ),
        LookupConditionRecord::new(
            Some(ConditionFormat4::new(1, vec![range.clone().into()]).into()),
            LookupIndexList::new(vec![6]),
        ),
        LookupConditionRecord::new(
            Some(ConditionFormat5::new(range.into()).into()),
            LookupIndexList::new(vec![7]),
        ),
    ];
    let mut table = FeatureVariations::new(vec![FeatureVariationRecord::new(
        None,
        Some(FeatureTableSubstitution::new(vec![
            FeatureTableSubstitutionRecord::new(4, Feature::new(None, vec![8])),
        ])),
    )]);
    table.lookup_variation_records = Some(vec![
        LookupVariationRecord::new(
            4,
            FeatureLookups::new(FeatureLookupsFlags::ADD_DEFAULT_LOOKUPS, conditions),
        ),
        LookupVariationRecord::new(9, FeatureLookups::new(FeatureLookupsFlags::empty(), vec![])),
    ]);
    table
}

#[test]
fn lookup_variations_roundtrip_versions() {
    for records in [None, Some(vec![])] {
        let mut table = FeatureVariations::new(vec![]);
        table.lookup_variation_records = records;
        let bytes = dump_table(&table).unwrap();
        let parsed = read_layout::FeatureVariations::read(bytes.as_slice().into()).unwrap();
        assert_eq!(parsed.version(), table.compute_version());
        assert_eq!(
            bytes.len(),
            if table.lookup_variation_records.is_some() {
                12
            } else {
                8
            }
        );
        let owned: FeatureVariations = parsed.to_owned_table();
        assert_eq!(owned, table);
        assert_eq!(dump_table(&owned).unwrap(), bytes);
    }
}

#[test]
fn lookup_variations_roundtrip_records_and_offsets() {
    let table = lookup_variations();
    let bytes = dump_table(&table).unwrap();
    let parsed = read_layout::FeatureVariations::read(bytes.as_slice().into()).unwrap();
    assert_eq!(parsed.version(), MajorMinor::VERSION_1_1);
    assert_eq!(parsed.feature_variation_record_count(), 1);
    assert_eq!(parsed.lookup_variation_record_count(), Some(2));
    assert_eq!(parsed.lookup_variation_record_count_byte_range(), 16..20);
    assert_eq!(parsed.lookup_variation_records_byte_range(), 20..32);
    assert_eq!(parsed.index_for_coords(&[]), Some(0));
    let substitutions = parsed.feature_variation_records()[0]
        .feature_table_substitution(parsed.offset_data())
        .unwrap()
        .unwrap();
    let alternate = substitutions.substitutions()[0]
        .alternate_feature(substitutions.offset_data())
        .unwrap();
    assert_eq!(alternate.lookup_list_indices()[0].get(), 8);

    let lookups = feature_lookups(&parsed, 4).unwrap().unwrap();
    assert_eq!(lookups.version(), MajorMinor::VERSION_1_0);
    assert_eq!(lookups.flags().bits(), 1);
    assert_eq!(lookups.lookup_condition_count(), 6);
    let records = lookups.lookup_condition_records();
    assert!(records[0].condition_offset().is_null());
    assert!(records[0].condition(lookups.offset_data()).is_none());
    let indices = records[0].lookup_index_list(lookups.offset_data()).unwrap();
    assert_eq!(indices.lookup_index_count(), 2);
    assert_eq!(
        indices
            .lookup_indices()
            .iter()
            .map(|i| i.get())
            .collect::<Vec<_>>(),
        [1, 2]
    );
    for (record, format) in records[1..].iter().zip(1..=5) {
        let condition = record.condition(lookups.offset_data()).unwrap().unwrap();
        assert_eq!(condition.format(), format);
        assert_eq!(
            record
                .lookup_index_list(lookups.offset_data())
                .unwrap()
                .lookup_index_count(),
            1
        );
    }
    let read_layout::Condition::Format2VariableValue(value) = records[2]
        .condition(lookups.offset_data())
        .unwrap()
        .unwrap()
    else {
        panic!()
    };
    assert_eq!(value.default_value(), -10);
    assert_eq!(value.var_index(), 0x0002_0003);
    let empty = feature_lookups(&parsed, 9).unwrap().unwrap();
    assert!(empty.flags().is_empty());
    assert_eq!(empty.lookup_condition_count(), 0);
    for missing in [0, 3, 5, 10, u16::MAX] {
        assert!(feature_lookups(&parsed, missing).is_none());
    }
    let owned: FeatureVariations = parsed.to_owned_table();
    assert_eq!(owned, table);
    assert_eq!(dump_table(&owned).unwrap(), bytes);
}

#[test]
fn lookup_variations_in_gsub_and_gpos() {
    let variations = lookup_variations();
    let gsub = Gsub {
        feature_variations: Some(variations.clone()).into(),
        ..Default::default()
    };
    let bytes = dump_table(&gsub).unwrap();
    let parsed = read_fonts::tables::gsub::Gsub::read(bytes.as_slice().into()).unwrap();
    let feature_vars = parsed.feature_variations().unwrap().unwrap();
    assert_eq!(parsed.version(), MajorMinor::VERSION_1_1);
    assert_eq!(
        feature_lookups(&feature_vars, 4)
            .unwrap()
            .unwrap()
            .lookup_condition_count(),
        6
    );
    let owned: Gsub = parsed.to_owned_table();
    assert_eq!(owned, gsub);

    let gpos = Gpos {
        feature_variations: Some(variations).into(),
        ..Default::default()
    };
    let bytes = dump_table(&gpos).unwrap();
    let parsed = read_fonts::tables::gpos::Gpos::read(bytes.as_slice().into()).unwrap();
    let feature_vars = parsed.feature_variations().unwrap().unwrap();
    assert_eq!(parsed.version(), MajorMinor::VERSION_1_1);
    assert_eq!(
        feature_lookups(&feature_vars, 4)
            .unwrap()
            .unwrap()
            .lookup_condition_count(),
        6
    );
    let owned: Gpos = parsed.to_owned_table();
    assert_eq!(owned, gpos);
}

#[test]
fn lookup_variations_use_32_bit_record_counts() {
    let mut table = FeatureVariations::new(vec![]);
    table.lookup_variation_records = Some(
        (0..=u16::MAX)
            .map(|feature| LookupVariationRecord::new(feature, FeatureLookups::default()))
            .collect(),
    );
    let bytes = dump_table(&table).unwrap();
    let parsed = read_layout::FeatureVariations::read(bytes.as_slice().into()).unwrap();
    assert_eq!(parsed.lookup_variation_record_count(), Some(65536));
    assert_eq!(parsed.lookup_variation_records().unwrap().len(), 65536);
    assert_eq!(
        feature_lookups(&parsed, u16::MAX)
            .unwrap()
            .unwrap()
            .lookup_condition_count(),
        0
    );

    let lookups = FeatureLookups::new(
        FeatureLookupsFlags::empty(),
        vec![LookupConditionRecord::new(None, LookupIndexList::new(vec![u16::MAX])); 65537],
    );
    let bytes = dump_table(&lookups).unwrap();
    let parsed = read_layout::FeatureLookups::read(bytes.as_slice().into()).unwrap();
    assert_eq!(parsed.lookup_condition_count(), 65537);
    assert_eq!(parsed.lookup_condition_records().len(), 65537);
    let list = parsed.lookup_condition_records()[65536]
        .lookup_index_list(parsed.offset_data())
        .unwrap();
    assert_eq!(list.lookup_indices()[0].get(), u16::MAX);
}

#[test]
fn lookup_variations_validate_order_and_index_counts() {
    let mut table = lookup_variations();
    let records = table.lookup_variation_records.as_mut().unwrap();
    records.swap(0, 1);
    assert!(dump_table(&table).is_err());
    let records = table.lookup_variation_records.as_mut().unwrap();
    records[1].feature_index = records[0].feature_index;
    assert!(dump_table(&table).is_err());
    assert!(dump_table(&LookupIndexList::new(vec![0; 65536])).is_err());
}
