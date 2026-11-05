use super::*;
use crate::graph::test::{add_object, add_offset, add_wide_offset};
use read_fonts::{
    tables::gpos::{Gpos, PairPos, PositionSubtables},
    TopLevelTable,
};

fn input(class1: u16, class2: u16, devices: bool) -> Serializer {
    let mut s = Serializer::new(4 * 1024 * 1024);
    s.start_serialize().unwrap();
    s.push().unwrap();
    CoverageTable::serialize(
        &mut s,
        &(0..class1)
            .map(|i| GlyphId::new(65536 + u32::from(i)))
            .collect::<Vec<_>>(),
    )
    .unwrap();
    let coverage = s.pop_pack(false).unwrap();
    s.push().unwrap();
    ClassDef::serialize(
        &mut s,
        &(0..class1)
            .map(|i| (65536 + u32::from(i), u32::from(i)))
            .collect::<Vec<_>>(),
    )
    .unwrap();
    let classes1 = s.pop_pack(false).unwrap();
    s.push().unwrap();
    ClassDef::serialize(
        &mut s,
        &(0..class2)
            .map(|i| (70000 + u32::from(i), u32::from(i)))
            .collect::<Vec<_>>(),
    )
    .unwrap();
    let classes2 = s.pop_pack(false).unwrap();
    let device_indices: Vec<_> = (0..if devices { class1 * 2 } else { 0 })
        .map(|i| {
            let mut bytes = Vec::new();
            bytes.extend(0u16.to_be_bytes());
            bytes.extend(i.to_be_bytes());
            bytes.extend(0x8000u16.to_be_bytes());
            add_object(&mut s, &bytes, bytes.len(), false)
        })
        .collect();
    s.push().unwrap();
    s.embed(4u16).unwrap();
    add_wide_offset(&mut s, coverage);
    s.embed(if devices { 0x44u16 } else { 0x4u16 }).unwrap();
    s.embed(if devices { 0x11u16 } else { 0x1u16 }).unwrap();
    add_wide_offset(&mut s, classes1);
    add_wide_offset(&mut s, classes2);
    s.embed(class1).unwrap();
    s.embed(class2).unwrap();
    for i in 0..class1 {
        for j in 0..class2 {
            s.embed(100 + i as i16).unwrap();
            if devices {
                add_offset(&mut s, device_indices[usize::from(i) * 2]);
            }
            s.embed(j as i16).unwrap();
            if devices {
                add_offset(&mut s, device_indices[usize::from(i) * 2 + 1]);
            }
        }
    }
    let table = s.pop_pack(false).unwrap();
    s.push().unwrap();
    s.embed(2u16).unwrap();
    s.embed(0u16).unwrap();
    s.embed(1u16).unwrap();
    add_offset(&mut s, table);
    let lookup = s.pop_pack(false).unwrap();
    s.push().unwrap();
    s.embed(1u16).unwrap();
    add_offset(&mut s, lookup);
    let list = s.pop_pack(false).unwrap();
    s.push().unwrap();
    s.embed(1u16).unwrap();
    s.embed(0u16).unwrap();
    s.embed(0u16).unwrap();
    s.embed(0u16).unwrap();
    add_offset(&mut s, list);
    s.pop_pack(false).unwrap();
    s.end_serialize();
    s
}

#[test]
fn extended_class_pairs_split_device_offsets_without_narrowing() {
    let s = input(4, 3000, true);
    let output = crate::repack::resolve_overflows(s, Gpos::TAG, 32).unwrap();
    let table = Gpos::read(FontData::new(&output)).unwrap();
    let list = table.lookup_list().unwrap();
    let lookup = list.lookups().get(0).unwrap();
    let PositionSubtables::Pair(subtables) = lookup.subtables().unwrap() else {
        panic!()
    };
    assert_eq!(subtables.len(), 2);
    let mut glyphs = Vec::new();
    for table in subtables.iter() {
        let PairPos::Format4(table) = table.unwrap() else {
            panic!()
        };
        assert_eq!(table.class1_count(), 2);
        assert_eq!(table.class2_count(), 3000);
        let classes = table.class_def1().unwrap();
        for gid in table.coverage().unwrap().iter() {
            glyphs.push(gid.to_u32());
            let row = classes.get(gid) as usize;
            for j in [0, 2999] {
                let record = table
                    .class1_records()
                    .get(row)
                    .unwrap()
                    .class2_records()
                    .get(j)
                    .unwrap();
                assert_eq!(
                    record.value_record1().x_advance(),
                    Some(100 + (gid.to_u32() - 65536) as i16)
                );
                assert_eq!(record.value_record2().x_placement(), Some(j as i16));
                for (device, index) in [
                    (
                        record.value_record1().x_advance_device().unwrap().unwrap(),
                        (gid.to_u32() - 65536) * 2,
                    ),
                    (
                        record
                            .value_record2()
                            .x_placement_device()
                            .unwrap()
                            .unwrap(),
                        (gid.to_u32() - 65536) * 2 + 1,
                    ),
                ] {
                    let read_fonts::tables::layout::DeviceOrVariationIndex::VariationIndex(device) =
                        device
                    else {
                        panic!()
                    };
                    assert_eq!(u32::from(device.delta_set_inner_index()), index);
                }
            }
        }
    }
    assert_eq!(glyphs, [65536, 65537, 65538, 65539]);
}

#[test]
fn extended_class_pairs_do_not_split_wide_offsets_without_devices() {
    let s = input(4, 20000, false);
    let mut graph = Graph::from_serializer(s).unwrap();
    let list = graph.index_for_position(graph.root_idx(), 8).unwrap();
    let lookup = graph.index_for_position(list, 2).unwrap();
    let table = graph.index_for_position(lookup, 6).unwrap();
    assert!(split_pairpos(&mut graph, table).unwrap().is_empty());
}

#[test]
fn extended_class_pairs_reject_unsplittable_device_rows() {
    let s = input(1, 10000, true);
    let mut graph = Graph::from_serializer(s).unwrap();
    let list = graph.index_for_position(graph.root_idx(), 8).unwrap();
    let lookup = graph.index_for_position(list, 2).unwrap();
    let table = graph.index_for_position(lookup, 6).unwrap();
    assert!(matches!(
        split_pairpos(&mut graph, table),
        Err(RepackError::ErrorSplitSubtable)
    ));
}
