//! A loaded outline that borrows the caller's buffers.

use super::{outline_to_path, PathContourStart};
use crate::{
    model::glyph::outline::OutlinePen,
    tables::glyf::{PointCoord, PointFlags, PHANTOM_POINT_COUNT},
    types::Point,
};

/// A loaded outline that borrows the caller's buffers.
///
/// Has the same accessors as [`Outline`](super::Outline).
#[derive(Clone, Debug)]
pub struct OutlineRef<'a, C: PointCoord> {
    pub(super) points: &'a [Point<C>],
    pub(super) flags: &'a [PointFlags],
    pub(super) contours: &'a [u16],
    pub(super) phantom: [Point<C>; PHANTOM_POINT_COUNT],
}

impl<C: PointCoord> OutlineRef<'_, C> {
    /// The loaded points, excluding phantom points.
    pub fn points(&self) -> &[Point<C>] {
        self.points
    }

    /// The flag for each point in [`Self::points`].
    pub fn flags(&self) -> &[PointFlags] {
        self.flags
    }

    /// The index of the last point of each contour.
    pub fn contours(&self) -> &[u16] {
        self.contours
    }

    /// The four phantom points, after scaling, variation and hinting.
    pub fn phantom_points(&self) -> &[Point<C>; PHANTOM_POINT_COUNT] {
        &self.phantom
    }

    /// The left side bearing after scaling, variation and hinting.
    pub fn adjusted_lsb(&self) -> C {
        self.phantom[0].x
    }

    /// The advance width after scaling, variation and hinting.
    pub fn adjusted_advance_width(&self) -> C {
        self.phantom[1].x - self.phantom[0].x
    }

    /// Converts the outline to path commands, calling `pen` for each.
    ///
    /// `start` decides where a contour begins when its first point is
    /// off-curve. The point stream leaves that open.
    ///
    /// Returns `false` if the points do not describe a path, on the same terms
    /// as [`outline_to_path`].
    #[must_use]
    pub fn to_path(&self, start: PathContourStart, pen: &mut impl OutlinePen) -> bool {
        outline_to_path(self.points, self.flags, self.contours, start, pen)
    }
}
