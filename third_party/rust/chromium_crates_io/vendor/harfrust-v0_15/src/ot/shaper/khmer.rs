use super::indic::category;
use super::syllabic::*;
use super::*;
use crate::ot::map::*;
use crate::plan::ShapePlan;
use crate::unicode::{CharExt, Codepoint};
use crate::ShaperFont;
use crate::{GlyphInfo, Mask, Tag};
use alloc::boxed::Box;

pub const KHMER_SHAPER: OtShaper = OtShaper {
    collect_features: Some(collect_features),
    override_features: Some(override_features),
    create_data: Some(|plan| Box::new(KhmerShapePlan::new(plan))),
    preprocess_text: None,
    postprocess_glyphs: None,
    normalization_preference: NormalizationMode::ComposedDiacriticsNoShortCircuit,
    decompose: Some(decompose),
    compose: Some(compose),
    setup_masks: Some(setup_masks),
    gpos_tag: None,
    reorder_marks: None,
    zero_width_marks: ZeroWidthMarks::None,
    fallback_position: false,
};

impl GlyphInfo {
    declare_buffer_var_alias!(
        OT_SHAPER_VAR_U8_CATEGORY_VAR,
        u8,
        KHMER_CATEGORY_VAR,
        khmer_category,
        set_khmer_category
    );
}

const KHMER_FEATURES: &[(Tag, MapFeatureFlags)] = &[
    // Basic features.
    // These features are applied all at once, before reordering, constrained
    // to the syllable.
    (Tag::new(b"pref"), F_MANUAL_JOINERS | F_PER_SYLLABLE),
    (Tag::new(b"blwf"), F_MANUAL_JOINERS | F_PER_SYLLABLE),
    (Tag::new(b"abvf"), F_MANUAL_JOINERS | F_PER_SYLLABLE),
    (Tag::new(b"pstf"), F_MANUAL_JOINERS | F_PER_SYLLABLE),
    (Tag::new(b"cfar"), F_MANUAL_JOINERS | F_PER_SYLLABLE),
    // Other features.
    // These features are applied all at once after clearing syllables.
    (Tag::new(b"pres"), F_GLOBAL_MANUAL_JOINERS),
    (Tag::new(b"abvs"), F_GLOBAL_MANUAL_JOINERS),
    (Tag::new(b"blws"), F_GLOBAL_MANUAL_JOINERS),
    (Tag::new(b"psts"), F_GLOBAL_MANUAL_JOINERS),
];

// Must be in the same order as the KHMER_FEATURES array.
mod khmer_feature {
    pub const PREF: usize = 0;
    pub const BLWF: usize = 1;
    pub const ABVF: usize = 2;
    pub const PSTF: usize = 3;
    pub const CFAR: usize = 4;
}

impl GlyphInfo {
    fn set_khmer_properties(&mut self) {
        let u = self.glyph_id;
        let (cat, _) = indic_table::get_categories(u);

        self.set_khmer_category(cat);
    }
}

struct KhmerShapePlan {
    mask_array: [Mask; KHMER_FEATURES.len()],
}

impl KhmerShapePlan {
    fn new(plan: &ShapePlan) -> Self {
        let mut mask_array = [0; KHMER_FEATURES.len()];
        for (i, feature) in KHMER_FEATURES.iter().enumerate() {
            mask_array[i] = if feature.1 & F_GLOBAL != 0 {
                0
            } else {
                plan.ot_map.get_1_mask(feature.0)
            }
        }

        KhmerShapePlan { mask_array }
    }
}

fn collect_features(planner: &mut ShapePlanner) {
    // Do this before any lookups have been applied.
    planner.ot_map.add_gsub_pause(Some(setup_syllables));
    planner.ot_map.add_gsub_pause(Some(reorder_khmer));

    // Testing suggests that Uniscribe does NOT pause between basic
    // features.  Test with KhmerUI.ttf and the following three
    // sequences:
    //
    //   U+1789,U+17BC
    //   U+1789,U+17D2,U+1789
    //   U+1789,U+17D2,U+1789,U+17BC
    //
    // https://github.com/harfbuzz/harfbuzz/issues/974
    planner
        .ot_map
        .enable_feature(Tag::new(b"locl"), F_PER_SYLLABLE, 1);
    planner
        .ot_map
        .enable_feature(Tag::new(b"ccmp"), F_PER_SYLLABLE, 1);

    for feature in KHMER_FEATURES.iter().take(5) {
        planner.ot_map.add_feature(feature.0, feature.1, 1);
    }

    /* https://github.com/harfbuzz/harfbuzz/issues/3531 */
    planner.ot_map.add_gsub_pause(Some(syllabic_clear_var)); // Don't need syllables anymore.

    for feature in KHMER_FEATURES.iter().skip(5) {
        planner.ot_map.add_feature(feature.0, feature.1, 1);
    }
}

fn setup_syllables(_: &ShapePlan, _: &ShaperFont<'_, '_>, buffer: &mut Buffer) -> Option<bool> {
    buffer.allocate_var(GlyphInfo::SYLLABLE_VAR);

    khmer_machine::find_syllables_khmer(buffer);

    let mut start = 0;
    let mut end = buffer.next_syllable(0);
    while start < buffer.len {
        buffer.unsafe_to_break(Some(start), Some(end));
        start = end;
        end = buffer.next_syllable(start);
    }

    Some(false)
}

fn reorder_khmer(plan: &ShapePlan, font: &ShaperFont<'_, '_>, buffer: &mut Buffer) -> Option<bool> {
    use super::khmer_machine::SyllableType;

    let mut ret = false;
    if !buffer_message!(buffer, font, "start reordering khmer") {
        buffer.deallocate_var(GlyphInfo::KHMER_CATEGORY_VAR);
        return Some(ret);
    }

    if insert_dotted_circles(
        font,
        buffer,
        SyllableType::BrokenCluster as u8,
        category::OT_DOTTEDCIRCLE,
        Some(category::OT_Repha),
        None,
    )? {
        ret = true;
    }

    let khmer_plan = plan.data::<KhmerShapePlan>();

    let mut start = 0;
    let mut end = buffer.next_syllable(0);
    while start < buffer.len {
        if reorder_syllable_khmer(khmer_plan, start, end, buffer).is_none() {
            buffer.index_failed();
        }
        start = end;
        end = buffer.next_syllable(start);
    }

    buffer.deallocate_var(GlyphInfo::KHMER_CATEGORY_VAR);

    buffer_message!(buffer, font, "end reordering khmer");
    Some(ret)
}

fn reorder_syllable_khmer(
    khmer_plan: &KhmerShapePlan,
    start: usize,
    end: usize,
    buffer: &mut Buffer,
) -> Option<()> {
    use super::khmer_machine::SyllableType;

    let syllable_type = match buffer.info.get(start)?.syllable() & 0x0F {
        0 => SyllableType::ConsonantSyllable,
        1 => SyllableType::BrokenCluster,
        2 => SyllableType::NonKhmerCluster,
        _ => unreachable!(),
    };

    match syllable_type {
        SyllableType::ConsonantSyllable | SyllableType::BrokenCluster => {
            reorder_consonant_syllable(khmer_plan, start, end, buffer)?;
        }
        SyllableType::NonKhmerCluster => {}
    }
    Some(())
}

// Rules from:
// https://docs.microsoft.com/en-us/typography/script-development/devanagari
fn reorder_consonant_syllable(
    plan: &KhmerShapePlan,
    start: usize,
    end: usize,
    buffer: &mut Buffer,
) -> Option<()> {
    // Setup masks.
    {
        // Post-base
        let mask = plan.mask_array[khmer_feature::BLWF]
            | plan.mask_array[khmer_feature::ABVF]
            | plan.mask_array[khmer_feature::PSTF];
        for info in buffer.info.get_mut(start + 1..end)? {
            info.mask |= mask;
        }
    }

    let mut num_coengs = 0;
    for i in start + 1..end {
        // When a COENG + (Cons | IndV) combination are found (and subscript count
        // is less than two) the character combination is handled according to the
        // subscript type of the character following the COENG.
        //
        // ...
        //
        // Subscript Type 2 - The COENG + RO characters are reordered to immediately
        // before the base glyph. Then the COENG + RO characters are assigned to have
        // the 'pref' OpenType feature applied to them.
        let cat = buffer.info.get(i)?.indic_category();
        if cat == category::OT_H && num_coengs <= 2 && i + 1 < end {
            num_coengs += 1;

            if buffer.info.get(i + 1)?.indic_category() == category::OT_Ra {
                for info in buffer.info.get_mut(i..i + 2)? {
                    info.mask |= plan.mask_array[khmer_feature::PREF];
                }

                // Move the Coeng,Ro sequence to the start.
                buffer.merge_clusters(start, i + 2);
                buffer.info.get_mut(start..i + 2)?.rotate_right(2);

                // Mark the subsequent stuff with 'cfar'.  Used in Khmer.
                // Read the feature spec.
                // This allows distinguishing the following cases with MS Khmer fonts:
                // U+1784,U+17D2,U+179A,U+17D2,U+1782
                // U+1784,U+17D2,U+1782,U+17D2,U+179A
                if plan.mask_array[khmer_feature::CFAR] != 0 {
                    for info in buffer.info.get_mut(i + 2..end)? {
                        info.mask |= plan.mask_array[khmer_feature::CFAR];
                    }
                }

                num_coengs = 2; // Done.
            }
        } else if cat == category::OT_VPre {
            // Reorder left matra piece.

            // Move to the start.
            buffer.merge_clusters(start, i + 1);
            buffer.info.get_mut(start..=i)?.rotate_right(1);
        }
    }
    Some(())
}

fn override_features(planner: &mut ShapePlanner) {
    // Khmer spec has 'clig' as part of required shaping features:
    // "Apply feature 'clig' to form ligatures that are desired for
    // typographical correctness.", hence in overrides...
    planner.ot_map.enable_feature(Tag::new(b"clig"), F_NONE, 1);

    planner.ot_map.disable_feature(Tag::new(b"liga"));
}

fn decompose(_: &NormalizeContext, ab: Codepoint) -> Option<(Codepoint, Codepoint)> {
    // Decompose split matras that don't have Unicode decompositions.
    match ab {
        0x17BE | 0x17BF | 0x17C0 | 0x17C4 | 0x17C5 => Some((0x17C1, ab)),
        _ => unicode::decompose_impl(ab),
    }
}

fn compose(_: &NormalizeContext, a: Codepoint, b: Codepoint) -> Option<Codepoint> {
    // Avoid recomposing split matras.
    if a.general_category().is_mark() {
        return None;
    }
    unicode::compose(a, b)
}

fn setup_masks(_: &ShapePlan, _: &ShaperFont<'_, '_>, buffer: &mut Buffer) {
    buffer.allocate_var(GlyphInfo::KHMER_CATEGORY_VAR);

    // We cannot setup masks here.  We save information about characters
    // and setup masks later on in a pause-callback.
    for info in buffer.info_slice_mut() {
        info.set_khmer_properties();
    }
}
