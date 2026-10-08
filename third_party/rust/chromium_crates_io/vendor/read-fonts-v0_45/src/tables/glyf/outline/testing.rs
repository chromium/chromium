//! Shared scaffolding for the tests in this module.

use super::{Outline, OutlineTables, Scale};
use crate::{
    tables::glyf::{PointCoord, PHANTOM_POINT_COUNT},
    types::{GlyphId, Point},
    FontRef,
};
use alloc::vec::Vec;

/// What one load produced, copied out so it outlives the outline.
pub(super) struct Built<C: PointCoord> {
    pub points: Vec<Point<C>>,
    pub contours: Vec<u16>,
    pub phantom: [Point<C>; PHANTOM_POINT_COUNT],
}

/// Loads one glyph of a font that is known to be well formed.
pub(super) fn build<S: Scale>(data: &'static [u8], gid: u32, scale: &S) -> Built<S::Coord> {
    let font = FontRef::new(data).unwrap();
    let tables = OutlineTables::new(&font).unwrap();
    let mut outline = Outline::<S>::new();
    let loaded = outline
        .load_with(&tables, GlyphId::new(gid), scale, None)
        .unwrap();
    Built {
        points: loaded.points().to_vec(),
        contours: loaded.contours().to_vec(),
        phantom: *loaded.phantom_points(),
    }
}
