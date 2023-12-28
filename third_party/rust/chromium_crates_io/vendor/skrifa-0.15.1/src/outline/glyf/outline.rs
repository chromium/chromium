//! TrueType outline types.

use std::mem::size_of;

use read_fonts::{
    tables::glyf::{to_path, Glyph, PointFlags, ToPathError},
    types::{F26Dot6, Fixed, GlyphId, Pen, Point},
};

use super::{super::Hinting, OutlineMemory};

/// Represents the information necessary to scale a glyph outline.
///
/// Contains a reference to the glyph data itself as well as metrics that
/// can be used to compute the memory requirements for scaling the glyph.
#[derive(Clone, Default)]
pub struct Outline<'a> {
    pub glyph_id: GlyphId,
    /// The associated top-level glyph for the outline.
    pub glyph: Option<Glyph<'a>>,
    /// Sum of the point counts of all simple glyphs in an outline.
    pub points: usize,
    /// Sum of the contour counts of all simple glyphs in an outline.
    pub contours: usize,
    /// Maximum number of points in a single simple glyph.
    pub max_simple_points: usize,
    /// "Other" points are the unscaled or original scaled points.
    ///
    /// The size of these buffer is the same and this value tracks the size
    /// for one (not both) of the buffers. This is the maximum of
    /// `max_simple_points` and the total number of points for all component
    /// glyphs in a single composite glyph.
    pub max_other_points: usize,
    /// Maximum size of the component delta stack.
    ///
    /// For composite glyphs in variable fonts, delta values are computed
    /// for each component. This tracks the maximum stack depth necessary
    /// to store those values during processing.
    pub max_component_delta_stack: usize,
    /// True if any component of a glyph has bytecode instructions.
    pub has_hinting: bool,
    /// True if the glyph requires variation delta processing.
    pub has_variations: bool,
    /// True if the glyph contains any simple or compound overlap flags.
    pub has_overlaps: bool,
}

impl<'a> Outline<'a> {
    /// Returns the minimum size in bytes required to scale an outline based
    /// on the computed sizes.
    pub fn required_buffer_size(&self, hinting: Hinting) -> usize {
        let mut size = 0;
        let hinting = self.has_hinting && hinting == Hinting::Embedded;
        // Scaled, unscaled and (for hinting) original scaled points
        size += self.points * size_of::<Point<F26Dot6>>();
        // Unscaled and (if hinted) original scaled points
        size += self.max_other_points * size_of::<Point<i32>>() * if hinting { 2 } else { 1 };
        // Contour end points
        size += self.contours * size_of::<u16>();
        // Point flags
        size += self.points * size_of::<PointFlags>();
        if self.has_variations {
            // Interpolation buffer for delta IUP
            size += self.max_simple_points * size_of::<Point<Fixed>>();
            // Delta buffer for points
            size += self.max_simple_points * size_of::<Point<Fixed>>();
            // Delta buffer for composite components
            size += self.max_component_delta_stack * size_of::<Point<Fixed>>();
        }
        if size != 0 {
            // If we're given a buffer that is not aligned, we'll need to
            // adjust, so add our maximum alignment requirement in bytes.
            size += std::mem::align_of::<i32>();
        }
        size
    }

    /// Allocates new memory for scaling this glyph from the given buffer.
    ///
    /// The size of the buffer must be at least as large as the size returned
    /// by [`Self::required_buffer_size`].
    pub fn memory_from_buffer(
        &self,
        buf: &'a mut [u8],
        hinting: Hinting,
    ) -> Option<OutlineMemory<'a>> {
        OutlineMemory::new(self, buf, hinting)
    }
}

#[derive(Debug)]
pub struct ScaledOutline<'a> {
    pub points: &'a mut [Point<F26Dot6>],
    pub flags: &'a mut [PointFlags],
    pub contours: &'a mut [u16],
    pub phantom_points: [Point<F26Dot6>; 4],
}

impl<'a> ScaledOutline<'a> {
    pub fn adjusted_lsb(&self) -> F26Dot6 {
        self.phantom_points[0].x
    }

    pub fn adjusted_advance_width(&self) -> F26Dot6 {
        self.phantom_points[1].x - self.phantom_points[0].x
    }

    pub fn to_path(&self, pen: &mut impl Pen) -> Result<(), ToPathError> {
        to_path(self.points, self.flags, self.contours, pen)
    }
}
