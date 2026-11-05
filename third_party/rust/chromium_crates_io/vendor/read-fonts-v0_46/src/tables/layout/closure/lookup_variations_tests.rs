use super::*;
use crate::{FontData, FontRead};
use font_test_data::bebuffer::BeBuffer;

fn input() -> Vec<u8> {
    BeBuffer::new()
        .push(1u16)
        .push(1u16)
        .push(0u32)
        .push(3u32)
        .push(0u16)
        .push(30u32)
        .push(2u16)
        .push(66u32)
        .push(7u16)
        .push(90u32)
        .push(1u16)
        .push(0u16)
        .push(1u16)
        .push(2u32)
        .push(0u32)
        .push(26u32)
        .push(0u32)
        .push(32u32)
        .push(2u16)
        .push(1u16)
        .push(3u16)
        .push(1u16)
        .push(7u16)
        .push(1u16)
        .push(0u16)
        .push(0u16)
        .push(1u32)
        .push(0u32)
        .push(18u32)
        .push(2u16)
        .push(3u16)
        .push(8u16)
        .push(1u16)
        .push(0u16)
        .push(0u16)
        .push(1u32)
        .push(0u32)
        .push(18u32)
        .push(1u16)
        .push(9u16)
        .to_vec()
}

#[test]
fn lookup_variations_collect_all_conditional_lookups_for_selected_features() {
    let bytes = input();
    let table = FeatureVariations::read(FontData::new(&bytes)).unwrap();
    for (features, expected) in [
        (vec![0], vec![1, 3, 7]),
        (vec![2], vec![3, 8]),
        (vec![0, 2], vec![1, 3, 7, 8]),
        (vec![1], vec![]),
    ] {
        let selected = features.into_iter().collect();
        assert_eq!(
            table
                .collect_lookups(&selected)
                .unwrap()
                .iter()
                .collect::<Vec<_>>(),
            expected
        );
    }
    assert_eq!(
        table
            .collect_lookups(&IntSet::all())
            .unwrap()
            .iter()
            .collect::<Vec<_>>(),
        [1, 3, 7, 8, 9]
    );
}

#[test]
fn lookup_variations_validate_selected_records_and_skip_unselected_ones() {
    let mut bytes = input();
    let selected = [0u16].into_iter().collect();
    bytes[26..30].fill(0xff);
    let table = FeatureVariations::read(FontData::new(&bytes)).unwrap();
    assert_eq!(
        table
            .collect_lookups(&selected)
            .unwrap()
            .iter()
            .collect::<Vec<_>>(),
        [1, 3, 7]
    );
    assert_eq!(
        table.collect_lookups(&IntSet::all()),
        Err(ReadError::OutOfBounds)
    );
    for len in [8, 11, 29] {
        let table = FeatureVariations::read(FontData::new(&bytes[..len])).unwrap();
        assert_eq!(
            table.collect_lookups(&selected),
            Err(ReadError::OutOfBounds)
        );
    }
    for range in [4..8, 8..12] {
        let mut bytes = input();
        bytes[range].fill(0xff);
        let table = FeatureVariations::read(FontData::new(&bytes)).unwrap();
        assert_eq!(
            table.collect_lookups(&selected),
            Err(ReadError::OutOfBounds)
        );
    }
    bytes = input();
    bytes[36..40].copy_from_slice(&u32::MAX.to_be_bytes());
    let table = FeatureVariations::read(FontData::new(&bytes)).unwrap();
    assert_eq!(
        table.collect_lookups(&selected),
        Err(ReadError::OutOfBounds)
    );
    bytes = input();
    bytes[56..58].copy_from_slice(&u16::MAX.to_be_bytes());
    let table = FeatureVariations::read(FontData::new(&bytes)).unwrap();
    assert_eq!(
        table.collect_lookups(&selected),
        Err(ReadError::OutOfBounds)
    );
}
