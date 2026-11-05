use super::*;
use font_test_data::bebuffer::BeBuffer;
use read_fonts::tables::{
    gpos::{PositionLookup, PositionSubtables},
    layout::SelectedLookupList,
};

fn lookup(gid: u32, advance: i16) -> Vec<u8> {
    BeBuffer::new()
        .push(1u16)
        .push(0u16)
        .push(1u16)
        .push(8u16)
        .push(3u16)
        .push(10u32)
        .push(4u16)
        .push(advance)
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(gid))
        .to_vec()
}

fn input(wide: bool) -> Vec<u8> {
    let mut bytes = BeBuffer::new().push(3u16);
    if wide {
        bytes = bytes.extend([70000, 0, 70026]);
    } else {
        bytes = bytes.extend([14u16, 0, 40]);
    }
    let mut bytes = bytes.to_vec();
    bytes.resize(if wide { 70000 } else { 14 }, 0);
    bytes.extend(lookup(65536, 40));
    bytes.extend(lookup(70000, 50));
    bytes
}

fn plan() -> Plan {
    let mut plan = Plan {
        glyph_map_gsub: vec![INVALID_GID; 70001],
        ..Default::default()
    };
    for (old, new) in [(65536, 1), (70000, 2)] {
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
    }
    plan.gpos_lookups.insert(0, 0);
    plan.gpos_lookups.insert(2, 1);
    plan
}

fn subset(bytes: &[u8], wide: bool, plan: &Plan) -> Result<Vec<u8>, SerializeErrorFlags> {
    let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
    let table = if wide {
        SelectedLookupList::Extended(
            LookupList2::<PositionLookup>::read(FontData::new(bytes)).unwrap(),
        )
    } else {
        SelectedLookupList::Original(
            LookupList::<PositionLookup>::read(FontData::new(bytes)).unwrap(),
        )
    };
    let mut s = Serializer::new(4 * 1024 * 1024);
    s.start_serialize().unwrap();
    table.subset(
        plan,
        &mut s,
        (&SubsetState::default(), &font, &plan.gpos_lookups),
    )?;
    s.end_serialize();
    assert!(!s.in_error());
    Ok(s.copy_bytes())
}

#[test]
fn lookup_lists_preserve_width_order_and_prune_unselected_null_offsets() {
    let plan = plan();
    for wide in [false, true] {
        let bytes = subset(&input(wide), wide, &plan).unwrap();
        let list = if wide {
            SelectedLookupList::Extended(
                LookupList2::<PositionLookup>::read(FontData::new(&bytes)).unwrap(),
            )
        } else {
            SelectedLookupList::Original(
                LookupList::<PositionLookup>::read(FontData::new(&bytes)).unwrap(),
            )
        };
        assert_eq!(list.lookup_count(), 2);
        for (i, advance) in [40, 50].into_iter().enumerate() {
            let lookup = list.lookups().get(i).unwrap();
            let PositionSubtables::Single(subtables) = lookup.subtables().unwrap() else {
                panic!("expected SinglePos")
            };
            assert_eq!(subtables.len(), 1);
            let read_fonts::tables::gpos::SinglePos::Format3(single) = subtables.get(0).unwrap()
            else {
                panic!("expected format 3")
            };
            assert_eq!(
                single.coverage().unwrap().get(GlyphId::new(i as u32 + 1)),
                Some(0)
            );
            assert_eq!(single.value_record().x_advance(), Some(advance));
        }
    }
}

#[test]
fn lookup_list2_rejects_invalid_selection_offsets_and_truncation() {
    let input = input(true);
    let mut plan = plan();
    assert_eq!(
        subset(&input[..10], true, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    plan.gpos_lookups.insert(3, 2);
    assert_eq!(
        subset(&input, true, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    plan.gpos_lookups.remove(&3);
    let mut bytes = input.clone();
    bytes[2..6].fill(0xff);
    assert_eq!(
        subset(&bytes, true, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    plan.gpos_lookups.clear();
    plan.gpos_lookups.insert(1, 0);
    assert_eq!(
        subset(&input, true, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
    );
    plan.gpos_lookups = (0..=u16::MAX).map(|i| (i, i)).collect();
    assert_eq!(
        subset(&input, true, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
    );
}

#[test]
fn lookup_list2_keeps_16bit_count_and_32bit_offsets() {
    let count = 65535usize;
    let offset = 2 + count * 4;
    let mut bytes = BeBuffer::new()
        .push(count as u16)
        .extend(std::iter::repeat_n(offset as u32, count))
        .to_vec();
    bytes.extend(lookup(65536, 40));
    let mut plan = plan();
    plan.gpos_lookups = (0..u16::MAX).map(|i| (i, i)).collect();
    let result = subset(&bytes, true, &plan).unwrap();
    let list = LookupList2::<PositionLookup>::read(FontData::new(&result)).unwrap();
    assert_eq!(list.lookup_count(), 65535);
    assert!(list.lookup_offsets()[count - 1].get().to_u32() > 65535);
    let lookup = list.lookups().get(count - 1).unwrap();
    let PositionLookup::Single(inner) = lookup else {
        panic!("expected SinglePos lookup")
    };
    assert_eq!(inner.sub_table_count(), 1);
    assert_eq!(inner.subtable_offsets()[0].get().to_u32(), 8);
}
