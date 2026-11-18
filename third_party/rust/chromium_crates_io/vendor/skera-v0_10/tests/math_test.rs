//! Targeted tests for MATH closure and device bases.
use skera::{subset_font, Plan, SubsetFlags, DEFAULT_LAYOUT_FEATURES};
use write_fonts::{
    from_obj::ToOwnedTable,
    read::{collections::IntSet, FontData, FontRef, TableProvider},
    tables::{
        gdef::Gdef,
        gpos::{Gpos, PositionLookup, PositionLookupList, SinglePos, ValueRecord},
        gsub::*,
        layout::*,
        math::*,
        variations::*,
    },
    types::{F2Dot14, GlyphId, GlyphId16, Tag},
    FontBuilder,
};

fn source() -> Vec<u8> {
    std::fs::read("test-data/fonts/STIXTwoMath-Regular.ttf").unwrap()
}

fn plan(font: &FontRef, gids: &str, unicodes: &str, flags: SubsetFlags) -> Plan {
    Plan::new(
        &skera::populate_gids(gids).unwrap(),
        &skera::parse_unicodes(unicodes).unwrap(),
        font,
        flags,
        &IntSet::empty(),
        &IntSet::all(),
        &DEFAULT_LAYOUT_FEATURES.iter().copied().collect(),
        &IntSet::all(),
        &IntSet::all(),
    )
}

fn coverage(gids: &[u16]) -> CoverageTable {
    CoverageTable::from_iter(gids.iter().map(|&g| GlyphId16::new(g)))
}

fn value(value: i16) -> MathValueRecord {
    MathValueRecord::new(
        value.into(),
        Some(Device::new(10, 13, &[-2, -1, 0, 1]).into()),
    )
}

fn variable_value(value: i16, inner: u16) -> MathValueRecord {
    MathValueRecord::new(value.into(), Some(VariationIndex::new(1, inner).into()))
}

fn construction(variant: u16, part: u16) -> MathGlyphConstruction {
    MathGlyphConstruction::new(
        Some(GlyphAssembly::new(
            value(30),
            vec![GlyphPartRecord::new(
                GlyphId16::new(part),
                20.into(),
                40.into(),
                200.into(),
                PartFlags::EXTENDER_FLAG,
            )],
        )),
        vec![MathGlyphVariantRecord::new(
            GlyphId16::new(variant),
            300.into(),
        )],
    )
}

fn synthetic_font() -> Vec<u8> {
    let bytes = source();
    let font = FontRef::new(&bytes).unwrap();
    let mut builder = FontBuilder::new();
    for record in font.table_directory().table_records() {
        builder.add_raw(record.tag(), font.data_for_tag(record.tag()).unwrap());
    }
    let constants = MathConstants {
        math_leading: value(12),
        radical_kern_after_degree: value(-12),
        ..Default::default()
    };
    let gids = [2, 5, 6, 8, 10, 11];
    let kern = MathKern::new(vec![value(100)], vec![value(-10), value(20)]);
    let math = Math::new(
        constants,
        MathGlyphInfo::new(
            Some(MathItalicsCorrectionInfo::new(
                coverage(&gids),
                gids.iter().map(|&g| value(g as i16)).collect(),
            )),
            Some(MathTopAccentAttachment::new(
                coverage(&gids),
                gids.iter().map(|&g| value(100 + g as i16)).collect(),
            )),
            Some(coverage(&gids)),
            Some(MathKernInfo::new(
                coverage(&[2, 5]),
                vec![
                    MathKernInfoRecord::new(Some(kern.clone()), None, None, Some(kern.clone())),
                    MathKernInfoRecord::new(None, Some(kern.clone()), Some(kern), None),
                ],
            )),
        ),
        MathVariants::new(
            10.into(),
            Some(coverage(&[2, 5, 8])),
            Some(coverage(&[2])),
            vec![construction(5, 6), construction(7, 7), construction(9, 9)],
            vec![construction(10, 11)],
        ),
    );
    builder.add_table(&math).unwrap();
    let lookup = SubstitutionLookup::Single(Lookup::new(
        LookupFlag::empty(),
        vec![SingleSubstFormat2::new(coverage(&[2]), vec![GlyphId16::new(8)]).into()],
    ));
    builder
        .add_table(&Gsub::new(
            ScriptList::new(vec![ScriptRecord::new(
                Tag::new(b"DFLT"),
                Script::new(Some(LangSys::new(vec![0])), vec![]),
            )]),
            FeatureList::new(vec![FeatureRecord::new(
                Tag::new(b"liga"),
                Feature::new(None, vec![0]),
            )]),
            SubstitutionLookupList::new(vec![lookup]),
        ))
        .unwrap();
    builder.build()
}

fn variable_math_font() -> Vec<u8> {
    let bytes = source();
    let font = FontRef::new(&bytes).unwrap();
    let mut builder = FontBuilder::new();
    for record in font.table_directory().table_records() {
        builder.add_raw(record.tag(), font.data_for_tag(record.tag()).unwrap());
    }

    let kern = MathKern::new(
        vec![variable_value(100, 3)],
        vec![variable_value(-10, 4), variable_value(20, 5)],
    );
    let math = Math::new(
        MathConstants {
            math_leading: variable_value(12, 0),
            ..Default::default()
        },
        MathGlyphInfo::new(
            Some(MathItalicsCorrectionInfo::new(
                coverage(&[2]),
                vec![variable_value(2, 1)],
            )),
            Some(MathTopAccentAttachment::new(
                coverage(&[2]),
                vec![variable_value(102, 2)],
            )),
            None,
            Some(MathKernInfo::new(
                coverage(&[2]),
                vec![MathKernInfoRecord::new(Some(kern), None, None, None)],
            )),
        ),
        MathVariants::new(
            10.into(),
            Some(coverage(&[2])),
            None,
            vec![MathGlyphConstruction::new(
                Some(GlyphAssembly::new(variable_value(30, 6), vec![])),
                vec![],
            )],
            vec![],
        ),
    );
    builder.add_table(&math).unwrap();

    let axis = RegionAxisCoordinates::new(F2Dot14::ZERO, F2Dot14::ONE, F2Dot14::ONE);
    let regions = VariationRegionList::new(1, vec![VariationRegion::new(vec![axis])]);
    let data = |values: &[i16]| {
        ItemVariationData::new(
            values.len() as u16,
            1,
            vec![0],
            values
                .iter()
                .flat_map(|value| value.to_be_bytes())
                .collect(),
        )
    };
    let gdef = Gdef {
        item_var_store: Some(ItemVariationStore::new(
            regions,
            vec![
                Some(data(&[99])),
                Some(data(&[10, 11, 12, 13, 14, 15, 16, 17])),
            ],
        ))
        .into(),
        ..Default::default()
    };
    // Exercise the shared GDEF store through GPOS as well as MATH.
    builder
        .add_table(&Gpos::new(
            ScriptList::new(vec![ScriptRecord::new(
                Tag::new(b"DFLT"),
                Script::new(Some(LangSys::new(vec![0])), vec![]),
            )]),
            FeatureList::new(vec![FeatureRecord::new(
                Tag::new(b"kern"),
                Feature::new(None, vec![0]),
            )]),
            PositionLookupList::new(vec![PositionLookup::Single(Lookup::new(
                LookupFlag::empty(),
                vec![SinglePos::format_1(
                    coverage(&[2]),
                    ValueRecord::new()
                        .with_x_advance(40)
                        .with_x_advance_device(VariationIndex::new(1, 0)),
                )],
            ))]),
        ))
        .unwrap();
    builder.add_table(&gdef).unwrap();
    builder.build()
}

fn variable_math_font_with_gdef(gdef: Option<Gdef>) -> Vec<u8> {
    let bytes = variable_math_font();
    let font = FontRef::new(&bytes).unwrap();
    let mut builder = FontBuilder::new();
    for record in font.table_directory().table_records() {
        if record.tag() != Tag::new(b"GDEF") {
            builder.add_raw(record.tag(), font.data_for_tag(record.tag()).unwrap());
        }
    }
    if let Some(gdef) = gdef {
        builder.add_table(&gdef).unwrap();
    }
    builder.build()
}

fn device_varidx(record: &write_fonts::read::tables::math::MathValueRecord, data: FontData) -> u32 {
    let write_fonts::read::tables::layout::DeviceOrVariationIndex::VariationIndex(index) =
        record.device(data).unwrap().unwrap()
    else {
        panic!("expected variation index")
    };
    ((index.delta_set_outer_index() as u32) << 16) | index.delta_set_inner_index() as u32
}

#[test]
fn math_variation_indices_are_collected_and_remapped() {
    let bytes = variable_math_font();
    let font = FontRef::new(&bytes).unwrap();
    let bytes = subset_font(
        &font,
        &plan(&font, "2", "", SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS),
    )
    .unwrap();
    let font = FontRef::new(&bytes).unwrap();

    let store = font.gdef().unwrap().item_var_store().unwrap().unwrap();
    assert_eq!(store.item_variation_data_count(), 1);
    let data = store.item_variation_data().get(0).unwrap().unwrap();
    assert_eq!(data.item_count(), 7);
    assert_eq!(data.delta_set(0).collect::<Vec<_>>(), [10]);
    assert_eq!(data.delta_set(6).collect::<Vec<_>>(), [16]);

    let math = font.math().unwrap();
    let constants = math.math_constants().unwrap();
    assert_eq!(
        device_varidx(constants.math_leading(), constants.offset_data()),
        0
    );
    let info = math.math_glyph_info().unwrap();
    let italics = info.math_italics_correction_info().unwrap().unwrap();
    assert_eq!(
        device_varidx(&italics.italics_correction()[0], italics.offset_data()),
        1
    );
    let variants = math.math_variants().unwrap();
    let construction = variants.vert_glyph_constructions().get(0).unwrap();
    let assembly = construction.glyph_assembly().unwrap().unwrap();
    assert_eq!(
        device_varidx(assembly.italics_correction(), assembly.offset_data()),
        6
    );
}

#[test]
fn math_is_kept_when_gdef_is_dropped() {
    let bytes = variable_math_font();
    let font = FontRef::new(&bytes).unwrap();
    let plan = Plan::new(
        &skera::populate_gids("2").unwrap(),
        &IntSet::empty(),
        &font,
        SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS,
        &[Tag::new(b"GDEF")].into_iter().collect(),
        &IntSet::all(),
        &DEFAULT_LAYOUT_FEATURES.iter().copied().collect(),
        &IntSet::all(),
        &IntSet::all(),
    );
    let bytes = subset_font(&font, &plan).unwrap();
    let font = FontRef::new(&bytes).unwrap();

    assert!(font.gdef().is_err());
    let math = font.math().unwrap();
    let constants = math.math_constants().unwrap();
    assert!(constants
        .math_leading()
        .device(constants.offset_data())
        .is_none());
    let italics = math
        .math_glyph_info()
        .unwrap()
        .math_italics_correction_info()
        .unwrap()
        .unwrap();
    assert!(italics.italics_correction()[0]
        .device(italics.offset_data())
        .is_none());
    let construction = math
        .math_variants()
        .unwrap()
        .vert_glyph_constructions()
        .get(0)
        .unwrap();
    let assembly = construction.glyph_assembly().unwrap().unwrap();
    assert!(assembly
        .italics_correction()
        .device(assembly.offset_data())
        .is_none());
    let gpos = font.gpos().unwrap();
    let lookup = gpos.lookup_list().unwrap().lookups().get(0).unwrap();
    let write_fonts::read::tables::gpos::PositionLookup::Single(lookup) = lookup else {
        panic!("expected single positioning")
    };
    let write_fonts::read::tables::gpos::SinglePos::Format1(single) =
        lookup.subtables().get(0).unwrap()
    else {
        panic!("expected format 1")
    };
    assert_eq!(single.value_record().x_advance(), Some(40));
    assert!(single.value_record().x_advance_device().is_none());
}

#[test]
fn math_drops_unmappable_variation_devices_instead_of_the_table() {
    for bytes in [
        variable_math_font_with_gdef(None),
        variable_math_font_with_gdef(Some(Gdef::default())),
    ] {
        let font = FontRef::new(&bytes).unwrap();
        let bytes = subset_font(
            &font,
            &plan(&font, "2", "", SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS),
        )
        .unwrap();
        let font = FontRef::new(&bytes).unwrap();
        let constants = font.math().unwrap().math_constants().unwrap();

        assert!(constants
            .math_leading()
            .device(constants.offset_data())
            .is_none());
    }
}

#[test]
fn closure_precedes_gsub_and_only_selected_roots_keep_constructions() {
    let bytes = synthetic_font();
    let font = FontRef::new(&bytes).unwrap();
    for retain in [false, true] {
        let flags = if retain {
            SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS
        } else {
            SubsetFlags::default()
        };
        let plan = plan(&font, "2", "", flags);
        let mapping = plan
            .old_to_new_glyph_mapping()
            .collect::<std::collections::BTreeMap<_, _>>();
        for gid in [2, 5, 6, 8, 10, 11] {
            assert!(mapping.contains_key(&GlyphId::new(gid)));
        }
        let bytes = subset_font(&font, &plan).unwrap();
        let subset = FontRef::new(&bytes).unwrap();
        let math = subset.math().unwrap();
        let variants = math.math_variants().unwrap();
        let root = GlyphId16::new(mapping[&GlyphId::new(2)].to_u32() as u16);
        for c in [
            variants.vert_glyph_coverage(),
            variants.horiz_glyph_coverage(),
        ] {
            assert_eq!(c.unwrap().unwrap().iter().collect::<Vec<_>>(), [root]);
        }
        let values = math
            .math_glyph_info()
            .unwrap()
            .math_italics_correction_info()
            .unwrap()
            .unwrap();
        let expected =
            [2, 5, 6, 10, 11].map(|g| GlyphId16::new(mapping[&GlyphId::new(g)].to_u32() as u16));
        assert_eq!(
            values.coverage().unwrap().iter().collect::<Vec<_>>(),
            expected
        );
        assert_eq!(
            values
                .italics_correction()
                .iter()
                .map(|v| v.value().to_i16())
                .collect::<Vec<_>>(),
            [2, 5, 6, 10, 11]
        );
    }
}

#[test]
fn devices_keep_their_enclosing_table_base() {
    let bytes = synthetic_font();
    let font = FontRef::new(&bytes).unwrap();
    let original: Math = font.math().unwrap().to_owned_table();
    // HarfBuzz copies MATH devices even when hinting is removed elsewhere.
    for flags in [SubsetFlags::default(), SubsetFlags::SUBSET_FLAGS_NO_HINTING] {
        let bytes = subset_font(&font, &plan(&font, "*", "", flags)).unwrap();
        let subset = FontRef::new(&bytes).unwrap();
        let actual: Math = subset.math().unwrap().to_owned_table();
        assert_eq!(actual, original);
        let constants = subset.math().unwrap().math_constants().unwrap();
        assert_eq!(
            constants
                .math_leading()
                .value_for_ppem(constants.offset_data(), 10)
                .delta_px,
            -2
        );
    }
}

#[test]
fn dropping_math_skips_variant_closure() {
    let bytes = synthetic_font();
    let font = FontRef::new(&bytes).unwrap();
    let plan = Plan::new(
        &skera::populate_gids("2").unwrap(),
        &IntSet::empty(),
        &font,
        SubsetFlags::default(),
        &[Tag::new(b"MATH"), Tag::new(b"GSUB")].into_iter().collect(),
        &IntSet::empty(),
        &IntSet::empty(),
        &IntSet::empty(),
        &IntSet::empty(),
    );
    assert!(!plan
        .old_to_new_glyph_mapping()
        .any(|(g, _)| g == GlyphId::new(5)));
    let bytes = subset_font(&font, &plan).unwrap();
    assert!(FontRef::new(&bytes).unwrap().math().is_err());
}

#[test]
fn malformed_required_math_offsets_fail_subsetting() {
    let bytes = synthetic_font();
    let source = FontRef::new(&bytes).unwrap();
    let tag = Tag::new(b"MATH");
    let mut builder = FontBuilder::new();
    for record in source.table_directory().table_records() {
        builder.add_raw(record.tag(), source.data_for_tag(record.tag()).unwrap());
    }
    builder.add_raw(tag, &source.data_for_tag(tag).unwrap().as_bytes()[..10]);
    let bytes = builder.build();
    let font = FontRef::new(&bytes).unwrap();
    assert!(matches!(
        subset_font(&font, &plan(&font, "2", "", SubsetFlags::default())),
        Err(skera::SubsetError::SubsetTableError(t)) if t == tag
    ));
}
