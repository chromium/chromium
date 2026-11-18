//! Tests for the ISO OFF fifth-edition GDEF header and caret list.

use super::*;
use crate::{dump_table, from_obj::ToOwnedTable};
use read_fonts::{tables::gdef as raw, FontData, FontRead};

fn coverage(gid: u32) -> CoverageTable {
    [GlyphId::new(gid)].into_iter().collect()
}

#[test]
fn gdef_extended_header_precedence_fallback_and_roundtrip() {
    let table = Gdef {
        glyph_class_def: Some([(GlyphId::new(1), 1u32)].into_iter().collect()).into(),
        attach_list: Some(AttachList::new(
            coverage(1),
            vec![AttachPoint::new(vec![1])],
        ))
        .into(),
        lig_caret_list: Some(LigCaretList::new(
            coverage(1),
            vec![LigGlyph::new(vec![CaretValueFormat1::new(100).into()])],
        ))
        .into(),
        mark_attach_class_def: Some([(GlyphId::new(1), 1u32)].into_iter().collect()).into(),
        mark_glyph_sets_def: Some(MarkGlyphSets::default()).into(),
        glyph_class_def2: Some([(GlyphId::new(65536), 4u32)].into_iter().collect()).into(),
        attach_list2: Some(AttachList::new(
            coverage(65536),
            vec![AttachPoint::new(vec![65535])],
        ))
        .into(),
        lig_caret_list2: Some(LigCaretList2::new(
            coverage(65536),
            vec![LigGlyph::new(vec![CaretValueFormat3::new(
                123,
                VariationIndex::new(3, 7).into(),
            )
            .into()])],
        ))
        .into(),
        mark_attach_class_def2: Some([(GlyphId::new(65536), 7u32)].into_iter().collect()).into(),
        mark_glyph_sets_def2: Some(MarkGlyphSets::new(vec![coverage(65536)])).into(),
        ..Default::default()
    };
    let mut bytes = dump_table(&table).unwrap();
    let read = raw::Gdef::read(FontData::new(&bytes)).unwrap();
    assert_eq!(read.version(), MajorMinor::new(1, 4));
    assert_eq!(
        read.glyph_class_def()
            .unwrap()
            .unwrap()
            .get(GlyphId::new(65536)),
        4
    );
    assert_eq!(
        read.mark_attach_class_def()
            .unwrap()
            .unwrap()
            .get(GlyphId::new(65536)),
        7
    );
    assert_eq!(
        read.attach_list()
            .unwrap()
            .unwrap()
            .attach_points()
            .get(0)
            .unwrap()
            .point_indices()[0]
            .get(),
        65535
    );
    let carets = read.lig_caret_list().unwrap().unwrap();
    assert!(matches!(carets, raw::SelectedLigCaretList::Extended(_)));
    assert_eq!(carets.coverage().unwrap().get(GlyphId::new(65536)), Some(0));
    assert_eq!(
        read.mark_glyph_sets_def()
            .unwrap()
            .unwrap()
            .mark_glyph_set_count(),
        1
    );
    let owned: Gdef = read.to_owned_table();
    assert_eq!(dump_table(&owned).unwrap(), bytes);
    // The raw legacy fields must stay independent of the selected ones.
    assert_eq!(
        read.original_glyph_class_def()
            .unwrap()
            .unwrap()
            .get(GlyphId::new(1)),
        1
    );
    assert_eq!(
        read.original_attach_list()
            .unwrap()
            .unwrap()
            .attach_points()
            .get(0)
            .unwrap()
            .point_indices()[0]
            .get(),
        1
    );
    assert_eq!(
        read.original_mark_glyph_sets_def()
            .unwrap()
            .unwrap()
            .mark_glyph_set_count(),
        0
    );
    bytes[18..38].fill(0);
    let read = raw::Gdef::read(FontData::new(&bytes)).unwrap();
    assert_eq!(
        read.glyph_class_def()
            .unwrap()
            .unwrap()
            .get(GlyphId::new(1)),
        1
    );
    assert_eq!(
        read.mark_attach_class_def()
            .unwrap()
            .unwrap()
            .get(GlyphId::new(1)),
        1
    );
    assert_eq!(
        read.attach_list()
            .unwrap()
            .unwrap()
            .attach_points()
            .get(0)
            .unwrap()
            .point_indices()[0]
            .get(),
        1
    );
    assert!(matches!(
        read.lig_caret_list().unwrap().unwrap(),
        raw::SelectedLigCaretList::Original(_)
    ));
    assert_eq!(
        read.mark_glyph_sets_def()
            .unwrap()
            .unwrap()
            .mark_glyph_set_count(),
        0
    );
    // A nonzero but invalid wide offset is not a request for fallback.
    bytes[18..22].copy_from_slice(&u32::MAX.to_be_bytes());
    let read = raw::Gdef::read(FontData::new(&bytes)).unwrap();
    assert!(read.glyph_class_def().unwrap().is_err());
    assert!(read.original_glyph_class_def().unwrap().is_ok());
}

#[test]
fn gdef_null_and_truncated_extended_offsets() {
    let mut bytes = vec![0; 38];
    bytes[..4].copy_from_slice(&[0, 1, 0, 4]);
    let read = raw::Gdef::read(FontData::new(&bytes)).unwrap();
    assert!(read.glyph_class_def().is_none());
    assert!(read.attach_list().is_none());
    assert!(read.lig_caret_list().is_none());
    assert!(read.mark_attach_class_def().is_none());
    assert!(read.mark_glyph_sets_def().is_none());
    let read = raw::Gdef::read(FontData::new(&bytes[..18])).unwrap();
    assert!(read.glyph_class_def().unwrap().is_err());
    assert!(read.attach_list().unwrap().is_err());
    assert!(read.lig_caret_list().unwrap().is_err());
    assert!(read.mark_attach_class_def().unwrap().is_err());
    assert!(read.mark_glyph_sets_def().unwrap().is_err());
    bytes.resize(65544, 0);
    bytes[18..22].copy_from_slice(&65536u32.to_be_bytes());
    bytes[65536..].copy_from_slice(&[0, 3, 1, 0, 0, 0, 0, 0]);
    let read = raw::Gdef::read(FontData::new(&bytes)).unwrap();
    assert_eq!(
        read.glyph_class_def().unwrap().unwrap().offset_data().len(),
        8
    );
}

#[test]
fn caret_list_widens_only_the_outer_offsets_and_count() {
    let list = LigCaretList2::new(
        coverage(65536),
        vec![LigGlyph::new(vec![CaretValueFormat1::new(123).into()])],
    );
    let bytes = dump_table(&list).unwrap();
    assert_eq!(&bytes[4..7], &[0, 0, 1]);
    let read = raw::LigCaretList2::read(FontData::new(&bytes)).unwrap();
    let glyph = read.lig_glyphs().get(0).unwrap();
    // LigGlyph is unchanged: uint16 count and Offset16 caret offsets.
    assert_eq!(&glyph.offset_data().as_bytes()[..4], &[0, 1, 0, 4]);
    let raw::CaretValue::Format1(caret) = glyph.caret_values().get(0).unwrap() else {
        panic!("wrong caret type")
    };
    assert_eq!(caret.coordinate(), 123);
    let attach = AttachList::new(coverage(65536), vec![AttachPoint::new(vec![65535])]);
    let bytes = dump_table(&attach).unwrap();
    assert_eq!(&bytes[2..4], &[0, 1]);
    let read = raw::AttachList::read(FontData::new(&bytes)).unwrap();
    assert_eq!(read.attach_point_offsets().len(), 1);
    assert_eq!(read.coverage().unwrap().get(GlyphId::new(65536)), Some(0));
    let list = LigCaretList2::new(coverage(65536), vec![LigGlyph::default(); 65537]);
    let bytes = dump_table(&list).unwrap();
    assert_eq!(&bytes[4..7], &[1, 0, 1]);
    let read = raw::LigCaretList2::read(FontData::new(&bytes)).unwrap();
    assert_eq!(read.lig_glyphs().get(65536).unwrap().caret_count(), 0);
}
