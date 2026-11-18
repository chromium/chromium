//! Compact representation of an unscaled, unhinted outline.

#![allow(dead_code)]

use core::ops::Range;
pub(super) use read_fonts::model::glyph::outline::UnscaledOutlineSink;
use read_fonts::{model::glyph::outline::PathSink, tables::glyf::PointFlags};
use smallvec::SmallVec;

/// The largest coordinate magnitude, in font units, that the autohinter
/// takes.
///
/// Its analysis subtracts points and scales the differences in `i32`, which
/// overflows near `1 << 27`. Only a transformed component far outside the em
/// reaches this bound.
const MAX_COORD: i32 = 1 << 24;

/// Clamps a coordinate in font units to the range the autohinter takes.
pub(crate) fn clamp_coord(value: i32) -> i32 {
    value.clamp(-MAX_COORD, MAX_COORD)
}

/// A point of an outline in font units, with a contour start flag.
#[derive(Copy, Clone, Default, Debug)]
pub(crate) struct UnscaledPoint {
    pub x: i32,
    pub y: i32,
    pub flags: PointFlags,
    pub is_contour_start: bool,
}

impl UnscaledPoint {
    pub fn is_on_curve(self) -> bool {
        self.flags.is_on_curve()
    }
}

// please can I have smallvec?
pub(crate) struct UnscaledOutlineBuf<const INLINE_CAP: usize> {
    points: SmallVec<[UnscaledPoint; INLINE_CAP]>,
    /// True after a contour starts and before its first point is added.
    starting: bool,
}

impl<const INLINE_CAP: usize> UnscaledOutlineBuf<INLINE_CAP> {
    pub fn new() -> Self {
        Self {
            points: SmallVec::new(),
            starting: false,
        }
    }

    pub fn clear(&mut self) {
        self.points.clear();
        self.starting = false;
    }

    pub fn as_ref(&self) -> UnscaledOutlineRef<'_> {
        UnscaledOutlineRef {
            points: self.points.as_slice(),
        }
    }
}

impl<const INLINE_CAP: usize> UnscaledOutlineSink for UnscaledOutlineBuf<INLINE_CAP> {
    fn start_contour(&mut self) {
        self.starting = true;
    }

    fn point(&mut self, x: i32, y: i32, flags: PointFlags) {
        self.points.push(UnscaledPoint {
            x: clamp_coord(x),
            y: clamp_coord(y),
            flags,
            is_contour_start: self.starting,
        });
        self.starting = false;
    }

    fn end_contour(&mut self) {
        self.starting = false;
    }
}

#[derive(Copy, Clone, Debug)]
pub(crate) struct UnscaledOutlineRef<'a> {
    pub points: &'a [UnscaledPoint],
}

impl UnscaledOutlineRef<'_> {
    /// Returns the range of contour points and the index of the point within
    /// that contour for the last point where `f` returns true.
    ///
    /// This is common code used for finding extrema when materializing blue
    /// zones.
    ///
    /// For example: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/aflatin.c#L509>
    pub fn find_last_contour(
        &self,
        mut f: impl FnMut(&UnscaledPoint) -> bool,
    ) -> Option<(Range<usize>, usize)> {
        if self.points.is_empty() {
            return None;
        }
        let mut best_contour = 0..0;
        // Index of the best point relative to the start of the best contour
        let mut best_point = 0;
        let mut cur_contour = 0..0;
        let mut found_best_in_cur_contour = false;
        for (point_ix, point) in self.points.iter().enumerate() {
            if point.is_contour_start {
                if found_best_in_cur_contour {
                    best_contour = cur_contour;
                }
                cur_contour = point_ix..point_ix;
                found_best_in_cur_contour = false;
                // Ignore single point contours
                match self.points.get(point_ix + 1) {
                    Some(next_point) if next_point.is_contour_start => continue,
                    None => continue,
                    _ => {}
                }
            }
            cur_contour.end += 1;
            if f(point) {
                best_point = point_ix - cur_contour.start;
                found_best_in_cur_contour = true;
            }
        }
        if found_best_in_cur_contour {
            best_contour = cur_contour;
        }
        if !best_contour.is_empty() {
            Some((best_contour, best_point))
        } else {
            None
        }
    }
}

#[derive(Copy, Clone)]
enum PendingElement {
    Line([f32; 2]),
    Quad([f32; 4]),
    Cubic([f32; 6]),
}

/// Converts a path drawn by this crate's CFF and VARC code into the points
/// of an [`UnscaledOutlineSink`].
///
/// read-fonts does the same for its own outlines. This type handles the
/// paths drawn from a [`FontRef`](crate::FontRef). A segment's end point is
/// held until the next command, so a contour that closes on its start point
/// does not repeat it. This matches how FreeType builds a TrueType outline
/// from a charstring.
pub(super) struct PointsFromPath<'a, S: ?Sized> {
    sink: &'a mut S,
    open: bool,
    last_start: Option<(f32, f32)>,
    pending: Option<PendingElement>,
}

impl<'a, S: ?Sized> PointsFromPath<'a, S> {
    pub fn new(sink: &'a mut S) -> Self {
        Self {
            sink,
            open: false,
            last_start: None,
            pending: None,
        }
    }
}

impl<S: UnscaledOutlineSink + ?Sized> PointsFromPath<'_, S> {
    fn push(&mut self, x: f32, y: f32, flags: PointFlags) {
        if !self.open {
            self.sink.start_contour();
            self.open = true;
        }
        self.sink.point(x as i32, y as i32, flags);
    }

    fn flush_pending(&mut self, for_close: bool) {
        if let Some(element) = self.pending.take() {
            let [x, y] = match element {
                PendingElement::Line([x, y]) => [x, y],
                PendingElement::Quad([x0, y0, x, y]) => {
                    self.push(x0, y0, PointFlags::off_curve_quad());
                    [x, y]
                }
                PendingElement::Cubic([x0, y0, x1, y1, x, y]) => {
                    self.push(x0, y0, PointFlags::off_curve_cubic());
                    self.push(x1, y1, PointFlags::off_curve_cubic());
                    [x, y]
                }
            };
            if !for_close || self.last_start != Some((x, y)) {
                self.push(x, y, PointFlags::on_curve());
            }
        }
    }

    fn close_contour(&mut self) {
        self.flush_pending(true);
        if self.open {
            self.sink.end_contour();
            self.open = false;
        }
    }

    /// Closes the open contour, if any.
    pub fn finish(mut self) {
        self.close_contour();
    }
}

impl<S: UnscaledOutlineSink + ?Sized> PathSink for PointsFromPath<'_, S> {
    fn move_to(&mut self, x: f32, y: f32) {
        self.close_contour();
        self.push(x, y, PointFlags::on_curve());
        self.last_start = Some((x, y));
    }

    fn line_to(&mut self, x: f32, y: f32) {
        self.flush_pending(false);
        self.pending = Some(PendingElement::Line([x, y]));
    }

    fn quad_to(&mut self, cx0: f32, cy0: f32, x: f32, y: f32) {
        self.flush_pending(false);
        self.pending = Some(PendingElement::Quad([cx0, cy0, x, y]));
    }

    fn curve_to(&mut self, cx0: f32, cy0: f32, cx1: f32, cy1: f32, x: f32, y: f32) {
        self.flush_pending(false);
        self.pending = Some(PendingElement::Cubic([cx0, cy0, cx1, cy1, x, y]));
    }

    fn close(&mut self) {
        self.close_contour();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{prelude::LocationRef, MetadataProvider};
    use raw::{types::GlyphId, FontRef};

    fn points_of(data: &[u8], glyph_id: u32) -> Vec<(i32, i32, u8)> {
        let font = FontRef::new(data).unwrap();
        let glyph = font.outline_glyphs().get(GlyphId::new(glyph_id)).unwrap();
        let mut outline = UnscaledOutlineBuf::<32>::new();
        glyph
            .draw_unscaled(LocationRef::default(), None, &mut outline)
            .unwrap();
        outline
            .as_ref()
            .points
            .iter()
            .map(|point| (point.x, point.y, point.flags.to_bits()))
            .collect()
    }

    // A contour that closes with a quad does not repeat its start point.
    #[test]
    fn a_closing_quad_does_not_repeat_the_start() {
        use crate::outline::pen::OutlinePen;
        let mut outline = UnscaledOutlineBuf::<8>::new();
        let mut pen = PointsFromPath::new(&mut outline);
        pen.move_to(0.0, 0.0);
        pen.line_to(10.0, 0.0);
        pen.quad_to(10.0, 10.0, 0.0, 0.0);
        pen.close();
        pen.finish();
        let points: Vec<_> = outline
            .as_ref()
            .points
            .iter()
            .map(|point| (point.x, point.y, point.flags.to_bits()))
            .collect();
        let on = PointFlags::on_curve().to_bits();
        let off = PointFlags::off_curve_quad().to_bits();
        assert_eq!(points, [(0, 0, on), (10, 0, on), (10, 10, off)]);
    }

    #[test]
    fn read_glyf_outline() {
        let expected = [
            // contour 0
            (400, 80, 1),
            (400, 360, 1),
            (320, 360, 1),
            (320, 600, 1),
            (320, 633, 0),
            (367, 680, 0),
            (400, 680, 1),
            (560, 680, 1),
            (593, 680, 0),
            (640, 633, 0),
            (640, 600, 1),
            (640, 360, 1),
            (560, 360, 1),
            (560, 80, 1),
            // contour 1
            (480, 720, 1),
            (447, 720, 0),
            (400, 767, 0),
            (400, 800, 1),
            (400, 833, 0),
            (447, 880, 0),
            (480, 880, 1),
            (513, 880, 0),
            (560, 833, 0),
            (560, 800, 1),
            (560, 767, 0),
            (513, 720, 0),
        ];
        assert_eq!(
            points_of(font_test_data::MATERIAL_SYMBOLS_SUBSET, 5),
            expected
        );
    }

    #[test]
    fn read_cubic_glyf_outline() {
        let expected = [
            // contour 0
            (278, 710, 1),
            (278, 470, 1),
            (300, 500, 128),
            (800, 500, 128),
            (998, 470, 1),
            (998, 710, 1),
        ];
        assert_eq!(points_of(font_test_data::CUBIC_GLYF, 2), expected);
    }

    #[test]
    fn read_cff_outline() {
        let expected = [
            // contour 0
            (83, 0, 1),
            (163, 0, 1),
            (163, 482, 1),
            (83, 482, 1),
            // contour 1
            (124, 595, 1),
            (160, 595, 128),
            (181, 616, 128),
            (181, 652, 1),
            (181, 688, 128),
            (160, 709, 128),
            (124, 709, 1),
            (88, 709, 128),
            (67, 688, 128),
            (67, 652, 1),
            (67, 616, 128),
            (88, 595, 128),
        ];
        assert_eq!(points_of(font_test_data::CANTARELL_VF_TRIMMED, 2), expected);
    }

    #[test]
    fn find_vertical_extrema() {
        let font = FontRef::new(font_test_data::MATERIAL_SYMBOLS_SUBSET).unwrap();
        let glyph = font.outline_glyphs().get(GlyphId::new(5)).unwrap();
        let mut outline = UnscaledOutlineBuf::<32>::new();
        glyph
            .draw_unscaled(LocationRef::default(), None, &mut outline)
            .unwrap();
        let outline = outline.as_ref();
        // Find the maximum Y value and its containing contour
        let mut top_y = None;
        let (top_contour, top_point_ix) = outline
            .find_last_contour(|point| {
                if top_y.is_none() || Some(point.y) > top_y {
                    top_y = Some(point.y);
                    true
                } else {
                    false
                }
            })
            .unwrap();
        assert_eq!(top_contour, 14..26);
        assert_eq!(top_point_ix, 5);
        assert_eq!(top_y, Some(880));
        // Find the minimum Y value and its containing contour
        let mut bottom_y = None;
        let (bottom_contour, bottom_point_ix) = outline
            .find_last_contour(|point| {
                if bottom_y.is_none() || Some(point.y) < bottom_y {
                    bottom_y = Some(point.y);
                    true
                } else {
                    false
                }
            })
            .unwrap();
        assert_eq!(bottom_contour, 0..14);
        assert_eq!(bottom_point_ix, 0);
        assert_eq!(bottom_y, Some(80));
    }
}
