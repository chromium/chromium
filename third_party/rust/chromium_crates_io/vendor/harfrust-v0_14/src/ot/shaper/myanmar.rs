use super::indic::{category, position};
use super::syllabic::*;
use super::*;
use crate::ot::map::*;
use crate::ot::shaper::indic::category::OT_VPre;
use crate::plan::ShapePlan;
use crate::ShaperFont;
use crate::{GlyphInfo, Tag};

pub const MYANMAR_SHAPER: OtShaper = OtShaper {
    collect_features: Some(collect_features),
    override_features: None,
    create_data: None,
    preprocess_text: None,
    postprocess_glyphs: None,
    normalization_preference: NormalizationMode::ComposedDiacriticsNoShortCircuit,
    decompose: None,
    compose: None,
    setup_masks: Some(setup_masks),
    gpos_tag: None,
    reorder_marks: None,
    zero_width_marks: ZeroWidthMarks::ByGdefEarly,
    fallback_position: false,
};

impl GlyphInfo {
    declare_buffer_var_alias!(
        OT_SHAPER_VAR_U8_CATEGORY_VAR,
        u8,
        MYANMAR_CATEGORY_VAR,
        myanmar_category,
        set_myanmar_category
    );
    declare_buffer_var_alias!(
        OT_SHAPER_VAR_U8_AUXILIARY_VAR,
        u8,
        MYANMAR_POSITION_VAR,
        myanmar_position,
        set_myanmar_position
    );
}

// Ugly Zawgyi encoding.
// Disable all auto processing.
// https://github.com/harfbuzz/harfbuzz/issues/1162
pub const MYANMAR_ZAWGYI_SHAPER: OtShaper = OtShaper {
    collect_features: None,
    override_features: None,
    create_data: None,
    preprocess_text: None,
    postprocess_glyphs: None,
    normalization_preference: NormalizationMode::None,
    decompose: None,
    compose: None,
    setup_masks: None,
    gpos_tag: None,
    reorder_marks: None,
    zero_width_marks: ZeroWidthMarks::None,
    fallback_position: false,
};

static MYANMAR_FEATURES: &[Tag] = &[
    // Basic features.
    // These features are applied in order, one at a time, after reordering,
    // constrained to the syllable.
    Tag::new(b"rphf"),
    Tag::new(b"pref"),
    Tag::new(b"blwf"),
    Tag::new(b"pstf"),
    // Other features.
    // These features are applied all at once after clearing syllables.
    Tag::new(b"pres"),
    Tag::new(b"abvs"),
    Tag::new(b"blws"),
    Tag::new(b"psts"),
];

impl GlyphInfo {
    fn set_myanmar_properties(&mut self) {
        let u = self.glyph_id;
        let (cat, _) = indic_table::get_categories(u);

        self.set_myanmar_category(cat);
    }
}

fn collect_features(planner: &mut ShapePlanner) {
    // Do this before any lookups have been applied.
    planner.ot_map.add_gsub_pause(Some(setup_syllables));

    planner
        .ot_map
        .enable_feature(Tag::new(b"locl"), F_PER_SYLLABLE, 1);
    // The Indic specs do not require ccmp, but we apply it here since if
    // there is a use of it, it's typically at the beginning.
    planner
        .ot_map
        .enable_feature(Tag::new(b"ccmp"), F_PER_SYLLABLE, 1);

    planner.ot_map.add_gsub_pause(Some(reorder_myanmar));

    for feature in MYANMAR_FEATURES.iter().take(4) {
        planner
            .ot_map
            .enable_feature(*feature, F_MANUAL_ZWJ | F_PER_SYLLABLE, 1);
        planner.ot_map.add_gsub_pause(None);
    }

    planner.ot_map.add_gsub_pause(Some(syllabic_clear_var)); // Don't need syllables anymore.

    for feature in MYANMAR_FEATURES.iter().skip(4) {
        planner.ot_map.enable_feature(*feature, F_MANUAL_ZWJ, 1);
    }
}

fn setup_syllables(_: &ShapePlan, _: &ShaperFont<'_, '_>, buffer: &mut Buffer) -> bool {
    buffer.allocate_var(GlyphInfo::SYLLABLE_VAR);

    myanmar_machine::find_syllables_myanmar(buffer);

    let mut start = 0;
    let mut end = buffer.next_syllable(0);
    while start < buffer.len {
        buffer.unsafe_to_break(Some(start), Some(end));
        start = end;
        end = buffer.next_syllable(start);
    }

    false
}

fn reorder_myanmar(_: &ShapePlan, font: &ShaperFont<'_, '_>, buffer: &mut Buffer) -> bool {
    use super::myanmar_machine::SyllableType;

    let mut ret = false;

    if insert_dotted_circles(
        font,
        buffer,
        SyllableType::BrokenCluster as u8,
        category::OT_DOTTEDCIRCLE,
        None,
        None,
    ) {
        ret = true;
    }

    let mut start = 0;
    let mut end = buffer.next_syllable(0);
    while start < buffer.len {
        reorder_syllable_myanmar(start, end, buffer);
        start = end;
        end = buffer.next_syllable(start);
    }

    buffer.deallocate_var(GlyphInfo::MYANMAR_CATEGORY_VAR);
    buffer.deallocate_var(GlyphInfo::MYANMAR_POSITION_VAR);

    ret
}

fn reorder_syllable_myanmar(start: usize, end: usize, buffer: &mut Buffer) {
    use super::myanmar_machine::SyllableType;

    let syllable_type = match buffer.info[start].syllable() & 0x0F {
        0 => SyllableType::ConsonantSyllable,
        1 => SyllableType::PunctuationCluster,
        2 => SyllableType::BrokenCluster,
        3 => SyllableType::NonMyanmarCluster,
        _ => unreachable!(),
    };

    match syllable_type {
        // We already inserted dotted-circles, so just call the consonant_syllable.
        SyllableType::ConsonantSyllable | SyllableType::BrokenCluster => {
            initial_reordering_consonant_syllable(start, end, buffer);
        }
        SyllableType::PunctuationCluster | SyllableType::NonMyanmarCluster => {}
    }
}

// Rules from:
// https://docs.microsoft.com/en-us/typography/script-development/myanmar
fn initial_reordering_consonant_syllable(start: usize, end: usize, buffer: &mut Buffer) {
    let mut base = end;
    let mut has_reph = false;

    {
        let mut limit = start;
        if start + 3 <= end
            && buffer.info[start + 0].myanmar_category() == category::OT_Ra
            && buffer.info[start + 1].myanmar_category() == category::OT_As
            && buffer.info[start + 2].myanmar_category() == category::OT_H
        {
            limit += 3;
            base = start;
            has_reph = true;
        }

        {
            if !has_reph {
                base = limit;
            }

            for i in limit..end {
                if buffer.info[i].is_consonant() {
                    base = i;
                    break;
                }
            }
        }
    }

    // Reorder!
    {
        let mut i = start;
        while i < start + if has_reph { 3 } else { 0 } {
            buffer.info[i].set_myanmar_position(position::POS_AFTER_MAIN);
            i += 1;
        }

        while i < base {
            buffer.info[i].set_myanmar_position(position::POS_PRE_C);
            i += 1;
        }

        if i < end {
            buffer.info[i].set_myanmar_position(position::POS_BASE_C);
            i += 1;
        }

        let mut pos = position::POS_AFTER_MAIN;
        // The following loop may be ugly, but it implements all of
        // Myanmar reordering!
        for i in i..end {
            // Pre-base reordering
            if buffer.info[i].myanmar_category() == category::OT_MR {
                buffer.info[i].set_myanmar_position(position::POS_PRE_C);
                continue;
            }

            // Left matra
            if buffer.info[i].myanmar_category() == OT_VPre {
                buffer.info[i].set_myanmar_position(position::POS_PRE_M);
                continue;
            }

            if buffer.info[i].myanmar_category() == category::OT_VS {
                let t = buffer.info[i - 1].myanmar_position();
                buffer.info[i].set_myanmar_position(t);
                continue;
            }

            if pos == position::POS_AFTER_MAIN
                && buffer.info[i].myanmar_category() == category::OT_VBlw
            {
                pos = position::POS_BELOW_C;
                buffer.info[i].set_myanmar_position(pos);
                continue;
            }

            if pos == position::POS_BELOW_C && buffer.info[i].myanmar_category() == category::OT_A {
                buffer.info[i].set_myanmar_position(position::POS_BEFORE_SUB);
                continue;
            }

            if pos == position::POS_BELOW_C
                && buffer.info[i].myanmar_category() == category::OT_VBlw
            {
                buffer.info[i].set_myanmar_position(pos);
                continue;
            }

            if pos == position::POS_BELOW_C && buffer.info[i].myanmar_category() != category::OT_A {
                pos = position::POS_AFTER_SUB;
                buffer.info[i].set_myanmar_position(pos);
                continue;
            }

            buffer.info[i].set_myanmar_position(pos);
        }
    }

    buffer.sort(start, end, |a, b| {
        a.myanmar_position().cmp(&b.myanmar_position()) == core::cmp::Ordering::Greater
    });

    // Flip left-mantra sequence
    let mut first_left_matra = end;
    let mut last_left_matra = end;

    for i in start..end {
        if buffer.info[i].myanmar_position() == position::POS_PRE_M {
            if first_left_matra == end {
                first_left_matra = i;
            }

            last_left_matra = i;
        }
    }

    // https://github.com/harfbuzz/harfbuzz/issues/3863
    if first_left_matra < last_left_matra {
        // No need to merge clusters, done already?
        buffer.reverse_range(first_left_matra, last_left_matra + 1);
        // Reverse back VS, etc.
        let mut i = first_left_matra;

        for j in i..=last_left_matra {
            if buffer.info[j].myanmar_category() == OT_VPre {
                buffer.reverse_range(i, j + 1);
                i = j + 1;
            }
        }
    }
}

fn setup_masks(_: &ShapePlan, _: &ShaperFont<'_, '_>, buffer: &mut Buffer) {
    buffer.allocate_var(GlyphInfo::MYANMAR_CATEGORY_VAR);
    buffer.allocate_var(GlyphInfo::MYANMAR_POSITION_VAR);

    // No masks, we just save information about characters.
    for info in buffer.info_slice_mut() {
        info.set_myanmar_properties();
    }
}
