//! OpenType GPOS lookups.

use crate::buffer::*;
use crate::hint::Likely;
use crate::ot::layout::*;
use crate::plan::ShapePlan;
use crate::Direction;
use crate::ShaperFont;

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

/// See <https://github.com/harfbuzz/harfbuzz/blob/7d936359a27abb2d7cb14ecc102463bb15c11843/src/OT/Layout/GPOS/GPOS.hh#L75>
fn propagate_attachment_offsets(
    pos: &mut [GlyphPosition],
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
        propagate_attachment_offsets(pos, len, j, direction, nesting_level - 1);
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
            if j < i {
                // This is the common case: mark follows base.
                // And currently the only way in OpenType.
                if direction.is_forward() {
                    for k in pos.get(j..i).likely()? {
                        this.x_offset = this.x_offset.saturating_sub(k.x_advance);
                        this.y_offset = this.y_offset.saturating_sub(k.y_advance);
                    }
                } else {
                    for k in pos.get(j + 1..i + 1).likely()? {
                        this.x_offset = this.x_offset.saturating_add(k.x_advance);
                        this.y_offset = this.y_offset.saturating_add(k.y_advance);
                    }
                }
            } else {
                // This can happen with `kerx`: a mark attaching
                // to a base after it in the logical order.
                if direction.is_forward() {
                    for k in pos.get(i..j).likely()? {
                        this.x_offset = this.x_offset.saturating_add(k.x_advance);
                        this.y_offset = this.y_offset.saturating_add(k.y_advance);
                    }
                } else {
                    for k in pos.get(i + 1..j + 1).likely()? {
                        this.x_offset = this.x_offset.saturating_sub(k.x_advance);
                        this.y_offset = this.y_offset.saturating_sub(k.y_advance);
                    }
                }
            }
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
        // https://github.com/harfbuzz/harfbuzz/issues/5514
        if direction.is_forward() {
            for i in 0..len {
                if pos.get(i).is_some_and(|p| p.attach_chain() != 0) {
                    propagate_attachment_offsets(pos, len, i, direction, MAX_NESTING_LEVEL);
                }
            }
        } else {
            for i in (0..len).rev() {
                if pos.get(i).is_some_and(|p| p.attach_chain() != 0) {
                    propagate_attachment_offsets(pos, len, i, direction, MAX_NESTING_LEVEL);
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

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

        propagate_attachment_offsets(
            &mut pos,
            len,
            len - 1,
            Direction::LeftToRight,
            MAX_NESTING_LEVEL,
        );

        assert_eq!(pos[len - 1].x_offset, i32::MAX);
        assert_eq!(pos[len - 1].y_offset, 40_000_000);
    }
}
