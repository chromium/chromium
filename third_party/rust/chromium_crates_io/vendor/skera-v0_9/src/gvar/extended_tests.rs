use super::*;
use font_test_data::bebuffer::BeBuffer;
use read_fonts::{tables::gvar::GvarFlags, FontData, FontRead};

fn input(wide: bool, count: usize, axes: u16, tuples: u16, payload: &[u8]) -> Vec<u8> {
    let header_size = if wide { 21 } else { 20 };
    let shared_offset = header_size + (count + 1) * 4;
    let shared_size = 2 * axes as usize * tuples as usize;
    let mut bytes = BeBuffer::new()
        .push(1u16)
        .push(0u16)
        .push(axes)
        .push(tuples)
        .push(if tuples == 0 { 0 } else { shared_offset as u32 });
    bytes = if wide {
        bytes.push(Uint24::new(count as u32))
    } else {
        bytes.push(count as u16)
    };
    let mut bytes = bytes
        .push(1u16)
        .push((shared_offset + shared_size) as u32)
        .to_vec();
    bytes.resize(shared_offset, 0);
    bytes[shared_offset - 4..shared_offset].copy_from_slice(&(payload.len() as u32).to_be_bytes());
    bytes.resize(shared_offset + shared_size, 0);
    bytes.extend(payload);
    bytes
}

fn read(bytes: &[u8], wide: bool) -> SelectedGvar<'_> {
    if wide {
        GvarExtended::read(FontData::new(bytes)).unwrap().into()
    } else {
        Gvar::read(FontData::new(bytes)).unwrap().into()
    }
}

fn subset(bytes: &[u8], wide: bool, count: usize, retain: bool) -> Result<Vec<u8>, SubsetError> {
    let last = read(bytes, wide).glyph_count() - 1;
    let plan = Plan {
        num_output_glyphs: count,
        new_to_old_gid_list: vec![
            (GlyphId::NOTDEF, GlyphId::NOTDEF),
            (
                GlyphId::new(if retain { last } else { 1 }),
                GlyphId::new(last),
            ),
        ],
        ..Default::default()
    };
    let mut s = Serializer::new(2 * 1024 * 1024);
    s.start_serialize().unwrap();
    subset_gvar(&read(bytes, wide), &plan, &mut s)?;
    s.end_serialize();
    assert!(!s.in_error());
    Ok(s.copy_bytes())
}

#[test]
fn extended_gvar_preserves_high_rows_and_selects_offset_width() {
    for wide in [false, true] {
        for retain in [false, true] {
            for large in [false, true] {
                let source_count = if wide { 70002 } else { 3 };
                let mut payload = vec![0u8; if large { 0x20000 } else { 5 }];
                // A zero-tuple GlyphVariationData header. GVAR uses Offset24.
                payload[if wide { 4 } else { 3 }] = if wide { 5 } else { 4 };
                let bytes = input(wide, source_count, 1, 0, &payload);
                let output_count = if retain { source_count } else { 2 };
                let output = subset(&bytes, wide, output_count, retain).unwrap();
                let table = read(&output, wide);
                assert_eq!(table.glyph_count(), output_count as u32);
                assert_eq!(table.flags().contains(GvarFlags::LONG_OFFSETS), large);
                assert_eq!(
                    table.glyph_variation_data_array_offset() as usize,
                    if wide { 21 } else { 20 } + (output_count + 1) * if large { 4 } else { 2 }
                );
                let gid = GlyphId::new(output_count as u32 - 1);
                let data = table.data_for_gid(gid).unwrap().unwrap();
                assert!(data.as_bytes().starts_with(&payload));
                assert_eq!(
                    data.len(),
                    payload.len() + usize::from(!large && payload.len() % 2 != 0)
                );
                assert!(table.data_for_gid(GlyphId::NOTDEF).unwrap().is_none());
                if retain && wide {
                    assert!(table.data_for_gid(GlyphId::new(65536)).unwrap().is_none());
                }
            }
        }
    }
}

#[test]
fn extended_gvar_copies_large_shared_tuple_arrays() {
    for wide in [false, true] {
        let bytes = input(wide, 3, 200, 200, &[0u8; 6]);
        let output = subset(&bytes, wide, 2, false).unwrap();
        let table = read(&output, wide);
        let start = table.shared_tuples_offset().to_u32() as usize;
        assert_eq!(start, if wide { 21 } else { 20 } + 6);
        assert_eq!(
            table.glyph_variation_data_array_offset() as usize,
            start + 80000
        );
        assert_eq!(&output[start..start + 80000], &[0u8; 80000]);
        assert_eq!(table.shared_tuples().unwrap().tuples().len(), 200);
        let mut null = bytes.clone();
        null[8..12].fill(0);
        assert!(subset(&null, wide, 2, false).is_err());
        let start = read(&bytes, wide).shared_tuples_offset().to_u32() as usize;
        assert!(subset(&bytes[..start + 79999], wide, 2, false).is_err());
    }
}

#[test]
fn extended_gvar_rejects_unrepresentable_output_counts() {
    for (wide, count) in [(false, 65536), (true, 0x1000000)] {
        assert!(subset(&input(wide, 3, 1, 0, &[0u8; 6]), wide, count, false).is_err());
    }
}

#[test]
fn extended_gvar_is_dispatched_and_legacy_variations_are_dropped() {
    use read_fonts::TableProvider;
    let mut builder = FontBuilder::new();
    let payload = [0u8, 0, 0, 0, 5];
    builder.add_raw(GvarExtended::TAG, input(true, 70002, 1, 0, &payload));
    builder.add_raw(Gvar::TAG, &[0u8][..]);
    builder.add_raw(
        Tag::new(b"MAXP"),
        BeBuffer::new()
            .push(0x5000u32)
            .push(Uint24::new(70002))
            .to_vec(),
    );
    let bytes = builder.build();
    let font = FontRef::new(&bytes).unwrap();
    let plan = Plan::new(
        &[GlyphId::new(70001)].into_iter().collect(),
        &Default::default(),
        &font,
        SubsetFlags::SUBSET_FLAGS_DEFAULT,
        &Default::default(),
        &Default::default(),
        &Default::default(),
        &Default::default(),
        &Default::default(),
    );
    let bytes = crate::subset_font(&font, &plan).unwrap();
    let font = FontRef::new(&bytes).unwrap();
    assert!(font.data_for_tag(Gvar::TAG).is_none());
    let table = font.gvar_extended().unwrap();
    assert_eq!(table.glyph_count(), Uint24::new(2));
    assert!(table
        .data_for_gid(GlyphId::new(1))
        .unwrap()
        .unwrap()
        .as_bytes()
        .starts_with(&payload));
}
