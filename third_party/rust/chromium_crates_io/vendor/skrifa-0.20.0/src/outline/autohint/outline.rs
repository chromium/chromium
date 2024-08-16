//! Outline representation and helpers for autohinting.

use super::{
    super::{
        unscaled::{UnscaledOutlineSink, UnscaledPoint},
        DrawError, LocationRef, OutlineGlyph,
    },
    cycling::IndexCycler,
};
use crate::collections::SmallVec;

/// Hinting directions.
///
/// The values are such that `dir1 + dir2 == 0` when the directions are
/// opposite.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.h#L45>
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
#[repr(i8)]
pub(super) enum Direction {
    #[default]
    None = 4,
    Right = 1,
    Left = -1,
    Up = 2,
    Down = -2,
}

impl Direction {
    /// Computes a direction from a vector.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.c#L751>
    pub fn new(dx: i32, dy: i32) -> Self {
        let (dir, long_arm, short_arm) = if dy >= dx {
            if dy >= -dx {
                (Direction::Up, dy, dx)
            } else {
                (Direction::Left, -dx, dy)
            }
        } else if dy >= -dx {
            (Direction::Right, dx, dy)
        } else {
            (Direction::Down, -dy, dx)
        };
        // Return no direction if arm lengths do not differ enough.
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.c#L789>
        if long_arm <= 14 * short_arm.abs() {
            Direction::None
        } else {
            dir
        }
    }

    pub fn is_opposite(self, other: Self) -> bool {
        self as i8 + other as i8 == 0
    }

    pub fn is_same_axis(self, other: Self) -> bool {
        (self as i8).abs() == (other as i8).abs()
    }
}

/// Outline point with a lot of context for hinting.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.h#L239>
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub(super) struct Point {
    /// Describes the type and hinting state of the point.
    pub flags: u8,
    /// X coordinate in font units.
    pub fx: i32,
    /// Y coordinate in font units.
    pub fy: i32,
    /// Direction of inwards vector.
    pub in_dir: Direction,
    /// Direction of outwards vector.
    pub out_dir: Direction,
    /// Context dependent coordinate.
    pub u: i32,
    /// Context dependent coordinate.
    pub v: i32,
}

/// Point type flags.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.h#L210>
impl Point {
    /// Quadratic control point.
    pub const QUAD: u8 = 1 << 0;
    /// Cubic control point.
    pub const CUBIC: u8 = 1 << 1;
    /// Any control point.
    pub const CONTROL: u8 = Self::QUAD | Self::CUBIC;
    /// Touched in x direction.
    pub const TOUCH_X: u8 = 1 << 2;
    /// Touched in y direction.
    pub const TOUCH_Y: u8 = 1 << 3;
    /// Candidate for weak intepolation.
    pub const WEAK_INTERPOLATION: u8 = 1 << 4;
    /// Distance to next point is very small.
    pub const NEAR: u8 = 1 << 5;
}

// Matches FreeType's inline usage
//
// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.h#L332>
const MAX_INLINE_POINTS: usize = 96;
const MAX_INLINE_CONTOURS: usize = 8;

#[derive(Default)]
pub(super) struct Outline {
    pub points: SmallVec<Point, MAX_INLINE_POINTS>,
    // Range isn't Copy so can't be used in our SmallVec :(
    pub contours: SmallVec<(usize, usize), MAX_INLINE_CONTOURS>,
}

impl Outline {
    /// Fills the outline from the given glyph.
    pub fn fill(&mut self, glyph: &OutlineGlyph) -> Result<(), DrawError> {
        self.clear();
        glyph.draw_unscaled(LocationRef::default(), None, self)?;
        // Heuristic value
        let near_limit = 20 * glyph.units_per_em() as i32 / 2048;
        self.mark_near_points(near_limit);
        self.compute_directions(near_limit);
        self.simplify_topology();
        self.check_remaining_weak_points();
        Ok(())
    }

    pub fn clear(&mut self) {
        self.points.clear();
        self.contours.clear();
    }

    pub fn contours_mut(&mut self) -> impl Iterator<Item = &mut [Point]> {
        let mut points = Some(self.points.as_mut_slice());
        let mut consumed = 0;
        self.contours.iter().map(move |(_, end)| {
            let count = end - consumed;
            consumed = *end;
            let (contour_points, rest) = points.take().unwrap().split_at_mut(count);
            points = Some(rest);
            contour_points
        })
    }
}

impl Outline {
    /// Computes the near flag for each contour.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.c#L1017>
    fn mark_near_points(&mut self, near_limit: i32) {
        for points in self.contours_mut() {
            if points.is_empty() {
                continue;
            }
            let mut prev_ix = points.len() - 1;
            let mut ix = 0;
            while ix < points.len() {
                let point = points[ix];
                let prev = &mut points[prev_ix];
                // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.c#L1017>
                let out_x = point.fx - prev.fx;
                let out_y = point.fy - prev.fy;
                if out_x.abs() + out_y.abs() < near_limit {
                    prev.flags |= Point::NEAR;
                }
                prev_ix = ix;
                ix += 1;
            }
        }
    }

    /// Compute directions of in and out vectors.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.c#L1064>
    fn compute_directions(&mut self, near_limit: i32) {
        let near_limit2 = 2 * near_limit - 1;
        for points in self.contours_mut() {
            let Some(cycler) = IndexCycler::new(points.len()) else {
                continue;
            };
            // Walk backward to find the first non-near point.
            let mut first_ix = 0;
            let mut prev_ix = cycler.prev(first_ix);
            let mut point = points[0];
            while prev_ix != 0 {
                let prev = points[prev_ix];
                let out_x = point.fx - prev.fx;
                let out_y = point.fy - prev.fy;
                // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.c#L1102>
                if out_x.abs() + out_y.abs() >= near_limit2 {
                    break;
                }
                point = prev;
                first_ix = prev_ix;
                prev_ix = cycler.prev(prev_ix);
            }
            // Abuse u and v fields to store deltas to the next and previous
            // non-near points, respectively.
            let first = &mut points[first_ix];
            first.u = first_ix as _;
            first.v = first_ix as _;
            let mut next_ix = first_ix;
            let mut ix = first_ix;
            // Now loop over all points in the contour to compute in and
            // out directions
            let mut out_x = 0;
            let mut out_y = 0;
            loop {
                let point_ix = next_ix;
                next_ix = cycler.next(point_ix);
                let point = points[point_ix];
                let next = &mut points[next_ix];
                // Accumulate the deltas until we surpass near_limit
                out_x += next.fx - point.fx;
                out_y += next.fy - point.fy;
                if out_x.abs() + out_y.abs() < near_limit {
                    next.flags |= Point::WEAK_INTERPOLATION;
                    // The original code is a do-while loop, so make
                    // sure we keep this condition before the continue
                    if next_ix == first_ix {
                        break;
                    }
                    continue;
                }
                let out_dir = Direction::new(out_x, out_y);
                next.in_dir = out_dir;
                next.v = ix as _;
                let cur = &mut points[ix];
                cur.u = next_ix as _;
                cur.out_dir = out_dir;
                // Adjust directions for all intermediate points
                let mut inter_ix = cycler.next(ix);
                while inter_ix != next_ix {
                    let point = &mut points[inter_ix];
                    point.in_dir = out_dir;
                    point.out_dir = out_dir;
                    inter_ix = cycler.next(inter_ix);
                }
                ix = next_ix;
                points[ix].u = first_ix as _;
                points[first_ix].v = ix as _;
                out_x = 0;
                out_y = 0;
                if next_ix == first_ix {
                    break;
                }
            }
        }
    }

    /// Simplify so that we can identify local extrema more reliably.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.c#L1181>
    fn simplify_topology(&mut self) {
        for points in self.contours_mut() {
            for i in 0..points.len() {
                let point = points[i];
                if point.in_dir == Direction::None && point.out_dir == Direction::None {
                    let u_index = point.u as usize;
                    let v_index = point.v as usize;
                    let next_u = points[u_index];
                    let prev_v = points[v_index];
                    let in_x = point.fx - prev_v.fx;
                    let in_y = point.fy - prev_v.fy;
                    let out_x = next_u.fx - point.fx;
                    let out_y = next_u.fy - point.fy;
                    if (in_x ^ out_x) >= 0 || (in_y ^ out_y) >= 0 {
                        // Both vectors point into the same quadrant
                        points[i].flags |= Point::WEAK_INTERPOLATION;
                        points[v_index].u = u_index as _;
                        points[u_index].v = v_index as _;
                    }
                }
            }
        }
    }

    /// Check for remaining weak points.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.c#L1226>
    fn check_remaining_weak_points(&mut self) {
        for points in self.contours_mut() {
            for i in 0..points.len() {
                let point = points[i];
                let mut make_weak = false;
                if point.flags & Point::WEAK_INTERPOLATION != 0 {
                    // Already weak
                    continue;
                }
                if point.flags & Point::CONTROL != 0 {
                    // Control points are always weak
                    make_weak = true;
                } else if point.out_dir == point.in_dir {
                    if point.out_dir != Direction::None {
                        // Point lies on a vertical or horizontal segment but
                        // not at start or end
                        make_weak = true;
                    } else {
                        let u_index = point.u as usize;
                        let v_index = point.v as usize;
                        let next_u = points[u_index];
                        let prev_v = points[v_index];
                        if is_corner_flat(
                            point.fx - prev_v.fx,
                            point.fy - prev_v.fy,
                            next_u.fx - point.fx,
                            next_u.fy - point.fy,
                        ) {
                            // One of the vectors is more dominant
                            make_weak = true;
                            points[v_index].u = u_index as _;
                            points[u_index].v = v_index as _;
                        }
                    }
                } else if point.in_dir.is_opposite(point.out_dir) {
                    // Point forms a "spike"
                    make_weak = true;
                }
                if make_weak {
                    points[i].flags |= Point::WEAK_INTERPOLATION;
                }
            }
        }
    }
}

/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/base/ftcalc.c#L1026>
fn is_corner_flat(in_x: i32, in_y: i32, out_x: i32, out_y: i32) -> bool {
    let ax = in_x + out_x;
    let ay = in_y + out_y;
    fn hypot(x: i32, y: i32) -> i32 {
        let x = x.abs();
        let y = y.abs();
        if x > y {
            x + ((3 * y) >> 3)
        } else {
            y + ((3 * x) >> 3)
        }
    }
    let d_in = hypot(in_x, in_y);
    let d_out = hypot(out_x, out_y);
    let d_hypot = hypot(ax, ay);
    (d_in + d_out - d_hypot) < (d_hypot >> 4)
}

impl UnscaledOutlineSink for Outline {
    fn try_reserve(&mut self, additional: usize) -> Result<(), DrawError> {
        if self.points.try_reserve(additional) {
            Ok(())
        } else {
            Err(DrawError::InsufficientMemory)
        }
    }

    fn push(&mut self, point: UnscaledPoint) -> Result<(), DrawError> {
        let flags = if point.is_off_curve_quad() {
            Point::QUAD
        } else if point.is_off_curve_cubic() {
            Point::CUBIC
        } else {
            0
        };
        let new_point = Point {
            flags,
            fx: point.x as i32,
            fy: point.y as i32,
            ..Default::default()
        };
        let new_point_ix = self.points.len();
        if point.is_contour_start() {
            self.contours.push((new_point_ix, new_point_ix + 1));
        } else if let Some(last_contour) = self.contours.last_mut() {
            last_contour.1 += 1;
        } else {
            // If our first point is not marked as contour start, just
            // create a new contour.
            self.contours.push((new_point_ix, new_point_ix + 1));
        }
        self.points.push(new_point);
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::Outline;
    use super::*;
    use crate::MetadataProvider;
    use raw::{types::GlyphId, FontRef};

    #[test]
    fn direction_from_vectors() {
        assert_eq!(Direction::new(-100, 0), Direction::Left);
        assert_eq!(Direction::new(100, 0), Direction::Right);
        assert_eq!(Direction::new(0, -100), Direction::Down);
        assert_eq!(Direction::new(0, 100), Direction::Up);
        assert_eq!(Direction::new(7, 100), Direction::Up);
        // This triggers the too close heuristic
        assert_eq!(Direction::new(8, 100), Direction::None);
    }

    #[test]
    fn direction_axes() {
        use Direction::*;
        let hori = [Left, Right];
        let vert = [Up, Down];
        for h in hori {
            for h2 in hori {
                assert!(h.is_same_axis(h2));
                if h != h2 {
                    assert!(h.is_opposite(h2));
                } else {
                    assert!(!h.is_opposite(h2));
                }
            }
            for v in vert {
                assert!(!h.is_same_axis(v));
                assert!(!h.is_opposite(v));
            }
        }
        for v in vert {
            for v2 in vert {
                assert!(v.is_same_axis(v2));
                if v != v2 {
                    assert!(v.is_opposite(v2));
                } else {
                    assert!(!v.is_opposite(v2));
                }
            }
        }
    }

    #[test]
    fn fill_outline() {
        let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
        let glyphs = font.outline_glyphs();
        let glyph = glyphs.get(GlyphId::new(8)).unwrap();
        let mut outline = Outline::default();
        outline.fill(&glyph).unwrap();
        use Direction::*;
        let expected = &[
            // (x, y, in_dir, out_dir, flags)
            (107, 0, Left, Left, 16),
            (85, 0, Left, None, 17),
            (55, 26, None, Up, 17),
            (55, 71, Up, Up, 16),
            (55, 332, Up, Up, 16),
            (55, 360, Up, None, 17),
            (67, 411, None, None, 17),
            (93, 459, None, None, 17),
            (112, 481, None, Up, 0),
            (112, 504, Up, Right, 0),
            (168, 504, Right, Down, 0),
            (168, 483, Down, None, 0),
            (153, 473, None, None, 17),
            (126, 428, None, None, 17),
            (109, 366, None, Down, 17),
            (109, 332, Down, Down, 16),
            (109, 109, Down, Right, 0),
            (407, 109, Right, Right, 16),
            (427, 109, Right, None, 17),
            (446, 136, None, None, 17),
            (453, 169, None, Up, 17),
            (453, 178, Up, Up, 16),
            (453, 374, Up, Up, 16),
            (453, 432, Up, None, 17),
            (400, 483, None, Left, 17),
            (362, 483, Left, Left, 16),
            (109, 483, Left, Left, 16),
            (86, 483, Left, None, 17),
            (62, 517, None, Up, 17),
            (62, 555, Up, Up, 16),
            (62, 566, Up, None, 17),
            (64, 587, None, None, 17),
            (71, 619, None, None, 17),
            (76, 647, None, Right, 0),
            (103, 647, Right, Down, 32),
            (103, 644, Down, Down, 16),
            (103, 619, Down, None, 17),
            (131, 592, None, Right, 17),
            (155, 592, Right, Right, 16),
            (386, 592, Right, Right, 16),
            (437, 592, Right, None, 17),
            (489, 552, None, None, 17),
            (507, 485, None, Down, 17),
            (507, 443, Down, Down, 16),
            (507, 75, Down, Down, 16),
            (507, 40, Down, None, 17),
            (470, 0, None, Left, 17),
            (436, 0, Left, Left, 16),
        ];
        let points = outline
            .points
            .iter()
            .map(|point| {
                (
                    point.fx,
                    point.fy,
                    point.in_dir,
                    point.out_dir,
                    point.flags as i32,
                )
            })
            .collect::<Vec<_>>();
        assert_eq!(&points, expected);
    }
}
