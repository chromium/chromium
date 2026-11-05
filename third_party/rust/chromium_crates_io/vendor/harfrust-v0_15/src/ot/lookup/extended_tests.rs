//! Tests of the extended layout formats of ISO/IEC 14496-22:2026: 24-bit
//! glyph IDs, coverage indices, counts and offsets, through the cached
//! lookups that apply them.

use super::build::build_lookup_at;
use super::coverage::{ClassDef, Coverage};
use super::{coverage_index_cached, glyph_class_cached, CachedLookup};
use crate::aat::EMPTY_AAT_DATA;
use crate::budget::Budget;
use crate::buffer::GlyphPropsFlags;
use crate::ot::apply::{ApplyContext, MappingCache, WouldApplyContext};
use crate::ot::layout::LayoutTableKind;
use crate::ot::{gpos, lookup_flags, map, CachedGdef, MarkGlyphSetBitmap, OtData, EMPTY_OT_DATA};
use crate::set_digest::SetDigest;
use crate::shaper_font::LayoutCache;
use crate::{Buffer, Direction, Font, LayoutData, Scale};
use alloc::sync::Arc;
use alloc::vec;
use alloc::vec::Vec;
use read_fonts::model::{Blob, TableFunction};
use read_fonts::tables::gdef::Gdef;
use read_fonts::tables::layout::{self, CoverageTable};
use read_fonts::types::{GlyphId, Tag, Uint24};
use read_fonts::{FontData, FontRead};

/// A lookup of `lookup_type` holding `subtable` alone.
fn lookup_bytes(lookup_type: u16, subtable: &[u8]) -> Vec<u8> {
    let mut bytes = vec![0; 8];
    bytes[..2].copy_from_slice(&lookup_type.to_be_bytes());
    bytes[4..6].copy_from_slice(&1u16.to_be_bytes());
    bytes[6..8].copy_from_slice(&8u16.to_be_bytes());
    bytes.extend_from_slice(subtable);
    bytes
}

fn apply_subtable(lookup_type: u16, is_subst: bool, subtable: &[u8], glyphs: &[u32]) -> Buffer {
    apply_subtable_configured(lookup_type, is_subst, subtable, glyphs, |_| {})
}

fn apply_subtable_configured(
    lookup_type: u16,
    is_subst: bool,
    subtable: &[u8],
    glyphs: &[u32],
    configure: impl FnOnce(&mut ApplyContext),
) -> Buffer {
    let bytes = lookup_bytes(lookup_type, subtable);
    let lookup = build_lookup_at(
        FontData::new(&bytes),
        0,
        is_subst,
        &Budget::for_table_bytes(bytes.len()),
    )
    .unwrap();
    assert_eq!(lookup.subtables.len(), 1);
    let layout = LayoutData {
        ot: &EMPTY_OT_DATA,
        aat: &EMPTY_AAT_DATA,
        units_per_em: 1000,
        apply_trak: false,
    };
    apply_lookup_configured(&lookup, layout, glyphs, configure, false)
}

/// Whether a lookup holding `subtable` alone would apply to `glyphs`.
fn would_apply(lookup_type: u16, subtable: &[u8], glyphs: &[u32], zero_context: bool) -> bool {
    let bytes = lookup_bytes(lookup_type, subtable);
    let lookup = build_lookup_at(
        FontData::new(&bytes),
        0,
        true,
        &Budget::for_table_bytes(bytes.len()),
    )
    .unwrap();
    let glyphs: Vec<_> = glyphs.iter().copied().map(GlyphId::new).collect();
    lookup.would_apply(&WouldApplyContext {
        glyphs: &glyphs,
        zero_context,
    }) == Some(true)
}

fn apply_lookup_configured(
    lookup: &CachedLookup,
    layout: LayoutData,
    glyphs: &[u32],
    configure: impl FnOnce(&mut ApplyContext),
    use_hot_cache: bool,
) -> Buffer {
    let is_subst = lookup.is_subst;
    let mut buffer = Buffer::new();
    assert!(buffer.set_length(glyphs.len()));
    buffer.set_direction(Direction::LeftToRight);
    buffer.allocate_gsubgpos_vars();
    for (info, glyph) in buffer.glyph_infos_mut().iter_mut().zip(glyphs) {
        info.glyph_id = *glyph;
        info.mask = 1;
    }
    if is_subst && !lookup.is_reverse() {
        buffer.clear_output();
    } else if !is_subst {
        buffer.clear_positions();
    }
    // Messages need a font, though nothing listens for them here.
    #[cfg(feature = "tracing")]
    let font = Font::new(
        include_bytes!("../../../tests/fonts/text-rendering-tests/TestMORXOne.ttf").to_vec(),
        0,
    )
    .unwrap();
    #[cfg(feature = "tracing")]
    let shaper_font = crate::ShaperFont::new(&font);
    let mut ctx = ApplyContext::new(
        if is_subst {
            LayoutTableKind::Gsub
        } else {
            LayoutTableKind::Gpos
        },
        layout,
        Scale::default(),
        &mut buffer,
        #[cfg(feature = "tracing")]
        &shaper_font,
    );
    ctx.lookup_props = lookup.props();
    configure(&mut ctx);
    ctx.update_matchers();
    let use_hot_cache = use_hot_cache && lookup.cache_enter(&mut ctx);
    if lookup.is_reverse() {
        for index in (0..ctx.buffer.len).rev() {
            ctx.buffer.idx = index;
            lookup.apply(&mut ctx, use_hot_cache);
        }
    } else {
        while ctx.buffer.idx < ctx.buffer.len {
            if lookup.apply(&mut ctx, use_hot_cache).is_none() {
                ctx.buffer.next_glyph();
            }
        }
    }
    if use_hot_cache {
        lookup.cache_leave(&mut ctx);
    }
    if is_subst && !lookup.is_reverse() {
        assert!(buffer.sync());
    }
    buffer
}

fn apply_nested_context(
    lookup_type: u16,
    is_subst: bool,
    subtable: &[u8],
    glyphs: &[u32],
) -> Buffer {
    apply_nested_context_cached(lookup_type, is_subst, subtable, glyphs, false)
}

/// Applies a context subtable as lookup 0 of a GSUB or GPOS table whose
/// lookup 1 substitutes glyph 65537 by 65538, or adds 10 to its advance.
fn apply_nested_context_cached(
    lookup_type: u16,
    is_subst: bool,
    subtable: &[u8],
    glyphs: &[u32],
    use_hot_cache: bool,
) -> Buffer {
    let mut target = vec![0, 1, 0, 0, 0, 1, 0, 8];
    if is_subst {
        target.extend_from_slice(&[0, 3, 0, 0, 0, 9, 0, 0, 1]);
    } else {
        target.extend_from_slice(&[0, 3, 0, 0, 0, 10, 0, 4, 0, 10]);
    }
    target.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 1]);

    let mut bytes = vec![0, 1, 0, 0, 0, 0, 0, 0, 0, 10, 0, 2];
    bytes.extend_from_slice(&(6 + target.len() as u16).to_be_bytes());
    bytes.extend_from_slice(&6u16.to_be_bytes());
    bytes.extend_from_slice(&target);
    bytes.extend_from_slice(&lookup_type.to_be_bytes());
    bytes.extend_from_slice(&[0, 0, 0, 1, 0, 8]);
    bytes.extend_from_slice(subtable);

    let tag = Tag::new(if is_subst { b"GSUB" } else { b"GPOS" });
    let table = Blob::from(bytes);
    let font = Font::new(
        TableFunction::new(Arc::new(move |requested| {
            (requested == tag).then(|| table.clone())
        })),
        0,
    )
    .unwrap();
    let cache = LayoutCache::new(&font);
    let ot = OtData::new(&cache.ot, &[], layout::SelectedFeatureVariations::default());
    let lookups = if is_subst {
        ot.gsub().unwrap().lookups
    } else {
        ot.gpos().unwrap().lookups
    };
    let lookup = lookups.get(0).unwrap();
    assert_eq!(lookup.subtables.len(), 1);
    let layout = LayoutData {
        ot: &ot,
        aat: &EMPTY_AAT_DATA,
        units_per_em: 1000,
        apply_trak: false,
    };
    apply_lookup_configured(lookup, layout, glyphs, |_| {}, use_hot_cache)
}

fn coverage(bytes: &[u8]) -> Coverage<'_> {
    Coverage::new(CoverageTable::read(FontData::new(bytes)))
}

fn class_def(bytes: &[u8]) -> ClassDef<'_> {
    ClassDef::new(layout::ClassDef::read(FontData::new(bytes)))
}

#[test]
fn wide_coverage_indices_and_counts_are_not_truncated() {
    let bytes = [0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 2, 1, 0, 0];
    let coverage = coverage(&bytes);
    assert_eq!(coverage.get(GlyphId::new(65538)), Some(65538));
    assert_eq!(coverage.get(GlyphId::new(2)), None);

    let mut bytes = vec![0, 3, 1, 0, 1];
    for gid in 0..=65536 {
        bytes.extend_from_slice(&Uint24::new(gid).to_be_bytes());
    }
    let coverage = self::coverage(&bytes);
    assert_eq!(coverage.get(GlyphId::new(65536)), Some(65536));
}

#[test]
fn wide_classes_do_not_alias_narrow_classes() {
    let bytes = [0, 3, 1, 0, 0, 0, 0, 2, 1, 0, 1, 0xFF, 0xFF, 0xFF];
    let class_def = class_def(&bytes);
    assert_eq!(class_def.get(GlyphId::new(65536)), 65537);
    assert_eq!(class_def.get(GlyphId::new(65537)), 0x00FF_FFFF);
    assert_eq!(class_def.get(GlyphId::new(0)), 0);

    let bytes = [0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 2, 0xFF, 0xFF];
    let class_def = self::class_def(&bytes);
    assert_eq!(class_def.get(GlyphId::new(65538)), 65535);
    assert_eq!(class_def.get(GlyphId::new(2)), 0);
}

#[test]
fn wide_values_bypass_compact_mapping_caches() {
    let cache = MappingCache::new();
    let glyph = GlyphId::new(7);
    let wide = 65537;
    assert_eq!(
        coverage_index_cached(|_| Some(wide), glyph, &cache),
        Some(wide)
    );
    assert_eq!(cache.get(glyph.into()), None);
    assert_eq!(coverage_index_cached(|_| None, glyph, &cache), None);
    assert_eq!(cache.get(glyph.into()), Some(MappingCache::MAX_VALUE));

    // Glyph 7 in class 65537, then in class 1, and glyph 65543 in class 2.
    let wide_class = [0, 3, 0, 0, 7, 0, 0, 1, 1, 0, 1];
    let narrow_class = [0, 1, 0, 7, 0, 1, 0, 1];
    let wide_glyph = [0, 3, 1, 0, 7, 0, 0, 1, 0, 0, 2];
    let cache = MappingCache::new();
    assert_eq!(
        glyph_class_cached(&class_def(&wide_class), glyph, &cache),
        wide
    );
    assert_eq!(cache.get(glyph.into()), None);
    assert_eq!(
        glyph_class_cached(&class_def(&narrow_class), glyph, &cache),
        1
    );
    assert_eq!(cache.get(glyph.into()), Some(1));
    let wide_gid = GlyphId::new(65543);
    assert_eq!(
        glyph_class_cached(&class_def(&wide_glyph), wide_gid, &cache),
        2
    );
    assert_eq!(cache.get(glyph.into()), Some(1));
}

#[test]
fn digests_and_mark_bitmaps_cover_wide_glyphs() {
    for bytes in [
        &[0, 3, 0, 0, 2, 1, 0, 0, 1, 0, 2][..],
        &[0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 2, 0, 0, 0][..],
    ] {
        let coverage = CoverageTable::read(FontData::new(bytes)).unwrap();
        let digest = SetDigest::from_coverage(&coverage);
        let budget = Budget::for_table_bytes(bytes.len());
        let cached_digest = Coverage::new(Ok(coverage.clone())).digest(&budget).unwrap();
        let bitmap = MarkGlyphSetBitmap::from_coverage(&coverage, &budget).unwrap();
        assert!(matches!(
            bitmap,
            MarkGlyphSetBitmap::Page { bias: 65536, .. }
        ));
        for glyph in coverage.iter() {
            assert!(digest.may_have(glyph.to_u32()));
            assert!(cached_digest.may_have(glyph.to_u32()));
            assert_eq!(bitmap.covers(glyph.to_u32()), Some(true));
        }
        assert_eq!(bitmap.covers(0), Some(false));
        assert_eq!(bitmap.covers(65539), Some(false));
    }
}

#[test]
fn gdef_14_caches_resolved_wide_tables() {
    let mut bytes = vec![0; 65536];
    bytes[..4].copy_from_slice(&[0, 1, 0, 4]);
    bytes[18..22].copy_from_slice(&65536u32.to_be_bytes());
    bytes.extend_from_slice(&[0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 2, 0, 3]);
    let mark_class_offset = bytes.len() as u32;
    bytes[30..34].copy_from_slice(&mark_class_offset.to_be_bytes());
    bytes.extend_from_slice(&[0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 2, 0, 7]);
    let mark_sets_offset = bytes.len() as u32;
    bytes[34..38].copy_from_slice(&mark_sets_offset.to_be_bytes());
    bytes.extend_from_slice(&[0, 1, 0, 1, 0, 0, 0, 8]);
    bytes.extend_from_slice(&[0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 2, 0, 0, 0]);
    let gdef = Gdef::read(FontData::new(&bytes)).unwrap();
    let cache = CachedGdef::new(&gdef, &Budget::for_table_bytes(bytes.len()));
    assert_eq!(cache.classes.get(GlyphId::new(65538)), 3);
    assert_eq!(cache.mark_classes.get(GlyphId::new(65538)), 7);
    assert_eq!(cache.mark_sets.len(), 1);
    assert_eq!(cache.mark_sets[0].bitmap.covers(65538), Some(true));
    assert!(cache.mark_sets[0].covers(65538));
}

#[test]
fn truncated_wide_arrays_do_not_match() {
    for bytes in [&[0, 3, 0, 0, 1][..], &[0, 4, 0, 0, 1][..]] {
        assert_eq!(coverage(bytes).get(GlyphId::new(0)), None);
    }
    for bytes in [&[0, 3, 1, 0, 0, 0, 0, 1][..], &[0, 4, 0, 0, 1][..]] {
        assert_eq!(class_def(bytes).get(GlyphId::new(65536)), 0);
    }
}

#[test]
fn overflowing_original_coverage_indices_do_not_match() {
    let bytes = [0, 2, 0, 1, 0, 0, 0, 2, 0xFF, 0xFF];
    let coverage = coverage(&bytes);
    assert_eq!(coverage.get(GlyphId::new(0)), Some(65535));
    assert_eq!(coverage.get(GlyphId::new(2)), None);
}

#[test]
fn single_subst3_wraps_signed_deltas_in_24_bits() {
    use read_fonts::types::Int24;
    for (glyph, delta, expected) in [
        (65535, 1, 65536),
        (65536, -1, 65535),
        (0x00FF_FFFF, 1, 0),
        (0, -1, 0x00FF_FFFF),
    ] {
        let mut subtable = vec![0, 3, 0, 0, 0, 9];
        subtable.extend_from_slice(&Int24::new(delta).to_be_bytes());
        subtable.extend_from_slice(&[0, 3, 0, 0, 1]);
        subtable.extend_from_slice(&Uint24::new(glyph).to_be_bytes());
        let output = apply_subtable(1, true, &subtable, &[glyph]);
        assert_eq!(output.glyph_infos()[0].glyph_id, expected);
        assert!(would_apply(1, &subtable, &[glyph], false));
    }
}

#[test]
fn single_subst4_keeps_wide_coverage_indices_and_replacements() {
    let mut subtable = vec![0, 4, 0, 0, 0, 12, 0, 0, 1, 2, 0, 1];
    subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 0]);
    let output = apply_subtable(1, true, &subtable, &[65536, 1]);
    assert_eq!(
        output
            .glyph_infos()
            .iter()
            .map(|info| info.glyph_id)
            .collect::<Vec<_>>(),
        [131_073, 1]
    );

    // The selected replacement is at index 65536, not index zero.
    let count = 65537;
    subtable = vec![0, 4];
    subtable.extend_from_slice(&(9 + count * 3u32).to_be_bytes());
    subtable.extend_from_slice(&Uint24::new(count).to_be_bytes());
    for replacement in 0..count {
        subtable.extend_from_slice(&Uint24::new(replacement + 1).to_be_bytes());
    }
    subtable.extend_from_slice(&[0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0]);
    let output = apply_subtable(1, true, &subtable, &[65536]);
    assert_eq!(output.glyph_infos()[0].glyph_id, 65537);
}

#[test]
fn extension_lookup_applies_wide_single_substitution() {
    let mut subtable = vec![0, 1, 0, 1, 0, 0, 0, 8];
    subtable.extend_from_slice(&[0, 3, 0, 0, 0, 9, 0, 0, 1]);
    subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 0]);
    let output = apply_subtable(7, true, &subtable, &[65536]);
    assert_eq!(output.glyph_infos()[0].glyph_id, 65537);
}

#[test]
fn multiple_subst2_preserves_expansion_and_single_glyph_behavior() {
    for replacements in [&[65537, 131_073][..], &[131_073][..], &[][..]] {
        let mut subtable = vec![0, 2, 0, 0, 0, 12, 0, 0, 1, 0, 0, 20];
        subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 0]);
        subtable.extend_from_slice(&(replacements.len() as u16).to_be_bytes());
        for glyph in replacements {
            subtable.extend_from_slice(&Uint24::new(*glyph).to_be_bytes());
        }
        let output = apply_subtable(2, true, &subtable, &[65536, 1]);
        let mut expected = replacements.to_vec();
        expected.push(1);
        assert_eq!(
            output
                .glyph_infos()
                .iter()
                .map(|info| info.glyph_id)
                .collect::<Vec<_>>(),
            expected
        );
        if replacements.len() > 1 {
            assert!(output.glyph_infos()[0].multiplied());
            assert!(output.glyph_infos()[1].multiplied());
            assert_eq!(output.glyph_infos()[0].lig_comp(), 0);
            assert_eq!(output.glyph_infos()[1].lig_comp(), 1);
        } else if replacements.len() == 1 {
            assert!(!output.glyph_infos()[0].multiplied());
        }
    }
}

#[test]
fn multiple_subst2_resolves_large_sequence_offsets() {
    let mut subtable = vec![0, 2, 0, 0, 0, 12, 0, 0, 1, 1, 0, 0];
    subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 0]);
    subtable.resize(65536, 0);
    subtable.extend_from_slice(&[0, 1, 2, 0, 1]);
    let output = apply_subtable(2, true, &subtable, &[65536]);
    assert_eq!(output.glyph_infos()[0].glyph_id, 131_073);
}

#[test]
fn alternate_subst2_uses_feature_values_to_select_wide_glyphs() {
    let subtable = [
        0, 2, 0, 0, 0, 12, 0, 0, 1, 0, 0, 20, // Header and set offset.
        0, 3, 0, 0, 1, 1, 0, 0, // Coverage.
        0, 2, 1, 0, 1, 2, 0, 1, // Two alternates.
    ];
    for (value, expected) in [(0, 65536), (1, 65537), (2, 131_073), (3, 65536)] {
        let output = apply_subtable_configured(3, true, &subtable, &[65536], |ctx| {
            ctx.set_lookup_mask(3);
            ctx.buffer.glyph_infos_mut()[0].mask = value;
        });
        assert_eq!(output.glyph_infos()[0].glyph_id, expected);
    }

    let output = apply_subtable_configured(3, true, &subtable, &[65536], |ctx| {
        ctx.set_lookup_mask(map::OtMap::MAX_VALUE);
        ctx.buffer.glyph_infos_mut()[0].mask = map::OtMap::MAX_VALUE;
        ctx.random = true;
    });
    // The initial random state yields 48271, so choose the second alternate.
    assert_eq!(output.glyph_infos()[0].glyph_id, 131_073);
}

#[test]
fn alternate_subst2_resolves_large_set_offsets() {
    let mut subtable = vec![0, 2, 0, 0, 0, 12, 0, 0, 1, 1, 0, 0];
    subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 0]);
    subtable.resize(65536, 0);
    subtable.extend_from_slice(&[0, 1, 2, 0, 1]);
    let output = apply_subtable(3, true, &subtable, &[65536]);
    assert_eq!(output.glyph_infos()[0].glyph_id, 131_073);
}

#[test]
fn reverse_chain_subst2_matches_wide_backtrack_and_lookahead() {
    let subtable = [
        0, 2, 0, 0, 0, 22, // Header and coverage offset.
        0, 1, 0, 0, 30, // Backtrack coverage.
        0, 1, 0, 0, 38, // Lookahead coverage.
        0, 0, 1, 2, 0, 1, // One replacement.
        0, 3, 0, 0, 1, 1, 0, 0, // Input glyph.
        0, 3, 0, 0, 1, 1, 0, 1, // Backtrack glyph.
        0, 3, 0, 0, 1, 1, 0, 2, // Lookahead glyph.
    ];
    for (glyphs, expected) in [
        ([65537, 65536, 65538], [65537, 131_073, 65538]),
        ([1, 65536, 65538], [1, 65536, 65538]),
        ([65537, 65536, 1], [65537, 65536, 1]),
    ] {
        let output = apply_subtable(8, true, &subtable, &glyphs);
        assert_eq!(
            output
                .glyph_infos()
                .iter()
                .map(|info| info.glyph_id)
                .collect::<Vec<_>>(),
            expected
        );
    }

    let mut extension = vec![0, 1, 0, 8, 0, 0, 0, 8];
    extension.extend_from_slice(&subtable);
    let output = apply_subtable(7, true, &extension, &[65537, 65536, 65538]);
    assert_eq!(output.glyph_infos()[1].glyph_id, 131_073);
}

#[test]
fn reverse_chain_subst2_applies_in_reverse_order() {
    let subtable = [
        0, 2, 0, 0, 0, 19, // Header and coverage offset.
        0, 0, // No backtrack glyphs.
        0, 1, 0, 0, 19, // Lookahead uses the same coverage.
        0, 0, 1, 1, 0, 1, // One replacement.
        0, 3, 0, 0, 1, 1, 0, 0, // Coverage.
    ];
    let output = apply_subtable(8, true, &subtable, &[65536, 65536, 65536]);
    assert_eq!(
        output
            .glyph_infos()
            .iter()
            .map(|info| info.glyph_id)
            .collect::<Vec<_>>(),
        [65536, 65537, 65536]
    );
}

#[test]
fn ligature_subst2_matches_wide_components_and_results() {
    let subtable = [
        0, 2, 0, 0, 0, 12, 0, 0, 1, 0, 0, 20, // Header and set offset.
        0, 3, 0, 0, 1, 1, 0, 0, // Coverage.
        0, 2, 0, 0, 8, 0, 0, 16, // Two ligature offsets.
        1, 0, 1, 0, 2, 1, 0, 2, // First ligature.
        2, 0, 1, 0, 2, 1, 0, 3, // Second ligature.
    ];
    for (input, expected) in [
        ([65536, 65538], vec![65537]),
        ([65536, 65539], vec![131_073]),
        ([65536, 65540], vec![65536, 65540]),
    ] {
        let output = apply_subtable(4, true, &subtable, &input);
        assert_eq!(
            output
                .glyph_infos()
                .iter()
                .map(|info| info.glyph_id)
                .collect::<Vec<_>>(),
            expected
        );
    }
    assert!(would_apply(4, &subtable, &[65536, 65539], false));
    assert!(!would_apply(4, &subtable, &[65536, 3], false));
}

#[test]
fn ligature_subst2_resolves_large_ligature_offsets() {
    let mut subtable = vec![0, 2, 0, 0, 0, 12, 0, 0, 1, 0, 0, 20];
    subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 0]);
    subtable.extend_from_slice(&[0, 1, 1, 0, 0]);
    subtable.resize(20 + 65536, 0);
    subtable.extend_from_slice(&[2, 0, 1, 0, 2, 1, 0, 2]);
    let output = apply_subtable(4, true, &subtable, &[65536, 65538]);
    assert_eq!(output.glyph_infos().len(), 1);
    assert_eq!(output.glyph_infos()[0].glyph_id, 131_073);
}

#[test]
fn ligature_subst2_keeps_wide_set_counts_and_coverage_indices() {
    let count = 65537;
    let coverage_offset = 9 + count * 3u32;
    let mut subtable = vec![0, 2];
    subtable.extend_from_slice(&coverage_offset.to_be_bytes());
    subtable.extend_from_slice(&Uint24::new(count).to_be_bytes());
    subtable.resize(coverage_offset as usize - 3, 0);
    subtable.extend_from_slice(&Uint24::new(coverage_offset + 14).to_be_bytes());
    subtable.extend_from_slice(&[0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0]);
    subtable.extend_from_slice(&[0, 1, 0, 0, 5, 2, 0, 1, 0, 2, 1, 0, 2]);
    let output = apply_subtable(4, true, &subtable, &[65536, 65538]);
    assert_eq!(output.glyph_infos().len(), 1);
    assert_eq!(output.glyph_infos()[0].glyph_id, 131_073);
}

#[test]
fn single_pos3_applies_scaled_values_in_both_directions() {
    let subtable = [
        0, 3, 0, 0, 0, 16, 0, 15, // Header and value format.
        0, 10, 0xFF, 0xEC, 0, 30, 0xFF, 0xD8, // Placement and advances.
        0, 3, 0, 0, 1, 1, 0, 0, // Coverage.
    ];
    for (direction, expected) in [
        (Direction::LeftToRight, [20, -10, 60, 0]),
        (Direction::TopToBottom, [20, -10, 0, 20]),
    ] {
        let output = apply_subtable_configured(1, false, &subtable, &[65536, 1], |ctx| {
            ctx.buffer.set_direction(direction);
            ctx.scale = Scale::new(Some((2000, 500)), 1000);
        });
        let pos = &output.glyph_positions()[0];
        assert_eq!(
            [pos.x_offset, pos.y_offset, pos.x_advance, pos.y_advance],
            expected
        );
        let pos = &output.glyph_positions()[1];
        assert_eq!(
            [pos.x_offset, pos.y_offset, pos.x_advance, pos.y_advance],
            [0; 4]
        );
    }
}

#[test]
fn single_pos4_keeps_wide_record_counts_and_indices() {
    let count = 65537;
    let mut subtable = vec![0, 4];
    subtable.extend_from_slice(&(11 + count * 2u32).to_be_bytes());
    subtable.extend_from_slice(&4u16.to_be_bytes()); // X advance.
    subtable.extend_from_slice(&Uint24::new(count).to_be_bytes());
    subtable.resize(11 + (count as usize - 1) * 2, 0);
    subtable.extend_from_slice(&(-10i16).to_be_bytes());
    subtable.extend_from_slice(&[0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0]);
    let output = apply_subtable(1, false, &subtable, &[65536]);
    assert_eq!(output.glyph_positions()[0].x_advance, -10);

    let mut extension = vec![0, 1, 0, 1, 0, 0, 0, 8];
    extension.extend_from_slice(&subtable);
    let output = apply_subtable(9, false, &extension, &[65536]);
    assert_eq!(output.glyph_positions()[0].x_advance, -10);
}

#[test]
fn single_pos4_rejects_coverage_indices_outside_the_value_array() {
    let subtable = [
        0, 4, 0, 0, 0, 13, 0, 4, 0, 0, 1, 0, 20, // Header and one value.
        0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, // Index 1 is out of bounds.
    ];
    let output = apply_subtable(1, false, &subtable, &[65536]);
    assert_eq!(output.glyph_positions()[0].x_advance, 0);
}

#[test]
fn cursive_pos_formats_attach_glyphs_in_all_directions() {
    let narrow = [
        0, 1, 0, 14, 0, 2, // Header and two records.
        0, 0, 0, 22, // First glyph's exit anchor.
        0, 28, 0, 0, // Second glyph's entry anchor.
        0, 1, 0, 2, 0, 1, 0, 2, // Coverage.
        0, 1, 0, 100, 0, 50, // Exit anchor.
        0, 1, 0, 20, 0, 10, // Entry anchor.
    ];
    let wide = [
        0, 2, 0, 0, 0, 21, 0, 0, 2, // Header and two records.
        0, 0, 0, 0, 0, 32, // First glyph's exit anchor.
        0, 0, 38, 0, 0, 0, // Second glyph's entry anchor.
        0, 3, 0, 0, 2, 1, 0, 0, 1, 0, 1, // Coverage.
        0, 1, 0, 100, 0, 50, // Exit anchor.
        0, 1, 0, 20, 0, 10, // Entry anchor.
    ];
    for (subtable, glyphs) in [
        (narrow.as_slice(), [1, 2]),
        (wide.as_slice(), [65536, 65537]),
    ] {
        for (direction, expected) in [
            (Direction::LeftToRight, [[0, 0, 100, 0], [-20, 40, -20, 0]]),
            (Direction::RightToLeft, [[-100, 0, -100, 0], [0, 40, 20, 0]]),
            (Direction::TopToBottom, [[0, 0, 0, 50], [80, -10, 0, -10]]),
            (Direction::BottomToTop, [[0, -50, 0, -50], [80, 0, 0, 10]]),
        ] {
            let output = apply_subtable_configured(3, false, subtable, &glyphs, |ctx| {
                ctx.buffer.set_direction(direction);
            });
            for (pos, expected) in output.glyph_positions().iter().zip(expected) {
                assert_eq!(
                    [pos.x_offset, pos.y_offset, pos.x_advance, pos.y_advance],
                    expected
                );
            }
            assert_eq!(output.glyph_positions()[1].attach_chain(), -1);
            assert_eq!(
                output.glyph_positions()[1].attach_type(),
                gpos::attach_type::CURSIVE
            );
        }

        let output = apply_subtable_configured(3, false, subtable, &glyphs, |ctx| {
            ctx.lookup_props |= u32::from(lookup_flags::RIGHT_TO_LEFT);
        });
        assert_eq!(output.glyph_positions()[0].attach_chain(), 1);
        assert_eq!(output.glyph_positions()[0].y_offset, -40);
    }
}

#[test]
fn cursive_pos2_resolves_large_and_nullable_anchor_offsets() {
    let mut subtable = vec![
        0, 2, 0, 0, 0, 21, 0, 0, 2, // Header and two records.
        0, 0, 0, 1, 0, 0, // First glyph's exit anchor at 65536.
        1, 0, 6, 0, 0, 0, // Second glyph's entry anchor at 65542.
        0, 3, 0, 0, 2, 1, 0, 0, 1, 0, 1, // Coverage.
    ];
    subtable.resize(65536, 0);
    subtable.extend_from_slice(&[0, 1, 0, 100, 0, 50, 0, 1, 0, 20, 0, 10]);
    let output = apply_subtable(3, false, &subtable, &[65536, 65537]);
    assert_eq!(output.glyph_positions()[0].x_advance, 100);
    assert_eq!(output.glyph_positions()[1].y_offset, 40);

    subtable[15..18].fill(0);
    let output = apply_subtable(3, false, &subtable, &[65536, 65537]);
    assert_eq!(output.glyph_positions()[0].x_advance, 0);
    assert_eq!(output.glyph_positions()[1].attach_chain(), 0);
}

#[test]
fn pair_class_indices_cannot_alias_another_matrix_row() {
    for class in [2, 65536] {
        let mut subtable = vec![
            0, 2, 0, 24, 0, 4, 0, 0, 0, 32, 0, 43, 0, 2, 0, 2, // Header.
            0, 0, 0, 0, 0, 100, 0, 0, // A nonzero value in the next row.
            0, 3, 0, 0, 1, 1, 0, 0, // Coverage.
            0, 3, 1, 0, 0, 0, 0, 1, 0, 0, 0, // First class is zero.
            0, 3, 1, 0, 1, 0, 0, 1, // Second class is outside the matrix.
        ];
        subtable.extend_from_slice(&Uint24::new(class).to_be_bytes());
        let output = apply_subtable(2, false, &subtable, &[65536, 65537]);
        assert_eq!(output.glyph_positions()[0].x_advance, 0);
    }
}

fn class_pair_pos4(large_offsets: bool) -> Vec<u8> {
    let coverage_offset = if large_offsets { 65536u32 } else { 38 };
    let mut subtable = vec![0, 4];
    subtable.extend_from_slice(&coverage_offset.to_be_bytes());
    subtable.extend_from_slice(&[0, 4, 0, 2]); // X advance and Y placement.
    subtable.extend_from_slice(&(coverage_offset + 11).to_be_bytes());
    subtable.extend_from_slice(&(coverage_offset + 25).to_be_bytes());
    subtable.extend_from_slice(&[0, 2, 0, 2]); // Class counts remain 16-bit.
    subtable.extend_from_slice(&[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100, 0xFF, 0xEC]);
    subtable.resize(coverage_offset as usize, 0);
    subtable.extend_from_slice(&[0, 3, 0, 0, 2, 1, 0, 0, 1, 0, 1]);
    subtable.extend_from_slice(&[0, 3, 1, 0, 0, 0, 0, 2, 0, 0, 1, 0, 0, 0]);
    subtable.extend_from_slice(&[0, 3, 1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 1]);
    subtable
}

#[test]
fn pair_pos4_applies_wide_class_pairs() {
    for large_offsets in [false, true] {
        let subtable = class_pair_pos4(large_offsets);
        let output = apply_subtable(2, false, &subtable, &[65536, 65537]);
        assert_eq!(output.glyph_positions()[0].x_advance, 100);
        assert_eq!(output.glyph_positions()[1].y_offset, -20);
    }
}

#[test]
fn pair_pos4_does_not_truncate_wide_classes_to_matrix_indices() {
    let mut subtable = class_pair_pos4(false);
    subtable[57..60].copy_from_slice(&Uint24::new(65537).to_be_bytes());
    let output = apply_subtable(2, false, &subtable, &[65536, 65537]);
    assert_eq!(output.glyph_positions()[0].x_advance, 0);
    assert_eq!(output.glyph_positions()[1].y_offset, 0);
}

#[test]
fn pair_pos3_matches_wide_second_glyphs_and_applies_both_values() {
    let subtable = [
        0, 3, 0, 0, 0, 16, 0, 4, 0, 2, 0, 0, 1, 0, 0, 24, // Header.
        0, 3, 0, 0, 1, 1, 0, 0, // Coverage.
        0, 0, 2, // Pair count.
        1, 0, 1, 0, 10, 0, 20, // First pair.
        1, 0, 2, 0xFF, 0xF6, 0xFF, 0xEC, // Second pair.
    ];
    for (second, advance, placement) in [(65537, 10, 20), (65538, -10, -20), (1, 0, 0)] {
        let output = apply_subtable(2, false, &subtable, &[65536, second]);
        assert_eq!(output.glyph_positions()[0].x_advance, advance);
        assert_eq!(output.glyph_positions()[1].y_offset, placement);
    }
}

#[test]
fn pair_pos3_keeps_wide_pair_counts() {
    let mut subtable = vec![0, 3, 0, 0, 0, 16, 0, 4, 0, 0, 0, 0, 1, 1, 0, 0];
    subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 0]);
    subtable.resize(65536, 0);
    subtable.extend_from_slice(&Uint24::new(65537).to_be_bytes());
    for glyph in 0..=65536 {
        subtable.extend_from_slice(&Uint24::new(glyph).to_be_bytes());
        subtable.extend_from_slice(&(if glyph == 65536 { 100i16 } else { 0 }).to_be_bytes());
    }
    let output = apply_subtable(2, false, &subtable, &[65536, 65536]);
    assert_eq!(output.glyph_positions()[0].x_advance, 100);
}

#[test]
fn pair_pos3_keeps_wide_set_counts_and_coverage_indices() {
    let count = 65537;
    let coverage_offset = 13 + count * 3u32;
    let mut subtable = vec![0, 3];
    subtable.extend_from_slice(&coverage_offset.to_be_bytes());
    subtable.extend_from_slice(&[0, 4, 0, 0]);
    subtable.extend_from_slice(&Uint24::new(count).to_be_bytes());
    subtable.resize(coverage_offset as usize - 3, 0);
    subtable.extend_from_slice(&Uint24::new(coverage_offset + 14).to_be_bytes());
    subtable.extend_from_slice(&[0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0]);
    subtable.extend_from_slice(&[0, 0, 1, 1, 0, 1, 0, 100]);
    let output = apply_subtable(2, false, &subtable, &[65536, 65537]);
    assert_eq!(output.glyph_positions()[0].x_advance, 100);
}

fn mark_base_pos2(large_offsets: bool) -> Vec<u8> {
    let mark_anchor = if large_offsets { 65536u32 } else { 8 };
    let base_anchor = if large_offsets { 65536u32 } else { 6 };
    let base_array = 36 + mark_anchor + 6;
    let mut subtable = vec![0, 2, 0, 0, 0, 20, 0, 0, 0, 28, 0, 1, 0, 0, 0, 36];
    subtable.extend_from_slice(&base_array.to_be_bytes());
    subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 1]);
    subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 0]);
    subtable.extend_from_slice(&[0, 0, 1, 0, 0]);
    subtable.extend_from_slice(&Uint24::new(mark_anchor).to_be_bytes());
    subtable.resize(36 + mark_anchor as usize, 0);
    subtable.extend_from_slice(&[0, 1, 0, 20, 0, 10]);
    subtable.extend_from_slice(&[0, 0, 1]);
    subtable.extend_from_slice(&Uint24::new(base_anchor).to_be_bytes());
    subtable.resize(base_array as usize + base_anchor as usize, 0);
    subtable.extend_from_slice(&[0, 1, 0, 100, 0, 50]);
    subtable
}

#[test]
fn boxed_payload_is_charged_to_allocation_budget() {
    let bytes = lookup_bytes(4, &mark_base_pos2(false));
    let storage = size_of::<super::CachedSubtable>();
    let payload = size_of::<super::gpos::MarkBasePos1>();
    for allowance in [storage, storage + payload - 1] {
        let budget = Budget::new(1 << 20, allowance);
        assert!(matches!(
            build_lookup_at(FontData::new(&bytes), 0, false, &budget),
            Err(super::build::BuildError::Exhausted)
        ));
    }
    let budget = Budget::new(1 << 20, storage + payload);
    let lookup = build_lookup_at(FontData::new(&bytes), 0, false, &budget).unwrap();
    assert_eq!(lookup.subtables.len(), 1);
    assert_eq!(budget.remaining().1, 0);
}

#[test]
fn mark_base_pos2_attaches_wide_glyphs_and_resolves_large_anchors() {
    for large_offsets in [false, true] {
        let mut subtable = mark_base_pos2(large_offsets);
        let output = apply_subtable(4, false, &subtable, &[65536, 65537]);
        let pos = &output.glyph_positions()[1];
        assert_eq!([pos.x_offset, pos.y_offset], [80, 40]);
        assert_eq!(pos.attach_chain(), -1);
        assert_eq!(pos.attach_type(), gpos::attach_type::MARK);

        let base_array = FontData::new(&subtable).read_at::<u32>(16).unwrap() as usize;
        subtable[base_array + 3..base_array + 6].fill(0);
        let output = apply_subtable(4, false, &subtable, &[65536, 65537]);
        assert_eq!(output.glyph_positions()[1].attach_chain(), 0);
    }
}

#[test]
fn mark_base_pos2_keeps_wide_mark_and_base_counts() {
    let count = 65537;
    let mark_array = 48u32;
    let mark_anchor = 3 + count * 5;
    let base_array = mark_array + mark_anchor + 6;
    let base_anchor = 3 + count * 3;
    let mut subtable = vec![0, 2, 0, 0, 0, 20, 0, 0, 0, 34, 0, 1];
    subtable.extend_from_slice(&mark_array.to_be_bytes());
    subtable.extend_from_slice(&base_array.to_be_bytes());
    subtable.extend_from_slice(&[0, 4, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0]);
    subtable.extend_from_slice(&[0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0]);
    subtable.extend_from_slice(&Uint24::new(count).to_be_bytes());
    subtable.resize((mark_array + mark_anchor - 5) as usize, 0);
    subtable.extend_from_slice(&[0, 0]);
    subtable.extend_from_slice(&Uint24::new(mark_anchor).to_be_bytes());
    subtable.extend_from_slice(&[0, 1, 0, 20, 0, 10]);
    subtable.extend_from_slice(&Uint24::new(count).to_be_bytes());
    subtable.resize((base_array + base_anchor - 3) as usize, 0);
    subtable.extend_from_slice(&Uint24::new(base_anchor).to_be_bytes());
    subtable.extend_from_slice(&[0, 1, 0, 100, 0, 50]);
    let output = apply_subtable(4, false, &subtable, &[65536, 65537]);
    let pos = &output.glyph_positions()[1];
    assert_eq!([pos.x_offset, pos.y_offset], [80, 40]);
    assert_eq!(pos.attach_chain(), -1);
}

#[test]
fn mark_base_pos2_skips_marks_and_preserves_scaled_cross_offsets() {
    let subtable = mark_base_pos2(false);
    let output = apply_subtable_configured(4, false, &subtable, &[65536, 65538, 65537], |ctx| {
        for info in &mut ctx.buffer.glyph_infos_mut()[1..] {
            info.set_glyph_props(GlyphPropsFlags::MARK.bits());
        }
        ctx.scale = Scale::new(Some((2000, 500)), 1000);
        ctx.buffer.glyph_positions_mut()[0].y_offset = 30;
    });
    let pos = &output.glyph_positions()[2];
    assert_eq!([pos.x_offset, pos.y_offset], [160, 50]);
    assert_eq!(pos.attach_chain(), -2);
}

fn mark_mark_pos2(large_offsets: bool) -> Vec<u8> {
    let mut subtable = mark_base_pos2(large_offsets);
    let array = FontData::new(&subtable).read_at::<u32>(16).unwrap() as usize;
    let anchor = FontData::new(&subtable)
        .read_at::<Uint24>(array + 3)
        .unwrap();
    // Unlike BaseArray2, Mark2Array2 retains a 16-bit count.
    subtable[array..array + 2].copy_from_slice(&1u16.to_be_bytes());
    subtable[array + 2..array + 5].copy_from_slice(&anchor.to_be_bytes());
    subtable
}

#[test]
fn mark_mark_pos2_preserves_ligature_component_matching() {
    for large_offsets in [false, true] {
        let subtable = mark_mark_pos2(large_offsets);
        for (first, second, matches) in [
            ((0, 0), (0, 0), true),
            ((1, 2), (1, 2), true),
            ((1, 1), (1, 2), false),
            ((1, 1), (2, 1), false),
            ((1, 0), (2, 1), true),
        ] {
            let output = apply_subtable_configured(6, false, &subtable, &[65536, 65537], |ctx| {
                for (info, (id, component)) in
                    ctx.buffer.glyph_infos_mut().iter_mut().zip([first, second])
                {
                    info.set_glyph_props(GlyphPropsFlags::MARK.bits());
                    info.set_lig_props_for_mark(id, component);
                }
            });
            let pos = &output.glyph_positions()[1];
            assert_eq!(
                [pos.x_offset, pos.y_offset],
                if matches { [80, 40] } else { [0, 0] }
            );
            assert_eq!(pos.attach_chain(), if matches { -1 } else { 0 });
        }
    }
}

#[test]
fn mark_mark_pos2_does_not_truncate_wide_mark2_indices() {
    let mut subtable = mark_mark_pos2(false);
    subtable.splice(28..36, [0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0]);
    subtable[12..16].copy_from_slice(&42u32.to_be_bytes());
    subtable[16..20].copy_from_slice(&56u32.to_be_bytes());
    let output = apply_subtable_configured(6, false, &subtable, &[65536, 65537], |ctx| {
        for info in ctx.buffer.glyph_infos_mut() {
            info.set_glyph_props(GlyphPropsFlags::MARK.bits());
        }
    });
    assert_eq!(output.glyph_positions()[1].attach_chain(), 0);
}

fn mark_lig_pos2(large_offsets: bool) -> Vec<u8> {
    let mut subtable = mark_base_pos2(large_offsets);
    let array = FontData::new(&subtable).read_at::<u32>(16).unwrap() as usize;
    let attach_offset = if large_offsets { 65536 } else { 6 };
    let anchor_offset = if large_offsets { 65536 } else { 8 };
    subtable.truncate(array);
    subtable.extend_from_slice(&[0, 0, 1]);
    subtable.extend_from_slice(&Uint24::new(attach_offset).to_be_bytes());
    let attach = array + attach_offset as usize;
    subtable.resize(attach, 0);
    subtable.extend_from_slice(&2u16.to_be_bytes()); // Component count stays 16-bit.
    subtable.extend_from_slice(&Uint24::new(anchor_offset).to_be_bytes());
    subtable.extend_from_slice(&Uint24::new(anchor_offset + 6).to_be_bytes());
    subtable.resize(attach + anchor_offset as usize, 0);
    subtable.extend_from_slice(&[0, 1, 0, 100, 0, 50, 0, 1, 0, 200, 0, 100]);
    subtable
}

#[test]
fn mark_lig_pos2_selects_the_component_and_resolves_large_offsets() {
    for large_offsets in [false, true] {
        let subtable = mark_lig_pos2(large_offsets);
        for (mark_id, component, expected) in [
            (1, 1, [80, 40]),
            (1, 2, [180, 90]),
            (1, 15, [180, 90]),
            (1, 0, [180, 90]),
            (2, 1, [180, 90]),
        ] {
            let output = apply_subtable_configured(5, false, &subtable, &[65536, 65537], |ctx| {
                let infos = ctx.buffer.glyph_infos_mut();
                infos[0].set_glyph_props(GlyphPropsFlags::LIGATURE.bits());
                infos[0].set_lig_props_for_ligature(1, 2);
                infos[1].set_glyph_props(GlyphPropsFlags::MARK.bits());
                infos[1].set_lig_props_for_mark(mark_id, component);
            });
            let pos = &output.glyph_positions()[1];
            assert_eq!([pos.x_offset, pos.y_offset], expected);
            assert_eq!(pos.attach_chain(), -1);
        }
    }
}

#[test]
fn mark_lig_pos2_keeps_wide_ligature_counts_and_coverage_indices() {
    let count = 65537;
    let mut subtable = vec![
        0, 2, 0, 0, 0, 20, 0, 0, 0, 28, 0, 1, 0, 0, 0, 42, 0, 0, 0, 56,
    ];
    subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 1]);
    subtable.extend_from_slice(&[0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0]);
    subtable.extend_from_slice(&[0, 0, 1, 0, 0, 0, 0, 8, 0, 1, 0, 20, 0, 10]);
    subtable.extend_from_slice(&Uint24::new(count).to_be_bytes());
    let attach_offset = 3 + count * 3;
    subtable.resize(56 + attach_offset as usize - 3, 0);
    subtable.extend_from_slice(&Uint24::new(attach_offset).to_be_bytes());
    subtable.extend_from_slice(&[0, 1, 0, 0, 5, 0, 1, 0, 200, 0, 100]);
    let output = apply_subtable(5, false, &subtable, &[65536, 65537]);
    let pos = &output.glyph_positions()[1];
    assert_eq!([pos.x_offset, pos.y_offset], [180, 90]);
    assert_eq!(pos.attach_chain(), -1);
}

#[test]
fn mark_lig_pos2_rejects_empty_component_arrays() {
    let mut subtable = mark_lig_pos2(false);
    subtable[56..58].fill(0);
    let output = apply_subtable(5, false, &subtable, &[65536, 65537]);
    assert_eq!(output.glyph_positions()[1].attach_chain(), 0);
}

#[test]
fn context6_applies_nested_substitution_and_positioning() {
    for large_offsets in [false, true] {
        let coverage_offset = if large_offsets { 65536 } else { 16 };
        let mut subtable = vec![0, 6, 0, 2, 0, 1];
        subtable.extend_from_slice(&Uint24::new(coverage_offset).to_be_bytes());
        subtable.extend_from_slice(&Uint24::new(coverage_offset + 8).to_be_bytes());
        subtable.extend_from_slice(&[0, 1, 0, 1]); // Apply lookup 1 at the second glyph.
        subtable.resize(coverage_offset as usize, 0);
        subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 0]);
        subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 1]);
        for (input, matches) in [
            ([65536, 65537], true),
            ([65536, 1], false),
            ([1, 65537], false),
        ] {
            let output = apply_nested_context(5, true, &subtable, &input);
            assert_eq!(
                output.glyph_infos()[1].glyph_id,
                if matches { 65538 } else { input[1] }
            );
            let output = apply_nested_context(7, false, &subtable, &input);
            assert_eq!(
                output.glyph_positions()[1].x_advance,
                if matches { 10 } else { 0 }
            );
        }
    }
}

fn context4(rule_set_offset: u32, coverage_index: u32) -> Vec<u8> {
    let set_count = coverage_index + 1;
    let coverage_offset = 9 + set_count * 3;
    let mut subtable = vec![0, 4];
    subtable.extend_from_slice(&coverage_offset.to_be_bytes());
    subtable.extend_from_slice(&Uint24::new(set_count).to_be_bytes());
    subtable.resize(coverage_offset as usize, 0);
    let set_offset_pos = 9 + coverage_index as usize * 3;
    subtable[set_offset_pos..set_offset_pos + 3]
        .copy_from_slice(&Uint24::new(rule_set_offset).to_be_bytes());
    // Coverage 4 allows a high coverage index without a long glyph array.
    subtable.extend_from_slice(&[0, 4, 0, 0, 1, 1, 0, 0, 1, 0, 0]);
    subtable.extend_from_slice(&Uint24::new(coverage_index).to_be_bytes());
    subtable.resize(rule_set_offset as usize, 0);
    // Three rules: two fail on their first input glyph, the last succeeds.
    // Rule offsets remain 16-bit in SequenceRuleSet2.
    subtable.extend_from_slice(&[0, 3, 0, 8, 0, 22, 0, 36]);
    for second in [65538, 65538, 65537] {
        subtable.extend_from_slice(&[0, 3, 0, 1]);
        subtable.extend_from_slice(&Uint24::new(second).to_be_bytes());
        subtable.extend_from_slice(&Uint24::new(65539).to_be_bytes());
        subtable.extend_from_slice(&[0, 1, 0, 1]);
    }
    subtable
}

#[test]
fn context4_applies_full_width_glyph_rules() {
    for (rule_set_offset, coverage_index) in [(26, 0), (65536, 0), (200_000, 65536)] {
        let subtable = context4(rule_set_offset, coverage_index);
        for (input, matches) in [
            ([65536, 65537, 65539], true),
            ([65536, 1, 65539], false),
            ([65536, 65537, 3], false),
            ([0, 65537, 65539], false),
        ] {
            assert_eq!(would_apply(5, &subtable, &input, true), matches);
            let output = apply_nested_context(5, true, &subtable, &input);
            assert_eq!(
                output.glyph_infos()[1].glyph_id,
                if matches { 65538 } else { input[1] }
            );
            let output = apply_nested_context(7, false, &subtable, &input);
            assert_eq!(
                output.glyph_positions()[1].x_advance,
                if matches { 10 } else { 0 }
            );
        }
    }
}

#[test]
fn context4_rejects_null_rule_sets_and_truncated_rules() {
    let mut subtable = context4(26, 0);
    subtable[9..12].fill(0);
    let output = apply_nested_context(5, true, &subtable, &[65536, 65537, 65539]);
    assert_eq!(output.glyph_infos()[1].glyph_id, 65537);

    let mut subtable = context4(26, 0);
    subtable.truncate(subtable.len() - 6);
    let output = apply_nested_context(5, true, &subtable, &[65536, 65537, 65539]);
    assert_eq!(output.glyph_infos()[1].glyph_id, 65537);
}

fn context5(first_class: u32, second_class: u32, large_offsets: bool) -> Vec<u8> {
    let set_count = first_class + 1;
    let header_end = 13 + set_count * 3;
    let coverage_offset = if large_offsets {
        header_end.max(65536)
    } else {
        header_end
    };
    let class_def_offset = coverage_offset + 8;
    let rule_set_offset = class_def_offset + 17;
    let rule_offset = if large_offsets { 65536 } else { 11 };
    let mut subtable = vec![0, 5];
    subtable.extend_from_slice(&coverage_offset.to_be_bytes());
    subtable.extend_from_slice(&class_def_offset.to_be_bytes());
    subtable.extend_from_slice(&Uint24::new(set_count).to_be_bytes());
    subtable.resize(coverage_offset as usize, 0);
    let set_offset_pos = 13 + first_class as usize * 3;
    subtable[set_offset_pos..set_offset_pos + 3]
        .copy_from_slice(&Uint24::new(rule_set_offset).to_be_bytes());
    subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 0]);
    subtable.extend_from_slice(&[0, 3, 1, 0, 0, 0, 0, 3]); // ClassDef3: three glyphs.
    for class in [first_class, second_class, 301] {
        subtable.extend_from_slice(&Uint24::new(class).to_be_bytes());
    }
    subtable.extend_from_slice(&[0, 3]);
    for offset in [rule_offset, rule_offset + 12, rule_offset + 24] {
        subtable.extend_from_slice(&Uint24::new(offset).to_be_bytes());
    }
    subtable.resize((rule_set_offset + rule_offset) as usize, 0);
    // Classes within ClassSequenceRule remain 16-bit.
    for second in [302u16, 302, 300] {
        subtable.extend_from_slice(&[0, 3, 0, 1]);
        subtable.extend_from_slice(&second.to_be_bytes());
        subtable.extend_from_slice(&301u16.to_be_bytes());
        subtable.extend_from_slice(&[0, 1, 0, 1]);
    }
    subtable
}

#[test]
fn context5_applies_class_rules_with_wide_offsets_and_set_counts() {
    for first_class in [1, 300, 65536] {
        for large_offsets in [false, true] {
            let subtable = context5(first_class, 300, large_offsets);
            for (input, matches) in [
                ([65536, 65537, 65538], true),
                ([65536, 1, 65538], false),
                ([65536, 65537, 2], false),
            ] {
                assert_eq!(would_apply(5, &subtable, &input, true), matches);
                for use_hot_cache in [false, true] {
                    let output =
                        apply_nested_context_cached(5, true, &subtable, &input, use_hot_cache);
                    assert_eq!(
                        output.glyph_infos()[1].glyph_id,
                        if matches { 65538 } else { input[1] }
                    );
                    let output =
                        apply_nested_context_cached(7, false, &subtable, &input, use_hot_cache);
                    assert_eq!(
                        output.glyph_positions()[1].x_advance,
                        if matches { 10 } else { 0 }
                    );
                }
            }
        }
    }
}

#[test]
fn context5_does_not_truncate_wide_classes_to_match_rule_values() {
    let subtable = context5(1, 65536 + 300, false);
    for use_hot_cache in [false, true] {
        let output =
            apply_nested_context_cached(5, true, &subtable, &[65536, 65537, 65538], use_hot_cache);
        assert_eq!(output.glyph_infos()[1].glyph_id, 65537);
    }
}

fn chain_context4(coverage_index: u32, large_offsets: bool, input_count: u16) -> Vec<u8> {
    let set_count = coverage_index + 1;
    let coverage_offset = 9 + set_count * 3;
    let rule_set_offset = if large_offsets {
        (coverage_offset + 14).max(65536)
    } else {
        coverage_offset + 14
    };
    let rule_offset = if large_offsets { 65536 } else { 11 };
    let rule_len = 24 + u32::from(input_count - 1) * 3;
    let mut subtable = vec![0, 4];
    subtable.extend_from_slice(&coverage_offset.to_be_bytes());
    subtable.extend_from_slice(&Uint24::new(set_count).to_be_bytes());
    subtable.resize(coverage_offset as usize, 0);
    let set_offset_pos = 9 + coverage_index as usize * 3;
    subtable[set_offset_pos..set_offset_pos + 3]
        .copy_from_slice(&Uint24::new(rule_set_offset).to_be_bytes());
    subtable.extend_from_slice(&[0, 4, 0, 0, 1, 1, 0, 1, 1, 0, 1]);
    subtable.extend_from_slice(&Uint24::new(coverage_index).to_be_bytes());
    subtable.resize(rule_set_offset as usize, 0);
    subtable.extend_from_slice(&[0, 3]);
    for offset in [
        rule_offset,
        rule_offset + rule_len,
        rule_offset + 2 * rule_len,
    ] {
        subtable.extend_from_slice(&Uint24::new(offset).to_be_bytes());
    }
    subtable.resize((rule_set_offset + rule_offset) as usize, 0);
    for matches in [false, false, true] {
        subtable.extend_from_slice(&[0, 2]);
        for glyph in [65540, 65541] {
            subtable.extend_from_slice(&Uint24::new(glyph).to_be_bytes());
        }
        subtable.extend_from_slice(&input_count.to_be_bytes());
        for index in 1..input_count {
            let glyph = if !matches && index == 1 {
                65539
            } else {
                65537 + u32::from(index)
            };
            subtable.extend_from_slice(&Uint24::new(glyph).to_be_bytes());
        }
        subtable.extend_from_slice(&[0, 2]);
        let first_lookahead = if !matches && input_count == 1 {
            65543
        } else {
            65542
        };
        for glyph in [first_lookahead, 65543] {
            subtable.extend_from_slice(&Uint24::new(glyph).to_be_bytes());
        }
        subtable.extend_from_slice(&[0, 1, 0, 0, 0, 1]);
    }
    subtable
}

#[test]
fn chain_context4_matches_wide_backtrack_input_and_lookahead() {
    for input_count in 1..=3 {
        for (coverage_index, large_offsets) in [(0, false), (0, true), (65536, true)] {
            let subtable = chain_context4(coverage_index, large_offsets, input_count);
            let mut input = vec![65541, 65540];
            input.extend((0..input_count).map(|index| 65537 + u32::from(index)));
            input.extend([65542, 65543]);
            let glyphs = &input[2..2 + usize::from(input_count)];
            assert!(would_apply(6, &subtable, glyphs, false));
            assert!(!would_apply(6, &subtable, glyphs, true));
            let output = apply_nested_context(6, true, &subtable, &input);
            assert_eq!(output.glyph_infos()[2].glyph_id, 65538);
            let output = apply_nested_context(8, false, &subtable, &input);
            assert_eq!(output.glyph_positions()[2].x_advance, 10);
            for mismatch in (0..input.len()).filter(|index| *index != 2) {
                let mut unmatched = input.clone();
                unmatched[mismatch] &= 0xFFFF;
                let output = apply_nested_context(6, true, &subtable, &unmatched);
                assert_eq!(output.glyph_infos()[2].glyph_id, 65537);
                let output = apply_nested_context(8, false, &subtable, &unmatched);
                assert_eq!(output.glyph_positions()[2].x_advance, 0);
            }
        }
    }
}

fn chain_context5(first_class: u16, large_offsets: bool, input_count: u16) -> Vec<u8> {
    let set_count = first_class + 1;
    let header_end = 20 + u32::from(set_count) * 3;
    let coverage_offset = if large_offsets {
        header_end.max(65536)
    } else {
        header_end
    };
    let backtrack_class_offset = coverage_offset + 8;
    let input_class_offset = backtrack_class_offset + 32;
    let lookahead_class_offset = input_class_offset + 32;
    let rule_set_offset = lookahead_class_offset + 32;
    let rule_offset = if large_offsets { 65536 } else { 11 };
    let rule_len = 20 + u32::from(input_count - 1) * 2;
    let mut subtable = vec![0, 5];
    for offset in [
        coverage_offset,
        backtrack_class_offset,
        input_class_offset,
        lookahead_class_offset,
    ] {
        subtable.extend_from_slice(&offset.to_be_bytes());
    }
    // Unlike SequenceContext5, this rule-set count is still 16-bit.
    subtable.extend_from_slice(&set_count.to_be_bytes());
    subtable.resize(coverage_offset as usize, 0);
    let set_offset_pos = 20 + usize::from(first_class) * 3;
    subtable[set_offset_pos..set_offset_pos + 3]
        .copy_from_slice(&Uint24::new(rule_set_offset).to_be_bytes());
    subtable.extend_from_slice(&[0, 3, 0, 0, 1, 1, 0, 1]);
    for classes in [
        [0, 0, 0, 0, 401, 402, 0, 0],
        [0, u32::from(first_class), 201, 202, 0, 0, 0, 0],
        [0, 0, 0, 0, 0, 0, 501, 502],
    ] {
        subtable.extend_from_slice(&[0, 3, 1, 0, 0, 0, 0, 8]);
        for class in classes {
            subtable.extend_from_slice(&Uint24::new(class).to_be_bytes());
        }
    }
    subtable.extend_from_slice(&[0, 3]);
    for offset in [
        rule_offset,
        rule_offset + rule_len,
        rule_offset + 2 * rule_len,
    ] {
        subtable.extend_from_slice(&Uint24::new(offset).to_be_bytes());
    }
    subtable.resize((rule_set_offset + rule_offset) as usize, 0);
    for matches in [false, false, true] {
        subtable.extend_from_slice(&[0, 2, 1, 145, 1, 146]); // Backtrack classes 401/402.
        subtable.extend_from_slice(&input_count.to_be_bytes());
        for index in 1..input_count {
            let class = if !matches && index == 1 {
                202
            } else {
                200 + index
            };
            subtable.extend_from_slice(&class.to_be_bytes());
        }
        subtable.extend_from_slice(&[0, 2]);
        let first_lookahead = if !matches && input_count == 1 {
            502u16
        } else {
            501
        };
        subtable.extend_from_slice(&first_lookahead.to_be_bytes());
        subtable.extend_from_slice(&502u16.to_be_bytes());
        subtable.extend_from_slice(&[0, 1, 0, 0, 0, 1]);
    }
    subtable
}

#[test]
fn chain_context5_matches_class_rules_with_wide_offsets() {
    for (first_class, large_offsets) in [(1, false), (300, true), (65534, true)] {
        for input_count in 1..=3 {
            let subtable = chain_context5(first_class, large_offsets, input_count);
            let mut input = vec![65541, 65540];
            input.extend((0..input_count).map(|index| 65537 + u32::from(index)));
            input.extend([65542, 65543]);
            let glyphs = &input[2..2 + usize::from(input_count)];
            assert!(would_apply(6, &subtable, glyphs, false));
            assert!(!would_apply(6, &subtable, glyphs, true));
            for use_hot_cache in [false, true] {
                let output = apply_nested_context_cached(6, true, &subtable, &input, use_hot_cache);
                assert_eq!(output.glyph_infos()[2].glyph_id, 65538);
                let output =
                    apply_nested_context_cached(8, false, &subtable, &input, use_hot_cache);
                assert_eq!(output.glyph_positions()[2].x_advance, 10);
                for mismatch in (0..input.len()).filter(|index| *index != 2) {
                    let mut unmatched = input.clone();
                    unmatched[mismatch] &= 0xFFFF;
                    let output =
                        apply_nested_context_cached(6, true, &subtable, &unmatched, use_hot_cache);
                    assert_eq!(output.glyph_infos()[2].glyph_id, 65537);
                }
            }
        }
    }
}

#[test]
fn chain_context5_does_not_truncate_wide_classes() {
    for (offset_pos, class_index, class) in [(10, 1, 1), (10, 2, 201), (6, 4, 401), (14, 6, 501)] {
        let mut subtable = chain_context5(1, false, 2);
        let class_offset =
            u32::from_be_bytes(subtable[offset_pos..offset_pos + 4].try_into().unwrap()) as usize;
        let pos = class_offset + 8 + class_index * 3;
        subtable[pos..pos + 3].copy_from_slice(&Uint24::new(65536 + class).to_be_bytes());
        for use_hot_cache in [false, true] {
            let output = apply_nested_context_cached(
                6,
                true,
                &subtable,
                &[65541, 65540, 65537, 65538, 65542, 65543],
                use_hot_cache,
            );
            assert_eq!(output.glyph_infos()[2].glyph_id, 65537);
        }
    }
}
