use super::*;
use font_test_data::bebuffer::BeBuffer;
use read_fonts::{
    tables::layout::DeviceOrVariationIndex,
    types::{GlyphId, GlyphId24},
    FontRead,
};

fn input() -> Vec<u8> {
    BeBuffer::new()
        .push(2u16)
        .push(49u32)
        .push(Uint24::new(3))
        .extend([0, 0, 27, 0, 0, 33].map(Uint24::new))
        .push(1u16)
        .push(100i16)
        .push(200i16)
        .push(3u16)
        .push(300i16)
        .push(400i16)
        .push(10u16)
        .push(0u16)
        .push(1u16)
        .push(2u16)
        .push(0x8000u16)
        .push(3u16)
        .push(Uint24::new(3))
        .extend([65536, 70000, 0xffffff].map(GlyphId24::new))
        .to_vec()
}

fn plan(mapped: [u32; 3]) -> Plan {
    let mut plan = Plan {
        glyph_map_gsub: vec![crate::INVALID_GID; 0x1000000],
        ..Default::default()
    };
    for (old, new) in [65536, 70000, 0xffffff].into_iter().zip(mapped) {
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
    }
    plan.layout_varidx_delta_map
        .insert(0x00010002, (0x00070008, 0));
    plan
}

fn subset(bytes: &[u8], plan: &Plan) -> Result<Vec<u8>, SerializeErrorFlags> {
    let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
    let mut s = Serializer::new(2 * 1024 * 1024);
    s.start_serialize().unwrap();
    CursivePos::read(FontData::new(bytes)).unwrap().subset(
        plan,
        &mut s,
        (&SubsetState::default(), &font, &plan.gpos_lookups),
    )?;
    s.end_serialize();
    assert!(!s.in_error());
    Ok(s.copy_bytes())
}

#[test]
fn extended_cursive_subsetting_preserves_format_anchors_and_variations() {
    let input = input();
    for mapped in [[1, 2, 3], [65536, 70000, 0xffffff]] {
        let plan = plan(mapped);
        let mut indices = IntSet::empty();
        CursivePos::read(FontData::new(&input))
            .unwrap()
            .collect_variation_indices(&plan, &mut indices);
        assert_eq!(indices.iter().collect::<Vec<_>>(), [0x00010002]);
        let bytes = subset(&input, &plan).unwrap();
        let table = CursivePosFormat2::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.pos_format(), 2);
        assert_eq!(table.entry_exit_count().to_u32(), 2);
        assert_eq!(
            table.coverage().unwrap().iter().collect::<Vec<_>>(),
            [GlyphId::new(mapped[1]), GlyphId::new(mapped[2])]
        );
        let records = table.entry_exit_record();
        let entry = records[0]
            .entry_anchor(table.offset_data())
            .unwrap()
            .unwrap();
        assert_eq!(entry.x_coordinate(), 100);
        assert_eq!(entry.y_coordinate(), 200);
        assert!(records[0].exit_anchor_offset().is_null());
        assert!(records[1].entry_anchor_offset().is_null());
        let exit = records[1]
            .exit_anchor(table.offset_data())
            .unwrap()
            .unwrap();
        assert_eq!(exit.x_coordinate(), 300);
        assert_eq!(exit.y_coordinate(), 400);
        let read_fonts::tables::gpos::AnchorTable::Format3(anchor) = exit else {
            panic!("expected Anchor 3")
        };
        let DeviceOrVariationIndex::VariationIndex(index) = anchor.x_device().unwrap().unwrap()
        else {
            panic!("expected VariationIndex")
        };
        assert_eq!(index.delta_set_outer_index(), 7);
        assert_eq!(index.delta_set_inner_index(), 8);
    }
}

#[test]
fn extended_cursive_subsetting_drops_empty_tables_and_checks_offsets() {
    let input = input();
    let mut plan = plan([1, 2, 3]);
    for range in [2..6, 9..12, 12..15] {
        let mut bytes = input.clone();
        bytes[range.clone()].fill(0xff);
        assert_eq!(
            subset(&bytes, &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
    }
    let mut bytes = input.clone();
    bytes[2..6].fill(0);
    assert_eq!(
        subset(&bytes, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
    );
    assert_eq!(
        subset(&input[..26], &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    plan.glyphset_gsub = [GlyphId::new(65536)].into_iter().collect();
    let mut indices = IntSet::empty();
    CursivePos::read(FontData::new(&input))
        .unwrap()
        .collect_variation_indices(&plan, &mut indices);
    assert!(indices.is_empty());
    assert_eq!(
        subset(&input, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
    );
    plan.glyphset_gsub.insert(GlyphId::new(70000));
    plan.glyph_map_gsub[70000] = GlyphId::new(0x1000000);
    assert_eq!(
        subset(&input, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
    );
}

#[test]
fn extended_cursive_subsetting_keeps_large_counts_and_anchor_offsets() {
    let count = 65537u32;
    let anchor_offset = 9 + count * 6;
    let bytes = BeBuffer::new()
        .push(2u16)
        .push(anchor_offset + 6)
        .push(Uint24::new(count))
        .extend((0..count).flat_map(|_| [Uint24::new(anchor_offset), Uint24::new(0)]))
        .push(1u16)
        .push(100i16)
        .push(200i16)
        .push(4u16)
        .push(Uint24::new(1))
        .extend([65536, 131072, 0].map(Uint24::new))
        .to_vec();
    let mut plan = Plan {
        glyph_map_gsub: vec![crate::INVALID_GID; 131073],
        ..Default::default()
    };
    for i in 0..count {
        plan.glyphset_gsub.insert(GlyphId::new(65536 + i));
        plan.glyph_map_gsub[(65536 + i) as usize] = GlyphId::new(i);
    }
    let bytes = subset(&bytes, &plan).unwrap();
    let table = CursivePosFormat2::read(FontData::new(&bytes)).unwrap();
    assert_eq!(table.entry_exit_count().to_u32(), count);
    assert_eq!(
        table.coverage().unwrap().get(GlyphId::new(65536)),
        Some(65536)
    );
    let last = &table.entry_exit_record()[65536];
    assert!(last.entry_anchor_offset().offset().to_u32() > u16::MAX as u32);
    assert!(table.coverage_offset().to_u32() > u16::MAX as u32);
    assert_eq!(
        last.entry_anchor(table.offset_data())
            .unwrap()
            .unwrap()
            .x_coordinate(),
        100
    );
}
