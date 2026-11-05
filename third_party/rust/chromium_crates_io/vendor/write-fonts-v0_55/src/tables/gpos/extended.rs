//! Tests for the ISO OFF fifth-edition positioning formats.

use super::*;
use crate::{dump_table, from_obj::ToOwnedTable, tables::layout::LookupFlag};
use read_fonts::{tables::gpos as raw, FontData, FontRead};

fn coverage() -> CoverageTable {
    [GlyphId::new(65536)].into_iter().collect()
}

fn value() -> ValueRecord {
    ValueRecord::new()
        .with_x_advance(-123)
        .with_x_placement_device(VariationIndex::new(3, 7))
}

fn anchor() -> AnchorTable {
    AnchorFormat1::new(123, -456).into()
}

fn marks() -> MarkArray2 {
    // A gap in the class numbers still requires three anchor columns.
    MarkArray2::new(vec![MarkRecord2::new(2, anchor())])
}

fn anchors() -> Vec<Option<AnchorTable>> {
    vec![None, None, Some(anchor())]
}

macro_rules! roundtrip {
    ($table:expr, $type:ident, $format:literal) => {{
        let table = $table;
        let bytes = dump_table(&table).unwrap();
        assert_eq!(&bytes[..2], &$format.to_be_bytes());
        let read = raw::$type::read(FontData::new(&bytes)).unwrap();
        let owned: $type = read.to_owned_table();
        assert_eq!(dump_table(&owned).unwrap(), bytes);
        bytes
    }};
}

#[test]
fn all_extended_positioning_formats_roundtrip() {
    roundtrip!(
        SinglePosFormat3::new(coverage(), value()),
        SinglePosFormat3,
        3u16
    );
    roundtrip!(
        SinglePosFormat4::new(coverage(), vec![value()]),
        SinglePosFormat4,
        4u16
    );
    roundtrip!(
        PairPosFormat3::new(
            coverage(),
            vec![PairSet2::new(vec![PairValueRecord2::new(
                GlyphId24::new(65536),
                value(),
                ValueRecord::new()
            )])]
        ),
        PairPosFormat3,
        3u16
    );
    let class: ClassDef = [(GlyphId::new(65536), 2u32)].into_iter().collect();
    let matrix = vec![Class1Record::new(vec![Class2Record::new(value(), ValueRecord::new())]); 3];
    let bytes = roundtrip!(
        PairPosFormat4::new(coverage(), class, ClassDef::default(), matrix),
        PairPosFormat4,
        4u16
    );
    assert_eq!(&bytes[18..22], &[0, 3, 0, 1]);
    roundtrip!(
        CursivePosFormat2::new(
            coverage(),
            vec![EntryExitRecord2::new(Some(anchor()), None)]
        ),
        CursivePosFormat2,
        2u16
    );
    let bytes = roundtrip!(
        MarkBasePosFormat2::new(
            coverage(),
            coverage(),
            marks(),
            BaseArray2::new(vec![BaseRecord2::new(anchors())])
        ),
        MarkBasePosFormat2,
        2u16
    );
    assert_eq!(&bytes[10..12], &[0, 3]);
    let bytes = roundtrip!(
        MarkLigPosFormat2::new(
            coverage(),
            coverage(),
            marks(),
            LigatureArray2::new(vec![LigatureAttach2::new(vec![ComponentRecord2::new(
                anchors()
            )])])
        ),
        MarkLigPosFormat2,
        2u16
    );
    assert_eq!(&bytes[10..12], &[0, 3]);
    let bytes = roundtrip!(
        MarkMarkPosFormat2::new(
            coverage(),
            coverage(),
            marks(),
            Mark2Array2::new(vec![Mark2Record2::new(anchors())])
        ),
        MarkMarkPosFormat2,
        2u16
    );
    assert_eq!(&bytes[10..12], &[0, 3]);
}

#[test]
fn extended_anchor_arrays_keep_the_specified_count_widths_and_offset_bases() {
    let pair = PairSet2::new(vec![PairValueRecord2::new(
        GlyphId24::new(65536),
        ValueRecord::new().with_x_advance(-123),
        ValueRecord::new(),
    )]);
    assert_eq!(dump_table(&pair).unwrap(), [0, 0, 1, 1, 0, 0, 0xFF, 0x85]);
    let bytes = dump_table(&marks()).unwrap();
    assert_eq!(&bytes[..8], &[0, 0, 1, 0, 2, 0, 0, 8]);
    let read = raw::MarkArray2::read(FontData::new(&bytes)).unwrap();
    assert_eq!(
        read.mark_records()[0]
            .mark_anchor(read.offset_data())
            .unwrap()
            .x_coordinate(),
        123
    );
    let bytes = dump_table(&BaseArray2::new(vec![BaseRecord2::new(vec![Some(
        anchor(),
    )])]))
    .unwrap();
    assert_eq!(&bytes[..6], &[0, 0, 1, 0, 0, 6]);
    let read = raw::BaseArray2::read_with_args(FontData::new(&bytes), 1).unwrap();
    assert_eq!(
        read.base_records()
            .get(0)
            .unwrap()
            .base_anchors(read.offset_data())
            .get(0)
            .unwrap()
            .unwrap()
            .y_coordinate(),
        -456
    );
    let bytes = dump_table(&LigatureAttach2::new(vec![ComponentRecord2::new(vec![
        Some(anchor()),
    ])]))
    .unwrap();
    assert_eq!(&bytes[..5], &[0, 1, 0, 0, 5]);
    let read = raw::LigatureAttach2::read_with_args(FontData::new(&bytes), 1).unwrap();
    assert_eq!(
        read.component_records()
            .get(0)
            .unwrap()
            .ligature_anchors(read.offset_data())
            .get(0)
            .unwrap()
            .unwrap()
            .x_coordinate(),
        123
    );
    let bytes = dump_table(&Mark2Array2::new(vec![Mark2Record2::new(vec![Some(
        anchor(),
    )])]))
    .unwrap();
    // Mark2Array2 retains a uint16 count, unlike BaseArray2/MarkArray2.
    assert_eq!(&bytes[..5], &[0, 1, 0, 0, 5]);
    let read = raw::Mark2Array2::read_with_args(FontData::new(&bytes), 1).unwrap();
    assert_eq!(
        read.mark2_records()
            .get(0)
            .unwrap()
            .mark2_anchors(read.offset_data())
            .get(0)
            .unwrap()
            .unwrap()
            .x_coordinate(),
        123
    );
}

#[test]
fn full_width_coverage_and_anchor_offsets() {
    let mut bytes = vec![0; 65542];
    bytes[..9].copy_from_slice(&[0, 2, 0, 0, 0, 15, 0, 0, 1]);
    bytes[9..15].copy_from_slice(&[1, 0, 0, 0, 0, 0]);
    bytes[15..23].copy_from_slice(&[0, 3, 0, 0, 1, 1, 0, 0]);
    bytes[65536..].copy_from_slice(&[0, 1, 0, 123, 0xFE, 0x38]);
    let read = raw::CursivePosFormat2::read(FontData::new(&bytes)).unwrap();
    let record = &read.entry_exit_record()[0];
    assert_eq!(
        record
            .entry_anchor(read.offset_data())
            .unwrap()
            .unwrap()
            .y_coordinate(),
        -456
    );
    assert!(record.exit_anchor(read.offset_data()).is_none());
    // SinglePos3's coverage is beyond the 16-bit offset range.
    let mut bytes = vec![0; 65544];
    bytes[..8].copy_from_slice(&[0, 3, 0, 1, 0, 0, 0, 0]);
    bytes[65536..].copy_from_slice(&[0, 3, 0, 0, 1, 1, 0, 0]);
    let read = raw::SinglePosFormat3::read(FontData::new(&bytes)).unwrap();
    assert_eq!(read.coverage().unwrap().get(GlyphId::new(65536)), Some(0));
}

#[test]
fn extended_value_records_resolve_devices_against_the_containing_table() {
    let single = SinglePosFormat3::new(coverage(), value());
    let bytes = dump_table(&single).unwrap();
    let read = raw::SinglePosFormat3::read(FontData::new(&bytes)).unwrap();
    let raw::DeviceOrVariationIndex::VariationIndex(index) =
        read.value_record().x_placement_device().unwrap().unwrap()
    else {
        panic!("wrong device")
    };
    assert_eq!(
        (index.delta_set_outer_index(), index.delta_set_inner_index()),
        (3, 7)
    );
    let pair = PairPosFormat3::new(
        coverage(),
        vec![PairSet2::new(vec![PairValueRecord2::new(
            GlyphId24::new(65536),
            value(),
            ValueRecord::new(),
        )])],
    );
    let bytes = dump_table(&pair).unwrap();
    let read = raw::PairPosFormat3::read(FontData::new(&bytes)).unwrap();
    let record = read
        .pair_sets()
        .get(0)
        .unwrap()
        .pair_value_records()
        .get(0)
        .unwrap();
    let raw::DeviceOrVariationIndex::VariationIndex(index) = record
        .value_record1()
        .x_placement_device()
        .unwrap()
        .unwrap()
    else {
        panic!("wrong device")
    };
    assert_eq!(
        (index.delta_set_outer_index(), index.delta_set_inner_index()),
        (3, 7)
    );
}

#[test]
fn extended_positioning_validation_rejects_bad_dimensions_and_formats() {
    let class: ClassDef = [(GlyphId::new(65536), 65536u32)].into_iter().collect();
    let pair = PairPosFormat4::new(
        coverage(),
        class,
        ClassDef::default(),
        vec![Class1Record::new(vec![Class2Record::default()])],
    );
    assert!(dump_table(&pair).is_err());
    let mark = MarkBasePosFormat2::new(
        coverage(),
        coverage(),
        marks(),
        BaseArray2::new(vec![BaseRecord2::new(vec![Some(anchor())])]),
    );
    assert!(dump_table(&mark).is_err());
    let marks = MarkArray2::new(vec![MarkRecord2::new(u16::MAX, anchor())]);
    assert!(dump_table(&marks).is_err());
    let single = SinglePosFormat4::new(coverage(), vec![ValueRecord::new(), value()]);
    assert!(dump_table(&single).is_err());
    let mark2 = Mark2Array2::new(vec![Mark2Record2::default(); 65536]);
    assert!(dump_table(&mark2).is_err());
}

#[test]
fn extended_positioning_count_and_lookup_dispatch() {
    let single = SinglePosFormat4::new(
        coverage(),
        vec![ValueRecord::new().with_x_advance(1); 65537],
    );
    let bytes = dump_table(&single).unwrap();
    assert_eq!(&bytes[8..11], &[1, 0, 1]);
    let read = raw::SinglePosFormat4::read(FontData::new(&bytes)).unwrap();
    assert_eq!(
        read.value_records().get(65536).unwrap().x_advance(),
        Some(1)
    );
    let table: CursivePos = CursivePosFormat2::new(
        coverage(),
        vec![EntryExitRecord2::new(Some(anchor()), None)],
    )
    .into();
    for lookup in [
        PositionLookup::Cursive(Lookup::new(LookupFlag::empty(), vec![table.clone()])),
        PositionLookup::Extension(Lookup::new(
            LookupFlag::empty(),
            vec![ExtensionSubtable::Cursive(ExtensionPosFormat1::new(
                3, table,
            ))],
        )),
    ] {
        let bytes = dump_table(&lookup).unwrap();
        let read = raw::PositionLookup::read(FontData::new(&bytes)).unwrap();
        let raw::PositionSubtables::Cursive(tables) = read.subtables().unwrap() else {
            panic!("wrong lookup type")
        };
        assert!(matches!(
            tables.get(0).unwrap(),
            raw::CursivePos::Format2(_)
        ));
    }
}
