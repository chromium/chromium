use super::*;
use crate::FontData;
use font_test_data::bebuffer::BeBuffer;
use font_types::{GlyphId24, Uint24};

fn coverage(gid: u32) -> Vec<u8> {
    BeBuffer::new()
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(gid))
        .to_vec()
}

fn font(kind: u16, subtable: &[u8]) -> Vec<u8> {
    BeBuffer::new()
        .push(1u16)
        .push(0u16)
        .extend([0u16; 2])
        .push(10u16)
        .push(1u16)
        .push(4u16)
        .push(kind)
        .push(0u16)
        .push(1u16)
        .push(8u16)
        .extend(subtable.iter().copied())
        .to_vec()
}

fn close(kind: u16, subtable: &[u8], inputs: &[u32]) -> Result<Vec<u16>, ReadError> {
    let bytes = font(kind, subtable);
    let gpos = Gpos::read(FontData::new(&bytes)).unwrap();
    let mut lookups = [0].into_iter().collect();
    gpos.closure_lookups(
        &inputs.iter().copied().map(GlyphId::new).collect(),
        &mut lookups,
    )?;
    Ok(lookups.iter().collect())
}

fn fixtures() -> Vec<(u16, Vec<u8>, bool)> {
    vec![
        (
            1,
            BeBuffer::new()
                .push(3u16)
                .push(10u32)
                .push(4u16)
                .push(20i16)
                .extend(coverage(65536))
                .to_vec(),
            false,
        ),
        (
            1,
            BeBuffer::new()
                .push(4u16)
                .push(13u32)
                .push(4u16)
                .push(Uint24::new(1))
                .push(20i16)
                .extend(coverage(65536))
                .to_vec(),
            false,
        ),
        (
            2,
            BeBuffer::new()
                .push(3u16)
                .push(16u32)
                .extend([0u16; 2])
                .push(Uint24::new(1))
                .push(Uint24::new(24))
                .extend(coverage(65536))
                .push(Uint24::new(1))
                .push(GlyphId24::new(70000))
                .to_vec(),
            true,
        ),
        (
            2,
            BeBuffer::new()
                .push(4u16)
                .push(22u32)
                .extend([0u16; 2])
                .push(30u32)
                .push(41u32)
                .extend([2u16; 2])
                .extend(coverage(65536))
                .push(3u16)
                .push(GlyphId24::new(65536))
                .push(Uint24::new(1))
                .push(Uint24::new(1))
                .push(3u16)
                .push(GlyphId24::new(70000))
                .push(Uint24::new(1))
                .push(Uint24::new(1))
                .to_vec(),
            true,
        ),
        (
            3,
            BeBuffer::new()
                .push(2u16)
                .push(15u32)
                .push(Uint24::new(1))
                .extend([Uint24::new(0); 2])
                .extend(coverage(65536))
                .to_vec(),
            false,
        ),
        (
            4,
            BeBuffer::new()
                .push(2u16)
                .push(20u32)
                .push(28u32)
                .push(1u16)
                .extend([0u32; 2])
                .extend(coverage(65536))
                .extend(coverage(70000))
                .to_vec(),
            true,
        ),
        (
            5,
            BeBuffer::new()
                .push(2u16)
                .push(20u32)
                .push(28u32)
                .push(1u16)
                .extend([0u32; 2])
                .extend(coverage(65536))
                .extend(coverage(70000))
                .to_vec(),
            true,
        ),
        (
            6,
            BeBuffer::new()
                .push(2u16)
                .push(20u32)
                .push(28u32)
                .push(1u16)
                .extend([0u32; 2])
                .extend(coverage(65536))
                .extend(coverage(70000))
                .to_vec(),
            true,
        ),
    ]
}

#[test]
fn extended_positioning_lookup_closure_and_extensions() {
    for (kind, bytes, both) in fixtures() {
        assert_eq!(close(kind, &bytes, &[65536, 70000]).unwrap(), [0]);
        assert!(close(kind, &bytes, &[0, 4464]).unwrap().is_empty());
        assert_eq!(
            close(kind, &bytes, &[65536]).unwrap(),
            if both { vec![] } else { vec![0] }
        );
        let extension = BeBuffer::new()
            .push(1u16)
            .push(kind)
            .push(8u32)
            .extend(bytes.iter().copied());
        assert_eq!(close(9, &extension, &[65536, 70000]).unwrap(), [0]);
        let mut null = bytes.clone();
        null[2..6].fill(0);
        assert!(close(kind, &null, &[65536, 70000]).unwrap().is_empty());
        null[2..6].fill(0xff);
        assert!(close(kind, &null, &[65536, 70000]).is_err());
    }
}

#[test]
fn extended_pair_closure_preserves_large_set_indices_and_offsets() {
    let count = 65537u32;
    let coverage_offset = 13 + count * 3;
    let set_offset = coverage_offset + 14;
    let bytes = BeBuffer::new()
        .push(3u16)
        .push(coverage_offset)
        .extend([0u16; 2])
        .push(Uint24::new(count))
        .extend((0..count).map(|i| Uint24::new(if i == count - 1 { set_offset } else { 0 })))
        .push(4u16)
        .push(Uint24::new(1))
        .extend([0, 65536, 0].map(Uint24::new))
        .push(Uint24::new(1))
        .push(GlyphId24::new(0xffffff))
        .to_vec();
    assert_eq!(close(2, &bytes, &[65536, 0xffffff]).unwrap(), [0]);
    assert!(close(2, &bytes, &[0, 0xffffff]).unwrap().is_empty());
    let mut null = bytes.clone();
    let index = 13 + 65536 * 3;
    null[index..index + 3].fill(0);
    assert!(close(2, &null, &[65536, 0xffffff]).unwrap().is_empty());
    null[index..index + 3].fill(0xff);
    assert!(close(2, &null, &[65536, 0xffffff]).is_err());
}

#[test]
fn extended_pair_closure_preserves_large_pair_counts() {
    let count = 65537u32;
    let bytes = BeBuffer::new()
        .push(3u16)
        .push(16u32)
        .extend([0u16; 2])
        .push(Uint24::new(1))
        .push(Uint24::new(24))
        .extend(coverage(70000))
        .push(Uint24::new(count))
        .extend((0..count).map(GlyphId24::new))
        .to_vec();
    assert_eq!(close(2, &bytes, &[65536, 70000]).unwrap(), [0]);
    assert!(close(2, &bytes, &[70000]).unwrap().is_empty());
}
