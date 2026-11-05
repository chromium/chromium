use super::*;
use crate::{subset_font, IntSet};
use font_test_data::bebuffer::BeBuffer;
use read_fonts::{
    types::{Tag, Uint24},
    FontData, FontRead,
};

const GLYF: Tag = Tag::new(b"GLYF");
const LOCA: Tag = Tag::new(b"LOCA");
const MAXP: Tag = Tag::new(b"MAXP");

fn simple() -> Vec<u8> {
    BeBuffer::new()
        .push(1i16)
        .extend([0i16; 4])
        .push(3u16)
        .push(2u16)
        .extend([0u8; 2])
        .extend([0x31u8, 0xb0, 0xb0, 0x31])
        .to_vec()
}

fn composite() -> Vec<u8> {
    BeBuffer::new()
        .push(-1i16)
        .extend([0i16; 4])
        .push(
            (CompositeGlyphFlags::GID_IS_24_BIT
                | CompositeGlyphFlags::MORE_COMPONENTS
                | CompositeGlyphFlags::ARGS_ARE_XY_VALUES
                | CompositeGlyphFlags::WE_HAVE_INSTRUCTIONS)
                .bits(),
        )
        .push(Uint24::new(65536))
        .extend([0i8; 2])
        .push(CompositeGlyphFlags::ARGS_ARE_XY_VALUES.bits())
        .push(2u16)
        .extend([0i8; 2])
        .push(2u16)
        .extend([0u8; 2])
        .to_vec()
}

fn source(hybrid: bool) -> Vec<u8> {
    let simple = simple();
    let mut glyf = simple.clone();
    glyf.extend(&simple);
    glyf.extend(composite());
    let mut loca = vec![0u32; 70003];
    loca[3..=65536].fill(simple.len() as u32);
    loca[65537..=70001].fill((simple.len() * 2) as u32);
    loca[70002] = glyf.len() as u32;
    let loca = loca
        .into_iter()
        .flat_map(u32::to_be_bytes)
        .collect::<Vec<_>>();
    let mut builder = FontBuilder::new();
    builder.add_raw(GLYF, glyf);
    builder.add_raw(LOCA, loca);
    builder.add_raw(
        MAXP,
        BeBuffer::new()
            .push(0x10000u32)
            .push(Uint24::new(70002))
            .extend([0u16; 13])
            .to_vec(),
    );
    let fixture = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
    let mut head = fixture.head().unwrap().offset_data().as_bytes().to_vec();
    head[50..52].copy_from_slice(&1i16.to_be_bytes());
    builder.add_raw(Head::TAG, head);
    if hybrid {
        // These deliberately disagree with GLYF/LOCA and must not be selected.
        builder.add_raw(Glyf::TAG, &[0u8][..]);
        builder.add_raw(Loca::TAG, &[0u8; 8][..]);
        builder.add_raw(Tag::new(b"maxp"), &[0, 0, 0x50, 0, 0, 1][..]);
    }
    builder.build()
}

#[test]
fn extended_composites_remap_mixed_width_ids_and_keep_instructions() {
    let bytes = composite();
    let glyph = Glyph::read(FontData::new(&bytes)).unwrap();
    for no_hinting in [false, true] {
        let plan = Plan {
            glyph_map: [
                (GlyphId::new(65536), GlyphId::new(70000)),
                (GlyphId::new(2), GlyphId::new(3)),
            ]
            .into_iter()
            .collect(),
            subset_flags: if no_hinting {
                SubsetFlags::SUBSET_FLAGS_NO_HINTING | SubsetFlags::SUBSET_FLAGS_SET_OVERLAPS_FLAG
            } else {
                SubsetFlags::SUBSET_FLAGS_DEFAULT
            },
            ..Default::default()
        };
        let subset = SubsetGlyph::new(&glyph, &plan);
        assert!(subset.len() > 0);
        let mut s = Serializer::new(1024);
        s.start_serialize().unwrap();
        subset.serialize(&mut s, &plan).unwrap();
        s.end_serialize();
        let bytes = s.copy_bytes();
        let Glyph::Composite(glyph) = Glyph::read(FontData::new(&bytes)).unwrap() else {
            panic!()
        };
        let components = glyph.components().collect::<Vec<_>>();
        assert_eq!(
            components
                .iter()
                .map(|c| c.glyph.to_u32())
                .collect::<Vec<_>>(),
            [70000, 3]
        );
        assert!(components[0]
            .flags
            .contains(CompositeGlyphFlags::GID_IS_24_BIT));
        assert!(!components[1]
            .flags
            .contains(CompositeGlyphFlags::GID_IS_24_BIT));
        assert_eq!(
            components[0]
                .flags
                .contains(CompositeGlyphFlags::OVERLAP_COMPOUND),
            no_hinting
        );
        assert_eq!(
            glyph.instructions(),
            if no_hinting {
                None
            } else {
                Some(&[0u8; 2][..])
            }
        );
    }
}

#[test]
fn extended_outlines_close_components_and_preserve_cubic_flags() {
    for hybrid in [false, true] {
        for retain_ids in [false, true] {
            for no_hinting in [false, true] {
                let bytes = source(hybrid);
                let font = FontRef::new(&bytes).unwrap();
                let mut flags = SubsetFlags::SUBSET_FLAGS_DEFAULT;
                if retain_ids {
                    flags |= SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS;
                }
                if no_hinting {
                    flags |= SubsetFlags::SUBSET_FLAGS_NO_HINTING;
                }
                let plan = Plan::new(
                    &[GlyphId::new(70001)].into_iter().collect(),
                    &IntSet::empty(),
                    &font,
                    flags,
                    &Default::default(),
                    &Default::default(),
                    &Default::default(),
                    &Default::default(),
                    &Default::default(),
                );
                assert_eq!(
                    plan.glyphset
                        .iter()
                        .map(GlyphId::to_u32)
                        .collect::<Vec<_>>(),
                    [0, 2, 65536, 70001]
                );
                let bytes = subset_font(&font, &plan).unwrap();
                let output = FontRef::new(&bytes).unwrap();
                assert!(output.data_for_tag(Glyf::TAG).is_none());
                assert!(output.data_for_tag(Loca::TAG).is_none());
                let (glyf, loca) = output.selected_glyf_loca(None).unwrap();
                let expected = if retain_ids {
                    [2, 65536, 70001]
                } else {
                    [1, 2, 3]
                };
                assert_eq!(loca.len(), if retain_ids { 70002 } else { 4 });
                assert_eq!(
                    output.selected_maxp().unwrap().num_glyphs() as usize,
                    loca.len()
                );
                assert!(loca.all_offsets_are_ascending());
                let Some(Glyph::Composite(glyph)) = loca
                    .get(GlyphId::new(expected[2]), &glyf)
                    .and_then(|g| g.into_glyph())
                else {
                    panic!()
                };
                assert_eq!(
                    glyph
                        .components()
                        .map(|c| c.glyph.to_u32())
                        .collect::<Vec<_>>(),
                    [expected[1], expected[0]]
                );
                assert_eq!(glyph.instructions().is_none(), no_hinting);
                for gid in &expected[..2] {
                    let Some(Glyph::Simple(glyph)) = loca
                        .get(GlyphId::new(*gid), &glyf)
                        .and_then(|g| g.into_glyph())
                    else {
                        panic!()
                    };
                    assert_eq!(
                        glyph.points().map(|p| p.cubic).collect::<Vec<_>>(),
                        [false, true, true, false]
                    );
                    assert_eq!(glyph.instruction_length(), if no_hinting { 0 } else { 2 });
                }
            }
        }
    }
}

#[test]
fn extended_composites_reject_unrepresentable_remapped_ids() {
    let bytes = composite();
    let glyph = Glyph::read(FontData::new(&bytes)).unwrap();
    for (wide, narrow) in [(0x1000000, 3), (70000, 65536)] {
        let plan = Plan {
            glyph_map: [
                (GlyphId::new(65536), GlyphId::new(wide)),
                (GlyphId::new(2), GlyphId::new(narrow)),
            ]
            .into_iter()
            .collect(),
            ..Default::default()
        };
        let subset = SubsetGlyph::new(&glyph, &plan);
        let mut s = Serializer::new(1024);
        s.start_serialize().unwrap();
        assert!(subset.serialize(&mut s, &plan).is_err());
    }
}

#[test]
fn extended_outlines_require_their_own_location_table() {
    for hybrid in [false, true] {
        let bytes = source(hybrid);
        let font = FontRef::new(&bytes).unwrap();
        let mut builder = FontBuilder::new();
        for record in font.table_directory().table_records() {
            if record.tag() != LOCA {
                builder.add_raw(
                    record.tag(),
                    font.data_for_tag(record.tag()).unwrap().as_bytes(),
                );
            }
        }
        let bytes = builder.build();
        let font = FontRef::new(&bytes).unwrap();
        let plan = Plan::new(
            &[GlyphId::new(70001)].into_iter().collect(),
            &IntSet::empty(),
            &font,
            SubsetFlags::SUBSET_FLAGS_DEFAULT,
            &Default::default(),
            &Default::default(),
            &Default::default(),
            &Default::default(),
            &Default::default(),
        );
        assert!(matches!(
            subset_font(&font, &plan),
            Err(SubsetError::SubsetTableError(GLYF))
        ));
    }
}
