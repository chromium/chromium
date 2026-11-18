use super::*;
use crate::{
    shape, Buffer, Direction, Font, ShapeOptions, ShapePlan, ShapePlanKey, ShaperFont, Tag,
};
use alloc::{sync::Arc, vec, vec::Vec};
use read_fonts::{
    model::{Blob, TableFunction},
    FontRef,
};

fn u16s(values: &[u16]) -> Vec<u8> {
    values
        .iter()
        .flat_map(|value| value.to_be_bytes())
        .collect()
}

fn put16(bytes: &mut [u8], position: usize, value: usize) {
    bytes[position..position + 2].copy_from_slice(&u16::try_from(value).unwrap().to_be_bytes());
}

fn put32(bytes: &mut [u8], position: usize, value: usize) {
    bytes[position..position + 4].copy_from_slice(&u32::try_from(value).unwrap().to_be_bytes());
}

fn range(min: f32, max: f32) -> Vec<u8> {
    u16s(&[
        0x0001,
        0,
        F2Dot14::from_f32(min).to_bits() as u16,
        F2Dot14::from_f32(max).to_bits() as u16,
    ])
}

fn conditional(records: &[(Option<Vec<u8>>, Vec<u16>)], add_defaults: bool) -> Vec<u8> {
    let mut bytes = vec![0; 10 + records.len() * 8];
    put16(&mut bytes, 0, 1);
    put16(&mut bytes, 4, usize::from(add_defaults));
    put32(&mut bytes, 6, records.len());
    for (index, (condition, lookups)) in records.iter().enumerate() {
        let position = 10 + index * 8;
        if let Some(condition) = condition {
            let offset = bytes.len();
            put32(&mut bytes, position, offset);
            bytes.extend_from_slice(condition);
        }
        let offset = bytes.len();
        put32(&mut bytes, position + 4, offset);
        bytes.extend_from_slice(&u16s(&[u16::try_from(lookups.len()).unwrap()]));
        bytes.extend_from_slice(&u16s(lookups));
    }
    bytes
}

fn variations(conditional: &[u8], alternate: bool) -> Vec<u8> {
    let count = usize::from(alternate);
    let record = 12 + count * 8;
    let mut bytes = vec![0; record + 6];
    put16(&mut bytes, 0, 1);
    put16(&mut bytes, 2, 1);
    put32(&mut bytes, 4, count);
    put32(&mut bytes, record - 4, 1);
    let offset = bytes.len();
    put32(&mut bytes, record + 2, offset);
    bytes.extend_from_slice(conditional);
    if alternate {
        let offset = bytes.len();
        put32(&mut bytes, 12, offset);
        // Universal feature substitution: feature 0 uses lookup 2, not 0.
        bytes.extend_from_slice(&u16s(&[1, 0, 1, 0, 0, 12, 0, 1, 2]));
    }
    bytes
}

fn layout(kind: LayoutTableKind, variations: &[u8]) -> Vec<u8> {
    let mut bytes = u16s(&[1, 1, 14, 0, 0, 0, 0]);
    // DFLT script, with feature 0 required by its default language system.
    bytes.extend_from_slice(&u16s(&[1, 0x4446, 0x4C54, 8, 4, 0, 0, 0, 0]));
    let offset = bytes.len();
    put16(&mut bytes, 6, offset);
    bytes.extend_from_slice(&u16s(&[1, 0x6C69, 0x6761, 8, 0, 1, 0]));
    let list = bytes.len();
    put16(&mut bytes, 8, list);
    bytes.extend_from_slice(&u16s(&[4, 0, 0, 0, 0]));
    for index in 0..4 {
        let offset = bytes.len() - list;
        put16(&mut bytes, list + 2 + index * 2, offset);
        bytes.extend_from_slice(&u16s(&[1, 0, 1, 8]));
        let (input, output) = [(1, 2), (2, 3), (3, 4), (1, 5)][index];
        match kind {
            LayoutTableKind::Gsub => {
                bytes.extend_from_slice(&u16s(&[2, 8, 1, output, 1, 1, input]));
            }
            LayoutTableKind::Gpos => {
                bytes.extend_from_slice(&u16s(&[1, 8, 4, 10 << index, 1, 1, 1]));
            }
        }
    }
    let offset = bytes.len();
    put32(&mut bytes, 10, offset);
    bytes.extend_from_slice(variations);
    bytes
}

fn font(kind: LayoutTableKind, variations: &[u8]) -> Font {
    font_with_store(kind, variations, None)
}

fn font_with_store(kind: LayoutTableKind, variations: &[u8], store: Option<Vec<u8>>) -> Font {
    let source = FontRef::new(include_bytes!("../../benches/fonts/Roboto-Regular.ttf")).unwrap();
    let layout = layout(kind, variations);
    let layout_tag = match kind {
        LayoutTableKind::Gsub => Tag::new(b"GSUB"),
        LayoutTableKind::Gpos => Tag::new(b"GPOS"),
    };
    let cmap = u16s(&[
        0, 1, 3, 10, 0, 12, // cmap header and encoding record.
        12, 0, 0, 28, 0, 0, 0, 1, 0, 97, 0, 97, 0, 1,
    ]);
    let fvar = u16s(&[
        1, 0, 16, 2, 1, 20, 0, 8, // fvar header.
        0x7767, 0x6874, 0xFFFF, 0, 0, 0, 1, 0, 0, 256, // wght: -1..0..1.
    ]);
    let tables = TableFunction::new(Arc::new(move |tag| {
        if tag == layout_tag {
            return Some(Blob::from(layout.clone()));
        }
        if tag == Tag::new(b"GDEF") {
            return store.as_ref().map(|store| {
                let mut gdef = u16s(&[1, 3, 0, 0, 0, 0, 0, 0, 18]);
                gdef.extend_from_slice(store);
                Blob::from(gdef)
            });
        }
        if tag == Tag::new(b"GSUB") || tag == Tag::new(b"GPOS") || tag == Tag::new(b"GDEF") {
            return None;
        }
        if tag == Tag::new(b"cmap") {
            return Some(Blob::from(cmap.clone()));
        }
        if tag == Tag::new(b"fvar") {
            return Some(Blob::from(fvar.clone()));
        }
        source
            .data_for_tag(tag)
            .map(|data| Blob::from(data.as_bytes().to_vec()))
    }));
    Font::new(tables, 0).unwrap()
}

fn instance(font: &Font, coord: f32) -> Font {
    font.instance_builder()
        .normalized_coords([F2Dot14::from_f32(coord)])
        .build()
}

fn shape_a(font: &Font) -> Buffer {
    let mut buffer = Buffer::new();
    buffer.push_str("a");
    buffer.set_direction(Direction::LeftToRight);
    shape(&ShaperFont::new(font), &mut buffer, ShapeOptions::new()).unwrap();
    buffer
}

/// The lookups a plan for `font` applies from `kind`, sorted.
fn plan_lookups(font: &Font, kind: LayoutTableKind) -> Vec<u16> {
    let plan = ShapePlan::new(font, Direction::LeftToRight, None, None, &[]);
    let map = &plan.ot_map;
    let mut lookups: Vec<u16> = (0..map.stages(kind).len())
        .flat_map(|stage| {
            map.stage_lookups(kind, stage)
                .iter()
                .map(|lookup| lookup.index)
        })
        .collect();
    lookups.sort_unstable();
    lookups.dedup();
    lookups
}

/// The feature variation indices a plan for `font` uses.
fn plan_variations(font: &Font) -> [Option<u32>; 2] {
    *ShapePlan::new(font, Direction::LeftToRight, None, None, &[])
        .ot_map
        .feature_variations()
}

/// The lookups of `indices` that exist: the test tables have four.
fn existing(indices: &[u16]) -> Vec<u16> {
    indices.iter().copied().filter(|index| *index < 4).collect()
}

fn records() -> Vec<(Option<Vec<u8>>, Vec<u16>)> {
    // Deliberately unsorted and overlapping lists; every record must be tested.
    vec![
        (None, vec![1, 1]),
        (Some(range(0.25, 1.0)), vec![u16::MAX, 1, 0, 0]),
        (Some(range(-1.0, -0.25)), vec![3]),
    ]
}

#[test]
fn lookup_variations_union_sort_deduplicate_and_shape_gsub_gpos() {
    for kind in [LayoutTableKind::Gsub, LayoutTableKind::Gpos] {
        let font = font(kind, &variations(&conditional(&records(), false), false));
        for (coord, indices, glyph, delta) in [
            (0.0, vec![1], 1, 20),
            (0.5, vec![0, 1, u16::MAX], 3, 30),
            (-0.5, vec![1, 3], 5, 100),
        ] {
            let instance = instance(&font, coord);
            assert_eq!(plan_variations(&instance), [None; 2]);
            assert_eq!(plan_lookups(&instance, kind), existing(&indices));
            let shaped = shape_a(&instance);
            assert_eq!(shaped.glyph_infos().len(), 1);
            match kind {
                LayoutTableKind::Gsub => assert_eq!(shaped.glyph_infos()[0].glyph_id, glyph),
                LayoutTableKind::Gpos => {
                    let advance =
                        ShaperFont::new(&instance).default_glyph_h_advance(GlyphId::new(1));
                    assert_eq!(shaped.glyph_positions()[0].x_advance, advance + delta);
                }
            }
        }
    }
}

#[test]
fn default_lookups_come_from_the_current_alternate_feature() {
    for kind in [LayoutTableKind::Gsub, LayoutTableKind::Gpos] {
        for add_defaults in [false, true] {
            let font = font(
                kind,
                &variations(&conditional(&records(), add_defaults), true),
            );
            for (coord, mut indices) in [(0.0, vec![1]), (0.5, vec![0, 1, u16::MAX])] {
                if add_defaults {
                    indices.push(2);
                    indices.sort_unstable();
                }
                let font = instance(&font, coord);
                assert_eq!(plan_variations(&font)[kind as usize], Some(0));
                assert_eq!(plan_lookups(&font, kind), existing(&indices));
                let shaped = shape_a(&font);
                if kind == LayoutTableKind::Gsub {
                    let glyph = if coord == 0.0 {
                        1
                    } else if add_defaults {
                        4
                    } else {
                        3
                    };
                    assert_eq!(shaped.glyph_infos()[0].glyph_id, glyph);
                } else {
                    let advance = ShaperFont::new(&font).default_glyph_h_advance(GlyphId::new(1));
                    let delta =
                        if coord == 0.0 { 20 } else { 30 } + if add_defaults { 40 } else { 0 };
                    assert_eq!(shaped.glyph_positions()[0].x_advance, advance + delta);
                }
            }
        }
    }
}

#[test]
fn feature_variations_apply_at_the_default_location() {
    // A record without conditions selects its alternate feature everywhere,
    // including the default location, where lookup 2 replaces lookup 0.
    let font = font(
        LayoutTableKind::Gsub,
        &variations(&conditional(&[], true), true),
    );
    assert_eq!(
        plan_variations(&font)[LayoutTableKind::Gsub as usize],
        Some(0)
    );
    assert_eq!(plan_lookups(&font, LayoutTableKind::Gsub), [2]);
    assert_eq!(shape_a(&font).glyph_infos()[0].glyph_id, 1);
    // Plans built for the font are found again by its key.
    let plan = ShapePlan::new(&font, Direction::LeftToRight, None, None, &[]);
    assert!(ShapePlanKey::new(&font, None, Direction::LeftToRight).matches(&plan));
}

#[test]
fn empty_resolved_set_replaces_defaults_even_at_default_coords() {
    for kind in [LayoutTableKind::Gsub, LayoutTableKind::Gpos] {
        let font = font(kind, &variations(&conditional(&[], false), false));
        assert_eq!(plan_lookups(&font, kind), Vec::<u16>::new());
        let shaped = shape_a(&font);
        assert_eq!(shaped.glyph_infos()[0].glyph_id, 1);
        let advance = ShaperFont::new(&font).default_glyph_h_advance(GlyphId::new(1));
        assert_eq!(shaped.glyph_positions()[0].x_advance, advance);
    }
}

#[test]
fn plan_keys_compare_locations_under_lookup_variations() {
    for kind in [LayoutTableKind::Gsub, LayoutTableKind::Gpos] {
        let font = font(kind, &variations(&conditional(&records(), false), true));
        let positive = instance(&font, 0.5);
        let same_set = instance(&font, 0.75);
        let negative = instance(&font, -0.5);
        let default = instance(&font, 0.0);
        let plan = ShapePlan::new(&positive, Direction::LeftToRight, None, None, &[]);
        for (font, matches) in [
            (&positive, true),
            (&same_set, false),
            (&negative, false),
            (&default, false),
        ] {
            assert_eq!(plan_variations(font)[kind as usize], Some(0));
            assert_eq!(
                ShapePlanKey::new(font, None, Direction::LeftToRight).matches(&plan),
                matches
            );
        }
    }
}

#[test]
fn variable_conditions_use_fractional_gdef_deltas() {
    let store = vec![
        0, 1, 0, 0, 0, 12, 0, 1, 0, 0, 0, 22, // ItemVariationStore.
        0, 1, 0, 1, 0, 0, 0x40, 0, 0x40, 0, // One axis and one region.
        0, 1, 0, 1, 0, 1, 0, 0, 0, 1, // One delta of 1.
    ];
    let records = vec![(Some(u16s(&[2, 0, 0, 0])), vec![0]), (None, vec![1])];
    for kind in [LayoutTableKind::Gsub, LayoutTableKind::Gpos] {
        let variations = variations(&conditional(&records, false), false);
        for store in [None, Some(store.clone())] {
            let font = font_with_store(kind, &variations, store.clone());
            for coord in [-0.5, 0.0, 0.5, 1.0] {
                let font = instance(&font, coord);
                let expected = if store.is_some() && coord > 0.0 {
                    vec![0, 1]
                } else {
                    vec![1]
                };
                assert_eq!(plan_lookups(&font, kind), expected);
                let shaped = shape_a(&font);
                if kind == LayoutTableKind::Gsub {
                    assert_eq!(
                        shaped.glyph_infos()[0].glyph_id,
                        if expected.len() == 2 { 3 } else { 1 }
                    );
                } else {
                    let advance = ShaperFont::new(&font).default_glyph_h_advance(GlyphId::new(1));
                    assert_eq!(
                        shaped.glyph_positions()[0].x_advance,
                        advance + if expected.len() == 2 { 30 } else { 20 }
                    );
                }
            }
        }
    }
}

#[test]
fn reused_plans_shape_correctly_across_lookup_variation_locations() {
    for kind in [LayoutTableKind::Gsub, LayoutTableKind::Gpos] {
        let font = font(kind, &variations(&conditional(&records(), false), true));
        let mut plans = Vec::new();
        for coord in [0.5, 0.75, -0.5, 0.0, 1.0, -1.0, 0.0, 0.5] {
            let instance = instance(&font, coord);
            let key = ShapePlanKey::new(&instance, None, Direction::LeftToRight);
            let index = plans
                .iter()
                .position(|plan| key.matches(plan))
                .unwrap_or_else(|| {
                    plans.push(ShapePlan::new(
                        &instance,
                        Direction::LeftToRight,
                        None,
                        None,
                        &[],
                    ));
                    plans.len() - 1
                });
            let mut reused = Buffer::new();
            reused.push_str("a");
            reused.set_direction(Direction::LeftToRight);
            shape(
                &ShaperFont::new(&instance),
                &mut reused,
                ShapeOptions::new().plan(Some(&plans[index])),
            )
            .unwrap();
            let fresh = shape_a(&instance);
            assert_eq!(reused.glyph_infos().len(), 1);
            assert_eq!(
                reused.glyph_infos()[0].glyph_id,
                fresh.glyph_infos()[0].glyph_id
            );
            assert_eq!(
                reused.glyph_infos()[0].cluster,
                fresh.glyph_infos()[0].cluster
            );
            let actual = reused.glyph_positions()[0];
            let expected = fresh.glyph_positions()[0];
            assert_eq!(
                (
                    actual.x_advance,
                    actual.y_advance,
                    actual.x_offset,
                    actual.y_offset
                ),
                (
                    expected.x_advance,
                    expected.y_advance,
                    expected.x_offset,
                    expected.y_offset
                ),
                "{kind:?}, coord {coord}"
            );
        }
        // Plans are keyed by location, so each distinct one gets its own.
        assert_eq!(plans.len(), 6);
        let key = ShapePlanKey::new(&font, None, Direction::LeftToRight);
        let plan = plans.iter().find(|plan| key.matches(plan)).unwrap();
        let mut buffer = Buffer::new();
        buffer.push_str("a");
        buffer.set_direction(Direction::LeftToRight);
        shape(
            &ShaperFont::new(&font),
            &mut buffer,
            ShapeOptions::new().plan(Some(plan)),
        )
        .unwrap();
        let fresh = shape_a(&font);
        assert_eq!(
            buffer.glyph_infos()[0].glyph_id,
            fresh.glyph_infos()[0].glyph_id
        );
        assert_eq!(
            buffer.glyph_positions()[0].x_advance,
            fresh.glyph_positions()[0].x_advance
        );
    }
}

#[test]
fn malformed_lookup_tables_fall_back_but_malformed_conditions_do_not_match() {
    for kind in [LayoutTableKind::Gsub, LayoutTableKind::Gpos] {
        let valid = conditional(&[(None, vec![0])], false);
        let mut bad_version = valid.clone();
        put16(&mut bad_version, 0, 2);
        for conditional in [bad_version, valid[..10].to_vec()] {
            let font = font(kind, &variations(&conditional, false));
            assert_eq!(plan_lookups(&font, kind), [0]);
        }
        let base = variations(&valid, false);
        let mut null_table = base.clone();
        put32(&mut null_table, 14, 0);
        let mut invalid_feature = base.clone();
        put16(&mut invalid_feature, 12, usize::from(u16::MAX));
        let mut duplicate_features = base.clone();
        duplicate_features.splice(18..18, base[12..18].iter().copied());
        put32(&mut duplicate_features, 8, 2);
        put32(&mut duplicate_features, 14, 24);
        put32(&mut duplicate_features, 20, 24);
        for variations in [
            null_table,
            invalid_feature,
            duplicate_features,
            base[..17].to_vec(),
        ] {
            let font = font(kind, &variations);
            assert_eq!(plan_lookups(&font, kind), [0]);
            let shaped = shape_a(&font);
            if kind == LayoutTableKind::Gsub {
                assert_eq!(shaped.glyph_infos()[0].glyph_id, 2);
            }
        }

        let mut bad_offset = valid.clone();
        put32(&mut bad_offset, 10, u32::MAX as usize);
        let mut null_indices = valid.clone();
        put32(&mut null_indices, 14, 0);
        let mut truncated_indices = valid.clone();
        truncated_indices.pop();
        for conditional in [bad_offset, null_indices, truncated_indices] {
            let font = font(kind, &variations(&conditional, false));
            assert_eq!(plan_lookups(&font, kind), Vec::<u16>::new());
            assert_eq!(shape_a(&font).glyph_infos()[0].glyph_id, 1);
        }
        // A condition of an unknown format is false, so negating one matches.
        let not_unknown = conditional(&[(Some(vec![0, 5, 0, 0, 5, 0, 99]), vec![0])], false);
        let negated = font(kind, &variations(&not_unknown, false));
        assert_eq!(plan_lookups(&negated, kind), [0]);
        // A child past the end of the data is an error, which never matches.
        let not_unreadable = conditional(&[(Some(vec![0, 5, 0, 1, 0]), vec![0])], false);
        let unreadable = font(kind, &variations(&not_unreadable, false));
        assert_eq!(plan_lookups(&unreadable, kind), Vec::<u16>::new());
    }
}

#[test]
fn lookup_condition_count_and_offsets_are_full_width() {
    // Only the final record adds lookup 1: truncating the 32-bit count loses it.
    let count = 0x1_0000;
    let mut conditional = vec![0; 10 + count * 8];
    put16(&mut conditional, 0, 1);
    put32(&mut conditional, 6, count);
    let empty = conditional.len();
    conditional.extend_from_slice(&u16s(&[0]));
    let nonempty = conditional.len();
    conditional.extend_from_slice(&u16s(&[1, 1]));
    for index in 0..count {
        put32(
            &mut conditional,
            14 + index * 8,
            if index + 1 == count { nonempty } else { empty },
        );
    }
    let condition = conditional.len();
    put32(&mut conditional, 10 + (count - 1) * 8, condition);
    conditional.extend_from_slice(&range(0.0, 0.0));
    let font = font(LayoutTableKind::Gpos, &variations(&conditional, false));
    assert_eq!(plan_lookups(&font, LayoutTableKind::Gpos), [1]);
}

#[test]
fn overlapping_lookup_index_lists_are_bounded() {
    // Each record reads its list one byte further into a run of 0x01 bytes:
    // 257 indices of lookup 257, which does not exist, from an 8-byte record.
    // Past 65536 indices in all, the feature keeps its own lookups.
    for (count, expected) in [(255, vec![]), (256, vec![0])] {
        let mut conditional = vec![0; 10 + count * 8];
        put16(&mut conditional, 0, 1);
        put32(&mut conditional, 6, count);
        let lists = conditional.len();
        for index in 0..count {
            put32(&mut conditional, 14 + index * 8, lists + index);
        }
        conditional.resize(lists + count + 2 + 257 * 2, 1);
        for kind in [LayoutTableKind::Gsub, LayoutTableKind::Gpos] {
            let font = font(kind, &variations(&conditional, false));
            assert_eq!(plan_lookups(&font, kind), expected);
        }
    }
}
