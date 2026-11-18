//! OpenType GPOS lookups.

use crate::buffer::*;
use crate::clamp_i64_to_i32;
use crate::hint::Likely;
use crate::ot::layout::*;
use crate::plan::ShapePlan;
use crate::Direction;
use crate::ShaperFont;
use alloc::vec::Vec;

pub fn position(plan: &ShapePlan, font: &ShaperFont<'_, '_>, buffer: &mut Buffer) {
    #[cfg(feature = "tracing")]
    let tag = crate::ot::map::MessageTag(plan.ot_map.chosen_script(LayoutTableKind::Gpos));
    if !buffer_message!(buffer, font, "start table GPOS script tag '{tag:#}'") {
        return;
    }
    let table = font.layout().ot.gpos();
    apply_layout_table(plan, font, buffer, table.as_ref());
    buffer_message!(buffer, font, "end table GPOS script tag '{tag:#}'");
}

pub mod attach_type {
    pub const MARK: u8 = 1;
    pub const CURSIVE: u8 = 2;
}

/// Running totals of the advances in `pos`: entry `k` is the sum over `pos[..k]`.
///
/// A mark can sit up to `i16::MAX` glyphs from its base, so summing the
/// advances between them once per mark is quadratic in a run of marks.
/// Propagation never changes advances, so the totals stay valid throughout.
/// Cursive attachments do not use the totals and leave this vector empty.
///
/// Corrections use the exact i64 advance sum and saturate once. At extreme
/// offsets with mixed-sign advances, this can differ from HarfBuzz's repeated
/// saturating additions/subtractions, which lose excess at each step.
fn advance_sums(pos: &[GlyphPosition]) -> Vec<(i64, i64)> {
    if !pos
        .iter()
        .any(|p| p.attach_chain() != 0 && p.attach_type() == attach_type::MARK)
    {
        return Vec::new();
    }
    let mut sums = Vec::with_capacity(pos.len() + 1);
    let (mut x, mut y) = (0i64, 0i64);
    sums.push((x, y));
    for p in pos {
        x += i64::from(p.x_advance);
        y += i64::from(p.y_advance);
        sums.push((x, y));
    }
    sums
}

/// See <https://github.com/harfbuzz/harfbuzz/blob/7d936359a27abb2d7cb14ecc102463bb15c11843/src/OT/Layout/GPOS/GPOS.hh#L75>
fn propagate_attachment_offsets(
    pos: &mut [GlyphPosition],
    advance_sums: &[(i64, i64)],
    len: usize,
    i: usize,
    direction: Direction,
    nesting_level: usize,
) -> Option<()> {
    // Adjusts offsets of attached glyphs (both cursive and mark) to accumulate
    // offset of glyph they are attached to.
    let this = pos.get_mut(i).likely()?;
    let chain = this.attach_chain();
    let kind = this.attach_type();

    this.set_attach_chain(0);

    let j = (i as isize + isize::from(chain)) as usize;
    if j >= len {
        return Some(());
    }

    if nesting_level == 0 {
        return Some(());
    }

    if pos.get(j).likely()?.attach_chain() != 0 {
        // A failure further up the chain still accumulates this glyph below.
        propagate_attachment_offsets(pos, advance_sums, len, j, direction, nesting_level - 1);
    }

    // Read only after the recursion, which updates the parent's offsets.
    let parent = *pos.get(j).likely()?;
    let mut this = *pos.get(i).likely()?;
    match kind {
        attach_type::MARK => {
            if direction.is_horizontal() {
                this.x_offset = this.x_offset.saturating_add(parent.x_offset);
            } else {
                this.y_offset = this.y_offset.saturating_add(parent.y_offset);
            }

            // i is the position of the mark; j is the base.
            // Undo the advances between them, as a difference of two
            // running totals so that the cost does not grow with the distance.
            let (start, end, sign) = if j < i {
                // This is the common case: mark follows base.
                // And currently the only way in OpenType.
                if direction.is_forward() {
                    (j, i, -1)
                } else {
                    (j + 1, i + 1, 1)
                }
            } else {
                // This can happen with `kerx`: a mark attaching
                // to a base after it in the logical order.
                if direction.is_forward() {
                    (i, j, 1)
                } else {
                    (i + 1, j + 1, -1)
                }
            };
            let (start_x, start_y) = *advance_sums.get(start).likely()?;
            let (end_x, end_y) = *advance_sums.get(end).likely()?;
            this.x_offset = clamp_i64_to_i32(i64::from(this.x_offset) + sign * (end_x - start_x));
            this.y_offset = clamp_i64_to_i32(i64::from(this.y_offset) + sign * (end_y - start_y));
        }
        attach_type::CURSIVE => {
            if direction.is_horizontal() {
                this.y_offset = this.y_offset.saturating_add(parent.y_offset);
            } else {
                this.x_offset = this.x_offset.saturating_add(parent.x_offset);
            }
        }
        _ => {}
    }
    *pos.get_mut(i).likely()? = this;
    Some(())
}

pub fn position_start(buffer: &mut Buffer) {
    let len = buffer.len;
    for pos in buffer.pos.get_mut(..len).unwrap_or_default() {
        pos.set_attach_chain(0);
        pos.set_attach_type(0);
    }
}

pub fn position_finish_advances(_: &mut Buffer) {
    //buffer.assert_gsubgpos_vars();
}

pub fn position_finish_offsets(buffer: &mut Buffer) {
    buffer.assert_gsubgpos_vars();

    let len = buffer.len;
    let direction = buffer.direction;

    // Handle attachments
    if buffer.scratch_flags & HB_BUFFER_SCRATCH_FLAG_HAS_GPOS_ATTACHMENT != 0 {
        let pos = &mut buffer.pos;
        let sums = advance_sums(pos.get(..len).unwrap_or_default());
        // https://github.com/harfbuzz/harfbuzz/issues/5514
        if direction.is_forward() {
            for i in 0..len {
                if pos.get(i).is_some_and(|p| p.attach_chain() != 0) {
                    propagate_attachment_offsets(pos, &sums, len, i, direction, MAX_NESTING_LEVEL);
                }
            }
        } else {
            for i in (0..len).rev() {
                if pos.get(i).is_some_and(|p| p.attach_chain() != 0) {
                    propagate_attachment_offsets(pos, &sums, len, i, direction, MAX_NESTING_LEVEL);
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cursive_only_attachments_do_not_allocate_advance_sums() {
        let mut pos = [GlyphPosition::default(); 2];
        pos[0].y_offset = 20;
        pos[1].y_offset = 10;
        pos[1].set_attach_type(attach_type::CURSIVE);
        pos[1].set_attach_chain(-1);
        let sums = advance_sums(&pos);
        assert_eq!(sums.capacity(), 0);
        propagate_attachment_offsets(
            &mut pos,
            &sums,
            2,
            1,
            Direction::LeftToRight,
            MAX_NESTING_LEVEL,
        );
        assert_eq!(pos[1].y_offset, 30);
    }

    #[test]
    fn mark_advance_correction_saturates_the_exact_mixed_sign_sum() {
        let mut pos = [GlyphPosition::default(); 3];
        pos[0].x_advance = 1;
        pos[1].x_advance = -1;
        pos[2].x_offset = i32::MIN;
        pos[2].set_attach_type(attach_type::MARK);
        pos[2].set_attach_chain(-2);
        let sums = advance_sums(&pos);
        propagate_attachment_offsets(
            &mut pos,
            &sums,
            3,
            2,
            Direction::LeftToRight,
            MAX_NESTING_LEVEL,
        );
        // The exact net advance is zero. Per-step saturation would instead
        // lose the first subtraction's underflow and produce MIN + 1.
        assert_eq!(pos[2].x_offset, i32::MIN);
    }

    #[test]
    fn propagate_attachment_offsets_saturates_deep_mark_chains() {
        let len = MAX_NESTING_LEVEL + 2;
        let mut pos = vec![GlyphPosition::default(); len];

        for i in 1..len {
            pos[i].x_offset = 40_000_000;
            pos[i].y_offset = 40_000_000;
            pos[i].set_attach_type(attach_type::MARK);
            pos[i].set_attach_chain(-1);
        }

        let sums = advance_sums(&pos);
        propagate_attachment_offsets(
            &mut pos,
            &sums,
            len,
            len - 1,
            Direction::LeftToRight,
            MAX_NESTING_LEVEL,
        );

        assert_eq!(pos[len - 1].x_offset, i32::MAX);
        assert_eq!(pos[len - 1].y_offset, 40_000_000);
    }

    // One base followed by three marks, or preceded by them as `kerx` can
    // produce, with distinct advances so that each range of the advance sums
    // is pinned.
    fn mark_offsets(direction: Direction, marks_first: bool) -> [i32; 4] {
        let mut pos = [GlyphPosition::default(); 4];
        for (i, p) in pos.iter_mut().enumerate() {
            p.x_advance = 100 * (i as i32 + 1);
        }
        let base = if marks_first { 3 } else { 0 };
        for (i, p) in pos.iter_mut().enumerate() {
            if i != base {
                p.set_attach_type(attach_type::MARK);
                p.set_attach_chain((base as isize - i as isize) as i16);
            }
        }

        let len = pos.len();
        let sums = advance_sums(&pos);
        for i in 0..len {
            if pos[i].attach_chain() != 0 {
                propagate_attachment_offsets(&mut pos, &sums, len, i, direction, MAX_NESTING_LEVEL);
            }
        }
        pos.map(|p| p.x_offset)
    }

    #[test]
    fn propagate_attachment_offsets_undoes_advances_between_mark_and_base() {
        assert_eq!(
            mark_offsets(Direction::LeftToRight, false),
            [0, -100, -300, -600]
        );
        assert_eq!(
            mark_offsets(Direction::RightToLeft, false),
            [0, 200, 500, 900]
        );
        assert_eq!(
            mark_offsets(Direction::LeftToRight, true),
            [600, 500, 300, 0]
        );
        assert_eq!(
            mark_offsets(Direction::RightToLeft, true),
            [-900, -700, -400, 0]
        );
    }
}
