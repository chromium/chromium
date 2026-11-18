use super::plan::ShapePlan;
use crate::buffer::*;
use crate::ot::shaper::{ComposeFn, DecomposeFn, MAX_COMBINING_MARKS};
use crate::unicode::{space_fallback, CharExt, Codepoint};
use crate::NominalGlyphs;
use crate::ShaperFont;
use read_fonts::types::GlyphId;

impl GlyphInfo {
    declare_buffer_var!(
        u32,
        1,
        0,
        NORMALIZER_GLYPH_INDEX_VAR,
        normalizer_glyph_index,
        set_normalizer_glyph_index
    );
}

pub struct NormalizeContext<'a, 'x, 'c, 'd> {
    pub plan: &'a ShapePlan,
    pub buffer: &'x mut Buffer,
    pub font: &'x ShaperFont<'c, 'd>,
    pub decompose: DecomposeFn,
    pub compose: ComposeFn,
}

impl NormalizeContext<'_, '_, '_, '_> {
    fn nominal_glyph(&mut self, codepoint: u32) -> Option<GlyphId> {
        self.font.nominal_glyph(codepoint)
    }

    fn variation_glyph(&mut self, codepoint: u32, selector: u32) -> Option<GlyphId> {
        self.font.variation_glyph(codepoint, selector)
    }

    fn set_current_glyph(&mut self) -> Option<()> {
        let info = self.buffer.cur_mut(0)?;
        if let Some(glyph_id) = self.font.nominal_glyph(info.glyph_id) {
            info.set_normalizer_glyph_index(u32::from(glyph_id));
        }
        Some(())
    }
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum NormalizationMode {
    None,
    Decomposed,
    /// Never composes base to base.
    ComposedDiacritics,
    /// Fully decomposes and recomposes.
    ComposedDiacriticsNoShortCircuit,
    Auto,
}

// HIGHLEVEL DESIGN:
//
// This file exports one main function: normalize().
//
// This function closely reflects the Unicode Normalization Algorithm,
// yet it's different.
//
// Each shaper specifies whether it prefers decomposed (NFD) or composed (NFC).
// The logic however tries to use whatever the font can support.
//
// In general what happens is that: each grapheme is decomposed in a chain
// of 1:2 decompositions, marks reordered, and then recomposed if desired,
// so far it's like Unicode Normalization.  However, the decomposition and
// recomposition only happens if the font supports the resulting characters.
//
// The goals are:
//
//   - Try to render all canonically equivalent strings similarly.  To really
//     achieve this we have to always do the full decomposition and then
//     selectively recompose from there.  It's kinda too expensive though, so
//     we skip some cases.  For example, if composed is desired, we simply
//     don't touch 1-character clusters that are supported by the font, even
//     though their NFC may be different.
//
//   - When a font has a precomposed character for a sequence but the 'ccmp'
//     feature in the font is not adequate, use the precomposed character
//     which typically has better mark positioning.
//
//   - When a font does not support a combining mark, but supports it precomposed
//     with previous base, use that.  This needs the itemizer to have this
//     knowledge too.  We need to provide assistance to the itemizer.
//
//   - When a font does not support a character but supports its canonical
//     decomposition, well, use the decomposition.
//
//   - The shapers can customize the compose and decompose functions to
//     offload some of their requirements to the normalizer.  For example, the
//     Indic shaper may want to disallow recomposing of two matras.

fn decompose_unicode(_: &NormalizeContext, ab: Codepoint) -> Option<(Codepoint, Codepoint)> {
    crate::unicode::decompose_impl(ab)
}

fn compose_unicode(_: &NormalizeContext, a: Codepoint, b: Codepoint) -> Option<Codepoint> {
    crate::unicode::compose(a, b)
}

fn output_char(buffer: &mut Buffer, unichar: u32, glyph: u32) -> Option<()> {
    // This is very confusing indeed.
    buffer.cur_mut(0)?.set_normalizer_glyph_index(glyph);
    buffer.output_glyph(unichar);
    // TODO: should be _hb_glyph_info_set_unicode_props (&buffer->prev(), buffer);
    let mut flags = buffer.scratch_flags;
    buffer.prev_mut()?.init_unicode_props(&mut flags);
    buffer.scratch_flags = flags;
    Some(())
}

fn next_char(buffer: &mut Buffer, glyph: u32) -> Option<()> {
    buffer.cur_mut(0)?.set_normalizer_glyph_index(glyph);
    buffer.next_glyph();
    Some(())
}

fn skip_char(buffer: &mut Buffer) {
    buffer.skip_glyph();
}

/// Returns 0 if didn't decompose, number of resulting characters otherwise.
fn decompose(ctx: &mut NormalizeContext, shortest: bool, ab: Codepoint) -> u32 {
    let Some((a, b)) = (ctx.decompose)(ctx, ab) else {
        return 0;
    };

    let a_glyph = ctx.nominal_glyph(a);
    let b_glyph = if b != 0 {
        match ctx.nominal_glyph(b) {
            Some(glyph_id) => Some(glyph_id),
            None => return 0,
        }
    } else {
        None
    };

    if let Some(a_glyph) = a_glyph {
        if shortest {
            // Output a and b
            if output_char(ctx.buffer, a, u32::from(a_glyph)).is_none() {
                ctx.buffer.index_failed();
            }
            if let Some(b_glyph) = b_glyph {
                if output_char(ctx.buffer, b, u32::from(b_glyph)).is_none() {
                    ctx.buffer.index_failed();
                }
                return 2;
            }
            return 1;
        }
    }

    let ret = decompose(ctx, shortest, a);
    if ret != 0 {
        if let Some(b_glyph) = b_glyph {
            if output_char(ctx.buffer, b, u32::from(b_glyph)).is_none() {
                ctx.buffer.index_failed();
            }
            return ret + 1;
        }
        return ret;
    }

    if let Some(a_glyph) = a_glyph {
        if output_char(ctx.buffer, a, u32::from(a_glyph)).is_none() {
            ctx.buffer.index_failed();
        }
        if let Some(b_glyph) = b_glyph {
            if output_char(ctx.buffer, b, u32::from(b_glyph)).is_none() {
                ctx.buffer.index_failed();
            }
            return 2;
        }
        return 1;
    }

    0
}

fn decompose_current_character(ctx: &mut NormalizeContext, shortest: bool) -> Option<()> {
    let u = ctx.buffer.cur(0)?.as_codepoint();
    let glyph = ctx.nominal_glyph(u);

    if let Some(glyph) = glyph {
        if shortest {
            if next_char(ctx.buffer, u32::from(glyph)).is_none() {
                ctx.buffer.index_failed();
            }
            return Some(());
        }
    }

    if decompose(ctx, shortest, u) > 0 {
        skip_char(ctx.buffer);
        return Some(());
    }

    if let Some(glyph) = glyph {
        if next_char(ctx.buffer, u32::from(glyph)).is_none() {
            ctx.buffer.index_failed();
        }
        return Some(());
    }

    if ctx.buffer.cur(0)?.is_unicode_space() {
        let space_type = u.space_fallback();
        if space_type != space_fallback::NOT_SPACE {
            let space_glyph = ctx.nominal_glyph(0x0020).or(ctx.buffer.invisible);

            if let Some(space_glyph) = space_glyph {
                ctx.buffer
                    .cur_mut(0)?
                    .set_unicode_space_fallback_type(space_type);
                if next_char(ctx.buffer, u32::from(space_glyph)).is_none() {
                    ctx.buffer.index_failed();
                }
                ctx.buffer.scratch_flags |= HB_BUFFER_SCRATCH_FLAG_HAS_SPACE_FALLBACK;
                return Some(());
            }
        }
    }

    // U+2011 is the only sensible character that is a no-break version of another character
    // and not a space.  The space ones are handled already.  Handle this lone one.
    if u == 0x2011 {
        if let Some(other_glyph) = ctx.nominal_glyph(0x2010) {
            if next_char(ctx.buffer, u32::from(other_glyph)).is_none() {
                ctx.buffer.index_failed();
            }
            return Some(());
        }
    }

    // Insert a .notdef glyph if decomposition failed.
    if next_char(ctx.buffer, 0).is_none() {
        ctx.buffer.index_failed();
    }
    Some(())
}

fn handle_variation_selector_cluster(
    ctx: &mut NormalizeContext,
    end: usize,
    _: bool,
) -> Option<()> {
    // Currently if there's a variation-selector we give-up on normalization, it's just too hard.
    while ctx.buffer.idx < end - 1 && ctx.buffer.successful {
        if ctx.buffer.cur(1)?.as_codepoint().is_variation_selector() {
            let base = ctx.buffer.cur(0)?.as_codepoint();
            let selector = ctx.buffer.cur(1)?.as_codepoint();
            if let Some(glyph_id) = ctx.variation_glyph(base, selector) {
                ctx.buffer
                    .cur_mut(0)?
                    .set_normalizer_glyph_index(u32::from(glyph_id));
                let unicode = ctx.buffer.cur(0)?.glyph_id;
                ctx.buffer.replace_glyphs(2, 1, &[unicode]);
            } else {
                // Just pass on the two characters separately, let GSUB do its magic.
                if ctx.set_current_glyph().is_none() {
                    ctx.buffer.index_failed();
                }
                ctx.buffer.next_glyph();

                ctx.buffer.scratch_flags |= HB_BUFFER_SCRATCH_FLAG_HAS_VARIATION_SELECTOR_FALLBACK;

                ctx.buffer.cur_mut(0)?.set_variation_selector(true);

                if ctx.buffer.not_found_variation_selector.is_some() {
                    ctx.buffer.cur_mut(0)?.clear_default_ignorable();
                }

                if ctx.set_current_glyph().is_none() {
                    ctx.buffer.index_failed();
                }
                ctx.buffer.next_glyph();
            }

            // Skip any further variation selectors.
            while ctx.buffer.idx < end && ctx.buffer.cur(0)?.as_codepoint().is_variation_selector()
            {
                if ctx.set_current_glyph().is_none() {
                    ctx.buffer.index_failed();
                }
                ctx.buffer.next_glyph();
            }
        } else {
            if ctx.set_current_glyph().is_none() {
                ctx.buffer.index_failed();
            }
            ctx.buffer.next_glyph();
        }
    }

    if ctx.buffer.idx < end {
        if ctx.set_current_glyph().is_none() {
            ctx.buffer.index_failed();
        }
        ctx.buffer.next_glyph();
    }
    Some(())
}

fn decompose_multi_char_cluster(ctx: &mut NormalizeContext, end: usize, short_circuit: bool) {
    let mut i = ctx.buffer.idx;
    while i < end && ctx.buffer.successful {
        if ctx
            .buffer
            .info
            .get(i)
            .is_some_and(|info| info.as_codepoint().is_variation_selector())
        {
            if handle_variation_selector_cluster(ctx, end, short_circuit).is_none() {
                ctx.buffer.index_failed();
            }
            return;
        }
        i += 1;
    }

    while ctx.buffer.idx < end && ctx.buffer.successful {
        if decompose_current_character(ctx, short_circuit).is_none() {
            ctx.buffer.index_failed();
        }
    }
}

fn compare_combining_class(pa: &GlyphInfo, pb: &GlyphInfo) -> bool {
    let a = pa.modified_combining_class();
    let b = pb.modified_combining_class();
    a > b
}

pub fn normalize<'x>(
    plan: &ShapePlan,
    buffer: &'x mut Buffer,
    font: &'x ShaperFont<'_, '_>,
) -> Option<()> {
    if buffer.is_empty() {
        return Some(());
    }

    buffer.assert_unicode_vars();

    let mut mode = plan.shaper.normalization_preference;
    if mode == NormalizationMode::Auto {
        if plan.has_gpos_mark {
            // https://github.com/harfbuzz/harfbuzz/issues/653#issuecomment-423905920
            // mode = Some(NormalizationMode::Decomposed);
            mode = NormalizationMode::ComposedDiacritics;
        } else {
            mode = NormalizationMode::ComposedDiacritics;
        }
    }

    let mut ctx = NormalizeContext {
        plan,
        buffer,
        font,
        decompose: plan.shaper.decompose.unwrap_or(decompose_unicode),
        compose: plan.shaper.compose.unwrap_or(compose_unicode),
    };

    let always_short_circuit = mode == NormalizationMode::None;
    let might_short_circuit = always_short_circuit
        || (mode != NormalizationMode::Decomposed
            && mode != NormalizationMode::ComposedDiacriticsNoShortCircuit);

    // We do a fairly straightforward yet custom normalization process in three
    // separate rounds: decompose, reorder, recompose (if desired).  Currently
    // this makes two buffer swaps.  We can make it faster by moving the last
    // two rounds into the inner loop for the first round, but it's more readable
    // this way.

    // First round, decompose
    let mut all_simple = true;
    if message!(ctx, "start decompose") {
        ctx.buffer.clear_output();
        let count = ctx.buffer.len;
        ctx.buffer.idx = 0;
        loop {
            let mut end = ctx.buffer.idx + 1;
            while end < count
                && ctx
                    .buffer
                    .info
                    .get(end)
                    .is_some_and(|info| !info.is_unicode_mark())
            {
                end += 1;
            }

            if end < count {
                // Leave one base for the marks to cluster with.
                end -= 1;
            }

            // From idx to end are simple clusters.
            if might_short_circuit {
                let idx = ctx.buffer.idx;
                // Any glyphs not mapped here are handled one at a time below.
                let done = ctx.buffer.info.get_mut(idx..end).map_or(0, |infos| {
                    ctx.font.nominal_glyphs(NominalGlyphs::new(infos))
                });
                ctx.buffer.next_glyphs(done);
            }

            while ctx.buffer.idx < end && ctx.buffer.successful {
                if decompose_current_character(&mut ctx, might_short_circuit).is_none() {
                    ctx.buffer.index_failed();
                }
            }

            if ctx.buffer.idx == count || !ctx.buffer.successful {
                break;
            }

            all_simple = false;

            // Find all the marks now.
            end = ctx.buffer.idx + 1;
            while end < count
                && ctx
                    .buffer
                    .info
                    .get(end)
                    .is_some_and(|info| info.is_unicode_mark())
            {
                end += 1;
            }

            // idx to end is one non-simple cluster.
            decompose_multi_char_cluster(&mut ctx, end, always_short_circuit);

            if ctx.buffer.idx >= count || !ctx.buffer.successful {
                break;
            }
        }

        ctx.buffer.sync();
        message!(ctx, "end decompose");
    }

    // Second round, reorder (inplace)
    if !all_simple && buffer_message!(ctx.buffer, ctx.font, "start reorder") {
        let count = ctx.buffer.len;
        let mut i = 0;
        while i < count {
            if ctx
                .buffer
                .info
                .get(i)
                .is_none_or(|info| info.modified_combining_class() == 0)
            {
                i += 1;
                continue;
            }

            let mut end = i + 1;
            while end < count
                && ctx
                    .buffer
                    .info
                    .get(end)
                    .is_some_and(|info| info.modified_combining_class() != 0)
            {
                end += 1;
            }

            // We are going to do a O(n^2).  Only do this if the sequence is short.
            if end - i <= MAX_COMBINING_MARKS {
                ctx.buffer.sort(i, end, compare_combining_class);

                if let Some(reorder_marks) = ctx.plan.shaper.reorder_marks {
                    reorder_marks(ctx.plan, ctx.buffer, i, end);
                }
            }

            i = end + 1;
        }
        message!(ctx, "end reorder");
    }
    if ctx.buffer.scratch_flags & HB_BUFFER_SCRATCH_FLAG_HAS_CGJ != 0 {
        // For all CGJ, check if it prevented any reordering at all.
        // If it did NOT, then make it skippable.
        // https://github.com/harfbuzz/harfbuzz/issues/554
        let infos = ctx.buffer.info_slice_mut();
        for i in 1..infos.len().saturating_sub(1) {
            let Some([last, info, next]) = infos.get_mut(i - 1..=i + 1) else {
                continue;
            };
            if info.glyph_id == 0x034F
            /* CGJ */
            {
                let last = last.modified_combining_class();
                let next = next.modified_combining_class();
                if next == 0 || last <= next {
                    info.unhide();
                }
            }
        }
    }

    // Third round, recompose
    if !all_simple
        && ctx.buffer.successful
        && (mode == NormalizationMode::ComposedDiacritics
            || mode == NormalizationMode::ComposedDiacriticsNoShortCircuit)
        && message!(ctx, "start compose")
    {
        // As noted in the comment earlier, we don't try to combine
        // ccc=0 chars with their previous Starter.

        let count = ctx.buffer.len;
        let mut starter = 0;
        ctx.buffer.clear_output();
        ctx.buffer.next_glyph();
        while ctx.buffer.idx < count && ctx.buffer.successful {
            // We don't try to compose a non-mark character with it's preceding starter.
            // This is both an optimization to avoid trying to compose every two neighboring
            // glyphs in most scripts AND a desired feature for Hangul.  Apparently Hangul
            // fonts are not designed to mix-and-match pre-composed syllables and Jamo.
            let cur = ctx.buffer.cur(0)?;
            if cur.is_unicode_mark() &&
                // If there's anything between the starter and this char, they should have CCC
                // smaller than this character's.
                (starter == ctx.buffer.out_len - 1
                    || ctx.buffer.prev()?.modified_combining_class() < cur.modified_combining_class())
            {
                let composed = ctx.buffer.out_info().get(starter).and_then(|starter_info| {
                    (ctx.compose)(&ctx, starter_info.as_codepoint(), cur.as_codepoint())
                });
                if let Some(composed) = composed {
                    if let Some(glyph_id) = ctx.nominal_glyph(composed) {
                        // Copy to out-ctx.buffer.
                        ctx.buffer.next_glyph();
                        if !ctx.buffer.successful {
                            return Some(());
                        }

                        // Merge and remove the second composable.
                        ctx.buffer.merge_out_clusters(starter, ctx.buffer.out_len);
                        ctx.buffer.out_len -= 1;

                        // Modify starter and carry on.
                        let mut flags = ctx.buffer.scratch_flags;
                        if let Some(info) = ctx.buffer.out_info_mut().get_mut(starter) {
                            info.glyph_id = composed;
                            info.set_normalizer_glyph_index(u32::from(glyph_id));
                            info.init_unicode_props(&mut flags);
                        }
                        ctx.buffer.scratch_flags = flags;

                        continue;
                    }
                }
            }

            // Blocked, or doesn't compose.
            ctx.buffer.next_glyph();

            if ctx.buffer.prev()?.modified_combining_class() == 0 {
                starter = ctx.buffer.out_len - 1;
            }
        }

        ctx.buffer.sync();
        message!(ctx, "end compose");
    }
    Some(())
}
