use alloc::boxed::Box;

use super::*;
use crate::normalize::NormalizationMode;
use crate::ot::map::*;
use crate::plan::ShapePlan;
use crate::unicode::Codepoint;
use crate::BufferFlags;
use crate::Mask;
use crate::ShaperFont;

const LJMO: u8 = 1;
const VJMO: u8 = 2;
const TJMO: u8 = 3;

impl GlyphInfo {
    declare_buffer_var_alias!(
        OT_SHAPER_VAR_U8_AUXILIARY_VAR,
        u8,
        HANGUL_SHAPING_FEATURE_VAR,
        hangul_shaping_feature,
        set_hangul_shaping_feature
    );
}

fn collect_features_hangul(planner: &mut ShapePlanner) {
    planner.ot_map.add_feature(Tag::new(b"ljmo"), F_NONE, 1);
    planner.ot_map.add_feature(Tag::new(b"vjmo"), F_NONE, 1);
    planner.ot_map.add_feature(Tag::new(b"tjmo"), F_NONE, 1);
}

fn override_features_hangul(planner: &mut ShapePlanner) {
    // Uniscribe does not apply 'calt' for Hangul, and certain fonts
    // (Noto Sans CJK, Source Han Sans, etc) apply all of jamo lookups
    // in calt, which is not desirable.
    //
    // Rather than disabling 'calt' for the entire run, which also turns
    // it off for any other characters in the run, allocate a mask for it
    // and clear that mask on jamo only (in setup_masks_hangul). That
    // keeps jamo out of reach of such lookups while letting the rest of
    // the run shape with 'calt' as usual.
    //
    // https://github.com/harfbuzz/harfbuzz/discussions/4853
    planner.ot_map.add_feature(Tag::new(b"calt"), F_NONE, 1);
}

struct HangulShapePlan {
    mask_array: [Mask; 4],
    calt_mask: Mask,
}

fn data_create_hangul(map: &OtMap) -> HangulShapePlan {
    HangulShapePlan {
        mask_array: [
            0,
            map.get_1_mask(Tag::new(b"ljmo")),
            map.get_1_mask(Tag::new(b"vjmo")),
            map.get_1_mask(Tag::new(b"tjmo")),
        ],
        calt_mask: map.get_1_mask(Tag::new(b"calt")),
    }
}

const L_BASE: u32 = 0x1100;
const V_BASE: u32 = 0x1161;
const T_BASE: u32 = 0x11A7;
const L_COUNT: u32 = 19;
const V_COUNT: u32 = 21;
const T_COUNT: u32 = 28;
const N_COUNT: u32 = V_COUNT * T_COUNT;
const S_COUNT: u32 = L_COUNT * N_COUNT;
const S_BASE: u32 = 0xAC00;

fn is_combining_l(u: u32) -> bool {
    (L_BASE..L_BASE + L_COUNT).contains(&u)
}

fn is_combining_v(u: u32) -> bool {
    (V_BASE..V_BASE + V_COUNT).contains(&u)
}

fn is_combining_t(u: u32) -> bool {
    (T_BASE + 1..T_BASE + T_COUNT).contains(&u)
}

fn is_combined_s(u: u32) -> bool {
    (S_BASE..S_BASE + S_COUNT).contains(&u)
}

fn is_l(u: u32) -> bool {
    (0x1100..=0x115F).contains(&u) || (0xA960..=0xA97C).contains(&u)
}

fn is_v(u: u32) -> bool {
    (0x1160..=0x11A7).contains(&u) || (0xD7B0..=0xD7C6).contains(&u)
}

fn is_t(u: u32) -> bool {
    (0x11A8..=0x11FF).contains(&u) || (0xD7CB..=0xD7FB).contains(&u)
}

fn is_hangul_tone(u: u32) -> bool {
    (0x302E..=0x302F).contains(&u)
}

fn is_zero_width_char(font: &ShaperFont<'_, '_>, c: Codepoint) -> bool {
    if let Some(glyph) = font.nominal_glyph(c) {
        font.glyph_h_advance(glyph) == 0
    } else {
        false
    }
}

fn preprocess_text_hangul(_: &ShapePlan, font: &ShaperFont<'_, '_>, buffer: &mut Buffer) {
    buffer.allocate_var(GlyphInfo::HANGUL_SHAPING_FEATURE_VAR);

    // Hangul syllables come in two shapes: LV, and LVT.  Of those:
    //
    //   - LV can be precomposed, or decomposed.  Lets call those
    //     <LV> and <L,V>,
    //   - LVT can be fully precomposed, partially precomposed, or
    //     fully decomposed.  Ie. <LVT>, <LV,T>, or <L,V,T>.
    //
    // The composition / decomposition is mechanical.  However, not
    // all <L,V> sequences compose, and not all <LV,T> sequences
    // compose.
    //
    // Here are the specifics:
    //
    //   - <L>: U+1100..115F, U+A960..A97F
    //   - <V>: U+1160..11A7, U+D7B0..D7C7
    //   - <T>: U+11A8..11FF, U+D7CB..D7FB
    //
    //   - Only the <L,V> sequences for some of the U+11xx ranges combine.
    //   - Only <LV,T> sequences for some of the Ts in U+11xx range combine.
    //
    // Here is what we want to accomplish in this shaper:
    //
    //   - If the whole syllable can be precomposed, do that,
    //   - Otherwise, fully decompose and apply ljmo/vjmo/tjmo features.
    //   - If a valid syllable is followed by a Hangul tone mark, reorder the tone
    //     mark to precede the whole syllable - unless it is a zero-width glyph, in
    //     which case we leave it untouched, assuming it's designed to overstrike.
    //
    // That is, of the different possible syllables:
    //
    //   <L>
    //   <L,V>
    //   <L,V,T>
    //   <LV>
    //   <LVT>
    //   <LV, T>
    //
    // - <L> needs no work.
    //
    // - <LV> and <LVT> can stay the way they are if the font supports them, otherwise we
    //   should fully decompose them if font supports.
    //
    // - <L,V> and <L,V,T> we should compose if the whole thing can be composed.
    //
    // - <LV,T> we should compose if the whole thing can be composed, otherwise we should
    //   decompose.

    buffer.clear_output();
    // Extent of most recently seen syllable; valid only if start < end
    let mut start = 0;
    let mut end = 0;
    buffer.idx = 0;
    while buffer.idx < buffer.len {
        let u = buffer.cur(0).glyph_id;
        let c = buffer.cur(0).as_codepoint();

        if is_hangul_tone(u) {
            // We could cache the width of the tone marks and the existence of dotted-circle,
            // but the use of the Hangul tone mark characters seems to be rare enough that
            // I didn't bother for now.
            if start < end && end == buffer.out_len {
                // Tone mark follows a valid syllable; move it in front, unless it's zero width.
                buffer.unsafe_to_break_from_outbuffer(Some(start), Some(buffer.idx));
                buffer.next_glyph();
                if !is_zero_width_char(font, c) {
                    buffer.merge_out_grapheme_clusters(start, end + 1);
                    let out_info = buffer.out_info_mut();
                    let tone = out_info[end];
                    for i in (0..end - start).rev() {
                        out_info[i + start + 1] = out_info[i + start];
                    }
                    out_info[start] = tone;
                }
            } else {
                // No valid syllable as base for tone mark; try to insert dotted circle.
                if !buffer
                    .flags
                    .contains(BufferFlags::DO_NOT_INSERT_DOTTED_CIRCLE)
                    && font.has_glyph(0x25CC)
                {
                    let mut chars = [0; 2];
                    if !is_zero_width_char(font, c) {
                        chars[0] = u;
                        chars[1] = 0x25CC;
                    } else {
                        chars[0] = 0x25CC;
                        chars[1] = u;
                    }

                    buffer.replace_glyphs(1, 2, &chars);
                } else {
                    // No dotted circle available in the font; just leave tone mark untouched.
                    buffer.next_glyph();
                }
            }

            start = buffer.out_len;
            end = buffer.out_len;
            continue;
        }

        // Remember current position as a potential syllable start;
        // will only be used if we set end to a later position.
        start = buffer.out_len;

        if is_l(u) && buffer.idx + 1 < buffer.len {
            let l = u;
            let v = buffer.cur(1).glyph_id;
            if is_v(v) {
                // Have <L,V> or <L,V,T>.
                let mut t = 0;
                let mut tindex = 0;
                if buffer.idx + 2 < buffer.len {
                    t = buffer.cur(2).glyph_id;
                    if is_t(t) {
                        // Only used if isCombiningT (t); otherwise invalid.
                        tindex = t - T_BASE;
                    } else {
                        // The next character was not a trailing jamo.
                        t = 0;
                    }
                }

                let offset = if t != 0 { 3 } else { 2 };
                buffer.unsafe_to_break(Some(buffer.idx), Some(buffer.idx + offset));

                // We've got a syllable <L,V,T?>; see if it can potentially be composed.
                if is_combining_l(l) && is_combining_v(v) && (t == 0 || is_combining_t(t)) {
                    // Try to compose; if this succeeds, end is set to start+1.
                    let s = S_BASE + (l - L_BASE) * N_COUNT + (v - V_BASE) * T_COUNT + tindex;
                    if font.has_glyph(s) {
                        let n = if t != 0 { 3 } else { 2 };
                        buffer.replace_glyphs(n, 1, &[s]);
                        end = start + 1;
                        continue;
                    }
                }

                // We didn't compose, either because it's an Old Hangul syllable without a
                // precomposed character in Unicode, or because the font didn't support the
                // necessary precomposed glyph.
                // Set jamo features on the individual glyphs, and advance past them.
                buffer.cur_mut(0).set_hangul_shaping_feature(LJMO);
                buffer.next_glyph();
                buffer.cur_mut(0).set_hangul_shaping_feature(VJMO);
                buffer.next_glyph();
                if t != 0 {
                    buffer.cur_mut(0).set_hangul_shaping_feature(TJMO);
                    buffer.next_glyph();
                    end = start + 3;
                } else {
                    end = start + 2;
                }

                buffer.merge_out_grapheme_clusters(start, end);

                continue;
            }
        } else if is_combined_s(u) {
            // Have <LV>, <LVT>, or <LV,T>
            let s = u;
            let has_glyph = font.has_glyph(s);

            let lindex = (s - S_BASE) / N_COUNT;
            let nindex = (s - S_BASE) % N_COUNT;
            let vindex = nindex / T_COUNT;
            let tindex = nindex % T_COUNT;

            if tindex == 0 && buffer.idx + 1 < buffer.len && is_combining_t(buffer.cur(1).glyph_id)
            {
                // <LV,T>, try to combine.
                let new_tindex = buffer.cur(1).glyph_id - T_BASE;
                let new_s = s + new_tindex;

                if font.has_glyph(new_s) {
                    buffer.replace_glyphs(2, 1, &[new_s]);
                    end = start + 1;
                    continue;
                } else {
                    // Mark unsafe between LV and T.
                    buffer.unsafe_to_break(Some(buffer.idx), Some(buffer.idx + 2));
                }
            }

            // Otherwise, decompose if font doesn't support <LV> or <LVT>,
            // or if having non-combining <LV,T>.  Note that we already handled
            // combining <LV,T> above.
            if !has_glyph
                || (tindex == 0 && buffer.idx + 1 < buffer.len && is_t(buffer.cur(1).glyph_id))
            {
                let decomposed = [L_BASE + lindex, V_BASE + vindex, T_BASE + tindex];
                if font.has_glyph(decomposed[0])
                    && font.has_glyph(decomposed[1])
                    && (tindex == 0 || font.has_glyph(decomposed[2]))
                {
                    let mut s_len = if tindex != 0 { 3 } else { 2 };
                    buffer.replace_glyphs(1, s_len, &decomposed);

                    // If we decomposed an LV because of a non-combining T following,
                    // we want to include this T in the syllable.
                    if has_glyph && tindex == 0 {
                        buffer.next_glyph();
                        s_len += 1;
                    }

                    // We decomposed S: apply jamo features to the individual glyphs
                    // that are now in `buffer.out_info`.
                    end = start + s_len;

                    buffer.out_info_mut()[start + 0].set_hangul_shaping_feature(LJMO);
                    buffer.out_info_mut()[start + 1].set_hangul_shaping_feature(VJMO);
                    if start + 2 < end {
                        buffer.out_info_mut()[start + 2].set_hangul_shaping_feature(TJMO);
                    }

                    buffer.merge_out_grapheme_clusters(start, end);

                    continue;
                } else if tindex == 0 && buffer.idx + 1 > buffer.len && is_t(buffer.cur(1).glyph_id)
                {
                    // Mark unsafe between LV and T.
                    buffer.unsafe_to_break(Some(buffer.idx), Some(buffer.idx + 2));
                }
            }

            if has_glyph {
                // We didn't decompose the S, so just advance past it.
                end = start + 1;
                buffer.next_glyph();
                continue;
            }
        }

        // Didn't find a recognizable syllable, so we leave end <= start;
        // this will prevent tone-mark reordering happening.
        buffer.next_glyph();
    }

    buffer.sync();
}

fn setup_masks_hangul(plan: &ShapePlan, _: &ShaperFont<'_, '_>, buffer: &mut Buffer) {
    let hangul_plan = plan.data::<HangulShapePlan>();
    for info in buffer.info_slice_mut() {
        info.mask |= hangul_plan.mask_array[info.hangul_shaping_feature() as usize];

        // Keep 'calt' away from jamo; see override_features_hangul().
        let u = info.glyph_id;
        if is_l(u) || is_v(u) || is_t(u) {
            info.mask &= !hangul_plan.calt_mask;
        }
    }

    buffer.deallocate_var(GlyphInfo::HANGUL_SHAPING_FEATURE_VAR);
}

pub const HANGUL_SHAPER: OtShaper = OtShaper {
    collect_features: Some(collect_features_hangul),
    override_features: Some(override_features_hangul),
    create_data: Some(|plan| Box::new(data_create_hangul(&plan.ot_map))),
    preprocess_text: Some(preprocess_text_hangul),
    postprocess_glyphs: None,
    normalization_preference: NormalizationMode::None,
    decompose: None,
    compose: None,
    setup_masks: Some(setup_masks_hangul),
    gpos_tag: None,
    reorder_marks: None,
    zero_width_marks: ZeroWidthMarks::None,
    fallback_position: true,
};
