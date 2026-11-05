//! Regression tests for extended contextual glyph and lookup closure.

use super::*;
use crate::FontData;
use font_test_data::bebuffer::BeBuffer;
use font_types::{GlyphId24, Uint24};

fn context(chain: bool, records: &[(u16, u16)]) -> Vec<u8> {
    let mut bytes = BeBuffer::new()
        .push(4u16)
        .push(12u32)
        .push(Uint24::new(1))
        .push(Uint24::new(20))
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(65536))
        .push(1u16);
    if chain {
        bytes = bytes
            .push(Uint24::new(5))
            .push(1u16)
            .push(GlyphId24::new(65535))
            .push(2u16)
            .push(GlyphId24::new(70000))
            .push(1u16)
            .push(GlyphId24::new(0xffffff))
            .push(records.len() as u16);
    } else {
        bytes = bytes
            .push(4u16)
            .push(2u16)
            .push(records.len() as u16)
            .push(GlyphId24::new(70000));
    }
    for &(sequence, lookup) in records {
        bytes = bytes.push(sequence).push(lookup);
    }
    bytes.to_vec()
}

fn font(kind: u16, context: &[u8]) -> Vec<u8> {
    let single = BeBuffer::new()
        .push(4u16)
        .push(15u32)
        .push(Uint24::new(2))
        .extend([70001, 70002].map(GlyphId24::new))
        .push(3u16)
        .push(Uint24::new(2))
        .extend([65536, 70000].map(GlyphId24::new));
    layout_font(kind, context, &single)
}

fn layout_font(kind: u16, context: &[u8], nested: &[u8]) -> Vec<u8> {
    BeBuffer::new()
        .push(1u16)
        .push(2u16)
        .extend([0u16; 3])
        .extend([0u32; 3])
        .push(26u32)
        .push(2u16)
        .push(10u32)
        .push(18u32 + context.len() as u32)
        .push(kind)
        .push(0u16)
        .push(1u16)
        .push(8u16)
        .extend(context.iter().copied())
        .push(1u16)
        .push(0u16)
        .push(1u16)
        .push(8u16)
        .extend(nested.iter().copied())
        .to_vec()
}

fn close(kind: u16, context: &[u8], inputs: &[u32]) -> Result<(Vec<u32>, Vec<u16>), ReadError> {
    let bytes = font(kind, context);
    let gsub = Gsub::read(FontData::new(&bytes)).unwrap();
    let mut glyphs = inputs.iter().copied().map(GlyphId::new).collect();
    let mut lookups = [0].into_iter().collect();
    gsub.closure_lookups(&glyphs, &mut lookups)?;
    // Only the contextual lookup is a feature root. Nested lookups must keep
    // the context's active-glyph restriction, rather than run independently.
    gsub.closure_glyphs(&[0].into_iter().collect(), &mut glyphs)?;
    Ok((
        glyphs.iter().map(GlyphId::to_u32).collect(),
        lookups.iter().collect(),
    ))
}

#[test]
fn wide_glyph_context_closure_restricts_nested_lookup_to_its_sequence_position() {
    for (chain, kind, inputs) in [
        (false, 5, vec![65536, 70000]),
        (true, 6, vec![65535, 65536, 70000, 0xffffff]),
    ] {
        for (records, outputs) in [
            (vec![(0, 1)], vec![70001]),
            (vec![(1, 1)], vec![70002]),
            (vec![(1, 1), (1, 1)], vec![70001, 70002]),
            (vec![(2, 1)], vec![]),
        ] {
            let (glyphs, lookups) = close(kind, &context(chain, &records), &inputs).unwrap();
            let mut expected = inputs.clone();
            expected.extend(outputs);
            expected.sort_unstable();
            assert_eq!(glyphs, expected);
            assert_eq!(lookups, [0, 1]);
        }
    }
}

#[test]
fn wide_glyph_context_closure_requires_every_context_glyph() {
    for (chain, kind, inputs) in [
        (false, 5, vec![65536, 70000]),
        (true, 6, vec![65535, 65536, 70000, 0xffffff]),
    ] {
        for excluded in &inputs {
            let inputs: Vec<_> = inputs
                .iter()
                .copied()
                .filter(|gid| gid != excluded)
                .collect();
            let (glyphs, lookups) = close(kind, &context(chain, &[(1, 1)]), &inputs).unwrap();
            assert_eq!(glyphs, inputs);
            assert!(lookups.is_empty());
        }
    }
}

#[test]
fn wide_glyph_context_closure_handles_null_invalid_offsets_and_cycles() {
    for (chain, kind) in [(false, 5), (true, 6)] {
        let inputs = [65535, 65536, 70000, 0xffffff];
        let base = context(chain, &[(1, 1)]);
        for range in [2..6, 9..12, if chain { 22..25 } else { 22..24 }] {
            let mut bytes = base.clone();
            bytes[range.clone()].fill(0);
            let (glyphs, lookups) = close(kind, &bytes, &inputs).unwrap();
            assert_eq!(glyphs, inputs);
            assert!(lookups.is_empty());
            bytes[range].fill(0xff);
            assert!(close(kind, &bytes, &inputs).is_err());
        }
        let (glyphs, lookups) = close(kind, &context(chain, &[(1, 0)]), &inputs).unwrap();
        assert_eq!(glyphs, inputs);
        assert_eq!(lookups, [0]);
    }
}

#[test]
fn wide_glyph_context_closure_keeps_large_set_indices_and_offsets() {
    let count = 65537u32;
    for (chain, kind) in [(false, 5), (true, 6)] {
        let base = context(chain, &[(1, 1)]);
        let coverage_offset = 9 + count * 3;
        let set_offset = coverage_offset + 14;
        let bytes = BeBuffer::new()
            .push(4u16)
            .push(coverage_offset)
            .push(Uint24::new(count))
            .extend(
                (0..count).map(|index| Uint24::new(if index == 65536 { set_offset } else { 0 })),
            )
            .push(4u16)
            .push(Uint24::new(1))
            .extend([0, 65536, 0].map(Uint24::new))
            .extend(base[20..].iter().copied());
        let (glyphs, lookups) = close(kind, &bytes, &[65535, 65536, 70000, 0xffffff]).unwrap();
        assert_eq!(glyphs, [65535, 65536, 70000, 70002, 0xffffff]);
        assert_eq!(lookups, [0, 1]);
    }
}

#[test]
fn wide_glyph_context_lookup_closure_also_handles_gpos() {
    use crate::tables::gpos::Gpos;
    let single = BeBuffer::new()
        .push(1u16)
        .push(8u16)
        .push(4u16)
        .push(10i16)
        .push(3u16)
        .push(Uint24::new(2))
        .extend([65536, 70000].map(GlyphId24::new));
    for (chain, kind, inputs) in [
        (false, 7, vec![65536, 70000]),
        (true, 8, vec![65535, 65536, 70000, 0xffffff]),
    ] {
        let bytes = layout_font(kind, &context(chain, &[(1, 1)]), &single);
        let gpos = Gpos::read(FontData::new(&bytes)).unwrap();
        let glyphs = inputs.iter().copied().map(GlyphId::new).collect();
        let mut lookups = [0].into_iter().collect();
        gpos.closure_lookups(&glyphs, &mut lookups).unwrap();
        assert_eq!(lookups.iter().collect::<Vec<_>>(), [0, 1]);
        for excluded in &inputs {
            let glyphs = inputs
                .iter()
                .copied()
                .filter(|gid| gid != excluded)
                .map(GlyphId::new)
                .collect();
            let mut lookups = [0].into_iter().collect();
            gpos.closure_lookups(&glyphs, &mut lookups).unwrap();
            assert!(lookups.is_empty());
        }
    }
}

fn coverage_context(records: &[(u16, u16)]) -> Vec<u8> {
    let first_coverage = 12 + 4 * records.len() as u32;
    let mut bytes = BeBuffer::new()
        .push(6u16)
        .push(2u16)
        .push(records.len() as u16)
        .push(Uint24::new(first_coverage))
        .push(Uint24::new(first_coverage + 8));
    for &(sequence, lookup) in records {
        bytes = bytes.push(sequence).push(lookup);
    }
    bytes
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(65536))
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(70000))
        .to_vec()
}

#[test]
fn wide_coverage_context_closure_preserves_active_glyphs_and_checks_every_position() {
    for (records, expected) in [
        (vec![(0, 1)], vec![65536, 70000, 70001]),
        (vec![(1, 1)], vec![65536, 70000, 70002]),
        (vec![(1, 1), (1, 1)], vec![65536, 70000, 70001, 70002]),
        (vec![(2, 1)], vec![65536, 70000]),
    ] {
        let bytes = coverage_context(&records);
        let (glyphs, lookups) = close(5, &bytes, &[65536, 70000]).unwrap();
        assert_eq!(glyphs, expected);
        assert_eq!(lookups, [0, 1]);
        for inputs in [[65536], [70000]] {
            let (glyphs, lookups) = close(5, &bytes, &inputs).unwrap();
            assert_eq!(glyphs, inputs);
            assert!(lookups.is_empty());
        }
    }
    let single = BeBuffer::new()
        .push(1u16)
        .push(8u16)
        .push(4u16)
        .push(10i16)
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(70000));
    let bytes = layout_font(7, &coverage_context(&[(1, 1)]), &single);
    let gpos = crate::tables::gpos::Gpos::read(FontData::new(&bytes)).unwrap();
    let mut lookups = [0].into_iter().collect();
    gpos.closure_lookups(
        &[65536, 70000].map(GlyphId::new).into_iter().collect(),
        &mut lookups,
    )
    .unwrap();
    assert_eq!(lookups.iter().collect::<Vec<_>>(), [0, 1]);
}

#[test]
fn wide_coverage_context_closure_handles_null_and_invalid_offsets() {
    for range in [6..9, 9..12] {
        let mut bytes = coverage_context(&[(1, 1)]);
        bytes[range.clone()].fill(0);
        let (glyphs, lookups) = close(5, &bytes, &[65536, 70000]).unwrap();
        assert_eq!(glyphs, [65536, 70000]);
        assert!(lookups.is_empty());
        bytes[range].fill(0xff);
        assert!(close(5, &bytes, &[65536, 70000]).is_err());
    }
}

#[test]
fn coverage_context_closure_accepts_maximum_sequence_length() {
    let count = u16::MAX;
    let coverage_offset = 6 + u32::from(count) * 3 + 4;
    let bytes = BeBuffer::new()
        .push(6u16)
        .push(count)
        .push(1u16)
        .extend((0..count).map(|_| Uint24::new(coverage_offset)))
        .push(count - 1)
        .push(1u16)
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(65536));
    let (glyphs, lookups) = close(5, &bytes, &[65536]).unwrap();
    assert_eq!(glyphs, [65536, 70001]);
    assert_eq!(lookups, [0, 1]);
}

fn class_context(chain: bool, primary_class: u32, records: &[(u16, u16)]) -> Vec<u8> {
    let count = primary_class + 1;
    let header_size = if chain { 20 } else { 13 };
    let coverage_offset = header_size + count * 3;
    let input_class_offset = coverage_offset + 8;
    let input_class = BeBuffer::new()
        .push(3u16)
        .push(GlyphId24::new(65536))
        .push(Uint24::new(4465))
        .extend((0..4465).map(|i| {
            Uint24::new(match i {
                0 => primary_class,
                4464 => 65535,
                _ => 0,
            })
        }));
    let backtrack_class_offset = input_class_offset + input_class.len() as u32;
    let lookahead_class_offset = backtrack_class_offset + 11;
    let set_offset = if chain {
        lookahead_class_offset + 11
    } else {
        backtrack_class_offset
    };
    let mut bytes = BeBuffer::new().push(5u16).push(coverage_offset);
    if chain {
        bytes = bytes
            .push(backtrack_class_offset)
            .push(input_class_offset)
            .push(lookahead_class_offset)
            .push(count as u16);
    } else {
        bytes = bytes.push(input_class_offset).push(Uint24::new(count));
    }
    bytes = bytes
        .extend((0..count).map(|i| Uint24::new(if i == primary_class { set_offset } else { 0 })))
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(65536))
        .extend(input_class.iter().copied());
    if chain {
        bytes = bytes
            .push(3u16)
            .push(GlyphId24::new(65535))
            .push(Uint24::new(1))
            .push(Uint24::new(3))
            .push(3u16)
            .push(GlyphId24::new(0xffffff))
            .push(Uint24::new(1))
            .push(Uint24::new(4));
    }
    bytes = bytes.push(1u16).push(Uint24::new(5));
    if chain {
        bytes = bytes
            .push(1u16)
            .push(3u16)
            .push(2u16)
            .push(65535u16)
            .push(1u16)
            .push(4u16)
            .push(records.len() as u16);
    } else {
        bytes = bytes.push(2u16).push(records.len() as u16).push(65535u16);
    }
    for &(sequence, lookup) in records {
        bytes = bytes.push(sequence).push(lookup);
    }
    bytes.to_vec()
}

#[test]
fn wide_class_context_closure_restricts_nested_lookups_and_keeps_primary_classes() {
    for (chain, kind, primary_class, inputs) in [
        (false, 5, 65536, vec![65536, 70000]),
        (true, 6, 32768, vec![65535, 65536, 70000, 0xffffff]),
        (false, 5, 0, vec![65536, 70000]),
        (true, 6, 0, vec![65535, 65536, 70000, 0xffffff]),
    ] {
        for (records, outputs) in [
            (vec![(0, 1)], vec![70001]),
            (vec![(1, 1)], vec![70002]),
            (vec![(1, 1), (1, 1)], vec![70001, 70002]),
        ] {
            let bytes = class_context(chain, primary_class, &records);
            let (glyphs, lookups) = close(kind, &bytes, &inputs).unwrap();
            let mut expected = inputs.clone();
            expected.extend(outputs);
            expected.sort_unstable();
            assert_eq!(glyphs, expected);
            assert_eq!(lookups, [0, 1]);
            for excluded in &inputs {
                let inputs: Vec<_> = inputs
                    .iter()
                    .copied()
                    .filter(|gid| gid != excluded)
                    .collect();
                let (glyphs, lookups) = close(kind, &bytes, &inputs).unwrap();
                assert_eq!(glyphs, inputs);
                assert!(lookups.is_empty());
            }
        }
    }
}

#[test]
fn wide_class_context_closure_handles_null_invalid_offsets_and_gpos() {
    for (chain, kind, primary_class, inputs) in [
        (false, 5, 65536, vec![65536, 70000]),
        (true, 6, 32768, vec![65535, 65536, 70000, 0xffffff]),
    ] {
        let base = class_context(chain, primary_class, &[(1, 1)]);
        let header = if chain { 20 } else { 13 };
        let set_offset = if chain {
            crate::tables::layout::ChainedSequenceContextFormat5::read(FontData::new(&base))
                .unwrap()
                .chained_class_seq_rule_set_offsets()[primary_class as usize]
                .get()
                .offset()
                .to_u32() as usize
        } else {
            crate::tables::layout::SequenceContextFormat5::read(FontData::new(&base))
                .unwrap()
                .class_seq_rule_set_offsets()[primary_class as usize]
                .get()
                .offset()
                .to_u32() as usize
        };
        let mut ranges = vec![
            2..6,
            6..10,
            header + primary_class as usize * 3..header + primary_class as usize * 3 + 3,
            set_offset + 2..set_offset + 5,
        ];
        if chain {
            ranges.extend([10..14, 14..18]);
        }
        for range in ranges {
            let mut bytes = base.clone();
            bytes[range.clone()].fill(0);
            let (glyphs, lookups) = close(kind, &bytes, &inputs).unwrap();
            assert_eq!(glyphs, inputs);
            assert!(lookups.is_empty());
            bytes[range].fill(0xff);
            assert!(close(kind, &bytes, &inputs).is_err());
        }
        let single = BeBuffer::new()
            .push(1u16)
            .push(8u16)
            .push(4u16)
            .push(10i16)
            .push(3u16)
            .push(Uint24::new(1))
            .push(GlyphId24::new(70000));
        let bytes = layout_font(if chain { 8 } else { 7 }, &base, &single);
        let gpos = crate::tables::gpos::Gpos::read(FontData::new(&bytes)).unwrap();
        let mut lookups = [0].into_iter().collect();
        gpos.closure_lookups(
            &inputs.into_iter().map(GlyphId::new).collect(),
            &mut lookups,
        )
        .unwrap();
        assert_eq!(lookups.iter().collect::<Vec<_>>(), [0, 1]);
    }
}
