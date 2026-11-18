use super::*;
use crate::normalize::NormalizationMode;
use crate::once::Once;
use crate::ot::map::*;
use crate::plan::ShapePlan;
use crate::unicode::*;
use crate::Direction;
use crate::ShaperFont;
use crate::{GlyphInfo, Mask, Script, Tag};
use alloc::boxed::Box;

const HB_BUFFER_SCRATCH_FLAG_ARABIC_HAS_STCH: ScratchFlags = HB_BUFFER_SCRATCH_FLAG_SHAPER0;

// See:
// https://github.com/harfbuzz/harfbuzz/commit/6e6f82b6f3dde0fc6c3c7d991d9ec6cfff57823d#commitcomment-14248516
fn is_word_category(gc: GeneralCategory) -> bool {
    (gc.flag_unsafe()
        & (GeneralCategory::UNASSIGNED.flag()
            | GeneralCategory::PRIVATE_USE.flag()
            | GeneralCategory::MODIFIER_LETTER.flag()
            | GeneralCategory::OTHER_LETTER.flag()
            | GeneralCategory::SPACING_MARK.flag()
            | GeneralCategory::ENCLOSING_MARK.flag()
            | GeneralCategory::NON_SPACING_MARK.flag()
            | GeneralCategory::DECIMAL_NUMBER.flag()
            | GeneralCategory::LETTER_NUMBER.flag()
            | GeneralCategory::OTHER_NUMBER.flag()
            | GeneralCategory::CURRENCY_SYMBOL.flag()
            | GeneralCategory::MODIFIER_SYMBOL.flag()
            | GeneralCategory::MATH_SYMBOL.flag()
            | GeneralCategory::OTHER_SYMBOL.flag()))
        != 0
}

#[derive(Clone, Copy, PartialEq, PartialOrd, Debug)]
pub enum ArabicJoiningType {
    U = 0,
    L = 1,
    R = 2,
    D = 3,
    // We don't have C, like harfbuzz, because Rust doesn't allow duplicated enum variants.
    GroupAlaph = 4,
    GroupDalathRish = 5,
    T = 6,
    X = 7, // means: use general-category to choose between U or T.
}

fn get_joining_type(u: Codepoint, gc: GeneralCategory) -> ArabicJoiningType {
    let j_type = arabic_table::joining_type(u);
    if j_type != ArabicJoiningType::X {
        return j_type;
    }

    let ok = gc.flag_unsafe()
        & (GeneralCategory::NON_SPACING_MARK.flag()
            | GeneralCategory::ENCLOSING_MARK.flag()
            | GeneralCategory::FORMAT.flag());

    if ok != 0 {
        ArabicJoiningType::T
    } else {
        ArabicJoiningType::U
    }
}

fn feature_is_syriac(tag: Tag) -> bool {
    matches!(tag.to_be_bytes()[3], b'2' | b'3')
}

const ARABIC_FEATURES: &[Tag] = &[
    Tag::new(b"isol"),
    Tag::new(b"fina"),
    Tag::new(b"fin2"),
    Tag::new(b"fin3"),
    Tag::new(b"medi"),
    Tag::new(b"med2"),
    Tag::new(b"init"),
];

mod arabic_action {
    pub const ISOL: u8 = 0;
    pub const FINA: u8 = 1;
    pub const FIN2: u8 = 2;
    pub const FIN3: u8 = 3;
    pub const MEDI: u8 = 4;
    pub const MED2: u8 = 5;
    pub const INIT: u8 = 6;
    pub const NONE: u8 = 7;

    // We abuse the same byte for other things...
    pub const STRETCHING_FIXED: u8 = 8;
    pub const STRETCHING_REPEATING: u8 = 9;

    #[inline]
    pub fn is_stch(n: u8) -> bool {
        matches!(n, STRETCHING_FIXED | STRETCHING_REPEATING)
    }
}

static STATE_TABLE: &[[(u8, u8, u16); 6]] = &[
    // jt_U,          jt_L,          jt_R,
    // jt_D,          jg_ALAPH,      jg_DALATH_RISH

    // State 0: prev was U, not willing to join.
    [
        (arabic_action::NONE, arabic_action::NONE, 0),
        (arabic_action::NONE, arabic_action::ISOL, 2),
        (arabic_action::NONE, arabic_action::ISOL, 1),
        (arabic_action::NONE, arabic_action::ISOL, 2),
        (arabic_action::NONE, arabic_action::ISOL, 1),
        (arabic_action::NONE, arabic_action::ISOL, 6),
    ],
    // State 1: prev was R or action::ISOL/ALAPH, not willing to join.
    [
        (arabic_action::NONE, arabic_action::NONE, 0),
        (arabic_action::NONE, arabic_action::ISOL, 2),
        (arabic_action::NONE, arabic_action::ISOL, 1),
        (arabic_action::NONE, arabic_action::ISOL, 2),
        (arabic_action::NONE, arabic_action::FIN2, 5),
        (arabic_action::NONE, arabic_action::ISOL, 6),
    ],
    // State 2: prev was D/L in action::ISOL form, willing to join.
    [
        (arabic_action::NONE, arabic_action::NONE, 0),
        (arabic_action::NONE, arabic_action::ISOL, 2),
        (arabic_action::INIT, arabic_action::FINA, 1),
        (arabic_action::INIT, arabic_action::FINA, 3),
        (arabic_action::INIT, arabic_action::FINA, 4),
        (arabic_action::INIT, arabic_action::FINA, 6),
    ],
    // State 3: prev was D in action::FINA form, willing to join.
    [
        (arabic_action::NONE, arabic_action::NONE, 0),
        (arabic_action::NONE, arabic_action::ISOL, 2),
        (arabic_action::MEDI, arabic_action::FINA, 1),
        (arabic_action::MEDI, arabic_action::FINA, 3),
        (arabic_action::MEDI, arabic_action::FINA, 4),
        (arabic_action::MEDI, arabic_action::FINA, 6),
    ],
    // State 4: prev was action::FINA ALAPH, not willing to join.
    [
        (arabic_action::NONE, arabic_action::NONE, 0),
        (arabic_action::NONE, arabic_action::ISOL, 2),
        (arabic_action::MED2, arabic_action::ISOL, 1),
        (arabic_action::MED2, arabic_action::ISOL, 2),
        (arabic_action::MED2, arabic_action::FIN2, 5),
        (arabic_action::MED2, arabic_action::ISOL, 6),
    ],
    // State 5: prev was FIN2/FIN3 ALAPH, not willing to join.
    [
        (arabic_action::NONE, arabic_action::NONE, 0),
        (arabic_action::NONE, arabic_action::ISOL, 2),
        (arabic_action::ISOL, arabic_action::ISOL, 1),
        (arabic_action::ISOL, arabic_action::ISOL, 2),
        (arabic_action::ISOL, arabic_action::FIN2, 5),
        (arabic_action::ISOL, arabic_action::ISOL, 6),
    ],
    // State 6: prev was DALATH/RISH, not willing to join.
    [
        (arabic_action::NONE, arabic_action::NONE, 0),
        (arabic_action::NONE, arabic_action::ISOL, 2),
        (arabic_action::NONE, arabic_action::ISOL, 1),
        (arabic_action::NONE, arabic_action::ISOL, 2),
        (arabic_action::NONE, arabic_action::FIN3, 5),
        (arabic_action::NONE, arabic_action::ISOL, 6),
    ],
];

impl GlyphInfo {
    declare_buffer_var_alias!(
        OT_SHAPER_VAR_U8_AUXILIARY_VAR,
        u8,
        ARABIC_SHAPING_ACTION_VAR,
        arabic_shaping_action,
        set_arabic_shaping_action
    );
}

fn deallocate_buffer_var(
    _: &ShapePlan,
    _: &ShaperFont<'_, '_>,
    buffer: &mut Buffer,
) -> Option<bool> {
    buffer.deallocate_var(GlyphInfo::ARABIC_SHAPING_ACTION_VAR);

    Some(false)
}

fn collect_features(planner: &mut ShapePlanner) {
    // We apply features according to the Arabic spec, with pauses
    // in between most.
    //
    // The pause between init/medi/... and rlig is required.  See eg:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=644184
    //
    // The pauses between init/medi/... themselves are not necessarily
    // needed as only one of those features is applied to any character.
    // The only difference it makes is when fonts have contextual
    // substitutions.  We now follow the order of the spec, which makes
    // for better experience if that's what Uniscribe is doing.
    //
    // At least for Arabic, looks like Uniscribe has a pause between
    // rlig and calt.  Otherwise the IranNastaliq's ALLAH ligature won't
    // work.  However, testing shows that rlig and calt are applied
    // together for Mongolian in Uniscribe.  As such, we only add a
    // pause for Arabic, not other scripts.
    //
    // A pause after calt is required to make KFGQPC Uthmanic Script HAFS
    // work correctly.  See https://github.com/harfbuzz/harfbuzz/issues/505

    planner.ot_map.enable_feature(Tag::new(b"stch"), F_NONE, 1);
    planner.ot_map.add_gsub_pause(Some(record_stch));

    planner
        .ot_map
        .enable_feature(Tag::new(b"ccmp"), F_MANUAL_ZWJ, 1);
    planner
        .ot_map
        .enable_feature(Tag::new(b"locl"), F_MANUAL_ZWJ, 1);

    planner.ot_map.add_gsub_pause(None);

    for feature in ARABIC_FEATURES {
        let has_fallback = planner.script == Some(Script::ARABIC) && !feature_is_syriac(*feature);
        let flags = if has_fallback { F_HAS_FALLBACK } else { F_NONE };
        planner
            .ot_map
            .add_feature(*feature, F_MANUAL_ZWJ | flags, 1);
        planner.ot_map.add_gsub_pause(None);
    }
    planner.ot_map.add_gsub_pause(Some(deallocate_buffer_var));

    // Normally, Unicode says a ZWNJ means "don't ligate".  In Arabic script
    // however, it says a ZWJ should also mean "don't ligate".  So we run
    // the main ligating features as MANUAL_ZWJ.

    planner
        .ot_map
        .enable_feature(Tag::new(b"rlig"), F_MANUAL_ZWJ | F_HAS_FALLBACK, 1);

    if planner.script == Some(Script::ARABIC) {
        planner.ot_map.add_gsub_pause(Some(arabic_fallback_shape));
    }

    // No pause after rclt.
    // See 98460779bae19e4d64d29461ff154b3527bf8420
    planner
        .ot_map
        .enable_feature(Tag::new(b"calt"), F_MANUAL_ZWJ, 1);
    /* https://github.com/harfbuzz/harfbuzz/issues/1573 */
    if !planner.ot_map.has_feature(Tag::new(b"rclt")) {
        planner.ot_map.add_gsub_pause(None);
    }

    planner
        .ot_map
        .enable_feature(Tag::new(b"liga"), F_MANUAL_ZWJ, 1);
    planner
        .ot_map
        .enable_feature(Tag::new(b"clig"), F_MANUAL_ZWJ, 1);

    // The spec includes 'cswh'.  Earlier versions of Windows
    // used to enable this by default, but testing suggests
    // that Windows 8 and later do not enable it by default,
    // and spec now says 'Off by default'.
    // We disabled this in ae23c24c32.
    // Note that IranNastaliq uses this feature extensively
    // to fixup broken glyph sequences.  Oh well...
    // Test case: U+0643,U+0640,U+0631.

    // planner.ot_map.enable_feature(feature::CONTEXTUAL_SWASH, F_MANUAL_ZWJ, 1);
    planner
        .ot_map
        .enable_feature(Tag::new(b"mset"), F_MANUAL_ZWJ, 1);
}

pub struct ArabicShapePlan {
    // The "+ 1" in the next array is to accommodate for the "NONE" command,
    // which is not an OpenType feature, but this simplifies the code by not
    // having to do a "if (... < NONE) ..." and just rely on the fact that
    // mask_array[NONE] == 0.
    mask_array: [Mask; ARABIC_FEATURES.len() + 1],
    do_fallback: bool,
    has_stch: bool,
    fallback_plan: Once<Option<arabic_fallback::FallbackPlan>>,
}

pub fn data_create_arabic(plan: &ShapePlan) -> ArabicShapePlan {
    let has_stch = plan.ot_map.get_1_mask(Tag::new(b"stch")) != 0;
    let mut do_fallback = plan.script == Some(Script::ARABIC);

    let mut mask_array = [0; ARABIC_FEATURES.len() + 1];
    for i in 0..ARABIC_FEATURES.len() {
        mask_array[i] = plan.ot_map.get_1_mask(ARABIC_FEATURES[i]);
        do_fallback &=
            feature_is_syriac(ARABIC_FEATURES[i]) || plan.ot_map.needs_fallback(ARABIC_FEATURES[i]);
    }

    ArabicShapePlan {
        mask_array,
        do_fallback,
        has_stch,
        fallback_plan: Once::new(),
    }
}

fn arabic_joining(buffer: &mut Buffer) -> Option<()> {
    let mut prev: Option<usize> = None;
    let mut state = 0;

    // Check pre-context.
    let pre_context = buffer.context[0]
        .get(..buffer.context_len[0])
        .unwrap_or_default();
    for &c in pre_context {
        let this_type = get_joining_type(c, c.general_category());
        if this_type == ArabicJoiningType::T {
            continue;
        }

        state = STATE_TABLE[state][this_type as usize].2 as usize;
        break;
    }

    for i in 0..buffer.len {
        let info = buffer.info.get_mut(i)?;
        let this_type = get_joining_type(info.as_codepoint(), info.general_category());
        if this_type == ArabicJoiningType::T {
            info.set_arabic_shaping_action(arabic_action::NONE);
            continue;
        }

        let entry = &STATE_TABLE[state][this_type as usize];
        if entry.0 != arabic_action::NONE && prev.is_some() {
            if let Some(prev) = prev {
                buffer
                    .info
                    .get_mut(prev)?
                    .set_arabic_shaping_action(entry.0);
                buffer.safe_to_insert_tatweel(Some(prev), Some(i + 1));
            }
        }
        // States that have a possible prev_action.
        else {
            if let Some(prev) = prev {
                if this_type >= ArabicJoiningType::R || (2 <= state && state <= 5) {
                    buffer.unsafe_to_concat(Some(prev), Some(i + 1));
                }
            } else {
                if this_type >= ArabicJoiningType::R {
                    buffer.unsafe_to_concat_from_outbuffer(Some(0), Some(i + 1));
                }
            }
        }

        buffer.info.get_mut(i)?.set_arabic_shaping_action(entry.1);

        prev = Some(i);
        state = entry.2 as usize;
    }

    // Copied so the buffer can be updated while walking it.
    let post_context = buffer.context[1];
    let post_context_len = buffer.context_len[1];
    for &c in post_context.get(..post_context_len).unwrap_or_default() {
        let this_type = get_joining_type(c, c.general_category());
        if this_type == ArabicJoiningType::T {
            continue;
        }

        let entry = &STATE_TABLE[state][this_type as usize];
        if entry.0 != arabic_action::NONE && prev.is_some() {
            if let Some(prev) = prev {
                buffer
                    .info
                    .get_mut(prev)?
                    .set_arabic_shaping_action(entry.0);
                buffer.safe_to_insert_tatweel(Some(prev), Some(buffer.len));
            }
        }
        // States that have a possible prev_action.
        else if 2 <= state && state <= 5 {
            if let Some(prev) = prev {
                buffer.unsafe_to_concat(Some(prev), Some(buffer.len));
            }
        }

        break;
    }
    Some(())
}

fn mongolian_variation_selectors(buffer: &mut Buffer) {
    // Copy arabic_shaping_action() from base to Mongolian variation selectors.
    let Some((first, rest)) = buffer.info_slice_mut().split_first_mut() else {
        return;
    };
    let mut action = first.arabic_shaping_action();
    for info in rest {
        if (0x180B..=0x180D).contains(&info.glyph_id) || info.glyph_id == 0x180F {
            info.set_arabic_shaping_action(action);
        }
        action = info.arabic_shaping_action();
    }
}

fn setup_masks_arabic_plan(plan: &ShapePlan, _: &ShaperFont<'_, '_>, buffer: &mut Buffer) {
    buffer.allocate_var(GlyphInfo::ARABIC_SHAPING_ACTION_VAR);

    let arabic_plan = plan.data::<ArabicShapePlan>();
    setup_masks_inner(arabic_plan, plan.script, buffer);
}

pub fn setup_masks_inner(
    arabic_plan: &ArabicShapePlan,
    script: Option<Script>,
    buffer: &mut Buffer,
) {
    if arabic_joining(buffer).is_none() {
        buffer.index_failed();
    }
    if script == Some(Script::MONGOLIAN) {
        mongolian_variation_selectors(buffer);
    }

    for info in buffer.info_slice_mut() {
        info.mask |= arabic_plan.mask_array[info.arabic_shaping_action() as usize];
    }
}

fn arabic_fallback_shape(
    plan: &ShapePlan,
    font: &ShaperFont<'_, '_>,
    buffer: &mut Buffer,
) -> Option<bool> {
    let arabic_plan = plan.data::<ArabicShapePlan>();
    if !arabic_plan.do_fallback {
        return Some(false);
    }

    // Built outside the cell: building asks the font's callbacks for glyphs,
    // and application code must not run while the cell blocks other callers.
    // If several callers build at once, the first to finish wins.
    let fallback_plan = if let Some(fallback_plan) = arabic_plan.fallback_plan.get() {
        fallback_plan
    } else {
        let built = arabic_fallback::FallbackPlan::new(plan, font);
        arabic_plan.fallback_plan.get_or_init(|| built)
    };
    if let Some(fallback_plan) = fallback_plan {
        fallback_plan.apply(font, buffer);
    }
    Some(true)
}

// Stretch feature: "stch".
// See example here:
// https://docs.microsoft.com/en-us/typography/script-development/syriac
// We implement this in a generic way, such that the Arabic subtending
// marks can use it as well.
fn record_stch(plan: &ShapePlan, _: &ShaperFont<'_, '_>, buffer: &mut Buffer) -> Option<bool> {
    let arabic_plan = plan.data::<ArabicShapePlan>();
    if !arabic_plan.has_stch {
        return Some(false);
    }

    // 'stch' feature was just applied.  Look for anything that multiplied,
    // and record it for stch treatment later.  Note that rtlm, frac, etc
    // are applied before stch, but we assume that they didn't result in
    // anything multiplying into 5 pieces, so it's safe-ish...

    let mut has_stch = false;
    for glyph_info in buffer.info_slice_mut() {
        if glyph_info.multiplied() {
            let comp = if glyph_info.lig_comp() % 2 != 0 {
                arabic_action::STRETCHING_REPEATING
            } else {
                arabic_action::STRETCHING_FIXED
            };

            glyph_info.set_arabic_shaping_action(comp);
            has_stch = true;
        }
    }

    if has_stch {
        buffer.scratch_flags |= HB_BUFFER_SCRATCH_FLAG_ARABIC_HAS_STCH;
    }

    Some(false)
}

fn apply_stch(font: &ShaperFont<'_, '_>, buffer: &mut Buffer) {
    if buffer.scratch_flags & HB_BUFFER_SCRATCH_FLAG_ARABIC_HAS_STCH == 0 {
        return;
    }

    let rtl = buffer.direction == Direction::RightToLeft;

    if !rtl {
        buffer.reverse();
    }

    // A failed lookup abandons the stretch but still restores the direction.
    if apply_stch_passes(font, buffer, rtl).is_none() {
        buffer.index_failed();
    }

    if !rtl {
        buffer.reverse();
    }
}

/// Copies the glyph at `from` to `to`, along with its position.
fn copy_glyph_to(buffer: &mut Buffer, from: usize, to: usize) -> Option<()> {
    let info = *buffer.info.get(from)?;
    let pos = *buffer.pos.get(from)?;
    *buffer.info.get_mut(to)? = info;
    *buffer.pos.get_mut(to)? = pos;
    Some(())
}

fn apply_stch_passes(font: &ShaperFont<'_, '_>, buffer: &mut Buffer, rtl: bool) -> Option<()> {
    // We do a two pass implementation:
    // First pass calculates the exact number of extra glyphs we need,
    // We then enlarge buffer to have that much room,
    // Second pass applies the stretch, copying things to the end of buffer.

    let mut extra_glyphs_needed: usize = 0; // Set during MEASURE, used during CUT
    const MEASURE: usize = 0;
    const CUT: usize = 1;

    for step in 0..2 {
        let new_len = buffer.len + extra_glyphs_needed; // write head during CUT
        let mut i = buffer.len;
        let mut j = new_len;
        while i != 0 {
            if !arabic_action::is_stch(buffer.info.get(i - 1)?.arabic_shaping_action()) {
                if step == CUT {
                    j -= 1;
                    copy_glyph_to(buffer, i - 1, j)?;
                }

                i -= 1;
                continue;
            }

            // Yay, justification!

            let mut w_total: i32 = 0; // Total to be filled
            let mut w_fixed: i32 = 0; // Sum of fixed tiles
            let mut w_repeating: i32 = 0; // Sum of repeating tiles
            let mut n_repeating: i32 = 0;

            let end = i;
            while i != 0 && arabic_action::is_stch(buffer.info.get(i - 1)?.arabic_shaping_action())
            {
                i -= 1;
                let info = buffer.info.get(i)?;
                let width = font.glyph_h_advance(info.as_glyph());

                if info.arabic_shaping_action() == arabic_action::STRETCHING_FIXED {
                    w_fixed = w_fixed.saturating_add(width);
                } else {
                    w_repeating = w_repeating.saturating_add(width);
                    n_repeating += 1;
                }
            }

            let start = i;
            let mut context = i;
            while context != 0 {
                let info = buffer.info.get(context - 1)?;
                if arabic_action::is_stch(info.arabic_shaping_action())
                    || !(info.is_default_ignorable() || is_word_category(info.general_category()))
                {
                    break;
                }
                context -= 1;
                w_total = w_total.saturating_add(buffer.pos.get(context)?.x_advance);
            }

            i += 1; // Don't touch i again.

            // Number of additional times to repeat each repeating tile.
            let mut n_copies: i32 = 0;

            let mut w_remaining = w_total.saturating_sub(w_fixed);
            if w_remaining > w_repeating && w_repeating > 0 {
                n_copies = w_remaining / (w_repeating) - 1;
            }

            // See if we can improve the fit by adding an extra repeat and squeezing them together a bit.
            let mut extra_repeat_overlap = 0;
            let shortfall =
                i64::from(w_remaining) - i64::from(w_repeating) * i64::from(n_copies + 1);
            if shortfall > 0 && n_repeating > 0 {
                n_copies += 1;
                let excess =
                    i64::from(n_copies + 1) * i64::from(w_repeating) - i64::from(w_remaining);
                if excess > 0 {
                    extra_repeat_overlap =
                        clamp_i64_to_i32(excess / (i64::from(n_copies) * i64::from(n_repeating)));
                    w_remaining = 0;
                }
            }

            if step == MEASURE {
                extra_glyphs_needed += (n_copies * n_repeating) as usize;
            } else {
                buffer.unsafe_to_break(Some(context), Some(end));
                let mut x_offset = w_remaining / 2;
                for k in (start + 1..=end).rev() {
                    let info = *buffer.info.get(k - 1)?;
                    let width = font.glyph_h_advance(info.as_glyph());

                    let mut repeat = 1;
                    if info.arabic_shaping_action() == arabic_action::STRETCHING_REPEATING {
                        repeat += n_copies;
                    }

                    // Each appended copy takes this position, which is stored back afterwards.
                    let mut pos = *buffer.pos.get(k - 1)?;
                    pos.x_advance = 0;

                    for n in 0..repeat {
                        if rtl {
                            x_offset = x_offset.saturating_sub(width);
                            if n > 0 {
                                x_offset = x_offset.saturating_add(extra_repeat_overlap);
                            }
                        }

                        pos.x_offset = x_offset;

                        // Append copy.
                        j -= 1;
                        *buffer.info.get_mut(j)? = info;
                        *buffer.pos.get_mut(j)? = pos;

                        if !rtl {
                            x_offset = x_offset.saturating_add(width);

                            if n > 0 {
                                x_offset = x_offset.saturating_sub(extra_repeat_overlap);
                            }
                        }
                    }
                    *buffer.pos.get_mut(k - 1)? = pos;
                }
            }

            i -= 1;
        }

        if step == MEASURE {
            if !buffer.ensure(buffer.len + extra_glyphs_needed) {
                break;
            }
        } else {
            debug_assert_eq!(j, 0);
            buffer.len = new_len;
        }
    }
    Some(())
}

fn postprocess_glyphs_arabic(_: &ShapePlan, font: &ShaperFont<'_, '_>, buffer: &mut Buffer) {
    apply_stch(font, buffer);
}

// http://www.unicode.org/reports/tr53/
static MODIFIER_COMBINING_MARKS: &[u32] = &[
    0x0654, // ARABIC HAMZA ABOVE
    0x0655, // ARABIC HAMZA BELOW
    0x0658, // ARABIC MARK NOON GHUNNA
    0x06DC, // ARABIC SMALL HIGH SEEN
    0x06E3, // ARABIC SMALL LOW SEEN
    0x06E7, // ARABIC SMALL HIGH YEH
    0x06E8, // ARABIC SMALL HIGH NOON
    0x08CA, // ARABIC SMALL HIGH FARSI YEH
    0x08CB, // ARABIC SMALL HIGH YEH BARREE WITH TWO DOTS BELOW
    0x08CD, // ARABIC SMALL HIGH ZAH
    0x08CE, // ARABIC LARGE ROUND DOT ABOVE
    0x08CF, // ARABIC LARGE ROUND DOT BELOW
    0x08D3, // ARABIC SMALL LOW WAW
    0x08F3, // ARABIC SMALL HIGH WAW
];

fn reorder_marks_arabic(_: &ShapePlan, buffer: &mut Buffer, start: usize, end: usize) {
    if reorder_modifier_combining_marks(buffer, start, end).is_none() {
        buffer.index_failed();
    }
}

fn reorder_modifier_combining_marks(
    buffer: &mut Buffer,
    mut start: usize,
    end: usize,
) -> Option<()> {
    let mut i = start;
    for cc in [220u8, 230] {
        while i < end && buffer.info.get(i)?.modified_combining_class() < cc {
            i += 1;
        }

        if i == end {
            break;
        }

        if buffer.info.get(i)?.modified_combining_class() > cc {
            continue;
        }

        let mut j = i;
        while j < end {
            let info = buffer.info.get(j)?;
            if info.modified_combining_class() != cc
                || !MODIFIER_COMBINING_MARKS.contains(&info.glyph_id)
            {
                break;
            }
            j += 1;
        }

        if i == j {
            continue;
        }

        // Shift it!
        buffer.merge_clusters(start, j);
        buffer.info.get_mut(start..j)?.rotate_left(i - start);

        // Renumber CC such that the reordered sequence is still sorted.
        // 22 and 26 are chosen because they are smaller than all Arabic categories,
        // and are folded back to 220/230 respectively during fallback mark positioning.
        //
        // We do this because the CGJ-handling logic in the normalizer relies on
        // mark sequences having an increasing order even after this reordering.
        // https://github.com/harfbuzz/harfbuzz/issues/554
        // This, however, does break some obscure sequences, where the normalizer
        // might compose a sequence that it should not.  For example, in the seequence
        // ALEF, HAMZAH, MADDAH, we should NOT try to compose ALEF+MADDAH, but with this
        // renumbering, we will.
        let new_start = start + j - i;
        let new_cc = if cc == 220 {
            modified_combining_class::CCC22
        } else {
            modified_combining_class::CCC26
        };

        for info in buffer.info.get_mut(start..new_start)? {
            info.set_modified_combining_class(new_cc);
        }
        start = new_start;

        i = j;
    }
    Some(())
}

pub const ARABIC_SHAPER: OtShaper = OtShaper {
    collect_features: Some(collect_features),
    override_features: None,
    create_data: Some(|plan| Box::new(data_create_arabic(plan))),
    preprocess_text: None,
    postprocess_glyphs: Some(postprocess_glyphs_arabic),
    normalization_preference: NormalizationMode::Auto,
    decompose: None,
    compose: None,
    setup_masks: Some(setup_masks_arabic_plan),
    gpos_tag: None,
    reorder_marks: Some(reorder_marks_arabic),
    zero_width_marks: ZeroWidthMarks::ByGdefLate,
    fallback_position: true,
};
