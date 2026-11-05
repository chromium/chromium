use super::*;
use font_test_data::bebuffer::BeBuffer;
use read_fonts::{tables::layout::DeviceOrVariationIndex, FontRead};

fn input() -> Vec<u8> {
    let mut bytes = BeBuffer::new()
        .push(3u16)
        .push(19u32)
        .push(0x44u16)
        .push(4u16)
        .push(Uint24::new(2))
        .extend([30, 78].map(Uint24::new))
        .push(3u16)
        .push(Uint24::new(2))
        .extend([65536, 70000].map(GlyphId24::new))
        .push(Uint24::new(3));
    for (i, gid) in [70000, 70001, 0xffffff].into_iter().enumerate() {
        bytes = bytes
            .push(GlyphId24::new(gid))
            .push((100 + i * 100) as i16)
            .push((30 + i * 6) as u16)
            .push(-10i16 * (i as i16 + 1));
    }
    for i in 0..3u16 {
        bytes = bytes.push(1 + i * 2).push(2 + i * 2).push(0x8000u16);
    }
    bytes
        .push(Uint24::new(1))
        .push(GlyphId24::new(0xffffff))
        .push(400i16)
        .push(12u16)
        .push(-40i16)
        .push(7u16)
        .push(8u16)
        .push(0x8000u16)
        .to_vec()
}

pub(super) fn plan(mapped: [u32; 3]) -> Plan {
    let mut plan = Plan {
        glyph_map_gsub: vec![crate::INVALID_GID; 0x1000000],
        ..Default::default()
    };
    for (old, new) in [65536, 70000, 0xffffff].into_iter().zip(mapped) {
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
    }
    for (old, new) in [
        (0x00010002, 0x0009000a),
        (0x00050006, 0x000b000c),
        (0x00070008, 0x000d000e),
    ] {
        plan.layout_varidx_delta_map.insert(old, (new, 0));
    }
    plan
}

pub(super) fn subset(bytes: &[u8], plan: &Plan) -> Result<Vec<u8>, SerializeErrorFlags> {
    let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
    let mut s = Serializer::new(4 * 1024 * 1024);
    s.start_serialize().unwrap();
    PairPos::read(FontData::new(bytes)).unwrap().subset(
        plan,
        &mut s,
        (&SubsetState::default(), &font, &plan.gpos_lookups),
    )?;
    s.end_serialize();
    assert!(!s.in_error());
    Ok(s.copy_bytes())
}

pub(super) fn varidx(record: &ValueRecord) -> u32 {
    let DeviceOrVariationIndex::VariationIndex(index) = record.x_advance_device().unwrap().unwrap()
    else {
        panic!("expected a VariationIndex")
    };
    (u32::from(index.delta_set_outer_index()) << 16) | u32::from(index.delta_set_inner_index())
}

#[test]
fn extended_glyph_pairs_preserve_format_records_and_variations() {
    let input = input();
    for mapped in [[1, 2, 3], [65536, 70000, 0xffffff]] {
        let plan = plan(mapped);
        let mut indices = IntSet::empty();
        PairPos::read(FontData::new(&input))
            .unwrap()
            .collect_variation_indices(&plan, &mut indices);
        assert_eq!(
            indices.iter().collect::<Vec<_>>(),
            [0x00010002, 0x00050006, 0x00070008]
        );
        let bytes = subset(&input, &plan).unwrap();
        let table = PairPosFormat3::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.pos_format(), 3);
        assert_eq!(table.pair_set_count().to_u32(), 2);
        assert_eq!(
            table.coverage().unwrap().get(GlyphId::new(mapped[1])),
            Some(1)
        );
        let set = table.pair_sets().get(0).unwrap();
        assert_eq!(set.pair_value_count().to_u32(), 2);
        let records = set.pair_value_records();
        for (i, gid, advance, second, index) in [
            (0, mapped[1], 100, -10, 0x0009000a),
            (1, mapped[2], 300, -30, 0x000b000c),
        ] {
            let record = records.get(i).unwrap();
            assert_eq!(record.second_glyph().to_u32(), gid);
            assert_eq!(record.value_record1().x_advance(), Some(advance));
            assert_eq!(record.value_record2().x_advance(), Some(second));
            assert_eq!(varidx(record.value_record1()), index);
        }
        let set = table.pair_sets().get(1).unwrap();
        assert_eq!(set.pair_value_count().to_u32(), 1);
        assert_eq!(
            varidx(set.pair_value_records().get(0).unwrap().value_record1()),
            0x000d000e
        );
    }
}

#[test]
fn extended_glyph_pairs_strip_hints_and_prune_empty_sets() {
    let input = input();
    let mut plan = plan([1, 2, 3]);
    plan.subset_flags = SubsetFlags::SUBSET_FLAGS_NO_HINTING;
    let bytes = subset(&input, &plan).unwrap();
    let table = PairPosFormat3::read(FontData::new(&bytes)).unwrap();
    assert_eq!(table.value_format1(), ValueFormat::X_ADVANCE);
    assert_eq!(table.value_format2(), ValueFormat::X_ADVANCE);
    plan.glyphset_gsub.remove(GlyphId::new(0xffffff));
    plan.glyph_map_gsub[0xffffff] = crate::INVALID_GID;
    let mut indices = IntSet::empty();
    PairPos::read(FontData::new(&input))
        .unwrap()
        .collect_variation_indices(&plan, &mut indices);
    assert_eq!(indices.iter().collect::<Vec<_>>(), [0x00010002]);
    let bytes = subset(&input, &plan).unwrap();
    let table = PairPosFormat3::read(FontData::new(&bytes)).unwrap();
    assert_eq!(table.pair_set_count().to_u32(), 1);
    assert_eq!(table.coverage().unwrap().get(GlyphId::new(2)), None);
    let set = table.pair_sets().get(0).unwrap();
    assert_eq!(set.pair_value_count().to_u32(), 1);
    assert_eq!(
        set.pair_value_records()
            .get(0)
            .unwrap()
            .second_glyph()
            .to_u32(),
        2
    );
}

#[test]
fn extended_glyph_pairs_check_offsets_truncation_and_overflow() {
    let input = input();
    let plan = plan([1, 2, 3]);
    for range in [2..6, 13..16, 16..19, 30..33] {
        let mut bytes = input.clone();
        bytes[range].fill(0xff);
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
    bytes = input.clone();
    bytes[13..16].fill(0);
    let bytes = subset(&bytes, &plan).unwrap();
    let table = PairPosFormat3::read(FontData::new(&bytes)).unwrap();
    assert_eq!(table.pair_set_count().to_u32(), 1);
    assert_eq!(table.coverage().unwrap().get(GlyphId::new(1)), None);
    assert_eq!(
        subset(&input[..input.len() - 1], &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    let mut plan = plan;
    plan.glyph_map_gsub[0xffffff] = GlyphId::new(0x1000000);
    assert_eq!(
        subset(&input, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
    );
}

#[test]
fn legacy_glyph_pairs_reject_second_glyph_narrowing() {
    let bytes = BeBuffer::new()
        .push(1u16)
        .push(12u16)
        .extend([0u16; 2])
        .push(1u16)
        .push(18u16)
        .push(1u16)
        .push(1u16)
        .push(10u16)
        .push(1u16)
        .push(20u16)
        .to_vec();
    let mut plan = Plan {
        glyph_map_gsub: vec![crate::INVALID_GID; 21],
        ..Default::default()
    };
    for (old, new) in [(10, 1), (20, 65536)] {
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
    }
    assert_eq!(
        subset(&bytes, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
    );
}

#[test]
fn extended_glyph_pairs_keep_large_pair_counts_and_binary_search_indices() {
    let count = 65537u32;
    let bytes = BeBuffer::new()
        .push(3u16)
        .push(16u32)
        .push(4u16)
        .push(0u16)
        .push(Uint24::new(1))
        .push(Uint24::new(24))
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(131075))
        .push(Uint24::new(count))
        .extend((0..count).flat_map(|i| {
            let mut bytes = GlyphId24::new(i).to_be_bytes().to_vec();
            bytes.extend(if i == count - 1 { 1234i16 } else { i as i16 }.to_be_bytes());
            bytes
        }))
        .to_vec();
    for all in [false, true] {
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 131076],
            ..Default::default()
        };
        let glyphs: Vec<_> = if all {
            (0..count).collect()
        } else {
            vec![0, 65536]
        };
        for (new, old) in glyphs.iter().copied().enumerate() {
            plan.glyphset_gsub.insert(GlyphId::new(old));
            plan.glyph_map_gsub[old as usize] = GlyphId::new(new as u32);
        }
        plan.glyphset_gsub.insert(GlyphId::new(131075));
        plan.glyph_map_gsub[131075] = GlyphId::new(glyphs.len() as u32);
        let bytes = subset(&bytes, &plan).unwrap();
        let table = PairPosFormat3::read(FontData::new(&bytes)).unwrap();
        let set = table.pair_sets().get(0).unwrap();
        assert_eq!(set.pair_value_count().to_u32(), glyphs.len() as u32);
        assert_eq!(
            set.pair_value_records()
                .get(glyphs.len() - 1)
                .unwrap()
                .value_record1()
                .x_advance(),
            Some(1234)
        );
        if all {
            assert!(set.min_table_bytes().len() > u16::MAX as usize);
        }
    }
}

#[test]
fn extended_glyph_pairs_keep_large_set_counts_and_offsets() {
    let count = 65537u32;
    let coverage_offset = 13 + count * 3;
    let set_offset = coverage_offset + 14;
    let bytes = BeBuffer::new()
        .push(3u16)
        .push(coverage_offset)
        .extend([0u16; 2])
        .push(Uint24::new(count))
        .extend((0..count).map(|_| Uint24::new(set_offset)))
        .push(4u16)
        .push(Uint24::new(1))
        .extend([65536, 131072, 0].map(Uint24::new))
        .push(Uint24::new(1))
        .push(GlyphId24::new(0xffffff))
        .to_vec();
    let mut plan = plan([0, 4464, 65537]);
    for i in 0..count {
        plan.glyphset_gsub.insert(GlyphId::new(65536 + i));
        plan.glyph_map_gsub[(65536 + i) as usize] = GlyphId::new(i);
    }
    let bytes = subset(&bytes, &plan).unwrap();
    let table = PairPosFormat3::read(FontData::new(&bytes)).unwrap();
    assert_eq!(table.pair_set_count().to_u32(), count);
    assert_eq!(
        table.coverage().unwrap().get(GlyphId::new(65536)),
        Some(65536)
    );
    assert!(table.pair_set_offsets()[65536].get().to_u32() > u16::MAX as u32);
    assert!(table.coverage_offset().to_u32() > u16::MAX as u32);
    assert_eq!(
        table
            .pair_sets()
            .get(65536)
            .unwrap()
            .pair_value_records()
            .get(0)
            .unwrap()
            .second_glyph()
            .to_u32(),
        65537
    );
}
