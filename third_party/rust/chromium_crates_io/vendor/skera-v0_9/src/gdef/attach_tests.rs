use super::*;
use font_test_data::bebuffer::BeBuffer;
use read_fonts::{types::GlyphId, FontData, FontRead};

fn input(wide: bool, points: &[u8]) -> Vec<u8> {
    let mut bytes = if wide {
        BeBuffer::new()
            .push(1u16)
            .push(4u16)
            .extend([0u16; 5])
            .push(0u32)
            .extend([0u32, 38, 0, 0, 0])
    } else {
        BeBuffer::new()
            .push(1u16)
            .push(0u16)
            .extend([0u16, 12, 0, 0])
    }
    .push(6u16)
    .push(1u16)
    .push(12u16)
    .push(1u16)
    .push(1u16)
    .push(1u16)
    .to_vec();
    bytes.extend(points);
    bytes
}

fn subset(bytes: &[u8]) -> Result<Vec<u8>, SerializeErrorFlags> {
    let plan = Plan {
        font_num_glyphs: 2,
        glyphset_gsub: [GlyphId::new(1)].into_iter().collect(),
        glyph_map_gsub: vec![crate::INVALID_GID, GlyphId::new(1)],
        ..Default::default()
    };
    let mut s = Serializer::new(1024);
    s.start_serialize().unwrap();
    subset_gdef(
        &Gdef::read(FontData::new(bytes)).unwrap(),
        &plan,
        &mut s,
        &mut SubsetState::default(),
    )?;
    s.end_serialize();
    assert!(!s.in_error());
    Ok(s.copy_bytes())
}

#[test]
fn attachment_points_reject_truncated_arrays() {
    for wide in [false, true] {
        for points in [&[0, 1][..], &[0, 2, 0, 3][..]] {
            assert_eq!(
                subset(&input(wide, points)),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
            );
        }
    }
}

#[test]
fn attachment_points_preserve_complete_and_empty_arrays() {
    for wide in [false, true] {
        for points in [&[0, 0][..], &[0, 2, 0, 3, 0, 5][..]] {
            let bytes = subset(&input(wide, points)).unwrap();
            let table = Gdef::read(FontData::new(&bytes)).unwrap();
            let attach = table.attach_list().unwrap().unwrap();
            assert_eq!(attach.glyph_count(), 1);
            assert_eq!(attach.coverage().unwrap().get(GlyphId::new(1)), Some(0));
            assert_eq!(
                attach.attach_points().get(0).unwrap().min_table_bytes(),
                points
            );
        }
    }
}
