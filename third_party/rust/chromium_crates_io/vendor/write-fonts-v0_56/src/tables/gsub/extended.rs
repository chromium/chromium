//! Tests for the ISO OFF fifth-edition substitution formats.

use super::*;
use crate::tables::layout::LookupFlag;
use crate::{dump_table, from_obj::ToOwnedTable};
use read_fonts::{tables::gsub as raw, FontData, FontRead};

fn coverage() -> CoverageTable {
    [GlyphId::new(65536)].into_iter().collect()
}

#[test]
fn all_extended_substitutions_roundtrip() {
    let gid = GlyphId24::new(0x12_3456);
    macro_rules! check {
        ($table:expr, $type:ident, $format:literal) => {{
            let table = $table;
            let bytes = dump_table(&table).unwrap();
            assert_eq!(&bytes[..2], &$format.to_be_bytes());
            let read = raw::$type::read(FontData::new(&bytes)).unwrap();
            assert_eq!(read.coverage().unwrap().get(GlyphId::new(65536)), Some(0));
            let owned: $type = read.to_owned_table();
            assert_eq!(dump_table(&owned).unwrap(), bytes);
        }};
    }
    check!(
        SingleSubstFormat3::new(coverage(), Int24::new(-65537)),
        SingleSubstFormat3,
        3u16
    );
    check!(
        SingleSubstFormat4::new(coverage(), vec![gid]),
        SingleSubstFormat4,
        4u16
    );
    check!(
        MultipleSubstFormat2::new(coverage(), vec![Sequence2::new(vec![gid])]),
        MultipleSubstFormat2,
        2u16
    );
    check!(
        AlternateSubstFormat2::new(coverage(), vec![AlternateSet2::new(vec![gid])]),
        AlternateSubstFormat2,
        2u16
    );
    check!(
        LigatureSubstFormat2::new(
            coverage(),
            vec![LigatureSet2::new(vec![Ligature2::new(gid, vec![gid])])]
        ),
        LigatureSubstFormat2,
        2u16
    );
    check!(
        ReverseChainSingleSubstFormat2::new(
            coverage(),
            vec![coverage()],
            vec![coverage()],
            vec![gid]
        ),
        ReverseChainSingleSubstFormat2,
        2u16
    );
}

#[test]
fn extended_substitution_field_widths() {
    let gid = GlyphId24::new(0x12_3456);
    let single = SingleSubstFormat3::new(coverage(), Int24::new(-65537));
    let bytes = dump_table(&single).unwrap();
    assert_eq!(&bytes[..9], &[0, 3, 0, 0, 0, 9, 0xFE, 0xFF, 0xFF]);
    assert_eq!(
        raw::SingleSubstFormat3::read(FontData::new(&bytes))
            .unwrap()
            .delta_glyph_id()
            .to_i32(),
        -65537
    );
    assert_eq!(
        dump_table(&Sequence2::new(vec![gid])).unwrap(),
        [0, 1, 0x12, 0x34, 0x56]
    );
    assert_eq!(
        dump_table(&AlternateSet2::new(vec![gid])).unwrap(),
        [0, 1, 0x12, 0x34, 0x56]
    );
    let lig = Ligature2::new(gid, vec![GlyphId24::new(65536)]);
    assert_eq!(dump_table(&lig).unwrap(), [0x12, 0x34, 0x56, 0, 2, 1, 0, 0]);
    let set = LigatureSet2::new(vec![lig]);
    let bytes = dump_table(&set).unwrap();
    assert_eq!(&bytes[..5], &[0, 1, 0, 0, 5]);
    let reverse = ReverseChainSingleSubstFormat2::new(
        coverage(),
        vec![coverage()],
        vec![coverage()],
        vec![gid],
    );
    let bytes = dump_table(&reverse).unwrap();
    assert_eq!(&bytes[6..8], &[0, 1]);
    assert_eq!(&bytes[11..13], &[0, 1]);
    assert_eq!(&bytes[16..22], &[0, 0, 1, 0x12, 0x34, 0x56]);
}

#[test]
fn extended_substitution_count_exceeds_16bits() {
    let single = SingleSubstFormat4::new(coverage(), vec![GlyphId24::new(65536); 65537]);
    let bytes = dump_table(&single).unwrap();
    assert_eq!(&bytes[6..9], &[1, 0, 1]);
    let read = raw::SingleSubstFormat4::read(FontData::new(&bytes)).unwrap();
    assert_eq!(read.glyph_count().to_u32(), 65537);
    assert_eq!(read.substitute_glyph_ids()[65536].get().to_u32(), 65536);
}

#[test]
fn extended_lookup_dispatch_and_legacy_conversion() {
    let table: MultipleSubst = MultipleSubstFormat2::new(
        coverage(),
        vec![Sequence2::new(vec![GlyphId24::new(65536)])],
    )
    .into();
    for lookup in [
        SubstitutionLookup::Multiple(Lookup::new(LookupFlag::empty(), vec![table.clone()])),
        SubstitutionLookup::Extension(Lookup::new(
            LookupFlag::empty(),
            vec![ExtensionSubtable::Multiple(ExtensionSubstFormat1::new(
                2, table,
            ))],
        )),
    ] {
        let bytes = dump_table(&lookup).unwrap();
        let read = raw::SubstitutionLookup::read(FontData::new(&bytes)).unwrap();
        let raw::SubstitutionSubtables::Multiple(subtables) = read.subtables().unwrap() else {
            panic!("wrong lookup type")
        };
        let raw::MultipleSubst::Format2(table) = subtables.iter().next().unwrap().unwrap() else {
            panic!("wrong subtable format")
        };
        assert_eq!(
            table.sequences().get(0).unwrap().substitute_glyph_ids()[0]
                .get()
                .to_u32(),
            65536
        );
    }
    let legacy: SubstitutionLookup =
        Lookup::new(LookupFlag::empty(), vec![MultipleSubstFormat1::default()]).into();
    let SubstitutionLookup::Multiple(lookup) = legacy else {
        panic!("wrong lookup type")
    };
    assert!(matches!(*lookup.subtables[0], MultipleSubst::Format1(_)));
}
