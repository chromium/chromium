//! Turning a loaded outline into path commands.

use super::super::{PointCoord, PointFlags};
use crate::{model::glyph::outline::OutlinePen, types::Point};

/// Where a contour starts when its first point is off-curve.
///
/// A `glyf` point stream does not say where such a contour begins, and the
/// major implementations disagree, so a caller matching one of them has to
/// say which.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub enum PathContourStart {
    /// Scan backward: the last point if it is on-curve, and the midpoint
    /// between it and the first otherwise.
    ///
    /// What FreeType and HarfBuzz do.
    #[default]
    ScanBackward,
    /// Scan forward: the second point if it is on-curve, and the midpoint
    /// between it and the first otherwise.
    ///
    /// What HarfBuzz used to do, kept for compatibility purposes.
    ScanForward,
}

/// Converts an outline given as points, flags and contour end points into
/// path commands, and calls `pen` for each.
///
/// Points may have any coordinate type; the pen is always called with `f32`.
///
/// Returns `false` where the points do not describe a path: a contour whose
/// points run off the end, a quadratic or cubic missing the points it needs,
/// or one flag per point missing. Whatever was drawn before that is already
/// on the pen, so a caller that cannot use half a glyph discards its own
/// work rather than asking for it back.
///
/// Roughly [`FT_Outline_Decompose`](https://freetype.org/freetype2/docs/reference/ft2-outline_processing.html#ft_outline_decompose).
/// See [`contour_to_path`] for one contour of points held in some other shape.
#[must_use]
pub fn outline_to_path<C: PointCoord>(
    points: &[Point<C>],
    flags: &[PointFlags],
    contours: &[u16],
    start: PathContourStart,
    pen: &mut impl OutlinePen,
) -> bool {
    if flags.len() != points.len() {
        return false;
    }
    let mut base = 0usize;
    for end in contours {
        let end = *end as usize;
        if end < base || end >= points.len() {
            return false;
        }
        let (points, flags) = (&points[base..=end], &flags[base..=end]);
        if !walk_contour(
            points.len(),
            |ix| ContourPoint::new(points[ix], flags[ix]),
            start,
            pen,
        ) {
            return false;
        }
        base = end + 1;
    }
    true
}

/// Converts one contour into path commands, taking its points from a slice of
/// anything `to_point` can read them out of.
///
/// For outlines held in some shape other than parallel point and flag arrays —
/// an autohinter's points, say, which carry more per point than these do.
///
/// Returns `false` on the same terms as [`outline_to_path`], and leaves what
/// it drew on the pen.
///
/// ```
/// use read_fonts::{
///     model::glyph::outline::SvgPen,
///     tables::glyf::{outline::{contour_to_path, PathContourStart}, PointFlags},
///     types::Point,
/// };
///
/// // Whatever the caller happens to hold, so long as points can be read out.
/// struct MyPoint { x: f32, y: f32, on_curve: bool, _more: [u32; 8] }
///
/// let contour = [
///     MyPoint { x: 0.0, y: 0.0, on_curve: true, _more: [0; 8] },
///     MyPoint { x: 8.0, y: 0.0, on_curve: true, _more: [0; 8] },
///     MyPoint { x: 8.0, y: 8.0, on_curve: true, _more: [0; 8] },
/// ];
///
/// let mut pen = SvgPen::with_precision(1);
/// assert!(contour_to_path(
///     &contour,
///     |p| {
///         let flags = if p.on_curve {
///             PointFlags::on_curve()
///         } else {
///             PointFlags::off_curve_quad()
///         };
///         (Point::new(p.x, p.y), flags)
///     },
///     PathContourStart::ScanBackward,
///     &mut pen,
/// ));
/// assert_eq!(pen.as_ref(), "M0.0,0.0 L8.0,0.0 L8.0,8.0 Z");
/// ```
#[must_use]
pub fn contour_to_path<T, C: PointCoord>(
    points: &[T],
    to_point: impl Fn(&T) -> (Point<C>, PointFlags),
    start: PathContourStart,
    pen: &mut impl OutlinePen,
) -> bool {
    walk_contour(
        points.len(),
        |ix| {
            let (point, flags) = to_point(&points[ix]);
            ContourPoint::new(point, flags)
        },
        start,
        pen,
    )
}

/// Emits one contour, reading its points by index.
///
/// Indexing rather than iterating is what keeps this to one loop: finding the
/// start may consult the last point or the second, and the walk that follows
/// is a rotation of the contour, which an iterator cannot express without
/// stashing the points it had to skip.
fn walk_contour<C: PointCoord>(
    len: usize,
    point: impl Fn(usize) -> ContourPoint<C>,
    start: PathContourStart,
    pen: &mut impl OutlinePen,
) -> bool {
    if len == 0 {
        return true;
    }
    let first = point(0);
    // A contour may not begin on a cubic off-curve point.
    if first.flags.is_off_curve_cubic() {
        return false;
    }
    // Where to begin, then `count` points from `base`, wrapping. Every case
    // below is one of those; the forward ones are rotations.
    let (start_point, base, count) = if !first.flags.is_off_curve_quad() {
        // Already on-curve, so start there and do not emit it again.
        (first, 1, len - 1)
    } else {
        match start {
            PathContourStart::ScanBackward => {
                let last = point(len - 1);
                if last.flags.is_on_curve() {
                    // Start at the last point, and leave it out of the walk.
                    (last, 0, len - 1)
                } else {
                    (last.midpoint(first), 0, len)
                }
            }
            PathContourStart::ScanForward => {
                if len < 2 {
                    // A single off-curve point is not a contour.
                    return true;
                }
                let second = point(1);
                if second.flags.is_on_curve() {
                    (second, 2, len)
                } else {
                    (first.midpoint(second), 1, len)
                }
            }
        }
    };
    let start_f32 = start_point.point_f32();
    pen.move_to(start_f32.x, start_f32.y);
    let mut state = PendingState::default();
    // The walk runs off the end at most once, so it is two straight runs
    // rather than one indexed modulo `len`.
    let wrapped = (base + count).saturating_sub(len);
    for ix in base..base + count - wrapped {
        if !state.emit(point(ix), pen) {
            return false;
        }
    }
    for ix in 0..wrapped {
        if !state.emit(point(ix), pen) {
            return false;
        }
    }
    state.finish(start_point, pen)
}

/// A point and its flag, which is all path conversion reads.
#[derive(Copy, Clone, Default, Debug)]
struct ContourPoint<C> {
    x: C,
    y: C,
    flags: PointFlags,
}

impl<C: PointCoord> ContourPoint<C> {
    fn new(point: Point<C>, flags: PointFlags) -> Self {
        Self {
            x: point.x,
            y: point.y,
            flags,
        }
    }

    fn point_f32(&self) -> Point<f32> {
        Point::new(self.x.to_f32(), self.y.to_f32())
    }

    fn midpoint(&self, other: Self) -> Self {
        Self {
            x: self.x.midpoint(other.x),
            y: self.y.midpoint(other.y),
            flags: other.flags,
        }
    }
}

/// Off-curve points seen but not yet emitted, since a curve cannot be drawn
/// until the point that ends it arrives.
#[derive(Copy, Clone, Default)]
enum PendingState<C> {
    #[default]
    Empty,
    Quad(ContourPoint<C>),
    Cubic(ContourPoint<C>),
    TwoCubics(ContourPoint<C>, ContourPoint<C>),
}

impl<C: PointCoord> PendingState<C> {
    #[inline(always)]
    fn emit(&mut self, point: ContourPoint<C>, pen: &mut impl OutlinePen) -> bool {
        let flags = point.flags;
        match *self {
            Self::Empty => {
                if flags.is_off_curve_quad() {
                    *self = Self::Quad(point);
                } else if flags.is_off_curve_cubic() {
                    *self = Self::Cubic(point);
                } else {
                    let p = point.point_f32();
                    pen.line_to(p.x, p.y);
                }
            }
            Self::Quad(quad) => {
                if flags.is_off_curve_quad() {
                    // Two quads in a row imply an on-curve point between them.
                    let c0 = quad.point_f32();
                    let p = quad.midpoint(point).point_f32();
                    pen.quad_to(c0.x, c0.y, p.x, p.y);
                    *self = Self::Quad(point);
                } else if flags.is_off_curve_cubic() {
                    return false;
                } else {
                    let c0 = quad.point_f32();
                    let p = point.point_f32();
                    pen.quad_to(c0.x, c0.y, p.x, p.y);
                    *self = Self::Empty;
                }
            }
            Self::Cubic(cubic) => {
                if flags.is_off_curve_cubic() {
                    *self = Self::TwoCubics(cubic, point);
                } else {
                    return false;
                }
            }
            Self::TwoCubics(cubic0, cubic1) => {
                if flags.is_off_curve_quad() {
                    return false;
                } else if flags.is_off_curve_cubic() {
                    // Three cubics in a row imply an on-curve point too.
                    let c0 = cubic0.point_f32();
                    let c1 = cubic1.point_f32();
                    let p = cubic1.midpoint(point).point_f32();
                    pen.curve_to(c0.x, c0.y, c1.x, c1.y, p.x, p.y);
                    *self = Self::Cubic(point);
                } else {
                    let c0 = cubic0.point_f32();
                    let c1 = cubic1.point_f32();
                    let p = point.point_f32();
                    pen.curve_to(c0.x, c0.y, c1.x, c1.y, p.x, p.y);
                    *self = Self::Empty;
                }
            }
        }
        true
    }

    fn finish(mut self, mut start_point: ContourPoint<C>, pen: &mut impl OutlinePen) -> bool {
        if !matches!(self, Self::Empty) {
            // A contour always ends on an explicit on-curve point.
            start_point.flags = PointFlags::on_curve();
            if !self.emit(start_point, pen) {
                return false;
            }
        }
        pen.close();
        true
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::glyph::outline::SvgPen;
    use crate::types::F26Dot6;
    use alloc::vec::Vec;

    /// Builds points and flags from `(x, y, kind)`, where kind is 'n' for an
    /// on-curve point, 'q' for a quadratic control and 'c' for a cubic.
    fn contour(points: &[(f32, f32, char)]) -> (Vec<Point<f32>>, Vec<PointFlags>) {
        points
            .iter()
            .map(|(x, y, kind)| {
                let flags = match kind {
                    'q' => PointFlags::off_curve_quad(),
                    'c' => PointFlags::off_curve_cubic(),
                    _ => PointFlags::on_curve(),
                };
                (Point::new(*x, *y), flags)
            })
            .unzip()
    }

    fn draw(
        points: &[(f32, f32, char)],
        contours: &[u16],
        start: PathContourStart,
    ) -> Option<String> {
        let (points, flags) = contour(points);
        let mut pen = SvgPen::with_precision(1);
        outline_to_path(&points, &flags, contours, start, &mut pen)
            .then(|| pen.as_ref().to_string())
    }

    /// Draws one four point contour in 26.6, with the second point on-curve
    /// unless `all_off_curve`.
    fn draw_off_curve(expected: &str, start: PathContourStart, all_off_curve: bool) {
        fn pt(x: i32, y: i32) -> Point<F26Dot6> {
            Point::new(x, y).map(F26Dot6::from_bits)
        }
        let mut flags = [PointFlags::off_curve_quad(); 4];
        if !all_off_curve {
            flags[1] = PointFlags::on_curve();
        }
        let points = [pt(640, 128), pt(256, 64), pt(640, 64), pt(128, 128)];
        let mut pen = SvgPen::with_precision(1);
        assert!(outline_to_path(&points, &flags, &[3], start, &mut pen));
        assert_eq!(pen.as_ref(), expected);
    }

    // A contour of nothing but off-curve points starts at the midpoint
    // between the first and last: [(640, 128) + (128, 128)] / 2 = (384, 128),
    // which is (6.0, 2.0) once converted. Getting that first move wrong was a
    // bug once.
    #[test]
    fn a_contour_of_off_curve_points_starts_between_its_ends_scanning_backward() {
        draw_off_curve(
            "M6.0,2.0 Q10.0,2.0 7.0,1.5 Q4.0,1.0 7.0,1.0 Q10.0,1.0 6.0,1.5 Q2.0,2.0 6.0,2.0 Z",
            PathContourStart::ScanBackward,
            true,
        );
    }

    #[test]
    fn a_contour_of_off_curve_points_starts_between_its_ends_scanning_forward() {
        draw_off_curve(
            "M7.0,1.5 Q4.0,1.0 7.0,1.0 Q10.0,1.0 6.0,1.5 Q2.0,2.0 6.0,2.0 Q10.0,2.0 7.0,1.5 Z",
            PathContourStart::ScanForward,
            true,
        );
    }

    #[test]
    fn a_contour_beginning_off_curve_scans_backward_to_an_on_curve_point() {
        draw_off_curve(
            "M6.0,2.0 Q10.0,2.0 4.0,1.0 Q10.0,1.0 6.0,1.5 Q2.0,2.0 6.0,2.0 Z",
            PathContourStart::ScanBackward,
            false,
        );
    }

    #[test]
    fn a_contour_beginning_off_curve_scans_forward_to_an_on_curve_point() {
        draw_off_curve(
            "M4.0,1.0 Q10.0,1.0 6.0,1.5 Q2.0,2.0 6.0,2.0 Q10.0,2.0 4.0,1.0 Z",
            PathContourStart::ScanForward,
            false,
        );
    }

    #[test]
    fn a_contour_of_on_curve_points_is_a_run_of_lines() {
        assert_eq!(
            draw(
                &[(0.0, 0.0, 'n'), (8.0, 0.0, 'n'), (8.0, 8.0, 'n')],
                &[2],
                PathContourStart::ScanBackward
            )
            .unwrap(),
            "M0.0,0.0 L8.0,0.0 L8.0,8.0 Z"
        );
    }

    #[test]
    fn a_contour_beginning_off_curve_starts_where_the_caller_asks() {
        // Nothing in the point stream says where such a contour begins, so
        // the two directions find different on-curve points to start from.
        let points = [(0.0, 0.0, 'q'), (8.0, 0.0, 'n'), (8.0, 8.0, 'n')];
        let backward = draw(&points, &[2], PathContourStart::ScanBackward).unwrap();
        let forward = draw(&points, &[2], PathContourStart::ScanForward).unwrap();
        assert!(backward.starts_with("M8.0,8.0"), "{backward}");
        assert!(forward.starts_with("M8.0,0.0"), "{forward}");
        assert_ne!(backward, forward);
    }

    #[test]
    fn two_contours_are_drawn_in_turn() {
        let drawn = draw(
            &[
                (0.0, 0.0, 'n'),
                (4.0, 0.0, 'n'),
                (8.0, 8.0, 'n'),
                (9.0, 9.0, 'n'),
            ],
            &[1, 3],
            PathContourStart::ScanBackward,
        )
        .unwrap();
        assert_eq!(drawn, "M0.0,0.0 L4.0,0.0 Z M8.0,8.0 L9.0,9.0 Z");
    }

    #[test]
    fn points_that_do_not_describe_a_path_are_refused() {
        // A cubic needs two controls before the point it bends toward, and a
        // contour cannot end past the points it was given.
        assert_eq!(
            draw(
                &[(0.0, 0.0, 'n'), (4.0, 4.0, 'c'), (8.0, 0.0, 'n')],
                &[2],
                PathContourStart::ScanBackward
            ),
            None
        );
        assert_eq!(
            draw(
                &[(0.0, 0.0, 'n'), (8.0, 0.0, 'n')],
                &[7],
                PathContourStart::ScanBackward
            ),
            None
        );
    }

    #[test]
    fn a_flag_per_point_is_required() {
        let (points, _) = contour(&[(0.0, 0.0, 'n'), (8.0, 0.0, 'n')]);
        let mut pen = SvgPen::with_precision(1);
        assert!(!outline_to_path(
            &points,
            &[PointFlags::on_curve()],
            &[1],
            PathContourStart::ScanBackward,
            &mut pen
        ));
    }
}
