//! TrueType hinting.

use read_fonts::{
    tables::glyf::PointFlags,
    types::{F26Dot6, F2Dot14, Point},
};

/// Outline data that is passed to the hinter.
pub struct HinterOutline<'a> {
    pub unscaled: &'a mut [Point<i32>],
    pub scaled: &'a mut [Point<F26Dot6>],
    pub original_scaled: &'a mut [Point<F26Dot6>],
    pub flags: &'a mut [PointFlags],
    pub contours: &'a [u16],
    pub phantom: &'a mut [Point<F26Dot6>],
    pub bytecode: &'a [u8],
    pub is_composite: bool,
    pub coords: &'a [F2Dot14],
}
