/// Shapes Unicode content in place using the configured font.
///
/// A supplied plan must match the buffer's direction and script. On success,
/// the buffer contains glyphs.
///
/// # Errors
///
/// Returns an error if the buffer cannot be shaped with the supplied plan or
/// lacks the properties needed to build one.
pub fn shape(
    font: &ShaperFont<'_, '_>,
    buffer: &mut Buffer,
    options: ShapeOptions<'_>,
) -> Result<(), ShapeError> {
    if buffer.content_type == Some(ContentType::Glyphs) {
        return Err(ShapeError::AlreadyShaped);
    }
    if let Some(plan) = options.plan {
        shape_with_font(plan, font, buffer, options.features, options.point_size)
    } else {
        if buffer.direction == Direction::Invalid {
            return Err(ShapeError::DirectionUnset);
        }
        let plan = ShapePlan::from_layout(
            font.layout(),
            buffer.direction,
            buffer.script,
            buffer.language.as_ref(),
            options.features,
        );
        shape_with_font(&plan, font, buffer, options.features, options.point_size)
    }
}

use crate::aat;
use crate::buffer::GlyphFlags;
use crate::buffer::*;
use crate::ot::gpos;
use crate::ot::layout::*;
use crate::ot::shaper::*;
use crate::ot::*;
use crate::plan::ShapePlan;
use crate::unicode::{CharExt, GeneralCategory};
use crate::{fallback, normalize};
use crate::{Advances, ShapeError, ShapeOptions, ShaperFont};
use crate::{BufferFlags, ContentType, Direction, Feature, Mask, Script};

/// Runs a compiled plan using prepared layout data and effective font queries.
pub(crate) fn shape_with_font(
    plan: &ShapePlan,
    font: &ShaperFont<'_, '_>,
    buffer: &mut Buffer,
    features: &[Feature],
    point_size: Option<f32>,
) -> Result<(), ShapeError> {
    if buffer.content_type == Some(ContentType::Glyphs) {
        return Err(ShapeError::AlreadyShaped);
    }
    if buffer.direction != plan.direction {
        return Err(ShapeError::DirectionMismatch {
            plan: plan.direction,
            buffer: buffer.direction,
        });
    }
    let plan_script = plan.script.unwrap_or(Script::UNKNOWN);
    let buffer_script = buffer.script.unwrap_or(Script::UNKNOWN);
    if buffer_script != plan_script {
        return Err(ShapeError::ScriptMismatch {
            plan: plan_script,
            buffer: buffer_script,
        });
    }

    buffer.enter();
    if buffer.len > 0 {
        let target_direction = buffer.direction;
        OtShapeContext {
            plan,
            font,
            buffer,
            features,
            target_direction,
            point_size,
        }
        .shape_internal();
    }
    buffer.leave();
    buffer.content_type = Some(ContentType::Glyphs);
    Ok(())
}

// hb_ot_shape_context_t: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L450>
struct OtShapeContext<'a, 'c, 'd> {
    plan: &'a ShapePlan,
    font: &'a ShaperFont<'c, 'd>,
    buffer: &'a mut Buffer,
    features: &'a [Feature],
    // Transient stuff
    target_direction: Direction,
    point_size: Option<f32>,
}

impl OtShapeContext<'_, '_, '_> {
    fn glyph_h_advances(&mut self) {
        if self.buffer.len == 0 {
            return;
        }
        let batched_advances = Advances::new(self.buffer);
        self.font.glyph_h_advances(batched_advances);
    }

    // hb_ot_shape_internal: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L1171>
    fn shape_internal(&mut self) {
        self.buffer.allocate_unicode_vars();

        self.set_unicode_props(self.plan.ot_map.get_global_mask());
        self.insert_dotted_circle();

        form_clusters(self.buffer);

        ensure_native_direction(self.buffer);

        if let Some(func) = self.plan.shaper.preprocess_text {
            func(self.plan, self.font, self.buffer);
        }

        self.substitute_pre();
        self.position();
        self.substitute_post();

        propagate_flags(self.buffer);

        self.buffer.deallocate_unicode_vars();

        self.buffer.direction = self.target_direction;
    }

    // hb_ot_substitute_pre: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L936>
    fn substitute_pre(&mut self) {
        self.substitute_default();

        self.buffer.allocate_gsubgpos_vars();

        self.substitute_plan();

        if self.plan.apply_morx && self.plan.apply_gpos {
            aat::layout::remove_deleted_glyphs(self.buffer);
        }
    }

    // hb_ot_substitute_post: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L951>
    fn substitute_post(&mut self) {
        if self.plan.apply_morx && !self.plan.apply_gpos {
            aat::layout::remove_deleted_glyphs(self.buffer);
        }

        deal_with_variation_selectors(self.buffer);
        hide_default_ignorables(self.buffer, self.font);

        if let Some(func) = self.plan.shaper.postprocess_glyphs {
            func(self.plan, self.font, self.buffer);
        }
    }

    // hb_ot_substitute_default: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L889>
    fn substitute_default(&mut self) {
        self.rotate_chars();

        self.buffer
            .allocate_var(GlyphInfo::NORMALIZER_GLYPH_INDEX_VAR);

        normalize::normalize(self.plan, self.buffer, self.font);

        self.setup_masks();

        // This is unfortunate to go here, but necessary...
        if self.plan.fallback_mark_positioning {
            fallback::recategorize_marks(self.buffer);
        }

        map_glyphs_fast(self.buffer);

        self.buffer
            .deallocate_var(GlyphInfo::NORMALIZER_GLYPH_INDEX_VAR);
    }

    // hb_ot_substitute_plan: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L911>
    fn substitute_plan(&mut self) {
        if self.plan.apply_morx {
            substitute_start(self.font.layout().ot, self.buffer);

            if self.plan.fallback_glyph_classes {
                synthesize_glyph_classes(self.buffer);
            }

            aat::layout::substitute(self.plan, self.font.layout(), self.buffer, self.features);
            // The digest is only read by the OT lookup-apply loop; without
            // GPOS ahead, nothing consumes it.
            if self.plan.apply_gpos {
                self.buffer.update_digest();
            }
        } else {
            substitute_start_with_digest(self.font.layout().ot, self.buffer);

            if self.plan.fallback_glyph_classes {
                synthesize_glyph_classes(self.buffer);
            }

            gsub::substitute(self.plan, self.font, self.buffer);
        }
    }

    // hb_ot_position: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L1094>
    fn position(&mut self) {
        self.buffer.clear_positions();

        self.position_default();
        self.position_plan();

        if self.buffer.direction.is_backward() {
            self.buffer.reverse();
        }

        self.buffer.deallocate_gsubgpos_vars();
    }

    // hb_ot_position_default: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L1002>
    fn position_default(&mut self) {
        let len = self.buffer.len;

        if self.buffer.direction.is_horizontal() {
            self.glyph_h_advances();
        } else {
            for (info, pos) in self.buffer.info[..len]
                .iter()
                .zip(&mut self.buffer.pos[..len])
            {
                let glyph = info.as_glyph();
                pos.y_advance = self.font.glyph_v_advance(glyph);
                let (x, y) = self.font.glyph_v_origin(glyph);
                pos.x_offset = pos.x_offset.saturating_sub(x);
                pos.y_offset = pos.y_offset.saturating_sub(y);
            }
        }

        if self.buffer.scratch_flags & HB_BUFFER_SCRATCH_FLAG_HAS_SPACE_FALLBACK != 0 {
            fallback::fallback_spaces(self.font, self.buffer);
        }
    }

    // hb_ot_position_plan: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L1029>
    fn position_plan(&mut self) {
        // If the font has no GPOS and direction is forward, then when
        // zeroing mark widths, we shift the mark with it, such that the
        // mark is positioned hanging over the previous glyph.  When
        // direction is backward we don't shift and it will end up
        // hanging over the next glyph after the final reordering.
        //
        // Note: If fallback positioning happens, we don't care about
        // this as it will be overridden.
        let adjust_offsets_when_zeroing =
            self.plan.adjust_mark_positioning_when_zeroing && self.buffer.direction.is_forward();

        // We change glyph origin to what GPOS expects (horizontal), apply GPOS, change it back.

        gpos::position_start(self.buffer);

        if self.plan.zero_marks && self.plan.shaper.zero_width_marks == ZeroWidthMarks::ByGdefEarly
        {
            zero_mark_widths_by_gdef(self.buffer, adjust_offsets_when_zeroing);
        }

        self.position_by_plan();

        if self.plan.zero_marks && self.plan.shaper.zero_width_marks == ZeroWidthMarks::ByGdefLate {
            zero_mark_widths_by_gdef(self.buffer, adjust_offsets_when_zeroing);
        }

        // Finish off.  Has to follow a certain order.
        gpos::position_finish_advances(self.buffer);
        zero_width_default_ignorables(self.buffer);
        gpos::position_finish_offsets(self.buffer);

        if self.plan.fallback_mark_positioning {
            fallback::position_marks(
                self.plan,
                self.font,
                self.buffer,
                adjust_offsets_when_zeroing,
            );
        }
    }

    // ShapePlan::position <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L271>
    fn position_by_plan(&mut self) {
        let plan = self.plan;
        let layout = self.font.layout();
        let buffer = &mut *self.buffer;
        if plan.apply_gpos {
            gpos::position(plan, self.font, buffer);
        } else if plan.apply_kerx {
            aat::layout::position(plan, layout, self.font.scale, buffer);
        }
        if plan.apply_kern {
            aat::kern::apply(plan, layout, self.font.scale, buffer);
        } else if plan.apply_fallback_kern {
            fallback::fallback_kern(plan, buffer);
        }

        if plan.apply_trak {
            aat::layout::track(plan, layout, self.font.scale, self.point_size, buffer);
        }
    }

    // hb_ot_shape_setup_masks: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L757>
    fn setup_masks(&mut self) {
        self.setup_masks_fraction();

        if let Some(func) = self.plan.shaper.setup_masks {
            func(self.plan, self.font, self.buffer);
        }

        for feature in self.features {
            if !feature.is_global() {
                let (mask, shift) = self.plan.ot_map.get_mask(feature.tag);
                self.buffer
                    .set_masks(feature.value << shift, mask, feature.start, feature.end);
            }
        }
    }

    // hb_ot_shape_setup_masks_fraction: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L685>
    fn setup_masks_fraction(&mut self) {
        let buffer = &mut *self.buffer;
        if buffer.scratch_flags & HB_BUFFER_SCRATCH_FLAG_HAS_FRACTION_SLASH == 0
            || !self.plan.has_frac
        {
            return;
        }

        let (pre_mask, post_mask) = if buffer.direction.is_forward() {
            (
                self.plan.numr_mask | self.plan.frac_mask,
                self.plan.frac_mask | self.plan.dnom_mask,
            )
        } else {
            (
                self.plan.frac_mask | self.plan.dnom_mask,
                self.plan.numr_mask | self.plan.frac_mask,
            )
        };

        let len = buffer.len;
        let mut i = 0;
        while i < len {
            // FRACTION SLASH
            if buffer.info[i].glyph_id == 0x2044 {
                let mut start = i;
                while start > 0
                    && buffer.info[start - 1].general_category() == GeneralCategory::DECIMAL_NUMBER
                {
                    start -= 1;
                }

                let mut end = i + 1;
                while end < len
                    && buffer.info[end].general_category() == GeneralCategory::DECIMAL_NUMBER
                {
                    end += 1;
                }

                if start == i || end == i + 1 {
                    if start == i {
                        buffer.unsafe_to_concat(Some(start), Some(start + 1));
                    }

                    if end == i + 1 {
                        buffer.unsafe_to_concat(Some(end - 1), Some(end));
                    }

                    i += 1;
                    continue;
                }

                buffer.unsafe_to_break(Some(start), Some(end));

                for info in &mut buffer.info[start..i] {
                    info.mask |= pre_mask;
                }

                buffer.info[i].mask |= self.plan.frac_mask;

                for info in &mut buffer.info[i + 1..end] {
                    info.mask |= post_mask;
                }

                i = end;
            } else {
                i += 1;
            }
        }
    }

    // hb_set_unicode_props: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L471>
    fn set_unicode_props(&mut self, global_mask: Mask) {
        let buffer = &mut *self.buffer;
        // Implement enough of Unicode Graphemes here that shaping
        // in reverse-direction wouldn't break graphemes.  Namely,
        // we mark all marks and ZWJ and ZWJ,Extended_Pictographic
        // sequences as continuations.  The foreach_grapheme()
        // macro uses this bit.
        //
        // https://www.unicode.org/reports/tr29/#Regex_Definitions

        let len = buffer.len;

        let mut i = 0;
        while i < len {
            let info = &mut buffer.info[i];
            info.mask = global_mask;
            info.init_unicode_props(&mut buffer.scratch_flags);

            if info.glyph_id < 0x80 {
                i += 1;
                continue;
            }

            let gen_cat = info.general_category();

            if gen_cat.flag_unsafe()
                & (GeneralCategory::LOWERCASE_LETTER.flag()
                    | GeneralCategory::UPPERCASE_LETTER.flag()
                    | GeneralCategory::TITLECASE_LETTER.flag()
                    | GeneralCategory::OTHER_LETTER.flag()
                    | GeneralCategory::SPACE_SEPARATOR.flag())
                != 0
            {
                i += 1;
                continue;
            }

            // Mutably borrow buffer.info[i] and immutably borrow
            // buffer.info[i - 1] (if present) in a way that the borrow
            // checker can understand.
            let (prior, later) = buffer.info.split_at_mut(i);
            let info = &mut later[0];

            // Marks are already set as continuation by the above line.
            // Handle Emoji_Modifier and ZWJ-continuation.
            if gen_cat == GeneralCategory::MODIFIER_SYMBOL
                && matches!(info.glyph_id, 0x1F3FB..=0x1F3FF)
            {
                info.set_continuation(&mut buffer.scratch_flags);
            } else if i != 0 && matches!(info.glyph_id, 0x1F1E6..=0x1F1FF) {
                // Should never fail because we checked for i > 0.
                // TODO: use let chains when they become stable
                let prev = prior.last().unwrap();
                if matches!(prev.glyph_id, 0x1F1E6..=0x1F1FF) && !prev.is_continuation() {
                    info.set_continuation(&mut buffer.scratch_flags);
                }
            } else if info.is_zwj() {
                info.set_continuation(&mut buffer.scratch_flags);
                if let Some(next) = buffer.info[..len].get_mut(i + 1) {
                    if next.as_codepoint().is_emoji_extended_pictographic() {
                        next.mask = global_mask;
                        next.init_unicode_props(&mut buffer.scratch_flags);
                        next.set_continuation(&mut buffer.scratch_flags);
                        i += 1;
                    }
                }
            } else if matches!(info.glyph_id, 0xFF9E..=0xFF9F | 0xE0020..=0xE007F) {
                // Or part of the Other_Grapheme_Extend that is not marks.
                // As of Unicode 15 that is just:
                //
                // 200C          ; Other_Grapheme_Extend # Cf       ZERO WIDTH NON-JOINER
                // FF9E..FF9F    ; Other_Grapheme_Extend # Lm   [2] HALFWIDTH KATAKANA VOICED SOUND MARK..HALFWIDTH KATAKANA
                // SEMI-VOICED SOUND MARK E0020..E007F  ; Other_Grapheme_Extend # Cf  [96] TAG SPACE..CANCEL TAG
                //
                // ZWNJ is special, we don't want to merge it as there's no need, and keeping
                // it separate results in more granular clusters.
                // Tags are used for Emoji sub-region flag sequences:
                // https://github.com/harfbuzz/harfbuzz/issues/1556
                // Katakana ones were requested:
                // https://github.com/harfbuzz/harfbuzz/issues/3844
                info.set_continuation(&mut buffer.scratch_flags);
            } else if info.glyph_id == 0x2044
            /* FRACTION SLASH */
            {
                buffer.scratch_flags |= HB_BUFFER_SCRATCH_FLAG_HAS_FRACTION_SLASH;
            }

            i += 1;
        }
    }

    // hb_insert_dotted_circle: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L549>
    fn insert_dotted_circle(&mut self) {
        let should_insert = {
            let buffer = &*self.buffer;
            !buffer
                .flags
                .contains(BufferFlags::DO_NOT_INSERT_DOTTED_CIRCLE)
                && buffer.flags.contains(BufferFlags::BEGINNING_OF_TEXT)
                && buffer.context_len[0] == 0
                && buffer.info[0].is_unicode_mark()
        };

        if should_insert && self.font.nominal_glyph(0x25CC).is_some() {
            let mask = self.buffer.cur(0).mask;
            let cluster = self.buffer.cur(0).cluster;
            let buffer = &mut *self.buffer;
            let mut info = GlyphInfo {
                glyph_id: 0x25CC,
                mask,
                cluster,
                ..GlyphInfo::default()
            };

            info.init_unicode_props(&mut buffer.scratch_flags);
            buffer.clear_output();
            buffer.output_info(info);
            buffer.sync();
        }
    }

    // hb_ot_rotate_chars: <https://github.com/harfbuzz/harfbuzz/blob/22ea52f42fa4fc168be91ef4e56aee3affda6e28/src/hb-ot-shape.cc#L652>
    fn rotate_chars(&mut self) {
        let len = self.buffer.len;

        if self.target_direction.is_backward() {
            let rtlm_mask = self.plan.rtlm_mask;

            for info in &mut self.buffer.info[..len] {
                if let Some(c) = info.as_codepoint().mirroring() {
                    if self.font.nominal_glyph(c).is_some() {
                        info.glyph_id = c;
                        continue;
                    }
                }
                info.mask |= rtlm_mask;
            }
        }

        if self.target_direction.is_vertical() && !self.plan.has_vert {
            for info in &mut self.buffer.info[..len] {
                if let Some(c) = info.as_codepoint().vertical() {
                    if self.font.nominal_glyph(c).is_some() {
                        info.glyph_id = c;
                    }
                }
            }
        }
    }
}

fn form_clusters(buffer: &mut Buffer) {
    if buffer.scratch_flags & HB_BUFFER_SCRATCH_FLAG_HAS_CONTINUATIONS != 0 {
        foreach_grapheme!(buffer, start, end, {
            buffer.merge_grapheme_clusters(start, end);
        });
    }
}

fn ensure_native_direction(buffer: &mut Buffer) {
    let dir = buffer.direction;
    let mut hor = buffer
        .script
        .and_then(Direction::from_script)
        .unwrap_or_default();

    // Numeric runs in natively-RTL scripts are actually native-LTR, so we reset
    // the horiz_dir if the run contains at least one decimal-number char, and no
    // letter chars (ideally we should be checking for chars with strong
    // directionality but hb-unicode currently lacks bidi categories).
    //
    // This allows digit sequences in Arabic etc to be shaped in "native"
    // direction, so that features like ligatures will work as intended.
    //
    // https://github.com/harfbuzz/harfbuzz/issues/501
    //
    // Similar thing about Regional_Indicators; They are bidi=L, but Script=Common.
    // If they are present in a run of natively-RTL text, they get assigned a script
    // with natively RTL direction, which would result in wrong shaping if we
    // assign such native RTL direction to them then. Detect that as well.
    //
    // https://github.com/harfbuzz/harfbuzz/issues/3314

    if hor == Direction::RightToLeft && dir == Direction::LeftToRight {
        let mut found_number = false;
        let mut found_letter = false;
        let mut found_ri = false;
        for info in &buffer.info {
            let gc = info.general_category();
            if gc == GeneralCategory::DECIMAL_NUMBER {
                found_number = true;
            } else if gc.is_letter() {
                found_letter = true;
                break;
            } else if matches!(info.glyph_id, 0x1F1E6..=0x1F1FF) {
                found_ri = true;
            }
        }
        if (found_number || found_ri) && !found_letter {
            hor = Direction::LeftToRight;
        }
    }

    // TODO vertical:
    // The only BTT vertical script is Ogham, but it's not clear to me whether OpenType
    // Ogham fonts are supposed to be implemented BTT or not.  Need to research that
    // first.
    if (dir.is_horizontal() && dir != hor && hor != Direction::Invalid)
        || (dir.is_vertical() && dir != Direction::TopToBottom)
    {
        reverse_graphemes(buffer);
        buffer.direction = buffer.direction.reverse();
    }
}

fn map_glyphs_fast(buffer: &mut Buffer) {
    // Normalization process sets up normalizer_glyph_index(), we just copy it.
    let len = buffer.len;
    for info in &mut buffer.info[..len] {
        info.glyph_id = info.normalizer_glyph_index();
    }

    for info in &mut buffer.out_info_mut()[..len] {
        info.glyph_id = info.normalizer_glyph_index();
    }
}

fn synthesize_glyph_classes(buffer: &mut Buffer) {
    let len = buffer.len;
    for info in &mut buffer.info[..len] {
        // Never mark default-ignorables as marks.
        // They won't get in the way of lookups anyway,
        // but having them as mark will cause them to be skipped
        // over if the lookup-flag says so, but at least for the
        // Mongolian variation selectors, looks like Uniscribe
        // marks them as non-mark.  Some Mongolian fonts without
        // GDEF rely on this.  Another notable character that
        // this applies to is COMBINING GRAPHEME JOINER.
        let class = if info.general_category() != GeneralCategory::NON_SPACING_MARK
            || info.is_default_ignorable()
        {
            GlyphPropsFlags::BASE_GLYPH
        } else {
            GlyphPropsFlags::MARK
        };

        info.set_glyph_props(class.bits());
    }
}

fn zero_width_default_ignorables(buffer: &mut Buffer) {
    if buffer.scratch_flags & HB_BUFFER_SCRATCH_FLAG_HAS_DEFAULT_IGNORABLES != 0
        && !buffer
            .flags
            .contains(BufferFlags::PRESERVE_DEFAULT_IGNORABLES)
        && !buffer
            .flags
            .contains(BufferFlags::REMOVE_DEFAULT_IGNORABLES)
    {
        let len = buffer.len;
        for (info, pos) in buffer.info[..len].iter().zip(&mut buffer.pos[..len]) {
            if info.is_default_ignorable() {
                pos.x_advance = 0;
                pos.y_advance = 0;
                if buffer.direction.is_horizontal() {
                    pos.x_offset = 0;
                } else {
                    pos.y_offset = 0;
                }
            }
        }
    }
}

fn deal_with_variation_selectors(buffer: &mut Buffer) {
    if buffer.scratch_flags & HB_BUFFER_SCRATCH_FLAG_HAS_VARIATION_SELECTOR_FALLBACK == 0 {
        return;
    }

    // Note: In harfbuzz, this is part of the condition above (with OR), so it needs to stay
    // in sync.
    let Some(nf) = buffer.not_found_variation_selector else {
        return;
    };

    let count = buffer.len;
    let info = &mut buffer.info;
    let pos = &mut buffer.pos;

    for i in 0..count {
        if info[i].is_variation_selector() {
            info[i].glyph_id = nf;
            pos[i].x_advance = 0;
            pos[i].y_advance = 0;
            pos[i].x_offset = 0;
            pos[i].y_offset = 0;
            info[0].set_variation_selector(false);
        }
    }
}

fn zero_mark_widths_by_gdef(buffer: &mut Buffer, adjust_offsets: bool) {
    let len = buffer.len;
    for (info, pos) in buffer.info[..len].iter().zip(&mut buffer.pos[..len]) {
        if info.is_mark() {
            if adjust_offsets {
                pos.x_offset = pos.x_offset.saturating_sub(pos.x_advance);
                pos.y_offset = pos.y_offset.saturating_sub(pos.y_advance);
            }

            pos.x_advance = 0;
            pos.y_advance = 0;
        }
    }
}

fn hide_default_ignorables(buffer: &mut Buffer, font: &ShaperFont) {
    if buffer.scratch_flags & HB_BUFFER_SCRATCH_FLAG_HAS_DEFAULT_IGNORABLES != 0
        && !buffer
            .flags
            .contains(BufferFlags::PRESERVE_DEFAULT_IGNORABLES)
    {
        if !buffer
            .flags
            .contains(BufferFlags::REMOVE_DEFAULT_IGNORABLES)
        {
            if let Some(invisible) = buffer
                .invisible
                .or_else(|| font.nominal_glyph(u32::from(' ')))
            {
                let len = buffer.len;
                for info in &mut buffer.info[..len] {
                    if info.is_default_ignorable() {
                        info.glyph_id = invisible.to_u32();
                    }
                }
                return;
            }
        }

        buffer.delete_glyphs_inplace(GlyphInfo::is_default_ignorable);
    }
}

fn propagate_flags(buffer: &mut Buffer) {
    // Propagate cluster-level glyph flags to be the same on all cluster glyphs.
    // Simplifies using them.

    let mut and_mask = GlyphFlags::DEFINED_BITS;
    if !buffer.flags.contains(BufferFlags::PRODUCE_UNSAFE_TO_CONCAT) {
        and_mask &= !GlyphFlags::UNSAFE_TO_CONCAT.0;
    }

    if !buffer
        .flags
        .contains(BufferFlags::PRODUCE_SAFE_TO_INSERT_TATWEEL)
    {
        foreach_cluster!(buffer, start, end, {
            if end - start == 1 {
                buffer.info[start].mask &= and_mask;
            } else {
                let mut mask = 0;
                for info in &buffer.info[start..end] {
                    mask |= info.mask;
                }

                mask &= and_mask;

                for info in &mut buffer.info[start..end] {
                    info.mask = mask;
                }
            }
        });
        return;
    }

    /* If we are producing SAFE_TO_INSERT_TATWEEL, then do two things:
     *
     * - If the places that the Arabic shaper marked as SAFE_TO_INSERT_TATWEEL,
     *   are UNSAFE_TO_BREAK, then clear the SAFE_TO_INSERT_TATWEEL,
     * - Any place that is SAFE_TO_INSERT_TATWEEL, is also now UNSAFE_TO_BREAK.
     *
     * We couldn't make this interaction earlier. It has to be done here.
     */
    foreach_cluster!(buffer, start, end, {
        // We cannot use `continue` in our `for_each_cluster!` macro.
        if end - start != 1 {
            let mut mask = 0;
            for info in &buffer.info[start..end] {
                mask |= info.mask;
            }

            mask &= GlyphFlags::DEFINED_BITS;

            if mask & GlyphFlags::UNSAFE_TO_BREAK.0 != 0 {
                mask &= !GlyphFlags::SAFE_TO_INSERT_TATWEEL.0;
            }

            if mask & GlyphFlags::SAFE_TO_INSERT_TATWEEL.0 != 0 {
                mask |= GlyphFlags::UNSAFE_TO_BREAK.0 | GlyphFlags::UNSAFE_TO_CONCAT.0;
            }

            mask &= and_mask;

            for info in &mut buffer.info[start..end] {
                info.mask = mask;
            }
        }
    });
}
