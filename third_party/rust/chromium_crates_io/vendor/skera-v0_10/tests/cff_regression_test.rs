//! Exercise actual CFF outlines, independently of the legacy TTX goldens.
use skera::{subset_font, Plan, SubsetFlags, DEFAULT_LAYOUT_FEATURES};
use write_fonts::read::{
    collections::IntSet,
    model::glyph::outline::PathElement,
    ps::cff::CffFontRef,
    types::{F2Dot14, GlyphId, NameId, Tag},
    FontRef, TableProvider,
};

fn plan(font: &FontRef, flags: SubsetFlags, unicodes: &str) -> Plan {
    Plan::new(
        &IntSet::empty(),
        &skera::parse_unicodes(unicodes).unwrap(),
        font,
        flags,
        &IntSet::empty(),
        &IntSet::all(),
        &DEFAULT_LAYOUT_FEATURES.iter().copied().collect(),
        &IntSet::<NameId>::all(),
        &IntSet::all(),
    )
}

fn assert_review_outlines_preserved(filename: &str) {
    let bytes = std::fs::read(format!("test-data/fonts/{filename}")).unwrap();
    let font = FontRef::new(&bytes).unwrap();
    let tag = Tag::new(if font.cff2().is_ok() {
        b"CFF2"
    } else {
        b"CFF "
    });
    let source = CffFontRef::new(font.data_for_tag(tag).unwrap().as_bytes(), 0, None).unwrap();
    for flags in [
        SubsetFlags::default(),
        SubsetFlags::SUBSET_FLAGS_NO_HINTING,
        SubsetFlags::SUBSET_FLAGS_DESUBROUTINIZE,
        SubsetFlags::SUBSET_FLAGS_NO_HINTING | SubsetFlags::SUBSET_FLAGS_DESUBROUTINIZE,
    ] {
        let p = plan(&font, flags, "41,42");
        let output = subset_font(&font, &p).unwrap();
        let output = FontRef::new(&output).unwrap();
        let subset =
            CffFontRef::new(output.data_for_tag(tag).unwrap().as_bytes(), 0, None).unwrap();
        for coord in [F2Dot14::ZERO, F2Dot14::from_f32(0.5), F2Dot14::ONE] {
            for (old, new) in p
                .old_to_new_glyph_mapping()
                .filter(|(_, g)| g.to_u32() != 0)
            {
                let coords = [coord];
                let mut original_path = Vec::<PathElement>::new();
                let mut subset_path = Vec::<PathElement>::new();
                source
                    .draw(
                        &source.subfont(0, &coords).unwrap(),
                        old,
                        &coords,
                        None,
                        &mut original_path,
                    )
                    .unwrap();
                subset
                    .draw(
                        &subset.subfont(0, &coords).unwrap(),
                        new,
                        &coords,
                        None,
                        &mut subset_path,
                    )
                    .unwrap();
                assert_eq!(
                    original_path, subset_path,
                    "{filename} {flags:?} {old:?} {coord:?}"
                );
            }
        }
    }
}

#[test]
fn dotsection_preserves_live_operands() {
    assert_review_outlines_preserved("cff1_dotsection_live_args.otf");
}

#[test]
fn shared_mask_fallback_is_independent_of_glyph_order() {
    for file in [
        "cff1_contextual_masks_forward.otf",
        "cff1_contextual_masks_reverse.otf",
    ] {
        assert_review_outlines_preserved(file);
    }
}

#[test]
fn symbolic_blends_preserve_prefixes_and_zero_value_groups() {
    assert_review_outlines_preserved("cff2_blend_identity.otf");
}

#[test]
fn shared_hint_operands_preserve_both_glyphs() {
    let bytes = include_bytes!("../test-data/fonts/cff1_shared_hint_operands.otf");
    let font = FontRef::new(bytes).unwrap();
    let p = plan(&font, SubsetFlags::SUBSET_FLAGS_NO_HINTING, "41,42");
    let bytes = subset_font(&font, &p).unwrap();
    let out = FontRef::new(&bytes).unwrap();
    let original = CffFontRef::new(
        font.data_for_tag(Tag::new(b"CFF ")).unwrap().as_bytes(),
        0,
        None,
    )
    .unwrap();
    let subset = CffFontRef::new(
        out.data_for_tag(Tag::new(b"CFF ")).unwrap().as_bytes(),
        0,
        None,
    )
    .unwrap();
    for (old, new) in p
        .old_to_new_glyph_mapping()
        .filter(|(_, g)| g.to_u32() != 0)
    {
        let mut a = Vec::<PathElement>::new();
        let mut b = Vec::<PathElement>::new();
        original
            .draw(&original.subfont(0, &[]).unwrap(), old, &[], None, &mut a)
            .unwrap();
        subset
            .draw(&subset.subfont(0, &[]).unwrap(), new, &[], None, &mut b)
            .unwrap();
        assert_eq!(a, b);
    }
}

#[test]
fn caller_dependent_calls_fail_retained_mode_and_desubroutinize_correctly() {
    for bytes in [
        include_bytes!("../test-data/fonts/cff1_caller_subr.otf").as_slice(),
        include_bytes!("../test-data/fonts/cff2_caller_subr.otf").as_slice(),
    ] {
        let font = FontRef::new(bytes).unwrap();
        assert!(subset_font(&font, &plan(&font, SubsetFlags::default(), "41,42")).is_err());
        let p = plan(&font, SubsetFlags::SUBSET_FLAGS_DESUBROUTINIZE, "41,42");
        let output = subset_font(&font, &p).unwrap();
        let subset = FontRef::new(&output).unwrap();
        let tag = if font.cff2().is_ok() {
            Tag::new(b"CFF2")
        } else {
            Tag::new(b"CFF ")
        };
        let cff = CffFontRef::new(subset.data_for_tag(tag).unwrap().as_bytes(), 0, None).unwrap();
        let original =
            CffFontRef::new(font.data_for_tag(tag).unwrap().as_bytes(), 0, None).unwrap();
        for (old, new) in p
            .old_to_new_glyph_mapping()
            .filter(|(_, g)| g.to_u32() != 0)
        {
            let mut a = Vec::<PathElement>::new();
            let mut b = Vec::<PathElement>::new();
            original
                .draw(&original.subfont(0, &[]).unwrap(), old, &[], None, &mut a)
                .unwrap();
            cff.draw(&cff.subfont(0, &[]).unwrap(), new, &[], None, &mut b)
                .unwrap();
            assert_eq!(a, b);
        }
    }
}

#[test]
fn long_private_blends_and_fd_independent_globals_survive() {
    for filename in ["cff2_long_private_blends.otf", "cff2_shared_global.otf"] {
        let bytes = std::fs::read(format!("test-data/fonts/{filename}")).unwrap();
        let font = FontRef::new(&bytes).unwrap();
        let p = plan(&font, SubsetFlags::default(), "41,42");
        let output = subset_font(&font, &p).unwrap();
        let subset = FontRef::new(&output).unwrap();
        let cff = CffFontRef::new(
            subset.data_for_tag(Tag::new(b"CFF2")).unwrap().as_bytes(),
            0,
            None,
        )
        .unwrap();
        if filename == "cff2_shared_global.otf" {
            assert_eq!(cff.global_subrs().count(), 1);
            assert_ne!(
                cff.subfont_index(GlyphId::new(1)),
                cff.subfont_index(GlyphId::new(2))
            );
        }
        for (_, gid) in p
            .old_to_new_glyph_mapping()
            .filter(|(_, g)| g.to_u32() != 0)
        {
            let sf = cff.subfont(cff.subfont_index(gid).unwrap(), &[]).unwrap();
            cff.draw(&sf, gid, &[], None, &mut Vec::<PathElement>::new())
                .unwrap();
        }
    }
}
