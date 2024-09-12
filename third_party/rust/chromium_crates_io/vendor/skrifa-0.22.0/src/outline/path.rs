//! TrueType style outline to path conversion.

use super::pen::{OutlinePen, PathStyle};
use core::fmt;
use raw::{
    tables::glyf::{PointCoord, PointFlags},
    types::Point,
};

/// Errors that can occur when converting an outline to a path.
#[derive(Clone, Debug)]
pub enum ToPathError {
    /// Contour end point at this index was less than its preceding end point.
    ContourOrder(usize),
    /// Expected a quadratic off-curve point at this index.
    ExpectedQuad(usize),
    /// Expected a quadratic off-curve or on-curve point at this index.
    ExpectedQuadOrOnCurve(usize),
    /// Expected a cubic off-curve point at this index.
    ExpectedCubic(usize),
    /// Expected number of points to == number of flags
    PointFlagMismatch { num_points: usize, num_flags: usize },
}

impl fmt::Display for ToPathError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Self::ContourOrder(ix) => write!(
                f,
                "Contour end point at index {ix} was less than preceding end point"
            ),
            Self::ExpectedQuad(ix) => write!(f, "Expected quadatic off-curve point at index {ix}"),
            Self::ExpectedQuadOrOnCurve(ix) => write!(
                f,
                "Expected quadatic off-curve or on-curve point at index {ix}"
            ),
            Self::ExpectedCubic(ix) => write!(f, "Expected cubic off-curve point at index {ix}"),
            Self::PointFlagMismatch {
                num_points,
                num_flags,
            } => write!(
                f,
                "Number of points ({num_points}) and flags ({num_flags}) must match"
            ),
        }
    }
}

/// Converts a `glyf` outline described by points, flags and contour end points
/// to a sequence of path elements and invokes the appropriate callback on the
/// given pen for each.
///
/// The input points can have any coordinate type that implements
/// [`PointCoord`]. Output points are always generated in `f32`.
///
/// This is roughly equivalent to [`FT_Outline_Decompose`](https://freetype.org/freetype2/docs/reference/ft2-outline_processing.html#ft_outline_decompose).
///
/// See [`contour_to_path`] for a more general function that takes an iterator
/// if your outline data is in a different format.
pub(crate) fn to_path<C: PointCoord>(
    points: &[Point<C>],
    flags: &[PointFlags],
    contours: &[u16],
    path_style: PathStyle,
    pen: &mut impl OutlinePen,
) -> Result<(), ToPathError> {
    for contour_ix in 0..contours.len() {
        let start_ix = (contour_ix > 0)
            .then(|| contours[contour_ix - 1] as usize + 1)
            .unwrap_or_default();
        let end_ix = contours[contour_ix] as usize;
        if end_ix < start_ix || end_ix >= points.len() {
            return Err(ToPathError::ContourOrder(contour_ix));
        }
        let points = &points[start_ix..=end_ix];
        let flags = flags
            .get(start_ix..=end_ix)
            .ok_or(ToPathError::PointFlagMismatch {
                num_points: points.len(),
                num_flags: flags.len(),
            })?;
        if points.is_empty() {
            continue;
        }
        let last_point = points.last().unwrap();
        let last_flags = flags.last().unwrap();
        let last_point = ContourPoint {
            x: last_point.x,
            y: last_point.y,
            flags: *last_flags,
        };
        contour_to_path(
            points.iter().zip(flags).map(|(point, flags)| ContourPoint {
                x: point.x,
                y: point.y,
                flags: *flags,
            }),
            last_point,
            path_style,
            pen,
        )
        .map_err(|e| match &e {
            ToPathError::ExpectedCubic(ix) => ToPathError::ExpectedCubic(ix + start_ix),
            ToPathError::ExpectedQuad(ix) => ToPathError::ExpectedQuad(ix + start_ix),
            ToPathError::ExpectedQuadOrOnCurve(ix) => {
                ToPathError::ExpectedQuadOrOnCurve(ix + start_ix)
            }
            _ => e,
        })?
    }
    Ok(())
}

/// Combination of point coordinates and flags.
#[derive(Copy, Clone, Default, Debug)]
pub(crate) struct ContourPoint<T> {
    pub x: T,
    pub y: T,
    pub flags: PointFlags,
}

impl<T> ContourPoint<T>
where
    T: PointCoord,
{
    fn point_f32(&self) -> Point<f32> {
        Point::new(self.x, self.y).map(|x| x.to_f32())
    }

    fn midpoint(&self, other: &Self) -> ContourPoint<T> {
        let mid = Point::new(self.x.midpoint(other.x), self.y.midpoint(other.y));
        Self {
            x: mid.x,
            y: mid.y,
            flags: other.flags,
        }
    }
}

/// Generates a path from an iterator of contour points.
///
/// Note that this requires the last point of the contour to be passed
/// separately to support FreeType style path conversion when the contour
/// begins with an off curve point. The points iterator should still
/// yield the last point as well.
///
/// This is more general than [`to_path`] and exists to support cases (such as
/// autohinting) where the source outline data is in a different format.
pub(crate) fn contour_to_path<C: PointCoord>(
    points: impl Iterator<Item = ContourPoint<C>>,
    last_point: ContourPoint<C>,
    style: PathStyle,
    pen: &mut impl OutlinePen,
) -> Result<(), ToPathError> {
    let mut points = points.enumerate().peekable();
    let Some((_, first_point)) = points.peek().copied() else {
        // This is an empty contour
        return Ok(());
    };
    // We don't accept an off curve cubic as the first point
    if first_point.flags.is_off_curve_cubic() {
        return Err(ToPathError::ExpectedQuadOrOnCurve(0));
    }
    // For FreeType style, we may need to omit the last point if we find the
    // first on curve there
    let mut omit_last = false;
    // For HarfBuzz style, may skip up to two points in finding the start, so
    // process these at the end
    let mut trailing_points = [None; 2];
    // Find our starting point
    let start_point = if first_point.flags.is_off_curve_quad() {
        // We're starting with an off curve, so select our first move based on
        // the path style
        match style {
            PathStyle::FreeType => {
                if last_point.flags.is_on_curve() {
                    // The last point is an on curve, so let's start there
                    omit_last = true;
                    last_point
                } else {
                    // It's also an off curve, so take implicit midpoint
                    last_point.midpoint(&first_point)
                }
            }
            PathStyle::HarfBuzz => {
                // Always consume the first point
                points.next();
                // Then check the next point
                let Some((_, next_point)) = points.peek().copied() else {
                    // This is a single point contour
                    return Ok(());
                };
                if next_point.flags.is_on_curve() {
                    points.next();
                    trailing_points = [Some((0, first_point)), Some((1, next_point))];
                    // Next is on curve, so let's start there
                    next_point
                } else {
                    // It's also an off curve, so take the implicit midpoint
                    trailing_points = [Some((0, first_point)), None];
                    first_point.midpoint(&next_point)
                }
            }
        }
    } else {
        // We're starting with an on curve, so consume the point
        points.next();
        first_point
    };
    let point = start_point.point_f32();
    pen.move_to(point.x, point.y);
    let mut emitter = ContourPathEmitter::default();
    while let Some((ix, point)) = points.next() {
        if omit_last && points.peek().is_none() {
            break;
        }
        emitter.emit(ix, point, pen)?;
    }
    for (ix, point) in trailing_points.iter().filter_map(|x| *x) {
        emitter.emit(ix, point, pen)?;
    }
    emitter.finish(start_point, pen)?;
    Ok(())
}

#[derive(Default)]
struct ContourPathEmitter<C> {
    pending: [(usize, ContourPoint<C>); 2],
    num_pending: usize,
}

impl<C: PointCoord> ContourPathEmitter<C> {
    fn emit(
        &mut self,
        ix: usize,
        point: ContourPoint<C>,
        pen: &mut impl OutlinePen,
    ) -> Result<(), ToPathError> {
        if point.flags.is_off_curve_quad() {
            // Quads can have 0 or 1 buffered point. If there is one, draw a
            // quad to the implicit oncurve
            if self.num_pending > 0 {
                self.quad_to(point, OnCurve::Implicit, pen)?;
            }
            self.push_off_curve((ix, point));
        } else if point.flags.is_off_curve_cubic() {
            // If this is the third consecutive cubic off-curve there's an
            // implicit oncurve between the last two
            if self.num_pending > 1 {
                self.cubic_to(point, OnCurve::Implicit, pen)?;
            }
            self.push_off_curve((ix, point));
        } else if point.flags.is_on_curve() {
            // A real on curve! What to draw depends on how many off curves are
            // pending
            self.flush(point, pen)?;
        }
        Ok(())
    }

    fn finish(
        mut self,
        start_point: ContourPoint<C>,
        pen: &mut impl OutlinePen,
    ) -> Result<(), ToPathError> {
        if self.num_pending != 0 {
            self.flush(start_point, pen)?;
        }
        pen.close();
        Ok(())
    }

    fn push_off_curve(&mut self, off_curve: (usize, ContourPoint<C>)) {
        self.pending[self.num_pending] = off_curve;
        self.num_pending += 1;
    }

    fn quad_to(
        &mut self,
        curr: ContourPoint<C>,
        oncurve: OnCurve,
        pen: &mut impl OutlinePen,
    ) -> Result<(), ToPathError> {
        let (ix, c0) = self.pending[0];
        if !c0.flags.is_off_curve_quad() {
            return Err(ToPathError::ExpectedQuad(ix));
        }
        let end = oncurve.endpoint(c0, curr);
        let c0 = c0.point_f32();
        pen.quad_to(c0.x, c0.y, end.x, end.y);
        self.num_pending = 0;
        Ok(())
    }

    fn cubic_to(
        &mut self,
        curr: ContourPoint<C>,
        oncurve: OnCurve,
        pen: &mut impl OutlinePen,
    ) -> Result<(), ToPathError> {
        let (ix0, c0) = self.pending[0];
        if !c0.flags.is_off_curve_cubic() {
            return Err(ToPathError::ExpectedQuad(ix0));
        }
        let (ix1, c1) = self.pending[1];
        if !c1.flags.is_off_curve_cubic() {
            return Err(ToPathError::ExpectedCubic(ix1));
        }
        let c0 = c0.point_f32();
        let end = oncurve.endpoint(c1, curr);
        let c1 = c1.point_f32();
        pen.curve_to(c0.x, c0.y, c1.x, c1.y, end.x, end.y);
        self.num_pending = 0;
        Ok(())
    }

    fn flush(
        &mut self,
        curr: ContourPoint<C>,
        pen: &mut impl OutlinePen,
    ) -> Result<(), ToPathError> {
        match self.num_pending {
            2 => self.cubic_to(curr, OnCurve::Explicit, pen)?,
            1 => self.quad_to(curr, OnCurve::Explicit, pen)?,
            _ => {
                // only zero should match and that's a line
                debug_assert!(self.num_pending == 0);
                let curr = curr.point_f32();
                pen.line_to(curr.x, curr.y);
            }
        }
        Ok(())
    }
}

#[derive(Debug, Copy, Clone)]
enum OnCurve {
    Implicit,
    Explicit,
}

impl OnCurve {
    #[inline(always)]
    fn endpoint<C: PointCoord>(
        &self,
        last_offcurve: ContourPoint<C>,
        current: ContourPoint<C>,
    ) -> Point<f32> {
        match self {
            OnCurve::Implicit => last_offcurve.midpoint(&current).point_f32(),
            OnCurve::Explicit => current.point_f32(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{super::pen::SvgPen, *};
    use raw::types::F26Dot6;

    fn assert_off_curve_path_to_svg(expected: &str, path_style: PathStyle, all_off_curve: bool) {
        fn pt(x: i32, y: i32) -> Point<F26Dot6> {
            Point::new(x, y).map(F26Dot6::from_bits)
        }
        let mut flags = [PointFlags::off_curve_quad(); 4];
        if !all_off_curve {
            flags[1] = PointFlags::on_curve();
        }
        let contours = [3];
        // This test is meant to prevent a bug where the first move-to was computed improperly
        // for a contour consisting of all off curve points.
        // In this case, the start of the path should be the midpoint between the first and last points.
        // For this test case (in 26.6 fixed point): [(640, 128) + (128, 128)] / 2 = (384, 128)
        // which becomes (6.0, 2.0) when converted to floating point.
        let points = [pt(640, 128), pt(256, 64), pt(640, 64), pt(128, 128)];
        let mut pen = SvgPen::with_precision(1);
        to_path(&points, &flags, &contours, path_style, &mut pen).unwrap();
        assert_eq!(pen.as_ref(), expected);
    }

    #[test]
    fn all_off_curve_to_path_scan_backward() {
        assert_off_curve_path_to_svg(
            "M6.0,2.0 Q10.0,2.0 7.0,1.5 Q4.0,1.0 7.0,1.0 Q10.0,1.0 6.0,1.5 Q2.0,2.0 6.0,2.0 Z",
            PathStyle::FreeType,
            true,
        );
    }

    #[test]
    fn all_off_curve_to_path_scan_forward() {
        assert_off_curve_path_to_svg(
            "M7.0,1.5 Q4.0,1.0 7.0,1.0 Q10.0,1.0 6.0,1.5 Q2.0,2.0 6.0,2.0 Q10.0,2.0 7.0,1.5 Z",
            PathStyle::HarfBuzz,
            true,
        );
    }

    #[test]
    fn start_off_curve_to_path_scan_backward() {
        assert_off_curve_path_to_svg(
            "M6.0,2.0 Q10.0,2.0 4.0,1.0 Q10.0,1.0 6.0,1.5 Q2.0,2.0 6.0,2.0 Z",
            PathStyle::FreeType,
            false,
        );
    }

    #[test]
    fn start_off_curve_to_path_scan_forward() {
        assert_off_curve_path_to_svg(
            "M4.0,1.0 Q10.0,1.0 6.0,1.5 Q2.0,2.0 6.0,2.0 Q10.0,2.0 4.0,1.0 Z",
            PathStyle::HarfBuzz,
            false,
        );
    }
}
