use super::*;
use font_test_data::bebuffer::BeBuffer;
use read_fonts::{
    tables::gdef::SelectedLigCaretList,
    types::{GlyphId, GlyphId24},
    FontData, FontRead,
};

fn coverage(gid: u32) -> Vec<u8> {
    BeBuffer::new()
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(gid))
        .to_vec()
}

fn input(wide: [bool; 5], large_offsets: bool) -> Vec<u8> {
    let glyph_classes = BeBuffer::new()
        .push(4u16)
        .push(Uint24::new(2))
        .push(GlyphId24::new(65536))
        .push(GlyphId24::new(65536))
        .push(1u16)
        .push(GlyphId24::new(70000))
        .push(GlyphId24::new(70000))
        .push(2u16)
        .to_vec();
    let mut attach = BeBuffer::new().push(6u16).push(1u16).push(14u16).to_vec();
    attach.extend(coverage(65536));
    attach.extend(BeBuffer::new().push(2u16).push(3u16).push(5u16).to_vec());
    let mut lig = if wide[2] {
        BeBuffer::new()
            .push(10u32)
            .push(Uint24::new(1))
            .push(Uint24::new(18))
            .to_vec()
    } else {
        BeBuffer::new().push(6u16).push(1u16).push(14u16).to_vec()
    };
    lig.extend(coverage(70000));
    lig.extend(
        BeBuffer::new()
            .push(1u16)
            .push(4u16)
            .push(1u16)
            .push(123i16)
            .to_vec(),
    );
    let mark_classes = BeBuffer::new()
        .push(3u16)
        .push(GlyphId24::new(70000))
        .push(Uint24::new(1))
        .push(Uint24::new(7))
        .to_vec();
    let mut sets = BeBuffer::new().push(1u16).push(1u16).push(8u32).to_vec();
    sets.extend(coverage(70000));
    let tables = [glyph_classes, attach, lig, mark_classes, sets];
    let start = if large_offsets { 70000 } else { 38 };
    let mut offset = start;
    let offsets = tables.each_ref().map(|table| {
        let result = offset;
        offset += table.len();
        result
    });
    let mut bytes = BeBuffer::new().push(1u16).push(4u16);
    for (offset, wide) in offsets.into_iter().zip(wide) {
        bytes = bytes.push(if wide { 1u16 } else { offset as u16 });
    }
    bytes = bytes.push(0u32);
    for (offset, wide) in offsets.into_iter().zip(wide) {
        bytes = bytes.push(if wide { offset as u32 } else { 0 });
    }
    let mut bytes = bytes.to_vec();
    bytes.resize(start, 0);
    for table in tables {
        bytes.extend(table);
    }
    bytes
}

fn store() -> Vec<u8> {
    BeBuffer::new()
        .push(1u16)
        .push(12u32)
        .push(1u16)
        .push(22u32)
        .push(1u16)
        .push(1u16)
        .push(0xc000u16)
        .push(0u16)
        .push(0x4000u16)
        .push(2u16)
        .push(1u16)
        .push(1u16)
        .push(0u16)
        .push(10i16)
        .push(20i16)
        .to_vec()
}

fn plan() -> Plan {
    let mut plan = Plan {
        font_num_glyphs: 70001,
        glyph_map_gsub: vec![crate::INVALID_GID; 70001],
        ..Default::default()
    };
    for (old, new) in [(65536, 1), (70000, 2)] {
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
    }
    plan
}

fn subset(input: &[u8], plan: &Plan) -> Result<(Vec<u8>, bool), SerializeErrorFlags> {
    let mut s = Serializer::new(1024 * 1024);
    let mut state = SubsetState::default();
    s.start_serialize().unwrap();
    let table = Gdef::read(FontData::new(input)).unwrap();
    super::super::subset_gdef(&table, plan, &mut s, &mut state)?;
    s.end_serialize();
    assert!(!s.in_error());
    Ok((s.copy_bytes(), state.has_gdef_varstore))
}

fn check_header(bytes: &[u8], wide: [bool; 5]) {
    let table = Gdef::read(FontData::new(bytes)).unwrap();
    assert_eq!(table.version(), MajorMinor::new(1, 4));
    let legacy = [
        table.glyph_class_def_offset(),
        table.attach_list_offset(),
        table.lig_caret_list_offset(),
        table.mark_attach_class_def_offset(),
        table.mark_glyph_sets_def_offset().unwrap(),
    ];
    let extended = [
        table.glyph_class_def2_offset(),
        table.attach_list2_offset(),
        table.lig_caret_list2_offset(),
        table.mark_attach_class_def2_offset(),
        table.mark_glyph_sets_def2_offset(),
    ];
    for i in 0..5 {
        assert_eq!(legacy[i].is_null(), wide[i]);
        assert_eq!(extended[i].unwrap().is_null(), !wide[i]);
    }
    let classes = table.glyph_class_def().unwrap().unwrap();
    assert_eq!(classes.get(GlyphId::new(1)), 1);
    assert_eq!(classes.get(GlyphId::new(2)), 2);
    let attach = table.attach_list().unwrap().unwrap();
    assert_eq!(attach.glyph_count(), 1);
    assert_eq!(attach.coverage().unwrap().get(GlyphId::new(1)), Some(0));
    let point = attach.attach_points().get(0).unwrap();
    assert_eq!(
        point
            .point_indices()
            .iter()
            .map(|v| v.get())
            .collect::<Vec<_>>(),
        [3, 5]
    );
    let lig = table.lig_caret_list().unwrap().unwrap();
    assert_eq!(matches!(lig, SelectedLigCaretList::Extended(_)), wide[2]);
    assert_eq!(lig.lig_glyph_count(), 1);
    assert_eq!(lig.coverage().unwrap().get(GlyphId::new(2)), Some(0));
    assert_eq!(lig.lig_glyphs().get(0).unwrap().caret_count(), 1);
    assert_eq!(
        table
            .mark_attach_class_def()
            .unwrap()
            .unwrap()
            .get(GlyphId::new(2)),
        7
    );
    let sets = table.mark_glyph_sets_def().unwrap().unwrap();
    assert_eq!(sets.mark_glyph_set_count(), 1);
    assert_eq!(
        sets.coverages().get(0).unwrap().get(GlyphId::new(2)),
        Some(0)
    );
}

#[test]
fn extended_gdef_preserves_all_independent_precedence_combinations() {
    let plan = plan();
    for bits in 0..32 {
        let wide = std::array::from_fn(|i| bits & (1 << i) != 0);
        let (bytes, has_store) = subset(&input(wide, false), &plan).unwrap();
        assert!(!has_store);
        check_header(&bytes, wide);
    }
    let (bytes, has_store) = subset(&input([true; 5], true), &plan).unwrap();
    assert!(!has_store);
    check_header(&bytes, [true; 5]);
}

#[test]
fn extended_gdef_keeps_subset_variation_store_last_and_sets_state() {
    let mut input = input([true; 5], false);
    let offset = input.len();
    input[14..18].copy_from_slice(&(offset as u32).to_be_bytes());
    input.extend(store());
    let mut plan = plan();
    plan.gdef_varstore_inner_maps = vec![[1u32].into_iter().collect()];
    let (bytes, has_store) = subset(&input, &plan).unwrap();
    assert!(has_store);
    check_header(&bytes, [true; 5]);
    let table = Gdef::read(FontData::new(&bytes)).unwrap();
    let var_offset = table.item_var_store_offset().unwrap().offset().to_u32();
    for offset in [
        table.glyph_class_def2_offset(),
        table.attach_list2_offset(),
        table.lig_caret_list2_offset(),
        table.mark_attach_class_def2_offset(),
        table.mark_glyph_sets_def2_offset(),
    ] {
        assert!(offset.unwrap().offset().to_u32() < var_offset);
    }
    let store = table.item_var_store().unwrap().unwrap();
    let data = store.item_variation_data().get(0).unwrap().unwrap();
    assert_eq!(data.item_count(), 1);
    assert_eq!(data.delta_set(0).collect::<Vec<_>>(), [20]);
    let region_end = store.variation_region_list_offset().to_u32() as usize
        + store
            .variation_region_list()
            .unwrap()
            .min_table_bytes()
            .len();
    let data_end = store.item_variation_data_offsets()[0]
        .get()
        .offset()
        .to_u32() as usize
        + data.min_table_bytes().len();
    assert_eq!(var_offset as usize + region_end.max(data_end), bytes.len());
    plan.gdef_varstore_inner_maps.clear();
    let (bytes, has_store) = subset(&input, &plan).unwrap();
    assert!(!has_store);
    check_header(&bytes, [true; 5]);
    assert!(Gdef::read(FontData::new(&bytes))
        .unwrap()
        .item_var_store()
        .is_none());
}

#[test]
fn extended_gdef_rejects_invalid_preferred_offsets_truncation_and_empty_output() {
    let plan = plan();
    let input = input([true; 5], false);
    assert_eq!(
        subset(&input[..37], &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    for range in [18..22, 22..26, 26..30, 30..34, 34..38] {
        let mut bytes = input.clone();
        bytes[range].fill(0xff);
        assert_eq!(
            subset(&bytes, &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
    }
    let mut empty = vec![0u8; 38];
    empty[..4].copy_from_slice(&MajorMinor::new(1, 4).to_be_bytes());
    assert_eq!(
        subset(&empty, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
    );
    let mut bytes = input.clone();
    bytes[14..18].fill(0xff);
    assert_eq!(
        subset(&bytes, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    bytes[2..4].copy_from_slice(&5u16.to_be_bytes());
    assert_eq!(
        subset(&bytes, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER)
    );
}
