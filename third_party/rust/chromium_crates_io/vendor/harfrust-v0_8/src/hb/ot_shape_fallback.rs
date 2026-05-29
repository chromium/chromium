use read_fonts::types::GlyphId;

use super::buffer::{hb_buffer_t, GlyphPosition};
use super::face::GlyphExtents;
use super::font_funcs::FontFuncsDispatch;
use super::ot_shape_plan::hb_ot_shape_plan_t;
use super::unicode::*;
use super::{hb_font_t, Direction};

struct FallbackShapeContext<'a, 'x, 'u> {
    plan: &'a hb_ot_shape_plan_t,
    face: &'a hb_font_t<'a>,
    buffer: &'x mut hb_buffer_t,
    adjust_offsets_when_zeroing: bool,
    font_funcs: &'x mut FontFuncsDispatch<'a, 'u>,
}

fn recategorize_combining_class(u: u32, mut class: u8) -> u8 {
    use modified_combining_class as mcc;

    if class >= 200 {
        return class;
    }

    // Thai / Lao need some per-character work.
    if u & !0xFF == 0x0E00 {
        if class == 0 {
            match u {
                0x0E31 | 0x0E34 | 0x0E35 | 0x0E36 | 0x0E37 | 0x0E47 | 0x0E4C | 0x0E4D | 0x0E4E => {
                    class = combining_class::AboveRight;
                }

                0x0EB1 | 0x0EB4 | 0x0EB5 | 0x0EB6 | 0x0EB7 | 0x0EBB | 0x0ECC | 0x0ECD => {
                    class = combining_class::Above;
                }

                0x0EBC => class = combining_class::Below,

                _ => {}
            }
        } else {
            // Thai virama is below-right
            if u == 0x0E3A {
                class = combining_class::BelowRight;
            }
        }
    }

    match class {
        // Hebrew
        mcc::CCC10 => combining_class::Below,         // sheva
        mcc::CCC11 => combining_class::Below,         // hataf segol
        mcc::CCC12 => combining_class::Below,         // hataf patah
        mcc::CCC13 => combining_class::Below,         // hataf qamats
        mcc::CCC14 => combining_class::Below,         // hiriq
        mcc::CCC15 => combining_class::Below,         // tsere
        mcc::CCC16 => combining_class::Below,         // segol
        mcc::CCC17 => combining_class::Below,         // patah
        mcc::CCC18 => combining_class::Below,         // qamats & qamats qatan
        mcc::CCC20 => combining_class::Below,         // qubuts
        mcc::CCC22 => combining_class::Below,         // meteg
        mcc::CCC23 => combining_class::AttachedAbove, // rafe
        mcc::CCC24 => combining_class::AboveRight,    // shin dot
        mcc::CCC25 => combining_class::AboveLeft,     // sin dot
        mcc::CCC19 => combining_class::AboveLeft,     // holam & holam haser for vav
        mcc::CCC26 => combining_class::Above,         // point varika
        mcc::CCC21 => class,                          // dagesh

        // Arabic and Syriac
        mcc::CCC27 => combining_class::Above, // fathatan
        mcc::CCC28 => combining_class::Above, // dammatan
        mcc::CCC30 => combining_class::Above, // fatha
        mcc::CCC31 => combining_class::Above, // damma
        mcc::CCC33 => combining_class::Above, // shadda
        mcc::CCC34 => combining_class::Above, // sukun
        mcc::CCC35 => combining_class::Above, // superscript alef
        mcc::CCC36 => combining_class::Above, // superscript alaph
        mcc::CCC29 => combining_class::Below, // kasratan
        mcc::CCC32 => combining_class::Below, // kasra

        // Thai
        mcc::CCC103 => combining_class::BelowRight, // sara u / sara uu
        mcc::CCC107 => combining_class::AboveRight, // mai

        // Lao
        mcc::CCC118 => combining_class::Below, // sign u / sign uu
        mcc::CCC122 => combining_class::Above, // mai

        // Tibetian
        mcc::CCC129 => combining_class::Below, // sign aa
        mcc::CCC130 => combining_class::Above, // sign i
        mcc::CCC132 => combining_class::Below, // sign u

        _ => class,
    }
}

pub fn _hb_ot_shape_fallback_mark_position_recategorize_marks(
    _: &hb_ot_shape_plan_t,
    _: &hb_font_t,
    buffer: &mut hb_buffer_t,
) {
    let len = buffer.len;
    for info in &mut buffer.info[..len] {
        if info.general_category() == GeneralCategory::NON_SPACING_MARK {
            let mut class = info.modified_combining_class();
            class = recategorize_combining_class(info.glyph_id, class);
            info.set_modified_combining_class(class);
        }
    }
}

fn zero_mark_advances(
    buffer: &mut hb_buffer_t,
    start: usize,
    end: usize,
    adjust_offsets_when_zeroing: bool,
) {
    for (info, pos) in buffer.info[start..end]
        .iter()
        .zip(&mut buffer.pos[start..end])
    {
        if info.general_category() == GeneralCategory::NON_SPACING_MARK {
            if adjust_offsets_when_zeroing {
                pos.x_offset -= pos.x_advance;
                pos.y_offset -= pos.y_advance;
            }
            pos.x_advance = 0;
            pos.y_advance = 0;
        }
    }
}

fn position_mark(
    face: &hb_font_t,
    direction: Direction,
    font_funcs: &mut FontFuncsDispatch<'_, '_>,
    glyph: GlyphId,
    pos: &mut GlyphPosition,
    base_extents: &mut GlyphExtents,
    combining_class: u8,
) {
    let Some(mark_extents) = font_funcs.extents(glyph) else {
        return;
    };

    let y_gap = font_funcs.scale().scale_y(face.units_per_em as i32 / 16);
    pos.x_offset = 0;
    pos.y_offset = 0;

    // We don't position LEFT and RIGHT marks.

    // X positioning
    match combining_class {
        combining_class::DoubleBelow | combining_class::DoubleAbove
            if direction.is_horizontal() =>
        {
            pos.x_offset += base_extents.x_bearing
                + if direction.is_forward() {
                    base_extents.width
                } else {
                    0
                }
                - mark_extents.width / 2
                - mark_extents.x_bearing;
        }

        combining_class::AttachedBelowLeft
        | combining_class::BelowLeft
        | combining_class::AboveLeft => {
            // Left align.
            pos.x_offset += base_extents.x_bearing - mark_extents.x_bearing;
        }

        combining_class::AttachedAboveRight
        | combining_class::BelowRight
        | combining_class::AboveRight => {
            // Right align.
            pos.x_offset += base_extents.x_bearing + base_extents.width
                - mark_extents.width
                - mark_extents.x_bearing;
        }

        combining_class::AttachedBelow
        | combining_class::AttachedAbove
        | combining_class::Below
        | combining_class::Above
        | _ => {
            // Center align.
            pos.x_offset += base_extents.x_bearing + (base_extents.width - mark_extents.width) / 2
                - mark_extents.x_bearing;
        }
    }

    let is_attached = matches!(
        combining_class,
        combining_class::AttachedBelowLeft
            | combining_class::AttachedBelow
            | combining_class::AttachedAbove
            | combining_class::AttachedAboveRight
    );

    // Y positioning.
    match combining_class {
        combining_class::DoubleBelow
        | combining_class::BelowLeft
        | combining_class::Below
        | combining_class::BelowRight
        | combining_class::AttachedBelowLeft
        | combining_class::AttachedBelow => {
            if !is_attached {
                // Add gap.
                base_extents.height -= y_gap;
            }

            pos.y_offset = base_extents.y_bearing + base_extents.height - mark_extents.y_bearing;

            // Never shift up "below" marks.
            if (y_gap > 0) == (pos.y_offset > 0) {
                base_extents.height -= pos.y_offset;
                pos.y_offset = 0;
            }

            base_extents.height += mark_extents.height;
        }

        combining_class::DoubleAbove
        | combining_class::AboveLeft
        | combining_class::Above
        | combining_class::AboveRight
        | combining_class::AttachedAbove
        | combining_class::AttachedAboveRight => {
            if !is_attached {
                // Add gap.
                base_extents.y_bearing += y_gap;
                base_extents.height -= y_gap;
            }

            pos.y_offset = base_extents.y_bearing - (mark_extents.y_bearing + mark_extents.height);

            // Don't shift down "above" marks too much.
            if (y_gap > 0) != (pos.y_offset > 0) {
                let correction = -pos.y_offset / 2;
                base_extents.y_bearing += correction;
                base_extents.height -= correction;
                pos.y_offset += correction;
            }

            base_extents.y_bearing -= mark_extents.height;
            base_extents.height += mark_extents.height;
        }

        _ => {}
    }
}

fn position_around_base(ctx: &mut FallbackShapeContext, base: usize, end: usize) {
    let mut horizontal_dir = Direction::Invalid;
    let face = ctx.face;
    ctx.buffer.unsafe_to_break(Some(base), Some(end));

    let base_info = ctx.buffer.info[base];
    let base_pos = ctx.buffer.pos[base];
    let base_glyph = base_info.as_glyph();

    let Some(mut base_extents) = ctx.font_funcs.extents(base_glyph) else {
        zero_mark_advances(ctx.buffer, base + 1, end, ctx.adjust_offsets_when_zeroing);
        return;
    };

    base_extents.y_bearing += base_pos.y_offset;
    base_extents.x_bearing = 0;

    // Use horizontal advance for horizontal positioning.
    // Generally a better idea. Also works for zero-ink glyphs. See:
    // https://github.com/harfbuzz/harfbuzz/issues/1532
    base_extents.width = ctx.font_funcs.advance_width(base_glyph);

    let lig_id = base_info.lig_id() as u32;
    let num_lig_components = base_info.lig_num_comps() as i32;

    let mut x_offset = 0;
    let mut y_offset = 0;
    if ctx.buffer.direction.is_forward() {
        x_offset -= base_pos.x_advance;
        y_offset -= base_pos.y_advance;
    }

    let mut last_lig_component: i32 = -1;
    let mut last_combining_class: u8 = 255;
    let mut component_extents = base_extents;
    let mut cluster_extents = base_extents;
    let direction = ctx.buffer.direction;
    let font_funcs = &mut *ctx.font_funcs;

    for (info, pos) in ctx.buffer.info[base + 1..end]
        .iter()
        .zip(&mut ctx.buffer.pos[base + 1..end])
    {
        if info.modified_combining_class() != 0 {
            if num_lig_components > 1 {
                let this_lig_id = info.lig_id() as u32;
                let mut this_lig_component = info.lig_comp() as i32 - 1;

                // Conditions for attaching to the last component.
                if lig_id == 0 || lig_id != this_lig_id || this_lig_component >= num_lig_components
                {
                    this_lig_component = num_lig_components - 1;
                }

                if last_lig_component != this_lig_component {
                    last_lig_component = this_lig_component;
                    last_combining_class = 255;
                    component_extents = base_extents;

                    if horizontal_dir == Direction::Invalid {
                        horizontal_dir = if ctx.plan.direction.is_horizontal() {
                            ctx.plan.direction
                        } else {
                            ctx.plan
                                .script
                                .and_then(Direction::from_script)
                                .unwrap_or(Direction::LeftToRight)
                        };
                    }

                    component_extents.x_bearing += (if horizontal_dir == Direction::LeftToRight {
                        this_lig_component
                    } else {
                        num_lig_components - 1 - this_lig_component
                    } * component_extents.width)
                        / num_lig_components;

                    component_extents.width /= num_lig_components;
                }
            }

            let this_combining_class = info.modified_combining_class();
            if last_combining_class != this_combining_class {
                last_combining_class = this_combining_class;
                cluster_extents = component_extents;
            }

            position_mark(
                face,
                direction,
                font_funcs,
                info.as_glyph(),
                pos,
                &mut cluster_extents,
                this_combining_class,
            );

            pos.x_advance = 0;
            pos.y_advance = 0;
            pos.x_offset += x_offset;
            pos.y_offset += y_offset;
        } else {
            if direction.is_forward() {
                x_offset -= pos.x_advance;
                y_offset -= pos.y_advance;
            } else {
                x_offset += pos.x_advance;
                y_offset += pos.y_advance;
            }
        }
    }
}

fn position_cluster_impl(ctx: &mut FallbackShapeContext, start: usize, end: usize) {
    // Find the base glyph
    let mut i = start;
    while i < end {
        if !ctx.buffer.info[i].is_unicode_mark() {
            // Find mark glyphs
            let mut j = i + 1;
            while j < end
                && (ctx.buffer.info[j].is_hidden()
                    || ctx.buffer.info[j].is_default_ignorable()
                    || ctx.buffer.info[j].is_unicode_mark())
            {
                j += 1;
            }

            position_around_base(ctx, i, j);
            i = j - 1;
        }
        i += 1;
    }
}

#[inline(always)]
fn position_cluster(ctx: &mut FallbackShapeContext, start: usize, end: usize) {
    if end - start < 2 {
        return;
    }

    position_cluster_impl(ctx, start, end);
}

pub fn position_marks<'a, 'x>(
    plan: &'a hb_ot_shape_plan_t,
    face: &'a hb_font_t<'a>,
    buffer: &'x mut hb_buffer_t,
    adjust_offsets_when_zeroing: bool,
    font_funcs: &'x mut FontFuncsDispatch<'a, '_>,
) {
    let mut ctx = FallbackShapeContext {
        plan,
        face,
        buffer,
        adjust_offsets_when_zeroing,
        font_funcs,
    };

    ctx.buffer.assert_gsubgpos_vars();

    let mut start = 0;
    let len = ctx.buffer.len;
    for i in 1..len {
        if !ctx.buffer.info[i].is_unicode_mark()
            && !ctx.buffer.info[i].is_hidden()
            && !ctx.buffer.info[i].is_default_ignorable()
        {
            position_cluster(&mut ctx, start, i);
            start = i;
        }
    }

    position_cluster(&mut ctx, start, len);
}

pub fn _hb_ot_shape_fallback_kern(_: &hb_ot_shape_plan_t, _: &hb_font_t, _: &mut hb_buffer_t) {
    // STUB: this is deprecated in HarfBuzz
}

pub fn _hb_ot_shape_fallback_spaces<'a, 'x>(
    plan: &'a hb_ot_shape_plan_t,
    face: &'a hb_font_t<'a>,
    buffer: &'x mut hb_buffer_t,
    font_funcs: &'x mut FontFuncsDispatch<'a, '_>,
) {
    use super::unicode::hb_unicode_funcs_t as t;

    let _ = plan;
    let len = buffer.len;
    let horizontal = buffer.direction.is_horizontal();
    let scale = *font_funcs.scale();
    for (info, pos) in buffer.info[..len].iter().zip(&mut buffer.pos[..len]) {
        if info.is_unicode_space() && !info.ligated() {
            let space_type = info.unicode_space_fallback_type();
            match space_type {
                t::SPACE_EM
                | t::SPACE_EM_2
                | t::SPACE_EM_3
                | t::SPACE_EM_4
                | t::SPACE_EM_5
                | t::SPACE_EM_6
                | t::SPACE_EM_16 => {
                    let length =
                        (face.units_per_em as i32 + (space_type as i32) / 2) / space_type as i32;
                    if horizontal {
                        pos.x_advance = scale.scale_x(length);
                    } else {
                        pos.y_advance = -scale.scale_y(length);
                    }
                }

                t::SPACE_4_EM_18 => {
                    let length = ((face.units_per_em as i64) * 4 / 18) as i32;
                    if horizontal {
                        pos.x_advance = scale.scale_x(length);
                    } else {
                        pos.y_advance = -scale.scale_y(length);
                    }
                }

                t::SPACE_FIGURE => {
                    for u in '0'..='9' {
                        if let Some(glyph) = font_funcs.nominal_glyph(u as u32) {
                            if horizontal {
                                pos.x_advance = font_funcs.advance_width(glyph);
                            } else {
                                pos.y_advance = font_funcs.advance_height(glyph);
                            }
                            break;
                        }
                    }
                }

                t::SPACE_PUNCTUATION => {
                    let punct = font_funcs
                        .nominal_glyph('.' as u32)
                        .or_else(|| font_funcs.nominal_glyph(',' as u32));

                    if let Some(glyph) = punct {
                        if horizontal {
                            pos.x_advance = font_funcs.advance_width(glyph);
                        } else {
                            pos.y_advance = font_funcs.advance_height(glyph);
                        }
                    }
                }

                t::SPACE_NARROW => {
                    // Half-space?
                    // Unicode doc https://unicode.org/charts/PDF/U2000.pdf says ~1/4 or 1/5 of EM.
                    // However, in my testing, many fonts have their regular space being about that
                    // size. To me, a percentage of the space width makes more sense. Half is as
                    // good as any.
                    if horizontal {
                        pos.x_advance /= 2;
                    } else {
                        pos.y_advance /= 2;
                    }
                }

                _ => {}
            }
        }
    }
}
