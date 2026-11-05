use super::*;
use font_test_data::bebuffer::BeBuffer;
use read_fonts::types::GlyphId24;
use read_fonts::{
    tables::{gpos::AnchorTable, layout::DeviceOrVariationIndex},
    FontRead,
};

fn anchor(index: u16) -> Vec<u8> {
    BeBuffer::new()
        .push(3u16)
        .push(index as i16)
        .push(200i16)
        .push(10u16)
        .push(0u16)
        .push(index)
        .push(index + 1)
        .push(0x8000u16)
        .to_vec()
}

fn input() -> Vec<u8> {
    let mut bytes = BeBuffer::new()
        .push(2u16)
        .push(20u32)
        .push(31u32)
        .push(3u16)
        .push(42u32)
        .push(87u32)
        .push(3u16)
        .push(Uint24::new(2))
        .extend([65536, 65537].map(GlyphId24::new))
        .push(3u16)
        .push(Uint24::new(2))
        .extend([70000, 70001].map(GlyphId24::new))
        .push(Uint24::new(2))
        .push(0u16)
        .push(Uint24::new(13))
        .push(2u16)
        .push(Uint24::new(29))
        .to_vec();
    bytes.extend(anchor(1));
    bytes.extend(anchor(3));
    bytes.extend(
        BeBuffer::new()
            .push(Uint24::new(2))
            .extend([9, 86].map(Uint24::new))
            .push(3u16)
            .extend([29, 0, 45, 0, 0, 0, 0, 61, 0].map(Uint24::new))
            .to_vec(),
    );
    for i in [5, 7, 9] {
        bytes.extend(anchor(i));
    }
    bytes.extend(
        BeBuffer::new()
            .push(1u16)
            .extend([0, 11, 0].map(Uint24::new))
            .to_vec(),
    );
    bytes.extend(anchor(11));
    bytes
}

fn plan(mapped: [u32; 4]) -> Plan {
    let mut plan = Plan {
        glyph_map_gsub: vec![crate::INVALID_GID; 70002],
        ..Default::default()
    };
    for (old, new) in [65536, 65537, 70000, 70001].into_iter().zip(mapped) {
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
    }
    for i in [1u32, 3, 5, 7, 9, 11] {
        plan.layout_varidx_delta_map
            .insert((i << 16) | (i + 1), (((i + 8) << 16) | (i + 9), 0));
    }
    plan
}

fn subset(bytes: &[u8], plan: &Plan) -> Result<Vec<u8>, SerializeErrorFlags> {
    let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
    let mut s = Serializer::new(4 * 1024 * 1024);
    s.start_serialize().unwrap();
    MarkLigPos::read(FontData::new(bytes)).unwrap().subset(
        plan,
        &mut s,
        (&SubsetState::default(), &font, &plan.gpos_lookups),
    )?;
    s.end_serialize();
    assert!(!s.in_error());
    Ok(s.copy_bytes())
}

fn varidx(anchor: AnchorTable<'_>) -> u32 {
    let AnchorTable::Format3(anchor) = anchor else {
        panic!("expected Anchor3")
    };
    let DeviceOrVariationIndex::VariationIndex(index) = anchor.x_device().unwrap().unwrap() else {
        panic!("expected VariationIndex")
    };
    (u32::from(index.delta_set_outer_index()) << 16) | u32::from(index.delta_set_inner_index())
}

#[test]
fn extended_mark_ligature_remaps_classes_anchors_and_preserves_components() {
    let input = input();
    for mapped in [[1, 2, 3, 4], [65536, 65537, 70000, 70001]] {
        let plan = plan(mapped);
        let mut indices = IntSet::empty();
        MarkLigPos::read(FontData::new(&input))
            .unwrap()
            .collect_variation_indices(&plan, &mut indices);
        assert_eq!(
            indices.iter().collect::<Vec<_>>(),
            [0x10002, 0x30004, 0x50006, 0x70008]
        );
        let bytes = subset(&input, &plan).unwrap();
        let table = MarkLigPosFormat2::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.pos_format(), 2);
        assert_eq!(table.mark_class_count(), 2);
        assert_eq!(
            table.mark_coverage().unwrap().iter().collect::<Vec<_>>(),
            mapped[..2]
                .iter()
                .copied()
                .map(GlyphId::new)
                .collect::<Vec<_>>()
        );
        assert_eq!(
            table
                .ligature_coverage()
                .unwrap()
                .iter()
                .collect::<Vec<_>>(),
            [GlyphId::new(mapped[2])]
        );
        let marks = table.mark_array().unwrap();
        assert_eq!(marks.mark_count().to_u32(), 2);
        for (i, record) in marks.mark_records().iter().enumerate() {
            assert_eq!(record.mark_class(), i as u16);
            assert_eq!(
                varidx(record.mark_anchor(marks.offset_data()).unwrap()),
                [(9 << 16) | 10, (11 << 16) | 12][i]
            );
        }
        let ligatures = table.ligature_array().unwrap();
        assert_eq!(ligatures.ligature_count().to_u32(), 1);
        let attach = ligatures.ligature_attaches().get(0).unwrap();
        assert_eq!(attach.component_count(), 3);
        for (i, component) in attach.component_records().iter().enumerate() {
            let component = component.unwrap();
            let anchors = component.ligature_anchors(attach.offset_data());
            assert_eq!(anchors.len(), 2);
            if i == 0 {
                assert_eq!(varidx(anchors.get(0).unwrap().unwrap()), (13 << 16) | 14);
                assert_eq!(varidx(anchors.get(1).unwrap().unwrap()), (15 << 16) | 16);
            } else {
                assert!(component
                    .ligature_anchor_offsets()
                    .iter()
                    .all(|o| o.get().is_null()));
            }
        }
    }
}

#[test]
fn extended_mark_ligature_checks_null_sets_invalid_offsets_and_classes() {
    let input = input();
    let mut plan = plan([1, 2, 3, 4]);
    for range in [2..6, 6..10, 12..16, 16..20] {
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
    let mut bytes = input.clone();
    bytes[90..93].fill(0);
    assert_eq!(
        subset(&bytes, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
    );
    bytes[90..93].fill(0xff);
    assert_eq!(
        subset(&bytes, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    bytes = input.clone();
    bytes[50..52].copy_from_slice(&3u16.to_be_bytes());
    assert_eq!(
        subset(&bytes, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    assert_eq!(
        subset(&input[..124], &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    plan.glyph_map_gsub[65536] = GlyphId::new(0x1000000);
    assert_eq!(
        subset(&input, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
    );
}

#[test]
fn extended_mark_ligature_keeps_large_ligature_counts_and_attach_offsets() {
    let count = 65537usize;
    let lig_array = 33 + count * 3;
    let attach_offset = 3 + count * 3;
    let mark_array = lig_array + attach_offset + 11;
    let bytes = BeBuffer::new()
        .push(2u16)
        .push(20u32)
        .push(28u32)
        .push(1u16)
        .push(mark_array as u32)
        .push(lig_array as u32)
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(131072))
        .push(3u16)
        .push(Uint24::new(count as u32))
        .extend((262144..262144 + count as u32).map(GlyphId24::new))
        .push(Uint24::new(count as u32))
        .extend(std::iter::repeat_n(
            Uint24::new(attach_offset as u32),
            count,
        ))
        .push(1u16)
        .push(Uint24::new(5))
        .push(1u16)
        .push(123i16)
        .push(456i16)
        .push(Uint24::new(1))
        .push(0u16)
        .push(Uint24::new(8))
        .push(1u16)
        .push(789i16)
        .push(123i16);
    let mut plan = Plan {
        glyph_map_gsub: vec![crate::INVALID_GID; 262144 + count],
        ..Default::default()
    };
    for (new, old) in std::iter::once(131072)
        .chain(262144..262144 + count as u32)
        .enumerate()
    {
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(new as u32 + 1);
    }
    let result = subset(&bytes, &plan).unwrap();
    let table = MarkLigPosFormat2::read(FontData::new(&result)).unwrap();
    let array = table.ligature_array().unwrap();
    assert_eq!(array.ligature_count().to_u32(), count as u32);
    assert!(array.ligature_attach_offsets()[count - 1].get().to_u32() > 65535);
    assert_eq!(
        table
            .ligature_coverage()
            .unwrap()
            .get(GlyphId::new(count as u32 + 1)),
        Some(count as u32 - 1)
    );
    let attach = array.ligature_attaches().get(count - 1).unwrap();
    assert_eq!(attach.component_count(), 1);
    assert_eq!(
        attach
            .component_records()
            .get(0)
            .unwrap()
            .ligature_anchors(attach.offset_data())
            .get(0)
            .unwrap()
            .unwrap()
            .x_coordinate(),
        123
    );
}

#[test]
fn extended_mark_ligature_keeps_16bit_component_count_and_large_anchor_offsets() {
    let count = 65535usize;
    let bytes = BeBuffer::new()
        .push(2u16)
        .push(20u32)
        .push(28u32)
        .push(1u16)
        .push(36u32)
        .push(50u32)
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(65536))
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(70000))
        .push(Uint24::new(1))
        .push(0u16)
        .push(Uint24::new(8))
        .push(1u16)
        .push(789i16)
        .push(123i16)
        .push(Uint24::new(1))
        .push(Uint24::new(6))
        .push(count as u16)
        .extend(std::iter::repeat_n(
            Uint24::new((2 + count * 3) as u32),
            count,
        ))
        .push(1u16)
        .push(123i16)
        .push(456i16);
    let mut plan = plan([1, 2, 3, 4]);
    plan.glyphset_gsub.remove(GlyphId::new(65537));
    plan.glyphset_gsub.remove(GlyphId::new(70001));
    let result = subset(&bytes, &plan).unwrap();
    let table = MarkLigPosFormat2::read(FontData::new(&result)).unwrap();
    let attach = table
        .ligature_array()
        .unwrap()
        .ligature_attaches()
        .get(0)
        .unwrap();
    assert_eq!(attach.component_count(), 65535);
    let component = attach.component_records().get(count - 1).unwrap();
    assert!(
        component.ligature_anchor_offsets()[0]
            .get()
            .offset()
            .to_u32()
            > 65535
    );
    assert_eq!(
        component
            .ligature_anchors(attach.offset_data())
            .get(0)
            .unwrap()
            .unwrap()
            .x_coordinate(),
        123
    );
}
