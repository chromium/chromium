use crate::buffer::*;
use crate::ot::layout::MAX_SYLLABLE_LENGTH;
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
) -> Option<bool> {
    if buffer
        .flags
        .contains(BufferFlags::DO_NOT_INSERT_DOTTED_CIRCLE)
    {
        return Some(false);
    }

    if (buffer.scratch_flags & HB_BUFFER_SCRATCH_FLAG_HAS_BROKEN_SYLLABLE) == 0 {
        buffer_message!(
            buffer,
            font,
            "skipped inserting dotted-circles because there is no broken syllables",
        );
        return Some(false);
    }

    if !buffer_message!(buffer, font, "start inserting dotted-circles") {
        return Some(false);
    }

    let dottedcircle_glyph = match font.nominal_glyph(0x25CC) {
        Some(g) => g.to_u32(),
        None => return Some(false),
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
    // `next_glyph()` stops advancing once the buffer has failed, so the
    // loops below must bail out then, as the HarfBuzz originals do.
    while buffer.idx < buffer.len && buffer.successful {
        let syllable = buffer.cur(0)?.syllable();
        if last_syllable != syllable && (syllable & 0x0F) == broken_syllable_type {
            last_syllable = syllable;

            let mut ginfo = dottedcircle;
            ginfo.cluster = buffer.cur(0)?.cluster;
            ginfo.mask = buffer.cur(0)?.mask;
            ginfo.set_syllable(buffer.cur(0)?.syllable());

            // Insert dottedcircle after possible Repha.
            if let Some(repha_category) = repha_category {
                while buffer.idx < buffer.len
                    && buffer.successful
                    && last_syllable == buffer.cur(0)?.syllable()
                    && buffer.cur(0)?.ot_shaper_var_u8_category() == repha_category
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

    buffer_message!(buffer, font, "end inserting dotted-circles");

    Some(true)
}

// Call for each piece [start, end) yielded by `next_syllable()`. Only pieces
// that reach the limit can need their cut recorded for later passes.
#[inline]
pub(crate) fn record_syllable_cut(buffer: &mut Buffer, start: usize, end: usize) {
    if end - start >= MAX_SYLLABLE_LENGTH && record_syllable_cut_slow(buffer, start, end).is_none()
    {
        buffer.index_failed();
    }
}

// Record cuts in `syllable()`, which substituted glyphs inherit, so walks
// before and after substitutions see the same pieces. Give every piece but
// the last a serial different from its neighbours, leaving other syllables
// untouched. The pieces are not syllables on their own and text must not be
// broken between them, so keep the whole syllable in one cluster.
#[inline(never)]
fn record_syllable_cut_slow(buffer: &mut Buffer, mut start: usize, mut end: usize) -> Option<()> {
    let infos = buffer.info.get(..buffer.len)?;
    let syllable = infos.get(start)?.syllable();
    if end == buffer.len || infos.get(end)?.syllable() != syllable {
        return Some(()); // Not cut.
    }

    let syllable_end = end
        + infos
            .get(end..)?
            .iter()
            .take_while(|info| info.syllable() == syllable)
            .count();
    let prev_serial = if start != 0 {
        infos.get(start - 1)?.syllable() >> 4
    } else {
        0
    };
    let last_serial = syllable >> 4;

    buffer.merge_clusters(start, syllable_end);

    // Alternate two serials different from both the previous syllable and
    // the serial kept by the last piece.
    let mut serials = [0; 2];
    let mut serial = 1;
    for value in &mut serials {
        while serial == prev_serial || serial == last_serial {
            serial += 1;
        }
        *value = serial;
        serial += 1;
    }

    let mut j = 0;
    while syllable_end - start > MAX_SYLLABLE_LENGTH {
        end = start + MAX_SYLLABLE_LENGTH;
        let value = (serials[j] << 4) | (syllable & 0x0F);
        for info in buffer.info.get_mut(start..end)? {
            info.set_syllable(value);
        }
        start = end;
        j ^= 1;
    }

    Some(())
}

pub(crate) fn syllabic_clear_var(
    _: &ShapePlan,
    _: &ShaperFont<'_, '_>,
    buffer: &mut Buffer,
) -> Option<bool> {
    for info in buffer.info_slice_mut() {
        info.set_syllable(0);
    }
    buffer.deallocate_var(GlyphInfo::SYLLABLE_VAR);

    Some(false)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn buffer_with_syllables(runs: &[(u8, usize)]) -> Buffer {
        let mut buffer = Buffer::new();
        for &(syllable, len) in runs {
            for _ in 0..len {
                let mut info = GlyphInfo {
                    cluster: buffer.info.len() as u32,
                    ..GlyphInfo::default()
                };
                info.set_syllable(syllable);
                buffer.info.push(info);
            }
        }
        buffer.len = buffer.info.len();
        buffer
    }

    fn syllables_and_clusters(buffer: &Buffer) -> Vec<(u8, u32)> {
        buffer
            .info
            .iter()
            .map(|info| (info.syllable(), info.cluster))
            .collect()
    }

    #[test]
    fn uncut_syllables_keep_their_serials_and_clusters() {
        // Include serial wraparound, an exact-limit syllable followed by a
        // different one, and an exact-limit syllable at the live buffer end.
        let mut buffer = buffer_with_syllables(&[(0xF1, 63), (0x12, 64), (0x23, 64)]);
        // Retained entries must not turn the last syllable into a cut.
        buffer.info.extend_from_within(buffer.len - 8..);
        let before = syllables_and_clusters(&buffer);
        foreach_syllable!(buffer, start, end, {
            record_syllable_cut(&mut buffer, start, end);
        });
        assert!(buffer.successful);
        assert_eq!(syllables_and_clusters(&buffer), before);
    }

    #[test]
    fn cut_pieces_stay_distinct_from_each_other_and_their_neighbours() {
        for len in [65, 127, 128, 129, 192, 193, 302] {
            // Zero tests a cut at the start of the buffer. The other values
            // exercise every possible preceding and original serial.
            for prev_serial in 0..16 {
                for serial in 1..16 {
                    let start = usize::from(prev_serial != 0);
                    let syllable = (serial << 4) | 7;
                    let mut buffer = buffer_with_syllables(&[
                        ((prev_serial << 4) | 6, start),
                        (syllable, len),
                        (0x18, 1),
                    ]);
                    let before = syllables_and_clusters(&buffer);
                    let syllable_end = start + len;
                    record_syllable_cut(&mut buffer, start, start + MAX_SYLLABLE_LENGTH);

                    assert!(buffer.successful);
                    assert_eq!(syllables_and_clusters(&buffer)[..start], before[..start]);
                    assert_eq!(
                        syllables_and_clusters(&buffer)[syllable_end..],
                        before[syllable_end..]
                    );
                    assert_eq!(buffer.info[syllable_end - 1].syllable(), syllable);
                    assert!(buffer.info[start..syllable_end]
                        .iter()
                        .all(|info| info.cluster == start as u32 && info.syllable() & 0x0F == 7));

                    let mut piece_start = start;
                    let mut previous = prev_serial;
                    while piece_start < syllable_end {
                        let end = buffer.next_syllable(piece_start);
                        let serial = buffer.info[piece_start].syllable() >> 4;
                        assert_ne!(serial, previous);
                        assert!((1..16).contains(&serial));
                        assert!(end <= syllable_end && end - piece_start <= MAX_SYLLABLE_LENGTH);
                        previous = serial;
                        piece_start = end;
                    }

                    // All cuts were recorded by the first call. Visiting
                    // later pieces, or another full pass, changes nothing.
                    let recorded = syllables_and_clusters(&buffer);
                    foreach_syllable!(buffer, start, end, {
                        record_syllable_cut(&mut buffer, start, end);
                    });
                    assert_eq!(syllables_and_clusters(&buffer), recorded);
                }
            }
        }
    }

    #[test]
    fn substitutions_that_shorten_pieces_preserve_the_cuts() {
        let mut buffer = buffer_with_syllables(&[(0xF7, 129)]);
        record_syllable_cut(&mut buffer, 0, MAX_SYLLABLE_LENGTH);

        // Simulate substitutions replacing each piece with a single glyph
        // that inherits its syllable. Position-based cuts would now vanish.
        buffer.info = [0, 64, 128].map(|i| buffer.info[i]).to_vec();
        buffer.len = buffer.info.len();
        assert_eq!(buffer.next_syllable(0), 1);
        assert_eq!(buffer.next_syllable(1), 2);
        assert_eq!(buffer.next_syllable(2), 3);
        assert!(buffer.info.iter().all(|info| info.cluster == 0));
    }
}
