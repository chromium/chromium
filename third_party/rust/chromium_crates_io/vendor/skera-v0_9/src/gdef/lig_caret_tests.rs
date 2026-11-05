use super::*;
use font_test_data::bebuffer::BeBuffer;
use read_fonts::{
    tables::{gdef::SelectedLigCaretList, layout::DeviceOrVariationIndex},
    types::{GlyphId, GlyphId24},
    FontData, FontRead,
};

fn input(wide: bool) -> Vec<u8> {
    let coverage = if wide { 16 } else { 10 };
    let first = coverage + 14;
    let last = first + 22;
    let mut bytes = BeBuffer::new();
    if wide {
        bytes = bytes
            .push(coverage)
            .push(Uint24::new(3))
            .extend([first, 0, last].map(Uint24::new));
    } else {
        bytes = bytes
            .push(coverage as u16)
            .push(3u16)
            .extend([first as u16, 0, last as u16]);
    }
    bytes
        .push(3u16)
        .push(Uint24::new(3))
        .extend([65536, 70000, 0xffffff].map(GlyphId24::new))
        .push(2u16)
        .push(6u16)
        .push(18u16)
        .push(3u16)
        .push(100i16)
        .push(6u16)
        .push(1u16)
        .push(2u16)
        .push(0x8000u16)
        .push(1u16)
        .push(200i16)
        .push(1u16)
        .push(4u16)
        .push(3u16)
        .push(300i16)
        .push(6u16)
        .push(3u16)
        .push(4u16)
        .push(0x8000u16)
        .to_vec()
}

fn plan() -> Plan {
    let mut plan = Plan {
        font_num_glyphs: 0x1000000,
        glyph_map_gsub: vec![crate::INVALID_GID; 0x1000000],
        ..Default::default()
    };
    for (old, new) in [(65536, 1), (70000, 2), (0xffffff, 3)] {
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
    }
    plan.layout_varidx_delta_map.insert(0x10002, (0x70008, 0));
    plan.layout_varidx_delta_map.insert(0x30004, (0x9000a, 0));
    plan
}

fn read(bytes: &[u8], wide: bool) -> SelectedLigCaretList<'_> {
    if wide {
        SelectedLigCaretList::Extended(LigCaretList2::read(FontData::new(bytes)).unwrap())
    } else {
        SelectedLigCaretList::Original(LigCaretList::read(FontData::new(bytes)).unwrap())
    }
}

fn subset(bytes: &[u8], wide: bool, plan: &Plan) -> Result<Vec<u8>, SerializeErrorFlags> {
    let mut s = Serializer::new(4 * 1024 * 1024);
    s.start_serialize().unwrap();
    read(bytes, wide).subset(plan, &mut s, ())?;
    s.end_serialize();
    assert!(!s.in_error());
    Ok(s.copy_bytes())
}

fn varidx(caret: CaretValue<'_>) -> u32 {
    let CaretValue::Format3(caret) = caret else {
        panic!("expected CaretValue3")
    };
    let DeviceOrVariationIndex::VariationIndex(index) = caret.device().unwrap() else {
        panic!("expected VariationIndex")
    };
    (u32::from(index.delta_set_outer_index()) << 16) | u32::from(index.delta_set_inner_index())
}

#[test]
fn ligature_carets_preserve_width_and_remap_glyphs_and_variations() {
    let plan = plan();
    for wide in [false, true] {
        let input = input(wide);
        let mut indices = IntSet::empty();
        read(&input, wide).collect_variation_indices(&plan, &mut indices);
        assert_eq!(indices.iter().collect::<Vec<_>>(), [0x10002, 0x30004]);
        let bytes = subset(&input, wide, &plan).unwrap();
        let table = read(&bytes, wide);
        assert_eq!(table.lig_glyph_count(), 2);
        assert_eq!(
            table.coverage().unwrap().iter().collect::<Vec<_>>(),
            [GlyphId::new(1), GlyphId::new(3)]
        );
        let first = table.lig_glyphs().get(0).unwrap();
        assert_eq!(first.caret_count(), 2);
        assert_eq!(varidx(first.caret_values().get(0).unwrap()), 0x70008);
        let CaretValue::Format1(caret) = first.caret_values().get(1).unwrap() else {
            panic!("expected CaretValue1")
        };
        assert_eq!(caret.coordinate(), 200);
        let last = table.lig_glyphs().get(1).unwrap();
        assert_eq!(last.caret_count(), 1);
        assert_eq!(varidx(last.caret_values().get(0).unwrap()), 0x9000a);
    }
}

#[test]
fn extended_ligature_carets_check_offsets_truncation_and_glyph_overflow() {
    let input = input(true);
    let mut plan = plan();
    let mut bytes = input.clone();
    bytes[..4].fill(0);
    assert_eq!(
        subset(&bytes, true, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
    );
    bytes[..4].fill(0xff);
    assert_eq!(
        subset(&bytes, true, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    assert_eq!(
        subset(&input[..15], true, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    bytes = input.clone();
    bytes[7..10].fill(0xff);
    assert_eq!(
        subset(&bytes, true, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    plan.glyph_map_gsub[65536] = GlyphId::new(0x1000000);
    assert_eq!(
        subset(&input, true, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
    );
}

#[test]
fn extended_ligature_carets_keep_24bit_counts_offsets_and_32bit_coverage_offset() {
    let count = 65537usize;
    let coverage = 7 + count * 3;
    let lig_glyph = coverage + 5 + count * 3;
    let bytes = BeBuffer::new()
        .push(coverage as u32)
        .push(Uint24::new(count as u32))
        .extend(std::iter::repeat_n(Uint24::new(lig_glyph as u32), count))
        .push(3u16)
        .push(Uint24::new(count as u32))
        .extend((262144..262144 + count as u32).map(GlyphId24::new))
        .push(1u16)
        .push(4u16)
        .push(1u16)
        .push(123i16);
    let mut plan = Plan {
        font_num_glyphs: 262144 + count,
        glyph_map_gsub: vec![crate::INVALID_GID; 262144 + count],
        ..Default::default()
    };
    for (new, old) in (262144..262144 + count as u32).enumerate() {
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(new as u32 + 1);
    }
    let result = subset(&bytes, true, &plan).unwrap();
    let table = LigCaretList2::read(FontData::new(&result)).unwrap();
    assert_eq!(table.lig_glyph_count().to_u32(), count as u32);
    assert!(table.coverage_offset().to_u32() > 65535);
    assert!(table.lig_glyph_offsets()[count - 1].get().to_u32() > 65535);
    assert_eq!(
        table.coverage().unwrap().get(GlyphId::new(count as u32)),
        Some(count as u32 - 1)
    );
    let glyph = table.lig_glyphs().get(count - 1).unwrap();
    assert_eq!(glyph.caret_count(), 1);
    let CaretValue::Format1(caret) = glyph.caret_values().get(0).unwrap() else {
        panic!("expected CaretValue1")
    };
    assert_eq!(caret.coordinate(), 123);
}
