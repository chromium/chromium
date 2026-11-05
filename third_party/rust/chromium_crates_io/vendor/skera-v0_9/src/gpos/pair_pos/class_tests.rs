use super::extended_tests::{plan, subset, varidx};
use super::*;
use font_test_data::bebuffer::BeBuffer;
use read_fonts::FontRead;

fn input() -> Vec<u8> {
    let mut bytes = BeBuffer::new()
        .push(4u16)
        .push(112u32)
        .push(0x44u16)
        .push(4u16)
        .push(120u32)
        .push(131u32)
        .push(3u16)
        .push(4u16);
    for row in 0..3u16 {
        for col in 0..4u16 {
            let index = row * 4 + col;
            bytes = bytes
                .push((100 * index) as i16)
                .push(94 + row * 6)
                .push(-10i16 * index as i16);
        }
    }
    for row in 0..3u16 {
        bytes = bytes.push(1 + row * 2).push(2 + row * 2).push(0x8000u16);
    }
    bytes
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(65536))
        .push(3u16)
        .push(GlyphId24::new(65536))
        .push(Uint24::new(1))
        .push(Uint24::new(2))
        .push(4u16)
        .push(Uint24::new(2))
        .push(GlyphId24::new(70000))
        .push(GlyphId24::new(70000))
        .push(1u16)
        .push(GlyphId24::new(0xffffff))
        .push(GlyphId24::new(0xffffff))
        .push(3u16)
        .to_vec()
}

#[test]
fn extended_class_pairs_preserve_format_matrix_and_variations() {
    let input = input();
    for mapped in [[1, 2, 3], [65536, 70000, 0xffffff]] {
        let plan = plan(mapped);
        let mut indices = IntSet::empty();
        PairPos::read(FontData::new(&input))
            .unwrap()
            .collect_variation_indices(&plan, &mut indices);
        assert_eq!(indices.iter().collect::<Vec<_>>(), [0x00050006]);
        let bytes = subset(&input, &plan).unwrap();
        let table = PairPosFormat4::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.pos_format(), 4);
        assert_eq!(table.class1_count(), 1);
        assert_eq!(table.class2_count(), 3);
        assert_eq!(table.class_def1().unwrap().get(GlyphId::new(mapped[0])), 0);
        assert_eq!(table.class_def2().unwrap().get(GlyphId::new(mapped[1])), 1);
        assert_eq!(table.class_def2().unwrap().get(GlyphId::new(mapped[2])), 2);
        assert_eq!(
            table.coverage().unwrap().get(GlyphId::new(mapped[0])),
            Some(0)
        );
        let base = table.class2_count_byte_range().end;
        for (col, advance) in [800, 900, 1100].into_iter().enumerate() {
            let first =
                ValueRecord::new(table.offset_data(), base + col * 6, table.value_format1());
            let second = ValueRecord::new(
                table.offset_data(),
                base + col * 6 + 4,
                table.value_format2(),
            );
            assert_eq!(first.x_advance(), Some(advance));
            assert_eq!(second.x_advance(), Some(-advance / 10));
            assert_eq!(varidx(&first), 0x000b000c);
        }
    }
}

#[test]
fn extended_class_pairs_strip_hints_and_prune_classes() {
    let input = input();
    let mut plan = plan([1, 2, 3]);
    plan.subset_flags = SubsetFlags::SUBSET_FLAGS_NO_HINTING;
    plan.glyphset_gsub.remove(GlyphId::new(0xffffff));
    plan.glyph_map_gsub[0xffffff] = crate::INVALID_GID;
    let bytes = subset(&input, &plan).unwrap();
    let table = PairPosFormat4::read(FontData::new(&bytes)).unwrap();
    assert_eq!(table.class1_count(), 1);
    assert_eq!(table.class2_count(), 2);
    assert_eq!(table.value_format1(), ValueFormat::X_ADVANCE);
    assert_eq!(table.value_format2(), ValueFormat::X_ADVANCE);
    let first = ValueRecord::new(
        table.offset_data(),
        table.class2_count_byte_range().end + 4,
        table.value_format1(),
    );
    assert_eq!(first.x_advance(), Some(900));
}

#[test]
fn extended_class_pairs_check_offsets_dimensions_and_overflow() {
    let input = input();
    let mut plan = plan([1, 2, 3]);
    for range in [2..6, 10..14, 14..18] {
        let mut bytes = input.clone();
        bytes[range.clone()].fill(0);
        assert_eq!(
            subset(&bytes, &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
        );
        bytes[range].fill(0xff);
        assert_eq!(
            subset(&bytes, &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
    }
    assert_eq!(
        subset(&input[..93], &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    let mut bytes = input.clone();
    bytes[128..131].copy_from_slice(&Uint24::new(65536).to_be_bytes());
    assert_eq!(
        subset(&bytes, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    bytes = input.clone();
    bytes[20..22].copy_from_slice(&3u16.to_be_bytes());
    assert_eq!(
        subset(&bytes, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    plan.glyph_map_gsub[70000] = GlyphId::new(0x1000000);
    assert_eq!(
        subset(&input, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
    );
}

#[test]
fn extended_class_pairs_keep_16bit_class_counts_and_large_offsets() {
    let count = u16::MAX;
    let coverage_offset = 22 + u32::from(count) * 4;
    let bytes = BeBuffer::new()
        .push(4u16)
        .push(coverage_offset)
        .push(4u16)
        .push(0u16)
        .push(coverage_offset + 8)
        .push(coverage_offset + 19)
        .push(count)
        .push(2u16)
        .extend((0..u32::from(count) * 2).map(|i| {
            if i == u32::from(count) * 2 - 1 {
                1234i16
            } else if i == u32::from(count) * 2 - 2 {
                1233i16
            } else {
                0i16
            }
        }))
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(131072))
        .push(3u16)
        .push(GlyphId24::new(131072))
        .push(Uint24::new(1))
        .push(Uint24::new(u32::from(count) - 1))
        .push(3u16)
        .push(GlyphId24::new(70000))
        .push(Uint24::new(1))
        .push(Uint24::new(1))
        .to_vec();
    let mut plan = Plan {
        glyph_map_gsub: vec![crate::INVALID_GID; 131073],
        ..Default::default()
    };
    for (old, new) in [(70000, 1), (131072, 2)] {
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
    }
    let output = subset(&bytes, &plan).unwrap();
    let table = PairPosFormat4::read(FontData::new(&output)).unwrap();
    assert_eq!(table.class1_count(), 1);
    assert_eq!(table.class2_count(), 2);
    let base = table.class2_count_byte_range().end;
    assert_eq!(
        ValueRecord::new(table.offset_data(), base, table.value_format1()).x_advance(),
        Some(1233)
    );
    assert_eq!(
        ValueRecord::new(table.offset_data(), base + 2, table.value_format1()).x_advance(),
        Some(1234)
    );
}
