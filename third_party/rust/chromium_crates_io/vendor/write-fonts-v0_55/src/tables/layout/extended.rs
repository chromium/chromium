//! Regression tests for the ISO OFF extended layout formats.

use super::*;
use crate::dump_table;
use read_fonts::FontRead;

#[test]
fn wide_coverage_roundtrips_without_truncation() {
    let coverage: CoverageTable = [65535, 65536, 0xFFFFFF]
        .map(GlyphId::new)
        .into_iter()
        .collect();
    let bytes = dump_table(&coverage).unwrap();
    assert_eq!(&bytes[..5], &[0, 3, 0, 0, 3]);
    assert_eq!(bytes.len(), 14);
    let read = read_fonts::tables::layout::CoverageTable::read(FontData::new(&bytes)).unwrap();
    assert_eq!(read.get(GlyphId::new(65536)), Some(1));
    let owned: CoverageTable = read.to_owned_table();
    assert_eq!(owned, coverage);
}

#[test]
fn wide_coverage_count() {
    let coverage: CoverageTable = (0..=65536).map(|g| GlyphId::new(g * 2)).collect();
    let bytes = dump_table(&coverage).unwrap();
    assert_eq!(&bytes[..5], &[0, 3, 1, 0, 1]);
    let read = read_fonts::tables::layout::CoverageTable::read(FontData::new(&bytes)).unwrap();
    assert_eq!(read.get(GlyphId::new(131072)), Some(65536));
    assert_eq!(read.population(), 65537);
}

#[test]
fn wide_coverage_uses_ranges_when_smaller() {
    let coverage: CoverageTable = (0..=65536).chain(70000..70003).map(GlyphId::new).collect();
    let bytes = dump_table(&coverage).unwrap();
    assert_eq!(&bytes[..5], &[0, 4, 0, 0, 2]);
    let read = read_fonts::tables::layout::CoverageTable::read(FontData::new(&bytes)).unwrap();
    assert_eq!(read.get(GlyphId::new(65536)), Some(65536));
    assert_eq!(read.get(GlyphId::new(70002)), Some(65539));
    assert_eq!(read.population(), 65540);
    let owned: CoverageTable = read.to_owned_table();
    assert_eq!(owned, coverage);
}

#[test]
fn wide_class_def_roundtrips_both_class_widths() {
    for items in [
        [(65536, 7), (65537, 7)],
        [(65536, 65536), (65537, 0xFFFFFF)],
    ] {
        let class: ClassDef = items
            .into_iter()
            .map(|(g, c)| (GlyphId::new(g), c))
            .collect();
        let bytes = dump_table(&class).unwrap();
        assert_eq!(bytes.len(), if items[0].1 > 65535 { 14 } else { 13 });
        let read = read_fonts::tables::layout::ClassDef::read(FontData::new(&bytes)).unwrap();
        for (g, c) in items {
            assert_eq!(read.get(GlyphId::new(g)), c);
        }
        let owned: ClassDef = read.to_owned_table();
        assert_eq!(owned, class);
    }
}

#[test]
fn small_glyphs_still_use_legacy_formats() {
    let coverage: CoverageTable = [1, 2, 3].map(GlyphId::new).into_iter().collect();
    assert!(matches!(
        coverage,
        CoverageTable::Format1(_) | CoverageTable::Format2(_)
    ));
    let class: ClassDef = [(GlyphId::new(1), 2)].into_iter().collect();
    assert!(matches!(class, ClassDef::Format1(_) | ClassDef::Format2(_)));
}

#[test]
fn wide_context_rules_keep_narrow_counts_and_indices() {
    let gid = GlyphId24::new(65536);
    let rec = SequenceLookupRecord::new(7, 0xFFFF);
    let rule = SequenceRule2::new(vec![gid], vec![rec]);
    let bytes = dump_table(&rule).unwrap();
    assert_eq!(bytes, [0, 2, 0, 1, 1, 0, 0, 0, 7, 0xFF, 0xFF]);
    let set = SequenceRuleSet2::new(vec![rule]);
    let bytes = dump_table(&set).unwrap();
    // The ISO SequenceRuleSet2 uses Offset16, despite the widened rule.
    assert_eq!(&bytes[..4], &[0, 1, 0, 4]);
    let read = read_fonts::tables::layout::SequenceRuleSet2::read(FontData::new(&bytes)).unwrap();
    assert_eq!(
        read.seq_rules().get(0).unwrap().input_sequence()[0].get(),
        gid
    );
    let owned: SequenceRuleSet2 = read.to_owned_table();
    assert_eq!(owned, set);
}

#[test]
fn all_wide_context_formats_roundtrip() {
    let gid = GlyphId24::new(65536);
    let coverage = CoverageTable::Format3(CoverageFormat3::new(vec![gid]));
    let class = ClassDef::Format4(ClassDefFormat4::new(vec![ClassRangeRecord2::new(
        gid, gid, 1,
    )]));
    let lookup = SequenceLookupRecord::new(0, 0xFFFF);
    let rule = SequenceRule2::new(vec![gid], vec![lookup.clone()]);
    let class_rule = ClassSequenceRule::new(vec![1], vec![lookup.clone()]);
    let chained_rule =
        ChainedSequenceRule2::new(vec![gid], vec![gid], vec![gid], vec![lookup.clone()]);
    let chained_class_rule =
        ChainedClassSequenceRule::new(vec![1], vec![1], vec![1], vec![lookup.clone()]);
    let contexts = [
        SequenceContext::Format4(SequenceContextFormat4::new(
            coverage.clone(),
            vec![Some(SequenceRuleSet2::new(vec![rule]))],
        )),
        SequenceContext::Format5(SequenceContextFormat5::new(
            coverage.clone(),
            class.clone(),
            vec![Some(ClassSequenceRuleSet2::new(vec![class_rule]))],
        )),
        SequenceContext::Format6(SequenceContextFormat6::new(
            vec![coverage.clone()],
            vec![lookup],
        )),
    ];
    for context in contexts {
        let bytes = dump_table(&context).unwrap();
        let read =
            read_fonts::tables::layout::SequenceContext::read(FontData::new(&bytes)).unwrap();
        let owned: SequenceContext = read.to_owned_table();
        assert_eq!(owned, context);
    }
    let contexts = [
        ChainedSequenceContext::Format4(ChainedSequenceContextFormat4::new(
            coverage.clone(),
            vec![Some(ChainedSequenceRuleSet2::new(vec![chained_rule]))],
        )),
        ChainedSequenceContext::Format5(ChainedSequenceContextFormat5::new(
            coverage,
            class.clone(),
            class.clone(),
            class,
            vec![Some(ChainedClassSequenceRuleSet2::new(vec![
                chained_class_rule,
            ]))],
        )),
    ];
    for context in contexts {
        let bytes = dump_table(&context).unwrap();
        let read = read_fonts::tables::layout::ChainedSequenceContext::read(FontData::new(&bytes))
            .unwrap();
        let owned: ChainedSequenceContext = read.to_owned_table();
        assert_eq!(owned, context);
    }
}

#[test]
fn wide_context_coverage_offsets_are_24bit() {
    let context = SequenceContextFormat6::new(
        vec![CoverageTable::Format3(CoverageFormat3::new(vec![
            GlyphId24::new(65536),
        ]))],
        vec![SequenceLookupRecord::new(0, 0xFFFF)],
    );
    let bytes = dump_table(&context).unwrap();
    assert_eq!(&bytes[..6], &[0, 6, 0, 1, 0, 1]);
    assert_eq!(&bytes[6..9], &[0, 0, 13]);
    assert_eq!(&bytes[9..13], &[0, 0, 0xFF, 0xFF]);
}

#[test]
fn gsub_and_gpos_extended_headers_roundtrip_and_fall_back() {
    macro_rules! check {
        ($module:ident, $header:ident, $list2:ident, $lookup:ident, $single:ident) => {{
            use crate::tables::$module as owned;
            use read_fonts::tables::$module as raw;
            let mut table = owned::$header::default();
            table.script_list2 = Some(ScriptList::new(vec![ScriptRecord::new(
                Tag::new(b"grek"),
                Script::default(),
            )]))
            .into();
            table.feature_list2 = Some(FeatureList::new(vec![FeatureRecord::new(
                Tag::new(b"test"),
                Feature {
                    lookup_list_indices: vec![0xFFFF],
                    ..Default::default()
                },
            )]))
            .into();
            table.lookup_list2 = Some(owned::$list2::new(vec![owned::$lookup::Single(
                Lookup::new(LookupFlag::empty(), vec![owned::$single::default()]),
            )]))
            .into();
            let mut bytes = dump_table(&table).unwrap();
            let read = raw::$header::read(FontData::new(&bytes)).unwrap();
            assert_eq!(read.version(), MajorMinor::new(1, 2));
            assert_eq!(
                read.script_list().unwrap().script_records()[0].script_tag(),
                Tag::new(b"grek")
            );
            assert_eq!(
                read.original_script_list().unwrap().unwrap().script_count(),
                0
            );
            assert_eq!(read.feature_list().unwrap().feature_count(), 1);
            assert_eq!(
                read.original_feature_list()
                    .unwrap()
                    .unwrap()
                    .feature_count(),
                0
            );
            assert_eq!(read.lookup_list().unwrap().lookup_count(), 1);
            assert_eq!(
                read.original_lookup_list().unwrap().unwrap().lookup_count(),
                0
            );
            assert!(matches!(
                read.lookup_list().unwrap(),
                read_fonts::tables::layout::SelectedLookupList::Extended(_)
            ));
            let owned: owned::$header = read.to_owned_table();
            assert_eq!(dump_table(&owned).unwrap(), bytes);
            for (start, end) in [(14, 18), (18, 22), (22, 26)] {
                bytes[start..end].fill(0);
            }
            let read = raw::$header::read(FontData::new(&bytes)).unwrap();
            assert_eq!(read.script_list().unwrap().script_count(), 0);
            assert_eq!(read.feature_list().unwrap().feature_count(), 0);
            assert!(matches!(
                read.lookup_list().unwrap(),
                read_fonts::tables::layout::SelectedLookupList::Original(_)
            ));
            // A bad nonzero wide offset is authoritative: never fall back.
            bytes[14..18].copy_from_slice(&u32::MAX.to_be_bytes());
            let read = raw::$header::read(FontData::new(&bytes)).unwrap();
            assert!(read.script_list().is_err());
            assert!(read.original_script_list().unwrap().is_ok());
        }};
    }
    check!(
        gsub,
        Gsub,
        SubstitutionLookupList2,
        SubstitutionLookup,
        SingleSubst
    );
    check!(gpos, Gpos, PositionLookupList2, PositionLookup, SinglePos);
}

#[test]
fn layout_header_uses_full_32bit_offsets_and_rejects_truncation() {
    let mut bytes = vec![0; 65538];
    bytes[..4].copy_from_slice(&[0, 1, 0, 2]);
    bytes[4..10].copy_from_slice(&[0, 26, 0, 26, 0, 26]);
    bytes[14..18].copy_from_slice(&65536u32.to_be_bytes());
    macro_rules! check {
        ($module:ident, $header:ident) => {{
            let read = read_fonts::tables::$module::$header::read(FontData::new(&bytes)).unwrap();
            assert_eq!(read.script_list().unwrap().offset_data().len(), 2);
            assert_eq!(read.feature_list().unwrap().feature_count(), 0);
            assert_eq!(read.lookup_list().unwrap().lookup_count(), 0);
            let truncated =
                read_fonts::tables::$module::$header::read(FontData::new(&bytes[..14])).unwrap();
            assert!(truncated.script_list().is_err());
            assert!(truncated.feature_list().is_err());
            assert!(truncated.lookup_list().is_err());
        }};
    }
    check!(gsub, Gsub);
    check!(gpos, Gpos);
}
