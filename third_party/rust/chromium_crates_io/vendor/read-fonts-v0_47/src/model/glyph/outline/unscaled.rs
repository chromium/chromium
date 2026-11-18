//! Outlines in font units, as points rather than path commands.
//!
//! An autohinter needs outline points in the form the `glyf` table stores
//! them: whole font units, marked on-curve or off-curve, and grouped by
//! contour. [`UnscaledOutlineSink`] receives these points. A `glyf` outline
//! provides its points directly. A charstring or a `VARC` composite is drawn
//! as a path and converted to points.

use super::PathSink;
use crate::tables::glyf::PointFlags;

/// Receives the points of an outline in font units, one contour at a time.
pub trait UnscaledOutlineSink {
    /// Begins a contour.
    fn start_contour(&mut self);

    /// Adds a point to the open contour.
    ///
    /// `flags` indicate whether the point is on the curve or is an off-curve
    /// control point of a quadratic or cubic curve. The interpreter marker
    /// flags are always clear.
    fn point(&mut self, x: i32, y: i32, flags: PointFlags);

    /// Ends the open contour.
    fn end_contour(&mut self);
}

impl<S: UnscaledOutlineSink + ?Sized> UnscaledOutlineSink for &mut S {
    fn start_contour(&mut self) {
        (**self).start_contour()
    }

    fn point(&mut self, x: i32, y: i32, flags: PointFlags) {
        (**self).point(x, y, flags)
    }

    fn end_contour(&mut self) {
        (**self).end_contour()
    }
}

#[derive(Copy, Clone)]
enum PendingElement {
    Line([f32; 2]),
    Quad([f32; 4]),
    Cubic([f32; 6]),
}

/// A [`PathSink`] that converts a path into points and sends them to an
/// [`UnscaledOutlineSink`].
///
/// This is used for sources that can only draw a path. The path must be in
/// font units. Each segment's end point is held until the next command. When
/// a contour closes at its start point, that point is not repeated. This
/// matches how FreeType builds a TrueType outline from a charstring.
pub(crate) struct PointsFromPath<'a, S: ?Sized> {
    sink: &'a mut S,
    /// True while a contour is open.
    open: bool,
    last_start: Option<(f32, f32)>,
    pending: Option<PendingElement>,
}

impl<'a, S: ?Sized> PointsFromPath<'a, S> {
    pub(crate) fn new(sink: &'a mut S) -> Self {
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
        // Truncation toward zero, as a charstring coordinate has always been
        // taken into TrueType points here.
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

    /// Ends the open contour, if any.
    pub(crate) fn finish(mut self) {
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
    use crate::model::glyph::outline::DrawMemory;
    use crate::{model::Font, types::GlyphId};
    use alloc::vec::Vec;

    /// Records the calls made to a sink, for testing.
    #[derive(Default)]
    struct Recorded {
        points: Vec<(i32, i32, u8)>,
        starts: Vec<usize>,
        open: bool,
        ended: usize,
    }

    impl UnscaledOutlineSink for Recorded {
        fn start_contour(&mut self) {
            assert!(!self.open, "a contour was started inside another");
            self.open = true;
            self.starts.push(self.points.len());
        }

        fn point(&mut self, x: i32, y: i32, flags: PointFlags) {
            assert!(self.open, "a point arrived outside a contour");
            self.points.push((x, y, flags.to_bits()));
        }

        fn end_contour(&mut self) {
            assert!(self.open, "a contour ended that never started");
            self.open = false;
            self.ended += 1;
        }
    }

    fn record(data: &[u8], glyph_id: u32) -> Recorded {
        let font = Font::new(data.to_vec(), 0).unwrap();
        let glyph = font.outline_glyphs().get(GlyphId::new(glyph_id)).unwrap();
        let mut recorded = Recorded::default();
        glyph
            .draw_unscaled(DrawMemory::Scratch, &mut recorded)
            .unwrap();
        assert!(!recorded.open);
        assert_eq!(recorded.ended, recorded.starts.len());
        recorded
    }

    #[test]
    fn read_glyf_outline() {
        let recorded = record(font_test_data::MATERIAL_SYMBOLS_SUBSET, 5);
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
        assert_eq!(recorded.points, expected);
        assert_eq!(recorded.starts, [0, 14]);
    }

    #[test]
    fn the_advance_comes_with_the_points() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET.to_vec(), 0).unwrap();
        let glyph = font.outline_glyphs().get(GlyphId::new(5)).unwrap();
        let metrics = glyph
            .draw_unscaled(DrawMemory::Scratch, &mut Recorded::default())
            .unwrap();
        assert_eq!(
            metrics.h_advance,
            Some(font.glyph_metrics().h_advance(GlyphId::new(5)))
        );
    }

    #[test]
    fn read_cubic_glyf_outline() {
        let recorded = record(font_test_data::CUBIC_GLYF, 2);
        let expected = [
            (278, 710, 1),
            (278, 470, 1),
            (300, 500, 128),
            (800, 500, 128),
            (998, 470, 1),
            (998, 710, 1),
        ];
        assert_eq!(recorded.points, expected);
        assert_eq!(recorded.starts, [0]);
    }

    #[test]
    fn read_cff_outline() {
        let recorded = record(font_test_data::CANTARELL_VF_TRIMMED, 2);
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
        assert_eq!(recorded.points, expected);
        assert_eq!(recorded.starts, [0, 4]);
    }

    #[test]
    fn a_varc_composite_rebuilds_from_its_path() {
        let font = Font::new(font_test_data::varc::CJK_6868.to_vec(), 0).unwrap();
        let outlines = font.outline_glyphs();
        let gid = (0..font.num_glyphs())
            .map(GlyphId::new)
            .find(|gid| {
                matches!(
                    outlines.get(*gid),
                    Some(crate::model::glyph::outline::OutlineGlyph::Varc(_))
                )
            })
            .unwrap();
        let mut recorded = Recorded::default();
        outlines
            .get(gid)
            .unwrap()
            .draw_unscaled(DrawMemory::Scratch, &mut recorded)
            .unwrap();
        assert!(recorded.points.len() > 2);
        assert_eq!(recorded.starts.first(), Some(&0));
        assert!(!recorded.open);
    }

    // A contour that closes with a quad does not repeat its start point.
    #[test]
    fn a_closing_quad_does_not_repeat_the_start() {
        let mut recorded = Recorded::default();
        let mut pen = PointsFromPath::new(&mut recorded);
        pen.move_to(0.0, 0.0);
        pen.line_to(10.0, 0.0);
        pen.quad_to(10.0, 10.0, 0.0, 0.0);
        pen.close();
        pen.finish();
        let on = PointFlags::on_curve().to_bits();
        let off = PointFlags::off_curve_quad().to_bits();
        assert_eq!(recorded.points, [(0, 0, on), (10, 0, on), (10, 10, off)]);
    }

    // No contour of a VARC composite, whose glyf components draw as quads,
    // repeats its start point.
    #[test]
    fn varc_contours_do_not_repeat_their_start() {
        let font = Font::new(font_test_data::varc::CJK_6868.to_vec(), 0).unwrap();
        let outlines = font.outline_glyphs();
        let mut contours = 0;
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            let Some(glyph @ crate::model::glyph::outline::OutlineGlyph::Varc(_)) =
                outlines.get(gid)
            else {
                continue;
            };
            let mut recorded = Recorded::default();
            glyph
                .draw_unscaled(DrawMemory::Scratch, &mut recorded)
                .unwrap();
            let ends = recorded.starts[1..]
                .iter()
                .copied()
                .chain([recorded.points.len()]);
            for (start, end) in recorded.starts.iter().copied().zip(ends) {
                let (first, last) = (recorded.points[start], recorded.points[end - 1]);
                assert!(
                    end - start < 2 || (first.0, first.1) != (last.0, last.1),
                    "glyph {gid} contour at {start}"
                );
                contours += 1;
            }
        }
        assert!(contours > 0);
    }

    /// When a contour ends with a line or cubic whose end matches the start
    /// point, omit the last on curve. This matches FreeType behavior when
    /// constructing a TrueType style outline from a CFF font.
    #[test]
    fn omit_unnecessary_trailing_oncurves() {
        let mut recorded = Recorded::default();
        let mut pen = PointsFromPath::new(&mut recorded);
        pen.move_to(0.5, 1.5);
        pen.line_to(1.0, 2.0);
        // matches start, omit last on curve
        pen.line_to(0.5, 1.5);
        pen.close();
        pen.move_to(5.0, 6.0);
        pen.curve_to(1.0, 1.0, 2.0, 2.0, 2.0, 3.0);
        // matches start, omit last on curve
        pen.curve_to(1.0, 1.0, 2.0, 2.0, 5.0, 6.0);
        pen.close();
        pen.move_to(5.0, 6.0);
        // doesn't match start, keep on curve
        pen.curve_to(1.0, 1.0, 2.0, 2.0, 2.0, 3.0);
        // and an unclosed contour is still ended
        pen.finish();
        let on_curves: Vec<_> = recorded
            .points
            .iter()
            .map(|(_, _, flags)| PointFlags::from_bits(*flags).is_on_curve())
            .collect();
        #[rustfmt::skip]
        let expected_on_curves = [
            true,  // move
            true,  // line
                   // trailing line omitted
            true,  // move
            false, // cubic 1 off curve 1
            false, // cubic 1 off curve 2
            true,  // cubic 1 on curve
            false, // cubic 2 off curve 1
            false, // cubic 2 off curve 2
                   // trailing on curve omitted
            true,  // move
            false, // cubic 1 off curve 1
            false, // cubic 1 off curve 2
            true,  // trailing on curve retained
        ];
        assert_eq!(on_curves, expected_on_curves);
        assert_eq!(recorded.starts, [0, 2, 8]);
        assert_eq!(recorded.ended, 3);
    }
}
