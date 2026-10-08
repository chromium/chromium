use crate::buffer::*;
use crate::plan::ShapePlan;
use crate::BufferFlags;
use crate::GlyphInfo;
use crate::ShaperFont;

pub fn insert_dotted_circles(
    font: &ShaperFont<'_, '_>,
    buffer: &mut Buffer,
    broken_syllable_type: u8,
    dottedcircle_category: u8,
    repha_category: Option<u8>,
    dottedcircle_position: Option<u8>,
) -> bool {
    if buffer
        .flags
        .contains(BufferFlags::DO_NOT_INSERT_DOTTED_CIRCLE)
    {
        return false;
    }

    if (buffer.scratch_flags & HB_BUFFER_SCRATCH_FLAG_HAS_BROKEN_SYLLABLE) == 0 {
        return false;
    }

    let dottedcircle_glyph = match font.nominal_glyph(0x25CC) {
        Some(g) => g.to_u32(),
        None => return false,
    };

    let mut dottedcircle = GlyphInfo {
        glyph_id: 0x25CC,
        ..GlyphInfo::default()
    };
    dottedcircle.set_ot_shaper_var_u8_category(dottedcircle_category);
    if let Some(dottedcircle_position) = dottedcircle_position {
        dottedcircle.set_ot_shaper_var_u8_auxiliary(dottedcircle_position);
    }
    dottedcircle.glyph_id = dottedcircle_glyph;

    buffer.clear_output();

    buffer.idx = 0;
    let mut last_syllable = 0;
    while buffer.idx < buffer.len {
        let syllable = buffer.cur(0).syllable();
        if last_syllable != syllable && (syllable & 0x0F) == broken_syllable_type {
            last_syllable = syllable;

            let mut ginfo = dottedcircle;
            ginfo.cluster = buffer.cur(0).cluster;
            ginfo.mask = buffer.cur(0).mask;
            ginfo.set_syllable(buffer.cur(0).syllable());

            // Insert dottedcircle after possible Repha.
            if let Some(repha_category) = repha_category {
                while buffer.idx < buffer.len
                    && last_syllable == buffer.cur(0).syllable()
                    && buffer.cur(0).ot_shaper_var_u8_category() == repha_category
                {
                    buffer.next_glyph();
                }
            }

            buffer.output_info(ginfo);
        } else {
            buffer.next_glyph();
        }
    }

    buffer.sync();

    true
}

pub(crate) fn syllabic_clear_var(
    _: &ShapePlan,
    _: &ShaperFont<'_, '_>,
    buffer: &mut Buffer,
) -> bool {
    for info in &mut buffer.info {
        info.set_syllable(0);
    }
    buffer.deallocate_var(GlyphInfo::SYLLABLE_VAR);

    false
}
