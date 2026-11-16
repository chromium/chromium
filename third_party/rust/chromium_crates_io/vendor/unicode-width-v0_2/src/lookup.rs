// Copyright 2012-2026 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#[path = "gen/lookup.rs"]
mod gen;

pub(crate) use gen::lookup_width;

#[cfg(feature = "cjk")]
pub(crate) use gen::lookup_width_cjk;

use crate::props::*;
use crate::width_info::WidthInfo;

#[inline]
fn lookup_width_generic<const IS_CJK: bool>(c: char) -> (u8, WidthInfo) {
    #[cfg(feature = "cjk")]
    if IS_CJK {
        return lookup_width_cjk(c);
    }
    lookup_width(c)
}

/// Returns the [UAX #11](https://www.unicode.org/reports/tr11/) based width of `c`, or
/// `None` if `c` is a control character.
/// Ambiguous width characters are treated as narrow.
#[inline]
pub fn single_char_width(c: char) -> Option<usize> {
    single_char_width_generic::<false>(c)
}

/// Returns the [UAX #11](https://www.unicode.org/reports/tr11/) based width of `c`, or
/// `None` if `c` is a control character.
/// Ambiguous width characters are treated as wide.
#[cfg(feature = "cjk")]
#[inline]
pub fn single_char_width_cjk(c: char) -> Option<usize> {
    single_char_width_generic::<true>(c)
}

#[inline]
fn single_char_width_generic<const IS_CJK: bool>(c: char) -> Option<usize> {
    if c < '\u{7F}' {
        if c >= '\u{20}' {
            // U+0020 to U+007F (exclusive) are single-width ASCII codepoints
            Some(1)
        } else {
            // U+0000 to U+0020 (exclusive) are control codes
            None
        }
    } else if c >= '\u{A0}' {
        // No characters >= U+00A0 are control codes, so we can consult the lookup tables
        Some(lookup_width_generic::<IS_CJK>(c).0.into())
    } else {
        // U+007F to U+00A0 (exclusive) are control codes
        None
    }
}

/// Returns the [UAX #11](https://www.unicode.org/reports/tr11/) based width of `c`.
/// Ambiguous width characters are treated as narrow.
#[inline]
pub(crate) fn width_in_str(c: char, next_info: WidthInfo) -> (i8, WidthInfo) {
    width_in_generic::<false>(c, next_info)
}

/// Returns the [UAX #11](https://www.unicode.org/reports/tr11/) based width of `c`.
/// Ambiguous width characters are treated as wide.
#[cfg(feature = "cjk")]
#[inline]
pub(crate) fn width_in_str_cjk(c: char, next_info: WidthInfo) -> (i8, WidthInfo) {
    width_in_generic::<true>(c, next_info)
}

#[inline]
pub(crate) fn width_in_generic<const IS_CJK: bool>(
    c: char,
    mut next_info: WidthInfo,
) -> (i8, WidthInfo) {
    if next_info.is_emoji_presentation() {
        if starts_emoji_presentation_seq(c) {
            let width = if next_info.is_zwj_emoji_presentation() {
                0
            } else {
                2
            };
            return (width, WidthInfo::EMOJI_PRESENTATION);
        } else {
            next_info = next_info.unset_emoji_presentation();
        }
    }

    if IS_CJK
        && (matches!(
            next_info,
            WidthInfo::COMBINING_LONG_SOLIDUS_OVERLAY | WidthInfo::SOLIDUS_OVERLAY_ALEF
        ) && matches!(c, '<' | '=' | '>'))
    {
        return (2, WidthInfo::DEFAULT);
    }

    if c <= '\u{A0}' {
        match c {
            '\n' => (1, WidthInfo::LINE_FEED),
            '\r' if next_info == WidthInfo::LINE_FEED => (0, WidthInfo::DEFAULT),
            _ => (1, WidthInfo::DEFAULT),
        }
    } else {
        // Fast path
        if next_info != WidthInfo::DEFAULT {
            if c == '\u{FE0F}' {
                return (0, next_info.set_emoji_presentation());
            }

            if IS_CJK {
                if matches!(c, '\u{FE00}' | '\u{FE02}') {
                    return (0, next_info.set_vs1_2_3());
                }
            } else {
                if c == '\u{FE01}' {
                    return (0, next_info.set_vs1_2_3());
                }
                if c == '\u{FE0E}' {
                    return (0, next_info.set_text_presentation());
                }
                if next_info.is_text_presentation() {
                    if starts_non_ideographic_text_presentation_seq(c) {
                        return (1, WidthInfo::DEFAULT);
                    } else {
                        next_info = next_info.unset_text_presentation();
                    }
                }
            }

            if next_info.is_vs1_2_3() {
                if matches!(c, '\u{2018}' | '\u{2019}' | '\u{201C}' | '\u{201D}') {
                    return (if IS_CJK { 1 } else { 2 }, WidthInfo::DEFAULT);
                } else {
                    next_info = next_info.unset_vs1_2_3();
                }
            }
            if next_info.is_ligature_transparent() {
                if c == '\u{200D}' {
                    return (0, next_info.set_zwj_bit());
                } else if is_ligature_transparent(c) {
                    return (0, next_info);
                }
            }

            match (next_info, c) {
                #[cfg(feature = "cjk")]
                (WidthInfo::COMBINING_LONG_SOLIDUS_OVERLAY, _)
                    if IS_CJK && is_solidus_transparent(c) =>
                {
                    return (
                        lookup_width_generic::<IS_CJK>(c).0 as i8,
                        WidthInfo::COMBINING_LONG_SOLIDUS_OVERLAY,
                    );
                }
                (WidthInfo::JOINING_GROUP_ALEF, '\u{0338}') if IS_CJK => {
                    return (0, WidthInfo::SOLIDUS_OVERLAY_ALEF);
                }
                // Arabic Lam-Alef ligature
                (WidthInfo::JOINING_GROUP_ALEF, _) if is_joining_group_lam(c) => {
                    return (0, WidthInfo::DEFAULT)
                }
                (WidthInfo::SOLIDUS_OVERLAY_ALEF, _) if IS_CJK && is_joining_group_lam(c) => {
                    return (0, WidthInfo::DEFAULT)
                }
                (WidthInfo::JOINING_GROUP_ALEF, _) if is_transparent_zero_width(c) => {
                    return (0, WidthInfo::JOINING_GROUP_ALEF);
                }

                // Hebrew Alef-ZWJ-Lamed ligature
                (WidthInfo::ZWJ_HEBREW_LETTER_LAMED, '\u{05D0}') => {
                    return (0, WidthInfo::DEFAULT);
                }

                // Khmer coeng signs
                (WidthInfo::KHMER_COENG_ELIGIBLE_LETTER, '\u{17D2}') => {
                    return (-1, WidthInfo::DEFAULT);
                }

                // Buginese <a, -i> ZWJ ya ligature
                (WidthInfo::ZWJ_BUGINESE_LETTER_YA, '\u{1A17}') => {
                    return (0, WidthInfo::BUGINESE_VOWEL_SIGN_I_ZWJ_LETTER_YA)
                }
                (WidthInfo::BUGINESE_VOWEL_SIGN_I_ZWJ_LETTER_YA, '\u{1A15}') => {
                    return (0, WidthInfo::DEFAULT)
                }

                // Tifinagh bi-consonants
                (WidthInfo::TIFINAGH_CONSONANT | WidthInfo::ZWJ_TIFINAGH_CONSONANT, '\u{2D7F}') => {
                    return (1, WidthInfo::TIFINAGH_JOINER_CONSONANT);
                }
                (WidthInfo::ZWJ_TIFINAGH_CONSONANT, '\u{2D31}'..='\u{2D65}' | '\u{2D6F}') => {
                    return (0, WidthInfo::DEFAULT);
                }
                (WidthInfo::TIFINAGH_JOINER_CONSONANT, '\u{2D31}'..='\u{2D65}' | '\u{2D6F}') => {
                    return (-1, WidthInfo::DEFAULT);
                }

                // Lisu tone letter combinations
                (WidthInfo::LISU_TONE_LETTER_MYA_NA_JEU, '\u{A4F8}'..='\u{A4FB}') => {
                    return (0, WidthInfo::DEFAULT);
                }

                // Old Turkic ligature
                (WidthInfo::ZWJ_OLD_TURKIC_LETTER_ORKHON_I, '\u{10C32}') => {
                    return (0, WidthInfo::DEFAULT);
                }
                // Emoji modifier
                (WidthInfo::EMOJI_MODIFIER, _) if is_emoji_modifier_base(c) => {
                    return (0, WidthInfo::EMOJI_PRESENTATION);
                }

                // Regional indicator
                (
                    WidthInfo::REGIONAL_INDICATOR | WidthInfo::SEVERAL_REGIONAL_INDICATOR,
                    '\u{1F1E6}'..='\u{1F1FF}',
                ) => return (1, WidthInfo::SEVERAL_REGIONAL_INDICATOR),

                // ZWJ emoji
                (
                    WidthInfo::EMOJI_PRESENTATION
                    | WidthInfo::SEVERAL_REGIONAL_INDICATOR
                    | WidthInfo::EVEN_REGIONAL_INDICATOR_ZWJ_PRESENTATION
                    | WidthInfo::ODD_REGIONAL_INDICATOR_ZWJ_PRESENTATION
                    | WidthInfo::EMOJI_MODIFIER,
                    '\u{200D}',
                ) => return (0, WidthInfo::ZWJ_EMOJI_PRESENTATION),
                (WidthInfo::ZWJ_EMOJI_PRESENTATION, '\u{20E3}') => {
                    return (0, WidthInfo::KEYCAP_ZWJ_EMOJI_PRESENTATION);
                }
                (WidthInfo::VS16_ZWJ_EMOJI_PRESENTATION, _) if starts_emoji_presentation_seq(c) => {
                    return (0, WidthInfo::EMOJI_PRESENTATION)
                }
                (WidthInfo::VS16_KEYCAP_ZWJ_EMOJI_PRESENTATION, '0'..='9' | '#' | '*') => {
                    return (0, WidthInfo::EMOJI_PRESENTATION)
                }
                (WidthInfo::ZWJ_EMOJI_PRESENTATION, '\u{1F1E6}'..='\u{1F1FF}') => {
                    return (1, WidthInfo::REGIONAL_INDICATOR_ZWJ_PRESENTATION);
                }
                (
                    WidthInfo::REGIONAL_INDICATOR_ZWJ_PRESENTATION
                    | WidthInfo::ODD_REGIONAL_INDICATOR_ZWJ_PRESENTATION,
                    '\u{1F1E6}'..='\u{1F1FF}',
                ) => return (-1, WidthInfo::EVEN_REGIONAL_INDICATOR_ZWJ_PRESENTATION),
                (
                    WidthInfo::EVEN_REGIONAL_INDICATOR_ZWJ_PRESENTATION,
                    '\u{1F1E6}'..='\u{1F1FF}',
                ) => return (3, WidthInfo::ODD_REGIONAL_INDICATOR_ZWJ_PRESENTATION),
                (WidthInfo::ZWJ_EMOJI_PRESENTATION, '\u{1F3FB}'..='\u{1F3FF}') => {
                    return (0, WidthInfo::EMOJI_MODIFIER);
                }
                (WidthInfo::ZWJ_EMOJI_PRESENTATION, '\u{E007F}') => {
                    return (0, WidthInfo::TAG_END_ZWJ_EMOJI_PRESENTATION);
                }
                (WidthInfo::TAG_END_ZWJ_EMOJI_PRESENTATION, '\u{E0061}'..='\u{E007A}') => {
                    return (0, WidthInfo::TAG_A1_END_ZWJ_EMOJI_PRESENTATION);
                }
                (WidthInfo::TAG_A1_END_ZWJ_EMOJI_PRESENTATION, '\u{E0061}'..='\u{E007A}') => {
                    return (0, WidthInfo::TAG_A2_END_ZWJ_EMOJI_PRESENTATION)
                }
                (WidthInfo::TAG_A2_END_ZWJ_EMOJI_PRESENTATION, '\u{E0061}'..='\u{E007A}') => {
                    return (0, WidthInfo::TAG_A3_END_ZWJ_EMOJI_PRESENTATION)
                }
                (WidthInfo::TAG_A3_END_ZWJ_EMOJI_PRESENTATION, '\u{E0061}'..='\u{E007A}') => {
                    return (0, WidthInfo::TAG_A4_END_ZWJ_EMOJI_PRESENTATION)
                }
                (WidthInfo::TAG_A4_END_ZWJ_EMOJI_PRESENTATION, '\u{E0061}'..='\u{E007A}') => {
                    return (0, WidthInfo::TAG_A5_END_ZWJ_EMOJI_PRESENTATION)
                }
                (WidthInfo::TAG_A5_END_ZWJ_EMOJI_PRESENTATION, '\u{E0061}'..='\u{E007A}') => {
                    return (0, WidthInfo::TAG_A6_END_ZWJ_EMOJI_PRESENTATION)
                }
                (
                    WidthInfo::TAG_END_ZWJ_EMOJI_PRESENTATION
                    | WidthInfo::TAG_A1_END_ZWJ_EMOJI_PRESENTATION
                    | WidthInfo::TAG_A2_END_ZWJ_EMOJI_PRESENTATION
                    | WidthInfo::TAG_A3_END_ZWJ_EMOJI_PRESENTATION
                    | WidthInfo::TAG_A4_END_ZWJ_EMOJI_PRESENTATION,
                    '\u{E0030}'..='\u{E0039}',
                ) => return (0, WidthInfo::TAG_D1_END_ZWJ_EMOJI_PRESENTATION),
                (WidthInfo::TAG_D1_END_ZWJ_EMOJI_PRESENTATION, '\u{E0030}'..='\u{E0039}') => {
                    return (0, WidthInfo::TAG_D2_END_ZWJ_EMOJI_PRESENTATION);
                }
                (WidthInfo::TAG_D2_END_ZWJ_EMOJI_PRESENTATION, '\u{E0030}'..='\u{E0039}') => {
                    return (0, WidthInfo::TAG_D3_END_ZWJ_EMOJI_PRESENTATION);
                }
                (
                    WidthInfo::TAG_A3_END_ZWJ_EMOJI_PRESENTATION
                    | WidthInfo::TAG_A4_END_ZWJ_EMOJI_PRESENTATION
                    | WidthInfo::TAG_A5_END_ZWJ_EMOJI_PRESENTATION
                    | WidthInfo::TAG_A6_END_ZWJ_EMOJI_PRESENTATION
                    | WidthInfo::TAG_D3_END_ZWJ_EMOJI_PRESENTATION,
                    '\u{1F3F4}',
                ) => return (0, WidthInfo::EMOJI_PRESENTATION),
                (WidthInfo::ZWJ_EMOJI_PRESENTATION, _)
                    if lookup_width_generic::<IS_CJK>(c).1 == WidthInfo::EMOJI_PRESENTATION =>
                {
                    return (0, WidthInfo::EMOJI_PRESENTATION)
                }

                (WidthInfo::KIRAT_RAI_VOWEL_SIGN_E, '\u{16D63}') => {
                    return (0, WidthInfo::DEFAULT);
                }
                (WidthInfo::KIRAT_RAI_VOWEL_SIGN_E, '\u{16D67}') => {
                    return (0, WidthInfo::KIRAT_RAI_VOWEL_SIGN_AI);
                }
                (WidthInfo::KIRAT_RAI_VOWEL_SIGN_E, '\u{16D68}') => {
                    return (1, WidthInfo::KIRAT_RAI_VOWEL_SIGN_E);
                }
                (WidthInfo::KIRAT_RAI_VOWEL_SIGN_E, '\u{16D69}') => {
                    return (0, WidthInfo::DEFAULT);
                }
                (WidthInfo::KIRAT_RAI_VOWEL_SIGN_AI, '\u{16D63}') => {
                    return (0, WidthInfo::DEFAULT);
                }

                // Fallback
                _ => {}
            }
        }

        let ret = lookup_width_generic::<IS_CJK>(c);
        (ret.0 as i8, ret.1)
    }
}

#[inline]
pub fn str_width<S: DoubleEndedIterator<Item = char>>(s: S) -> usize {
    s.rfold((0usize, WidthInfo::DEFAULT), |(sum, next_info), c| {
        let (add, info) = width_in_str(c, next_info);
        (sum.wrapping_add_signed(isize::from(add)), info)
    })
    .0
}

#[cfg(feature = "cjk")]
#[inline]
pub fn str_width_cjk<S: DoubleEndedIterator<Item = char>>(s: S) -> usize {
    s.rfold((0usize, WidthInfo::DEFAULT), |(sum, next_info), c| {
        let (add, info) = width_in_str_cjk(c, next_info);
        (sum.wrapping_add_signed(isize::from(add)), info)
    })
    .0
}
