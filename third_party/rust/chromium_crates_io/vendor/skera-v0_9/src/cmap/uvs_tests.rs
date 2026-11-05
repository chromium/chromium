use super::*;
use crate::{subset_font, SubsetFlags};
use font_test_data::cmap::{font_with_cmaps, format12, format14, format15, table};
use read_fonts::{
    tables::cmap::MapVariant,
    types::{NameId, Tag},
    FontData, FontRead, TableProvider,
};

fn serialize_variant(source: &CmapSubtable, plan: &Plan) -> Result<Vec<u8>, SerializeErrorFlags> {
    let mut s = Serializer::new(4096);
    s.start_serialize().unwrap();
    source.serialize(&mut s, plan, &[])?;
    s.end_serialize();
    Ok(s.copy_bytes())
}

fn wide_plan() -> Plan {
    let mut plan = Plan::default();
    plan.unicodes.extend([65, 67, 68, 69, 70, 0xe0100]);
    plan.glyphs_requested.insert(GlyphId::new(7));
    plan.glyph_map.extend([
        (GlyphId::new(65536), GlyphId::new(3)),
        (GlyphId::new(70000), GlyphId::new(70000)),
        (GlyphId::new(0xffffff), GlyphId::new(0xffffff)),
        (GlyphId::new(7), GlyphId::new(4)),
    ]);
    plan
}

#[test]
fn cmap15_subset_preserves_wide_format_and_prunes_uvs_records() {
    let data = format15(
        0xe0100,
        &[65, 66, 67],
        &[(68, 65536), (69, 70000), (70, 0xffffff), (71, 7), (72, 8)],
    );
    let source = CmapSubtable::read(FontData::new(&data)).unwrap();
    let output = serialize_variant(&source, &wide_plan()).unwrap();
    let output = Cmap15::read(FontData::new(&output)).unwrap();
    assert_eq!(output.format(), 15);
    assert_eq!(output.length() as usize, output.offset_data().len());
    assert_eq!(output.num_var_selector_records(), 1);
    let output = read_fonts::tables::cmap::UvsSubtable::from(output);
    for (cp, expected) in [
        (65, Some(MapVariant::UseDefault)),
        (66, None),
        (67, Some(MapVariant::UseDefault)),
        (68, Some(MapVariant::Variant(GlyphId::new(3)))),
        (69, Some(MapVariant::Variant(GlyphId::new(70000)))),
        (70, Some(MapVariant::Variant(GlyphId::new(0xffffff)))),
        (71, Some(MapVariant::Variant(GlyphId::new(4)))),
        (72, None),
    ] {
        assert_eq!(output.map_variant(cp as u32, 0xe0100u32), expected);
    }
    assert_eq!(output.iter().count(), 6);
}

#[test]
fn uvs_subset_omits_empty_selectors_and_missing_glyphs() {
    for format in [14, 15] {
        let data = if format == 14 {
            format14(0xe0100, &[], &[(68, 25)])
        } else {
            format15(0xe0100, &[], &[(68, 70000)])
        };
        let source = CmapSubtable::read(FontData::new(&data)).unwrap();
        assert!(serialize_variant(&source, &Plan::default())
            .unwrap()
            .is_empty());
        let mut plan = Plan::default();
        plan.unicodes.extend([68, 0xe0100]);
        assert!(serialize_variant(&source, &plan).unwrap().is_empty());
    }
}

#[test]
fn uvs_subset_rejects_glyph_and_array_overflow() {
    for (format, old_gid, max_gid) in [(14, 25, 0xffff), (15, 70000, 0xffffff)] {
        let data = if format == 14 {
            format14(0xe0100, &[], &[(68, old_gid as u16)])
        } else {
            format15(0xe0100, &[], &[(68, old_gid)])
        };
        let source = CmapSubtable::read(FontData::new(&data)).unwrap();
        let mut plan = Plan::default();
        plan.unicodes.extend([68, 0xe0100]);
        plan.glyph_map
            .insert(GlyphId::new(old_gid), GlyphId::new(max_gid));
        assert!(!serialize_variant(&source, &plan).unwrap().is_empty());
        plan.glyph_map
            .insert(GlyphId::new(old_gid), GlyphId::new(max_gid + 1));
        assert_eq!(
            serialize_variant(&source, &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
        );

        let mut truncated = data.clone();
        truncated[6..10].copy_from_slice(&2u32.to_be_bytes());
        let source = CmapSubtable::read(FontData::new(&truncated)).unwrap();
        assert_eq!(
            serialize_variant(&source, &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
        let mut truncated = data;
        truncated.pop();
        let source = CmapSubtable::read(FontData::new(&truncated)).unwrap();
        assert_eq!(
            serialize_variant(&source, &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
    }
}

#[test]
fn cmap15_subset_planner_closes_preferred_variation_glyphs() {
    let nominal = format12(&[(65, 1)]);
    let narrow = format14(0xe0100, &[], &[(65, 24)]);
    let wide = format15(0xe0100, &[], &[(65, 25)]);
    let cmap = table(&[(0, 4, &nominal), (0, 5, &narrow), (0, 5, &wide)]);
    let data = font_with_cmaps(Some(&cmap), None);
    let font = FontRef::new(&data).unwrap();
    let plan = Plan::new(
        &IntSet::empty(),
        &IntSet::from_iter([65, 0xe0100]),
        &font,
        SubsetFlags::SUBSET_FLAGS_DEFAULT,
        &IntSet::<Tag>::empty(),
        &IntSet::all(),
        &IntSet::all(),
        &IntSet::<NameId>::empty(),
        &IntSet::empty(),
    );
    assert!(plan.unicodes.contains(0xe0100));
    assert!(plan.glyphset_gsub.contains(GlyphId::new(25)));
    assert!(!plan.glyphset_gsub.contains(GlyphId::new(24)));
    assert_eq!(plan.num_output_glyphs, 3);
    let output = subset_font(&font, &plan).unwrap();
    let output = FontRef::new(&output).unwrap();
    let cmap = output.cmap().unwrap();
    let (_, variation) = cmap.uvs_subtable().unwrap();
    assert_eq!(variation.format(), 15);
    assert_eq!(
        variation.map_variant(65u32, 0xe0100u32),
        Some(MapVariant::Variant(GlyphId::new(2)))
    );
    assert_eq!(output.maxp().unwrap().num_glyphs(), 3);
}
