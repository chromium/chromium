//! Topology analysis of segments and edges.

mod edges;
mod segments;

use super::{
    metrics::ScaledWidth,
    outline::{Direction, Orientation, Point},
};
use crate::collections::SmallVec;

pub(crate) use edges::{compute_blue_edges, compute_edges};
pub(crate) use segments::{compute_segments, link_segments};

/// Source for an alignment zone.
#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
pub struct BlueProvenance {
    /// Index of the blue in the associated metrics.
    pub index: u16,
    /// Was the blue an overshoot?
    pub is_shoot: bool,
}

/// Maximum number of segments and edges stored inline.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.h#L306>
const MAX_INLINE_SEGMENTS: usize = 18;
const MAX_INLINE_EDGES: usize = 12;

/// The dimension of an axis.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub enum Dimension {
    /// Metrics and geometry in the horizontal direction.
    #[default]
    Horizontal = 0,
    /// Metrics and geometry in the vertical direction.
    Vertical = 1,
}

impl<T> core::ops::Index<Dimension> for [T] {
    type Output = T;

    fn index(&self, index: Dimension) -> &Self::Output {
        &self[index as usize]
    }
}

/// Segments and edges for one dimension of an outline.
///
// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.h#L309>
#[derive(Clone, Default, Debug)]
pub struct Axis {
    /// Either horizontal or vertical.
    pub(crate) dim: Dimension,
    /// Depends on dimension and outline orientation.
    pub(crate) major_dir: Direction,
    /// Collection of segments for the axis.
    pub(crate) segments: SmallVec<Segment, MAX_INLINE_SEGMENTS>,
    /// Collection of edges for the axis.
    pub(crate) edges: SmallVec<Edge, MAX_INLINE_EDGES>,
}

impl Axis {
    /// Returns the dimension of the axis.
    pub fn dimension(&self) -> Dimension {
        self.dim
    }

    /// Returns the dominant direction, depending on dimension and the
    /// orientation of the outline.
    pub fn major_direction(&self) -> Direction {
        self.major_dir
    }

    /// Returns the collection of computed segments.
    pub fn segments(&self) -> &[Segment] {
        self.segments.as_slice()
    }

    /// Returns the collection of computed edges.
    pub fn edges(&self) -> &[Edge] {
        self.edges.as_slice()
    }
}

impl Axis {
    #[cfg(test)]
    pub(crate) fn new(dim: Dimension, orientation: Option<Orientation>) -> Self {
        let mut axis = Self::default();
        axis.reset(dim, orientation);
        axis
    }

    pub(crate) fn reset(&mut self, dim: Dimension, orientation: Option<Orientation>) {
        self.dim = dim;
        self.major_dir = match (dim, orientation) {
            (Dimension::Horizontal, Some(Orientation::Clockwise)) => Direction::Down,
            (Dimension::Vertical, Some(Orientation::Clockwise)) => Direction::Right,
            (Dimension::Horizontal, _) => Direction::Up,
            (Dimension::Vertical, _) => Direction::Left,
        };
        self.segments.clear();
        self.edges.clear();
    }

    /// Inserts the given edge into the sorted edge list.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.c#L197>
    pub(crate) fn insert_edge(&mut self, edge: Edge, top_to_bottom_hinting: bool) {
        self.edges.push(edge);
        let edges = self.edges.as_mut_slice();
        // If this is the first edge, we're done.
        if edges.len() == 1 {
            return;
        }
        // Now move it into place
        let mut ix = edges.len() - 1;
        while ix > 0 {
            let prev_ix = ix - 1;
            let prev_fpos = edges[prev_ix].fpos;
            if (top_to_bottom_hinting && prev_fpos > edge.fpos)
                || (!top_to_bottom_hinting && prev_fpos < edge.fpos)
            {
                break;
            }
            // Edges with the same position and minor direction should appear
            // before those with the major direction
            if prev_fpos == edge.fpos && edge.dir == self.major_dir {
                break;
            }
            let prev_edge = edges[prev_ix];
            edges[ix] = prev_edge;
            ix -= 1;
        }
        edges[ix] = edge;
    }

    /// Links the given segment and edge.
    pub(crate) fn append_segment_to_edge(&mut self, segment_ix: usize, edge_ix: usize) {
        let edge = &mut self.edges[edge_ix];
        let first_ix = edge.first_ix;
        let last_ix = edge.last_ix;
        edge.last_ix = segment_ix as u16;
        let segment = &mut self.segments[segment_ix];
        segment.edge_next_ix = Some(first_ix);
        self.segments[last_ix as usize].edge_next_ix = Some(segment_ix as u16);
    }
}

/// Flags that define the properties of segments and edges.
///
// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.h#L227>
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub struct TopoFlags(pub(crate) u8);

impl TopoFlags {
    /// Regular segment or edge.
    pub const NORMAL: Self = Self(0);
    /// Segment or edge has rounded geometry.
    pub const ROUND: Self = Self(1);
    /// Segment or edge represents a serif.
    pub const SERIF: Self = Self(2);
    /// Segment or edge has been successfully processed.
    pub const DONE: Self = Self(4);
    /// Segment or edge aligns to a neutral blue zone.
    pub const NEUTRAL: Self = Self(8);
}

impl TopoFlags {
    /// Creates new flags, truncating the given bits to valid values.
    pub const fn from_bits_truncate(bits: u8) -> Self {
        Self(bits & 0b1111)
    }

    /// Returns the underlying flag bits.
    pub const fn to_bits(self) -> u8 {
        self.0
    }

    /// Returns true if `self` contains all flags in `other`.
    pub const fn contains(self, other: Self) -> bool {
        (self.0 & other.0) == other.0
    }

    /// Returns true if `self` contains any flags in `other`.
    pub const fn intersects(self, other: Self) -> bool {
        (self.0 & other.0) != 0
    }
}

impl core::ops::Not for TopoFlags {
    type Output = Self;

    fn not(self) -> Self::Output {
        Self(!self.0)
    }
}

impl core::ops::BitOr for TopoFlags {
    type Output = Self;

    fn bitor(self, rhs: Self) -> Self::Output {
        Self(self.0 | rhs.0)
    }
}

impl core::ops::BitOrAssign for TopoFlags {
    fn bitor_assign(&mut self, rhs: Self) {
        self.0 |= rhs.0;
    }
}

impl core::ops::BitAnd for TopoFlags {
    type Output = Self;

    fn bitand(self, rhs: Self) -> Self::Output {
        Self(self.0 & rhs.0)
    }
}

impl core::ops::BitAndAssign for TopoFlags {
    fn bitand_assign(&mut self, rhs: Self) {
        self.0 &= rhs.0;
    }
}

/// Sequence of points with a single dominant direction.
///
// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.h#L262>
#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
pub struct Segment {
    /// Flags describing the properties of the segment.
    pub(crate) flags: TopoFlags,
    /// Dominant direction of the segment.
    pub(crate) dir: Direction,
    /// Position of the segment.
    pub(crate) pos: i16,
    /// Deviation from segment position.
    pub(crate) delta: i16,
    /// Minimum coordinate of the segment.
    pub(crate) min_coord: i16,
    /// Maximum coordinate of the segment.
    pub(crate) max_coord: i16,
    /// Hinted segment height.
    pub(crate) height: i16,
    /// Used during stem matching.
    pub(crate) score: i32,
    /// Used during stem matching.
    pub(crate) len: i32,
    /// Index of best candidate for a stem link.
    pub(crate) link_ix: Option<u16>,
    /// Index of best candidate for a serif link.
    pub(crate) serif_ix: Option<u16>,
    /// Index of first point in the outline.
    pub(crate) first_ix: u16,
    /// Index of last point in the outline.
    pub(crate) last_ix: u16,
    /// Index of edge that is associated with the segment.
    pub(crate) edge_ix: Option<u16>,
    /// Index of next segment in edge's segment list.
    pub(crate) edge_next_ix: Option<u16>,
}

impl Segment {
    /// Returns flags describing the properties of the segment.
    pub fn flags(&self) -> TopoFlags {
        self.flags
    }

    /// Returns the dominant direction of the segment.
    pub fn direction(&self) -> Direction {
        self.dir
    }

    /// Returns the computed position of the segment.
    pub fn position(&self) -> i16 {
        self.pos
    }

    /// Returns the deviation from the computed position.
    pub fn delta(&self) -> i16 {
        self.delta
    }

    /// Returns the minimum coordinate of the segment.
    pub fn min_coord(&self) -> i16 {
        self.min_coord
    }

    /// Returns the maximum coordinate of the segment.
    pub fn max_coord(&self) -> i16 {
        self.max_coord
    }

    /// Returns the hinted height of the segment.
    pub fn height(&self) -> i16 {
        self.height
    }

    /// Returns the computed score of the segment; used during stem matching.
    pub fn score(&self) -> i32 {
        self.score
    }

    /// Returns the computed length of the segment; used during stem matching.
    pub fn length(&self) -> i32 {
        self.len
    }

    /// Returns the index of the best candidate for a stem link.
    pub fn link_index(&self) -> Option<u16> {
        self.link_ix
    }

    /// Returns the index of the best candidate for a serif link.
    pub fn serif_index(&self) -> Option<u16> {
        self.serif_ix
    }

    /// Returns the indices of first and last points that define the segment.
    pub fn point_indices(&self) -> (u16, u16) {
        (self.first_ix, self.last_ix)
    }

    /// Returns the index of edge that is associated with the segment.
    pub fn edge_index(&self) -> Option<u16> {
        self.edge_ix
    }

    /// Returns the index of next segment in the associated edge's segment
    /// list.
    pub fn next_in_edge_index(&self) -> Option<u16> {
        self.edge_next_ix
    }
}

impl Segment {
    pub(crate) fn first(&self) -> usize {
        self.first_ix as usize
    }

    pub(crate) fn first_point<'a>(&self, points: &'a [Point]) -> &'a Point {
        &points[self.first()]
    }

    pub(crate) fn last(&self) -> usize {
        self.last_ix as usize
    }

    pub(crate) fn last_point<'a>(&self, points: &'a [Point]) -> &'a Point {
        &points[self.last()]
    }

    pub(crate) fn edge<'a>(&self, edges: &'a [Edge]) -> Option<&'a Edge> {
        edges.get(self.edge_ix.map(|ix| ix as usize)?)
    }

    /// Returns the next segment in this segment's parent edge.
    pub(crate) fn next_in_edge<'a>(&self, segments: &'a [Segment]) -> Option<&'a Segment> {
        segments.get(self.edge_next_ix.map(|ix| ix as usize)?)
    }

    pub(crate) fn link<'a>(&self, segments: &'a [Segment]) -> Option<&'a Segment> {
        segments.get(self.link_ix.map(|ix| ix as usize)?)
    }
}

/// Sequence of segments used for grid-fitting.
///
// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afhints.h#L286>
#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
pub struct Edge {
    /// Original, unscaled position in font units.
    pub(crate) fpos: i16,
    /// Original, scaled position.
    pub(crate) opos: i32,
    /// Current position.
    pub(crate) pos: i32,
    /// Edge flags.
    pub(crate) flags: TopoFlags,
    /// Edge direction.
    pub(crate) dir: Direction,
    /// Present if this is a blue edge.
    pub(crate) blue_edge: Option<ScaledWidth>,
    /// Retains which blue zone was selected and whether the overshoot
    /// position won so recorders can reproduce CVT references later.
    pub(crate) blue_provenance: Option<BlueProvenance>,
    /// Index of linked edge.
    pub(crate) link_ix: Option<u16>,
    /// Index of primary edge for serif.
    pub(crate) serif_ix: Option<u16>,
    /// Used to speed up edge interpolation.
    pub(crate) scale: i32,
    /// Index of first segment in edge.
    pub(crate) first_ix: u16,
    /// Index of last segment in edge.
    pub(crate) last_ix: u16,
}

impl Edge {
    /// Returns the original unscaled position in font units.
    pub fn original_position(&self) -> i16 {
        self.fpos
    }

    /// Returns the original scaled position.
    pub fn scaled_position(&self) -> i32 {
        self.opos
    }

    /// Returns the hinted position.
    pub fn position(&self) -> i32 {
        self.pos
    }

    /// Returns flags that define the properties of the edge.
    pub fn flags(&self) -> TopoFlags {
        self.flags
    }

    /// Returns the dominant direction of the edge.
    pub fn direction(&self) -> Direction {
        self.dir
    }

    /// Returns the width of the captured blue zone.
    pub fn blue_edge(&self) -> Option<ScaledWidth> {
        self.blue_edge
    }

    /// Returns which blue zone was selected and whether the overshoot
    /// position won so recorders can reproduce CVT references later.    
    pub fn blue_provenance(&self) -> Option<BlueProvenance> {
        self.blue_provenance
    }

    /// Returns the index of the linked edge.
    pub fn link_index(&self) -> Option<u16> {
        self.link_ix
    }

    /// Returns the index of the associated serif edge.
    pub fn serif_index(&self) -> Option<u16> {
        self.serif_ix
    }

    /// Returns the computed scale factor of the edge.
    pub fn scale(&self) -> i32 {
        self.scale
    }

    /// Returns the indices of the first and last segments that define the
    /// edge.
    ///
    /// Use [`Segment::next_in_edge_index`] to walk the segment list.
    pub fn segment_indices(&self) -> (u16, u16) {
        (self.first_ix, self.last_ix)
    }
}

impl Edge {
    pub(crate) fn link<'a>(&self, edges: &'a [Edge]) -> Option<&'a Edge> {
        edges.get(self.link_ix.map(|ix| ix as usize)?)
    }

    pub(crate) fn serif<'a>(&self, edges: &'a [Edge]) -> Option<&'a Edge> {
        edges.get(self.serif_ix.map(|ix| ix as usize)?)
    }
}
