//! Latin blue values.

use super::super::{
    super::{unscaled::UnscaledOutlineBuf, OutlineGlyphCollection},
    cycling::{cycle_backward, cycle_forward},
    metrics::{UnscaledBlue, UnscaledBlues, MAX_BLUES},
    shape::{ShapedCluster, Shaper},
    style::{blue_flags, ScriptGroup, StyleClass},
};
use crate::{FontRef, MetadataProvider};
use raw::types::F2Dot14;
use raw::TableProvider;

// Chosen to maximize opportunity to avoid heap allocation while keeping stack
// size < 2k.
const MAX_INLINE_POINTS: usize = 256;

// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afblue.h#L73>
const BLUE_STRING_MAX_LEN: usize = 51;

impl UnscaledBlue {
    fn is_latin_any_top(&self) -> bool {
        self.flags & (blue_flags::TOP | blue_flags::LATIN_SUB_TOP) != 0
    }
}

/// Compute unscaled blues values for each axis.
pub(crate) fn compute_unscaled_blues(
    shaper: &Shaper,
    coords: &[F2Dot14],
    style: &StyleClass,
) -> [UnscaledBlues; 2] {
    match style.script.group {
        ScriptGroup::Default => [
            // Default group doesn't have horizontal blues
            Default::default(),
            compute_default_blues(shaper, coords, style),
        ],
        ScriptGroup::Cjk => compute_cjk_blues(shaper, coords, style),
        // Indic group doesn't use blue values (yet?)
        ScriptGroup::Indic => Default::default(),
    }
}

/// Compute unscaled blue values for the default script set.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/aflatin.c#L314>
fn compute_default_blues(shaper: &Shaper, coords: &[F2Dot14], style: &StyleClass) -> UnscaledBlues {
    let mut blues = UnscaledBlues::new();
    let (mut outline_buf, mut flats, mut rounds) = buffers();
    let (glyphs, units_per_em) = things_all_blues_need(shaper.font());
    let flat_threshold = units_per_em / 14;
    let mut shaped_cluster = ShapedCluster::default();
    // Walk over each of the blue character sets for our script.
    for (blue_str, blue_flags) in style.script.blues {
        let is_top_like = (blue_flags & (blue_flags::TOP | blue_flags::LATIN_SUB_TOP)) != 0;
        let is_top = blue_flags & blue_flags::TOP != 0;
        let is_x_height = blue_flags & blue_flags::LATIN_X_HEIGHT != 0;
        let is_neutral = blue_flags & blue_flags::LATIN_NEUTRAL != 0;
        let is_long = blue_flags & blue_flags::LATIN_LONG != 0;
        let mut ascender = i32::MIN;
        let mut descender = i32::MAX;
        let mut n_flats = 0;
        let mut n_rounds = 0;
        for cluster in blue_str.split(' ') {
            let mut best_y_extremum = if is_top { i32::MIN } else { i32::MAX };
            let mut best_is_round = false;
            shaper.shape_cluster(cluster, &mut shaped_cluster);
            for (glyph, y_offset) in shaped_cluster
                .iter()
                .filter(|g| g.id.to_u32() != 0)
                .filter_map(|g| Some((glyphs.get(g.id)?, g.y_offset)))
            {
                outline_buf.clear();
                if glyph.draw_unscaled(coords, None, &mut outline_buf).is_err() {
                    continue;
                }
                let outline = outline_buf.as_ref();
                // Reject glyphs that can't produce any rendering
                if outline.points.len() <= 2 {
                    continue;
                }
                let mut best_y: Option<i16> = None;
                // Find the extreme point depending on whether this is a top or
                // bottom blue
                let best_contour_and_point = if is_top_like {
                    outline.find_last_contour(|point| {
                        if best_y.is_none() || Some(point.y) > best_y {
                            best_y = Some(point.y);
                            ascender = ascender.max(point.y as i32 + y_offset);
                            true
                        } else {
                            descender = descender.min(point.y as i32 + y_offset);
                            false
                        }
                    })
                } else {
                    outline.find_last_contour(|point| {
                        if best_y.is_none() || Some(point.y) < best_y {
                            best_y = Some(point.y);
                            descender = descender.min(point.y as i32 + y_offset);
                            true
                        } else {
                            ascender = ascender.max(point.y as i32 + y_offset);
                            false
                        }
                    })
                };
                let Some((best_contour_range, best_point_ix)) = best_contour_and_point else {
                    continue;
                };
                let best_contour = &outline.points[best_contour_range];
                // If we have a contour and point then best_y is guaranteed to
                // be Some
                let mut best_y = best_y.unwrap() as i32;
                let best_x = best_contour[best_point_ix].x as i32;
                // Now determine whether the point belongs to a straight or
                // round segment by examining the previous and next points.
                let [mut on_point_first, mut on_point_last] =
                    if best_contour[best_point_ix].is_on_curve() {
                        [Some(best_point_ix); 2]
                    } else {
                        [None; 2]
                    };
                let mut segment_first = best_point_ix;
                let mut segment_last = best_point_ix;
                // Look for the previous and next points on the contour that
                // are not on the same Y coordinate, then threshold the
                // "closeness"
                for (ix, prev) in cycle_backward(best_contour, best_point_ix) {
                    let dist = (prev.y as i32 - best_y).abs();
                    // Allow a small distance or angle (20 == roughly 2.9 degrees)
                    if dist > 5 && ((prev.x as i32 - best_x).abs() <= (20 * dist)) {
                        break;
                    }
                    segment_first = ix;
                    if prev.is_on_curve() {
                        on_point_first = Some(ix);
                        if on_point_last.is_none() {
                            on_point_last = Some(ix);
                        }
                    }
                }
                let mut next_ix = 0;
                for (ix, next) in cycle_forward(best_contour, best_point_ix) {
                    // Save next_ix which is used in "long" blue computation
                    // later
                    next_ix = ix;
                    let dist = (next.y as i32 - best_y).abs();
                    // Allow a small distance or angle (20 == roughly 2.9 degrees)
                    if dist > 5 && ((next.x as i32 - best_x).abs() <= (20 * dist)) {
                        break;
                    }
                    segment_last = ix;
                    if next.is_on_curve() {
                        on_point_last = Some(ix);
                        if on_point_first.is_none() {
                            on_point_first = Some(ix);
                        }
                    }
                }
                if is_long {
                    // Taken verbatim from FreeType:
                    //
                    // "If this flag is set, we have an additional constraint to
                    // get the blue zone distance: Find a segment of the topmost
                    // (or bottommost) contour that is longer than a heuristic
                    // threshold.  This ensures that small bumps in the outline
                    // are ignored (for example, the `vertical serifs' found in
                    // many Hebrew glyph designs).
                    //
                    // If this segment is long enough, we are done.  Otherwise,
                    // search the segment next to the extremum that is long
                    // enough, has the same direction, and a not too large
                    // vertical distance from the extremum.  Note that the
                    // algorithm doesn't check whether the found segment is
                    // actually the one (vertically) nearest to the extremum.""
                    //
                    // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/aflatin.c#L641>
                    // heuristic threshold value
                    let length_threshold = units_per_em / 25;
                    let dist = (best_contour[segment_last].x as i32
                        - best_contour[segment_first].x as i32)
                        .abs();
                    if dist < length_threshold
                        && segment_last - segment_first + 2 <= best_contour.len()
                    {
                        // heuristic threshold value
                        let height_threshold = units_per_em / 4;
                        // find previous point with different x value
                        let mut prev_ix = best_point_ix;
                        for (ix, prev) in cycle_backward(best_contour, best_point_ix) {
                            if prev.x as i32 != best_x {
                                prev_ix = ix;
                                break;
                            }
                        }
                        // skip for degenerate case
                        if prev_ix == best_point_ix {
                            continue;
                        }
                        let is_ltr = (best_contour[prev_ix].x as i32) < best_x;
                        let mut first = segment_last;
                        let mut last = first;
                        let mut p_first = None;
                        let mut p_last = None;
                        let mut hit = false;
                        loop {
                            if !hit {
                                // no hit, adjust first point
                                first = last;
                                // also adjust first and last on curve point
                                if best_contour[first].is_on_curve() {
                                    p_first = Some(first);
                                    p_last = Some(first);
                                } else {
                                    p_first = None;
                                    p_last = None;
                                }
                                hit = true;
                            }
                            if last < best_contour.len() - 1 {
                                last += 1;
                            } else {
                                last = 0;
                            }
                            if (best_y - best_contour[first].y as i32).abs() > height_threshold {
                                // vertical distance too large
                                hit = false;
                                continue;
                            }
                            let dist =
                                (best_contour[last].y as i32 - best_contour[first].y as i32).abs();
                            if dist > 5
                                && (best_contour[last].x as i32 - best_contour[first].x as i32)
                                    .abs()
                                    <= 20 * dist
                            {
                                hit = false;
                                if last == segment_first {
                                    break;
                                }
                                continue;
                            }
                            if best_contour[last].is_on_curve() {
                                p_last = Some(last);
                                if p_first.is_none() {
                                    p_first = Some(last);
                                }
                            }
                            let first_x = best_contour[first].x as i32;
                            let last_x = best_contour[last].x as i32;
                            let is_cur_ltr = first_x < last_x;
                            let dx = (last_x - first_x).abs();
                            if is_cur_ltr == is_ltr && dx >= length_threshold {
                                loop {
                                    if last < best_contour.len() - 1 {
                                        last += 1;
                                    } else {
                                        last = 0;
                                    }
                                    let dy = (best_contour[last].y as i32
                                        - best_contour[first].y as i32)
                                        .abs();
                                    if dy > 5
                                        && (best_contour[next_ix].x as i32
                                            - best_contour[first].x as i32)
                                            .abs()
                                            <= 20 * dist
                                    {
                                        if last > 0 {
                                            last -= 1;
                                        } else {
                                            last = best_contour.len() - 1;
                                        }
                                        break;
                                    }
                                    p_last = Some(last);
                                    if best_contour[last].is_on_curve() {
                                        p_last = Some(last);
                                        if p_first.is_none() {
                                            p_first = Some(last);
                                        }
                                    }
                                    if last == segment_first {
                                        break;
                                    }
                                }
                                best_y = best_contour[first].y as i32;
                                segment_first = first;
                                segment_last = last;
                                on_point_first = p_first;
                                on_point_last = p_last;
                                break;
                            }
                            if last == segment_first {
                                break;
                            }
                        }
                    }
                }
                best_y += y_offset;
                // Is the segment round?
                // 1. horizontal distance between first and last oncurve point
                //    is larger than a heuristic flat threshold, then it's flat
                // 2. either first or last point of segment is offcurve then
                //    it's round
                let is_round = match (on_point_first, on_point_last) {
                    (Some(first), Some(last))
                        if (best_contour[last].x as i32 - best_contour[first].x as i32).abs()
                            > flat_threshold =>
                    {
                        false
                    }
                    _ => {
                        !best_contour[segment_first].is_on_curve()
                            || !best_contour[segment_last].is_on_curve()
                    }
                };
                if is_round && is_neutral {
                    // Ignore round segments for neutral zone
                    continue;
                }
                // This seems to ignore LATIN_SUB_TOP?
                if is_top {
                    if best_y > best_y_extremum {
                        best_y_extremum = best_y;
                        best_is_round = is_round;
                    }
                } else if best_y < best_y_extremum {
                    best_y_extremum = best_y;
                    best_is_round = is_round;
                }
            }
            if best_y_extremum != i32::MIN && best_y_extremum != i32::MAX {
                if best_is_round {
                    rounds[n_rounds] = best_y_extremum;
                    n_rounds += 1;
                } else {
                    flats[n_flats] = best_y_extremum;
                    n_flats += 1;
                }
            }
        }
        if n_flats == 0 && n_rounds == 0 {
            continue;
        }
        rounds[..n_rounds].sort_unstable();
        flats[..n_flats].sort_unstable();
        let (mut blue_ref, mut blue_shoot) = if n_flats == 0 {
            let val = rounds[n_rounds / 2];
            (val, val)
        } else if n_rounds == 0 {
            let val = flats[n_flats / 2];
            (val, val)
        } else {
            (flats[n_flats / 2], rounds[n_rounds / 2])
        };
        if blue_shoot != blue_ref {
            let over_ref = blue_shoot > blue_ref;
            if is_top_like ^ over_ref {
                let val = (blue_shoot + blue_ref) / 2;
                blue_ref = val;
                blue_shoot = val;
            }
        }
        let mut blue = UnscaledBlue {
            position: blue_ref,
            overshoot: blue_shoot,
            ascender,
            descender,
            flags: blue_flags
                & (blue_flags::TOP | blue_flags::LATIN_SUB_TOP | blue_flags::LATIN_NEUTRAL),
        };
        if is_x_height {
            blue.flags |= blue_flags::LATIN_BLUE_ADJUSTMENT;
        }
        blues.push(blue);
    }
    // sort bottoms
    let mut sorted_indices: [usize; MAX_BLUES] = core::array::from_fn(|ix| ix);
    let blue_values = blues.as_mut_slice();
    let len = blue_values.len();
    if len == 0 {
        return blues;
    }
    // sort from bottom to top
    for i in 1..len {
        for j in (1..=i).rev() {
            let first = &blue_values[sorted_indices[j - 1]];
            let second = &blue_values[sorted_indices[j]];
            let a = if first.is_latin_any_top() {
                first.position
            } else {
                first.overshoot
            };
            let b = if second.is_latin_any_top() {
                second.position
            } else {
                second.overshoot
            };
            if b >= a {
                break;
            }
            sorted_indices.swap(j, j - 1);
        }
    }
    // and adjust tops
    for i in 0..len - 1 {
        let index1 = sorted_indices[i];
        let index2 = sorted_indices[i + 1];
        let first = &blue_values[index1];
        let second = &blue_values[index2];
        let a = if first.is_latin_any_top() {
            first.overshoot
        } else {
            first.position
        };
        let b = if second.is_latin_any_top() {
            second.overshoot
        } else {
            second.position
        };
        if a > b {
            if first.is_latin_any_top() {
                blue_values[index1].overshoot = b;
            } else {
                blue_values[index1].position = b;
            }
        }
    }
    blues
}

/// Compute unscaled blue values for the CJK script set.
///
/// Note: unlike the default code above, this produces two sets of blues,
/// one for horizontal zones and one for vertical zones, respectively. The
/// horizontal set is currently not generated because this has been
/// disabled in FreeType but the code remains because we may want to revisit
/// in the future.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afcjk.c#L277>
fn compute_cjk_blues(
    shaper: &Shaper,
    coords: &[F2Dot14],
    style: &StyleClass,
) -> [UnscaledBlues; 2] {
    let mut blues = [UnscaledBlues::new(), UnscaledBlues::new()];
    let (mut outline_buf, mut flats, mut fills) = buffers();
    let (glyphs, _) = things_all_blues_need(shaper.font());
    let mut shaped_cluster = ShapedCluster::default();
    // Walk over each of the blue character sets for our script.
    for (blue_str, blue_flags) in style.script.blues {
        let is_horizontal = blue_flags & blue_flags::CJK_HORIZ != 0;
        // Note: horizontal blue zones are disabled by default and have been
        // for many years in FreeType:
        // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afcjk.c#L35>
        // and <https://gitlab.freedesktop.org/freetype/freetype/-/commit/084abf0469d32a94b1c315bee10f621284694328>
        if is_horizontal {
            continue;
        }
        let is_right = blue_flags & blue_flags::CJK_RIGHT != 0;
        let is_top = blue_flags & blue_flags::TOP != 0;
        let blues = &mut blues[!is_horizontal as usize];
        if blues.len() >= MAX_BLUES {
            continue;
        }
        let mut n_flats = 0;
        let mut n_fills = 0;
        let mut is_fill = true;
        for cluster in blue_str.split(' ') {
            // The '|' character is used as a sentinel in the blue string that
            // signifies a switch to characters that define "flat" values
            // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afcjk.c#L380>
            if cluster == "|" {
                is_fill = false;
                continue;
            }
            shaper.shape_cluster(cluster, &mut shaped_cluster);
            for glyph in shaped_cluster
                .iter()
                .filter(|g| g.id.to_u32() != 0)
                .filter_map(|g| glyphs.get(g.id))
            {
                outline_buf.clear();
                if glyph.draw_unscaled(coords, None, &mut outline_buf).is_err() {
                    continue;
                }
                let outline = outline_buf.as_ref();
                // Reject glyphs that can't produce any rendering
                if outline.points.len() <= 2 {
                    continue;
                }
                // Step right up and find an extrema!
                // Unwrap is safe because we know per ^ that we have at least 3 points
                let best_pos = outline
                    .points
                    .iter()
                    .map(|p| if is_horizontal { p.x } else { p.y })
                    .reduce(
                        if (is_horizontal && is_right) || (!is_horizontal && is_top) {
                            |a: i16, c: i16| a.max(c)
                        } else {
                            |a: i16, c: i16| a.min(c)
                        },
                    )
                    .unwrap();
                if is_fill {
                    fills[n_fills] = best_pos;
                    n_fills += 1;
                } else {
                    flats[n_flats] = best_pos;
                    n_flats += 1;
                }
            }
        }
        if n_flats == 0 && n_fills == 0 {
            continue;
        }
        // Now determine the reference and overshoot of the blue; simply
        // take the median after a sort
        fills[..n_fills].sort_unstable();
        flats[..n_flats].sort_unstable();
        let (mut blue_ref, mut blue_shoot) = if n_flats == 0 {
            let value = fills[n_fills / 2] as i32;
            (value, value)
        } else if n_fills == 0 {
            let value = flats[n_flats / 2] as i32;
            (value, value)
        } else {
            (fills[n_fills / 2] as i32, flats[n_flats / 2] as i32)
        };
        // Make sure blue_ref >= blue_shoot for top/right or vice versa for
        // bottom left
        if blue_shoot != blue_ref {
            let under_ref = blue_shoot < blue_ref;
            if is_top ^ under_ref {
                blue_ref = (blue_shoot + blue_ref) / 2;
                blue_shoot = blue_ref;
            }
        }
        blues.push(UnscaledBlue {
            position: blue_ref,
            overshoot: blue_shoot,
            ascender: 0,
            descender: 0,
            flags: blue_flags & blue_flags::TOP,
        });
    }
    blues
}

#[inline(always)]
fn buffers<T: Copy + Default>() -> (
    UnscaledOutlineBuf<MAX_INLINE_POINTS>,
    [T; BLUE_STRING_MAX_LEN],
    [T; BLUE_STRING_MAX_LEN],
) {
    (
        UnscaledOutlineBuf::<MAX_INLINE_POINTS>::new(),
        [T::default(); BLUE_STRING_MAX_LEN],
        [T::default(); BLUE_STRING_MAX_LEN],
    )
}

/// A thneed is something everyone needs
#[inline(always)]
fn things_all_blues_need<'a>(font: &FontRef<'a>) -> (OutlineGlyphCollection<'a>, i32) {
    (
        font.outline_glyphs(),
        font.head()
            .map(|head| head.units_per_em())
            .unwrap_or_default() as i32,
    )
}

#[cfg(test)]
mod tests {
    use super::{
        super::super::{
            shape::{Shaper, ShaperMode},
            style,
        },
        blue_flags, UnscaledBlue,
    };
    use raw::FontRef;

    #[test]
    fn latin_blues() {
        let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
        let shaper = Shaper::new(&font, ShaperMode::Nominal);
        let style = &style::STYLE_CLASSES[super::StyleClass::LATN];
        let blues = super::compute_default_blues(&shaper, &[], style);
        let values = blues.as_slice();
        let expected = [
            UnscaledBlue {
                position: 714,
                overshoot: 725,
                ascender: 725,
                descender: -230,
                flags: blue_flags::TOP,
            },
            UnscaledBlue {
                position: 0,
                overshoot: -10,
                ascender: 725,
                descender: -10,
                flags: 0,
            },
            UnscaledBlue {
                position: 760,
                overshoot: 760,
                ascender: 770,
                descender: -240,
                flags: blue_flags::TOP,
            },
            UnscaledBlue {
                position: 536,
                overshoot: 546,
                ascender: 546,
                descender: -10,
                flags: blue_flags::TOP | blue_flags::LATIN_BLUE_ADJUSTMENT,
            },
            UnscaledBlue {
                position: 0,
                overshoot: -10,
                ascender: 546,
                descender: -10,
                flags: 0,
            },
            UnscaledBlue {
                position: -240,
                overshoot: -240,
                ascender: 760,
                descender: -240,
                flags: 0,
            },
        ];
        assert_eq!(values, &expected);
    }

    #[test]
    fn hebrew_long_blues() {
        let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
        let shaper = Shaper::new(&font, ShaperMode::Nominal);
        // Hebrew triggers "long" blue code path
        let style = &style::STYLE_CLASSES[super::StyleClass::HEBR];
        let blues = super::compute_default_blues(&shaper, &[], style);
        let values = blues.as_slice();
        assert_eq!(values.len(), 3);
        let expected = [
            UnscaledBlue {
                position: 592,
                overshoot: 592,
                ascender: 647,
                descender: -240,
                flags: blue_flags::TOP,
            },
            UnscaledBlue {
                position: 0,
                overshoot: -9,
                ascender: 647,
                descender: -9,
                flags: 0,
            },
            UnscaledBlue {
                position: -240,
                overshoot: -240,
                ascender: 647,
                descender: -240,
                flags: 0,
            },
        ];
        assert_eq!(values, &expected);
    }

    #[test]
    fn cjk_blues() {
        let font = FontRef::new(font_test_data::NOTOSERIFTC_AUTOHINT_METRICS).unwrap();
        let shaper = Shaper::new(&font, ShaperMode::Nominal);
        let style = &style::STYLE_CLASSES[super::StyleClass::HANI];
        let blues = super::compute_cjk_blues(&shaper, &[], style);
        let values = blues[1].as_slice();
        let expected = [
            UnscaledBlue {
                position: 837,
                overshoot: 824,
                ascender: 0,
                descender: 0,
                flags: blue_flags::TOP,
            },
            UnscaledBlue {
                position: -78,
                overshoot: -66,
                ascender: 0,
                descender: 0,
                flags: 0,
            },
        ];
        assert_eq!(values, &expected);
    }
}
