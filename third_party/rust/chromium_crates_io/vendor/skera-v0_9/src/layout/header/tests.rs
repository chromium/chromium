use super::*;
use crate::{Subset, SubsetError, SubsetState};
use font_test_data::bebuffer::BeBuffer;
use read_fonts::{
    collections::IntSet,
    tables::{gpos::Gpos, gsub::Gsub, layout::SelectedLookupList},
    types::{GlyphId, GlyphId24, MajorMinor, Tag, Uint24},
    FontData, FontRead, FontRef,
};
use write_fonts::FontBuilder;

fn input(gpos: bool, wide: [bool; 3], large_offsets: bool) -> Vec<u8> {
    let start = if large_offsets { 70000 } else { 26 };
    let offsets = [start, start + 20, start + 34];
    let mut bytes = BeBuffer::new().push(1u16).push(2u16);
    for (offset, wide) in offsets.into_iter().zip(wide) {
        bytes = bytes.push(if wide { 1u16 } else { offset as u16 });
    }
    bytes = bytes.push(0u32);
    for (offset, wide) in offsets.into_iter().zip(wide) {
        bytes = bytes.push(if wide { offset as u32 } else { 0 });
    }
    let mut bytes = bytes.to_vec();
    bytes.resize(start, 0);
    bytes.extend(
        BeBuffer::new()
            .push(1u16)
            .push(Tag::new(b"DFLT"))
            .push(8u16)
            .push(4u16)
            .push(0u16)
            .push(0u16)
            .push(0xffffu16)
            .push(1u16)
            .push(0u16)
            .to_vec(),
    );
    bytes.extend(
        BeBuffer::new()
            .push(1u16)
            .push(if gpos {
                Tag::new(b"kern")
            } else {
                Tag::new(b"liga")
            })
            .push(8u16)
            .push(0u16)
            .push(1u16)
            .push(0u16)
            .push(1u16)
            .to_vec(),
    );
    bytes.extend(if wide[2] {
        6u32.to_be_bytes().to_vec()
    } else {
        4u16.to_be_bytes().to_vec()
    });
    bytes.extend(
        BeBuffer::new()
            .push(1u16)
            .push(0u16)
            .push(1u16)
            .push(8u16)
            .to_vec(),
    );
    if gpos {
        bytes.extend(
            BeBuffer::new()
                .push(3u16)
                .push(10u32)
                .push(4u16)
                .push(40i16)
                .to_vec(),
        );
    } else {
        bytes.extend(
            BeBuffer::new()
                .push(4u16)
                .push(12u32)
                .push(Uint24::new(1))
                .push(GlyphId24::new(70000))
                .to_vec(),
        );
    }
    bytes.extend(
        BeBuffer::new()
            .push(3u16)
            .push(Uint24::new(1))
            .push(GlyphId24::new(65536))
            .to_vec(),
    );
    bytes
}

fn plan() -> Plan {
    let mut plan = Plan {
        glyph_map_gsub: vec![crate::INVALID_GID; 70001],
        ..Default::default()
    };
    for (old, new) in [(65536, 1), (70000, 2)] {
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
    }
    plan.layout_scripts = IntSet::all();
    plan.gsub_lookups.insert(0, 0);
    plan.gpos_lookups.insert(0, 0);
    plan.gsub_features.insert(0, 0);
    plan.gpos_features.insert(0, 0);
    plan.gsub_features_w_duplicates.insert(0, 0);
    plan.gpos_features_w_duplicates.insert(0, 0);
    plan
}

fn subset(bytes: &[u8], gpos: bool) -> Result<Vec<u8>, SubsetError> {
    let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
    let plan = plan();
    let mut state = SubsetState::default();
    let mut s = Serializer::new(1024 * 1024);
    s.start_serialize().unwrap();
    let mut builder = FontBuilder::new();
    if gpos {
        Gpos::read(FontData::new(bytes))
            .unwrap()
            .subset_with_state(&plan, &font, &mut state, &mut s, &mut builder)?;
    } else {
        Gsub::read(FontData::new(bytes))
            .unwrap()
            .subset_with_state(&plan, &font, &mut state, &mut s, &mut builder)?;
    }
    s.end_serialize();
    assert!(!s.in_error());
    Ok(s.copy_bytes())
}

macro_rules! check_header {
    ($type:ident,$bytes:expr,$wide:expr) => {{
        let table = $type::read(FontData::new($bytes)).unwrap();
        let wide = $wide;
        assert_eq!(table.version(), MajorMinor::new(1, 2));
        assert_eq!(table.script_list_offset().is_null(), wide[0]);
        assert_eq!(table.feature_list_offset().is_null(), wide[1]);
        assert_eq!(table.lookup_list_offset().is_null(), wide[2]);
        assert_eq!(table.script_list2_offset().unwrap().is_null(), !wide[0]);
        assert_eq!(table.feature_list2_offset().unwrap().is_null(), !wide[1]);
        assert_eq!(table.lookup_list2_offset().unwrap().is_null(), !wide[2]);
        let scripts = table.script_list().unwrap();
        assert_eq!(scripts.script_count(), 1);
        let script = scripts.script_records()[0]
            .script(scripts.offset_data())
            .unwrap();
        assert_eq!(
            script
                .default_lang_sys()
                .unwrap()
                .unwrap()
                .feature_indices()[0]
                .get(),
            0
        );
        let features = table.feature_list().unwrap();
        assert_eq!(features.feature_count(), 1);
        let feature = features.feature_records()[0]
            .feature(features.offset_data())
            .unwrap();
        assert_eq!(feature.lookup_list_indices()[0].get(), 0);
        let lookups = table.lookup_list().unwrap();
        assert_eq!(matches!(lookups, SelectedLookupList::Extended(_)), wide[2]);
        assert_eq!(lookups.lookup_count(), 1);
        assert_eq!(lookups.lookups().get(0).unwrap().lookup_type(), 1);
        assert!(table.feature_variations().is_none());
    }};
}

#[test]
fn extended_headers_preserve_independent_list_precedence_and_fallbacks() {
    for gpos in [false, true] {
        for bits in 0..8 {
            let wide = [bits & 1 != 0, bits & 2 != 0, bits & 4 != 0];
            let bytes = subset(&input(gpos, wide, false), gpos).unwrap();
            if gpos {
                check_header!(Gpos, &bytes, wide);
            } else {
                check_header!(Gsub, &bytes, wide);
            }
        }
        let bytes = subset(&input(gpos, [true; 3], true), gpos).unwrap();
        if gpos {
            check_header!(Gpos, &bytes, [true; 3]);
        } else {
            check_header!(Gsub, &bytes, [true; 3]);
        }
    }
}

#[test]
fn extended_headers_keep_version_after_dropping_empty_variations() {
    for gpos in [false, true] {
        let mut input = input(gpos, [true; 3], false);
        let offset = input.len() as u32;
        input[10..14].copy_from_slice(&offset.to_be_bytes());
        input.extend(BeBuffer::new().push(1u16).push(0u16).push(0u32).to_vec());
        let bytes = subset(&input, gpos).unwrap();
        if gpos {
            check_header!(Gpos, &bytes, [true; 3]);
        } else {
            check_header!(Gsub, &bytes, [true; 3]);
        }
    }
}

#[test]
fn extended_headers_allow_absent_lists_but_reject_truncation_and_invalid_preferred_offsets() {
    for gpos in [false, true] {
        let mut empty = vec![0u8; 26];
        empty[..4].copy_from_slice(&MajorMinor::new(1, 2).to_be_bytes());
        assert_eq!(subset(&empty, gpos).unwrap(), empty);
        let input = input(gpos, [true; 3], false);
        assert!(subset(&input[..25], gpos).is_err());
        for range in [14..18, 18..22, 22..26] {
            let mut bytes = input.clone();
            bytes[range].fill(0xff);
            assert!(subset(&bytes, gpos).is_err());
        }
        let mut bytes = input.clone();
        bytes[10..14].fill(0xff);
        assert!(subset(&bytes, gpos).is_err());
        bytes[2..4].copy_from_slice(&3u16.to_be_bytes());
        assert!(subset(&bytes, gpos).is_err());
    }
}

#[test]
fn legacy_headers_still_serialize_version_1_0() {
    for gpos in [false, true] {
        let mut input = input(gpos, [false; 3], false);
        input[2..4].fill(0);
        let bytes = subset(&input, gpos).unwrap();
        if gpos {
            let table = Gpos::read(FontData::new(&bytes)).unwrap();
            assert_eq!(table.version(), MajorMinor::VERSION_1_0);
            assert_eq!(table.lookup_list().unwrap().lookup_count(), 1);
        } else {
            let table = Gsub::read(FontData::new(&bytes)).unwrap();
            assert_eq!(table.version(), MajorMinor::VERSION_1_0);
            assert_eq!(table.lookup_list().unwrap().lookup_count(), 1);
        }
    }
}
