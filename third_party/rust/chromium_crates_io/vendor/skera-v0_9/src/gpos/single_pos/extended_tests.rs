use super::*;
use font_test_data::bebuffer::BeBuffer;
use read_fonts::{tables::layout::DeviceOrVariationIndex, types::GlyphId24, FontRead};

fn input(array: bool, devices: bool, constant: bool) -> Vec<u8> {
    let count = if array { 2 } else { 1 };
    let header = if array { 11 } else { 8 };
    let device_offset = header + count * if devices { 4 } else { 2 };
    let coverage_offset = device_offset + if devices { count * 6 } else { 0 };
    let mut bytes = BeBuffer::new()
        .push(if array { 4u16 } else { 3u16 })
        .push(coverage_offset)
        .push(if devices { 0x44u16 } else { 4u16 });
    if array {
        bytes = bytes.push(Uint24::new(count));
    }
    for i in 0..count {
        bytes = bytes.push(if constant {
            100i16
        } else {
            (100 + i * 100) as i16
        });
        if devices {
            bytes = bytes.push((device_offset + i * 6) as u16);
        }
    }
    if devices {
        for i in 0..count {
            bytes = bytes
                .push((1 + i * 2) as u16)
                .push((2 + i * 2) as u16)
                .push(0x8000u16);
        }
    }
    bytes
        .push(3u16)
        .push(Uint24::new(2))
        .extend([65536, 70000].map(GlyphId24::new))
        .to_vec()
}

fn plan(mapped: [u32; 2]) -> Plan {
    let mut plan = Plan {
        glyph_map_gsub: vec![crate::INVALID_GID; 70001],
        ..Default::default()
    };
    for (old, new) in [65536, 70000].into_iter().zip(mapped) {
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
    }
    plan.layout_varidx_delta_map
        .insert(0x00010002, (0x00070008, 0));
    plan.layout_varidx_delta_map
        .insert(0x00030004, (0x0009000a, 0));
    plan
}

fn subset(bytes: &[u8], plan: &Plan) -> Result<Vec<u8>, SerializeErrorFlags> {
    let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
    let mut s = Serializer::new(2 * 1024 * 1024);
    s.start_serialize().unwrap();
    SinglePos::read(FontData::new(bytes)).unwrap().subset(
        plan,
        &mut s,
        (&SubsetState::default(), &font, &plan.gpos_lookups),
    )?;
    s.end_serialize();
    assert!(!s.in_error());
    Ok(s.copy_bytes())
}

fn varidx(record: &ValueRecord) -> u32 {
    let DeviceOrVariationIndex::VariationIndex(index) = record.x_advance_device().unwrap().unwrap()
    else {
        panic!("expected a VariationIndex")
    };
    (u32::from(index.delta_set_outer_index()) << 16) | u32::from(index.delta_set_inner_index())
}

#[test]
fn extended_single_positioning_preserves_format_values_and_variations() {
    for array in [false, true] {
        let input = input(array, true, false);
        for mapped in [[1, 2], [65536, 0xffffff]] {
            let plan = plan(mapped);
            let table = SinglePos::read(FontData::new(&input)).unwrap();
            let mut indices = IntSet::empty();
            table.collect_variation_indices(&plan, &mut indices);
            assert_eq!(
                indices.iter().collect::<Vec<_>>(),
                if array {
                    vec![0x00010002, 0x00030004]
                } else {
                    vec![0x00010002]
                }
            );
            let bytes = subset(&input, &plan).unwrap();
            if array {
                let table = SinglePosFormat4::read(FontData::new(&bytes)).unwrap();
                assert_eq!(table.value_count().to_u32(), 2);
                let records = table.value_records();
                assert_eq!(records.get(0).unwrap().x_advance(), Some(100));
                assert_eq!(records.get(1).unwrap().x_advance(), Some(200));
                assert_eq!(varidx(&records.get(0).unwrap()), 0x00070008);
                assert_eq!(varidx(&records.get(1).unwrap()), 0x0009000a);
                assert_eq!(
                    table.coverage().unwrap().get(GlyphId::new(mapped[1])),
                    Some(1)
                );
            } else {
                let table = SinglePosFormat3::read(FontData::new(&bytes)).unwrap();
                assert_eq!(table.value_record().x_advance(), Some(100));
                assert_eq!(varidx(&table.value_record()), 0x00070008);
                assert_eq!(
                    table.coverage().unwrap().get(GlyphId::new(mapped[1])),
                    Some(1)
                );
            }
        }
    }
}

#[test]
fn extended_single_positioning_collapses_constant_arrays_and_strips_hints() {
    let bytes = subset(&input(true, false, true), &plan([1, 2])).unwrap();
    let table = SinglePosFormat3::read(FontData::new(&bytes)).unwrap();
    assert_eq!(table.pos_format(), 3);
    assert_eq!(table.value_record().x_advance(), Some(100));
    let mut plan = plan([1, 2]);
    plan.subset_flags = SubsetFlags::SUBSET_FLAGS_NO_HINTING;
    for array in [false, true] {
        let bytes = subset(&input(array, true, false), &plan).unwrap();
        let table = SinglePos::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.value_format(), ValueFormat::X_ADVANCE);
    }
    plan.glyphset_gsub.remove(GlyphId::new(65536));
    plan.glyph_map_gsub[65536] = crate::INVALID_GID;
    let input = input(true, true, false);
    let mut indices = IntSet::empty();
    SinglePos::read(FontData::new(&input))
        .unwrap()
        .collect_variation_indices(&plan, &mut indices);
    assert_eq!(indices.iter().collect::<Vec<_>>(), [0x00030004]);
    let bytes = subset(&input, &plan).unwrap();
    let table = SinglePosFormat3::read(FontData::new(&bytes)).unwrap();
    assert_eq!(table.value_record().x_advance(), Some(200));
}

#[test]
fn extended_single_positioning_handles_null_malformed_and_overflow_input() {
    for array in [false, true] {
        let input = input(array, false, false);
        let mut bytes = input.clone();
        bytes[2..6].fill(0);
        assert_eq!(
            subset(&bytes, &plan([1, 2])),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
        );
        bytes[2..6].fill(0xff);
        assert_eq!(
            subset(&bytes, &plan([1, 2])),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
        let records_end = if array { 15 } else { 10 };
        assert_eq!(
            subset(&input[..records_end - 1], &plan([1, 2])),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
        assert_eq!(
            subset(&input, &plan([1, 0x1000000])),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
        );
        if array {
            bytes = input;
            bytes[8..11].copy_from_slice(&Uint24::new(1).to_be_bytes());
            assert_eq!(
                subset(&bytes, &plan([1, 2])),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
            );
        }
    }
}

#[test]
fn extended_single_positioning_keeps_counts_and_offsets_above_64k() {
    let count = 65537u32;
    let bytes = BeBuffer::new()
        .push(4u16)
        .push(11 + count * 2)
        .push(4u16)
        .push(Uint24::new(count))
        .extend((0..count).map(|i| if i == count - 1 { 1234i16 } else { i as i16 }))
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
    let output = subset(&bytes, &plan).unwrap();
    let table = SinglePosFormat4::read(FontData::new(&output)).unwrap();
    assert_eq!(table.value_count().to_u32(), count);
    assert_eq!(
        table.value_records().get(65536).unwrap().x_advance(),
        Some(1234)
    );
    assert_eq!(
        table.coverage().unwrap().get(GlyphId::new(65536)),
        Some(65536)
    );
    assert!(table.coverage_offset().to_u32() > u16::MAX as u32);
}
