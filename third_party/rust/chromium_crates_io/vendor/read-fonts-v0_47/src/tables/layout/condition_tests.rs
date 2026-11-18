use super::*;
use crate::model::Error;
use crate::tables::layout::FeatureVariations;
use crate::tables::variations::ItemVariationStore;
use alloc::vec;
use alloc::vec::Vec;

#[test]
fn delta_callback_errors_survive_negation_and_no_variation_skips_callback() {
    let bytes = [0, 5, 0, 0, 5, 0, 2, 0, 1, 0, 0, 0, 0];
    let condition = Condition::read(FontData::new(&bytes)).unwrap();
    assert_eq!(
        condition.evaluate(&[], |_| Err::<f64, _>(Error::InvalidOperation)),
        Err(Error::InvalidOperation)
    );
    let bytes = [0, 2, 0, 1, 255, 255, 255, 255];
    let condition = Condition::read(FontData::new(&bytes)).unwrap();
    assert_eq!(
        condition.evaluate(&[], |_| Err::<f64, _>(Error::InvalidOperation)),
        Ok(true)
    );
}

#[test]
fn condition_sets_and_lookup_records_reject_bad_offsets() {
    let bytes = [0, 1, 0, 0, 0, 6, 0, 5, 0, 0, 0];
    let set = ConditionSet::read(FontData::new(&bytes)).unwrap();
    assert_eq!(set.evaluate(&[], |_| Ok::<_, Error>(0.0)), Ok(false));
    let truncated = ConditionSet::read(FontData::new(&bytes[..2])).unwrap();
    assert_eq!(
        truncated.evaluate(&[], |_| Ok::<_, Error>(0.0)),
        Err(Error::Malformed)
    );
    let record = LookupConditionRecord {
        condition_offset: crate::types::BigEndian::new([0, 0, 0, 6]),
        lookup_index_list_offset: crate::types::Offset32::new(0).into(),
    };
    assert_eq!(
        record.evaluate(FontData::new(&bytes), &[], |_| Ok::<_, Error>(0.0)),
        Ok(false)
    );
    assert!(record
        .evaluate(FontData::new(&bytes[..2]), &[], |_| Ok::<_, Error>(0.0))
        .is_err());
    let null = LookupConditionRecord {
        condition_offset: crate::types::BigEndian::new([0; 4]),
        lookup_index_list_offset: crate::types::Offset32::new(0).into(),
    };
    assert_eq!(
        null.evaluate(FontData::new(&[]), &[], |_| Ok::<_, Error>(0.0)),
        Ok(true)
    );
}

fn evaluate(bytes: &[u8], coords: &[F2Dot14]) -> Option<bool> {
    Condition::read(FontData::new(bytes))
        .ok()?
        .evaluate(coords, |_| Ok::<_, Error>(0.0))
        .ok()
}

fn axis_range(axis: u16, min: f32, max: f32) -> Vec<u8> {
    let mut bytes = vec![0, 1];
    bytes.extend_from_slice(&axis.to_be_bytes());
    bytes.extend_from_slice(&F2Dot14::from_f32(min).to_be_bytes());
    bytes.extend_from_slice(&F2Dot14::from_f32(max).to_be_bytes());
    bytes
}

#[test]
fn axis_ranges_include_the_endpoints_and_missing_axes_are_zero() {
    let condition = axis_range(0, 0.25, 0.75);
    for (coord, matches) in [
        (0.0, false),
        (0.25, true),
        (0.5, true),
        (0.75, true),
        (1.0, false),
    ] {
        assert_eq!(
            evaluate(&condition, &[F2Dot14::from_f32(coord)]),
            Some(matches)
        );
    }
    assert_eq!(evaluate(&condition, &[]), Some(false));
    assert_eq!(evaluate(&axis_range(5, 0.0, 0.0), &[]), Some(true));
}

#[test]
fn variable_values_keep_fractional_deltas_and_use_a_strict_positive_test() {
    let bytes = [
        0, 1, 0, 0, 0, 12, 0, 1, 0, 0, 0, 22, // ItemVariationStore.
        0, 1, 0, 1, 0, 0, 0x40, 0, 0x40, 0, // One axis and one region.
        0, 1, 0, 1, 0, 1, 0, 0, 0, 1, // One delta of 1.
    ];
    let store = ItemVariationStore::read(FontData::new(&bytes)).unwrap();
    let condition = Condition::read(FontData::new(&[0, 2, 0, 0, 0, 0, 0, 0])).unwrap();
    for (coord, matches) in [(-1.0, false), (0.0, false), (0.5, true), (1.0, true)] {
        let coords = [F2Dot14::from_f32(coord)];
        assert_eq!(
            condition
                .evaluate(&coords, store_deltas(Some(&store), &coords))
                .ok(),
            Some(matches)
        );
    }
    for default_value in [-1i16, 0, 1] {
        let mut bytes = vec![0, 2];
        bytes.extend_from_slice(&default_value.to_be_bytes());
        bytes.extend_from_slice(&u32::MAX.to_be_bytes());
        assert_eq!(evaluate(&bytes, &[]), Some(default_value > 0));
    }
}

#[test]
fn compound_conditions_and_null_children() {
    let mut condition = vec![0, 3, 2, 0, 0, 9, 0, 0, 17];
    condition.extend_from_slice(&axis_range(0, 0.25, 0.75));
    condition.extend_from_slice(&axis_range(1, -1.0, -0.25));
    assert_eq!(
        evaluate(
            &condition,
            &[F2Dot14::from_f32(0.5), F2Dot14::from_f32(-0.5)]
        ),
        Some(true)
    );
    assert_eq!(evaluate(&condition, &[F2Dot14::from_f32(0.5)]), Some(false));
    condition[1] = 4;
    assert_eq!(evaluate(&condition, &[F2Dot14::from_f32(0.5)]), Some(true));
    assert_eq!(evaluate(&condition, &[]), Some(false));

    let mut negated = vec![0, 5, 0, 0, 5];
    negated.extend_from_slice(&condition);
    assert_eq!(evaluate(&negated, &[]), Some(true));
    assert_eq!(evaluate(&negated, &[F2Dot14::from_f32(0.5)]), Some(false));
    assert_eq!(evaluate(&[0, 3, 1, 0, 0, 0], &[]), Some(true));
    assert_eq!(evaluate(&[0, 4, 1, 0, 0, 0], &[]), Some(true));
    assert_eq!(evaluate(&[0, 5, 0, 0, 0], &[]), Some(false));
    assert_eq!(evaluate(&[0, 3, 0], &[]), Some(true));
    assert_eq!(evaluate(&[0, 4, 0], &[]), Some(false));
}

#[test]
fn malformed_and_expensive_conditions_fail_closed() {
    assert_eq!(evaluate(&[0, 3, 1], &[]), None);
    assert_eq!(evaluate(&[0, 5, 0xFF, 0xFF, 0xFF], &[]), None);
    let mut deep = Vec::new();
    for _ in 0..MAX_RECURSION_DEPTH {
        deep.extend_from_slice(&[0, 5, 0, 0, 5]);
    }
    deep.extend_from_slice(&axis_range(0, 0.0, 0.0));
    assert_eq!(evaluate(&deep, &[]), None);
    let mut repeated = Vec::new();
    for _ in 0..17 {
        repeated.extend_from_slice(&[0, 3, 2, 0, 0, 9, 0, 0, 9]);
    }
    repeated.extend_from_slice(&axis_range(0, 0.0, 0.0));
    assert_eq!(evaluate(&repeated, &[]), None);
}

#[test]
fn feature_variations_select_the_first_matching_record_at_default_too() {
    let mut bytes = vec![0, 1, 0, 0, 0, 0, 0, 2];
    bytes.extend_from_slice(&[0, 0, 0, 24, 0, 0, 0, 0]);
    bytes.extend_from_slice(&[0, 0, 0, 0, 0, 0, 0, 0]);
    bytes.extend_from_slice(&[0, 1, 0, 0, 0, 6]);
    bytes.extend_from_slice(&[0, 5, 0, 0, 5]);
    bytes.extend_from_slice(&axis_range(0, 0.25, 0.75));
    let variations = FeatureVariations::read(FontData::new(&bytes)).unwrap();
    assert_eq!(variations.index_for_coords(&[]), Some(0));
    assert_eq!(
        variations.index_for_coords(&[F2Dot14::from_f32(0.5)]),
        Some(1)
    );
}

#[test]
fn unknown_condition_formats_are_false() {
    // OR of an unknown format and a range holding at the default location.
    let bytes = [
        0, 4, 2, 0, 0, 9, 0, 0, 11, // OR with two children
        0, 99, // unknown format
        0, 1, 0, 0, 0xC0, 0, 0x40, 0, // axis 0 in [-1, 1]
    ];
    assert_eq!(evaluate(&bytes, &[]), Some(true));
    // NOT of an unknown format.
    let bytes = [0, 5, 0, 0, 5, 0, 99];
    assert_eq!(evaluate(&bytes, &[]), Some(true));
}
