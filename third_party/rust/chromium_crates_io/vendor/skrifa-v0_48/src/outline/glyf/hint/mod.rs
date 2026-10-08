//! TrueType hinting.

mod call_stack;
mod cow_slice;
mod cvt;
mod definition;
mod engine;
mod graphics;
mod instance;
mod math;
mod program;
mod projection;
mod round;
mod storage;
mod value_stack;
mod zone;

use super::super::Target;

use read_fonts::{
    tables::glyf::PointFlags,
    types::{F26Dot6, F2Dot14, GlyphId, Point},
};

// What can go wrong reading bytecode is the font format's, so read-fonts
// states it; this interpreter reports it.
pub use instance::HintInstance;
pub use read_fonts::tables::glyf::bytecode::HintError;
pub(crate) use read_fonts::tables::glyf::bytecode::HintErrorKind;

/// Outline data that is passed to the hinter.
pub struct HintOutline<'a> {
    pub glyph_id: GlyphId,
    pub unscaled: &'a [Point<i32>],
    pub scaled: &'a mut [Point<F26Dot6>],
    pub original_scaled: &'a mut [Point<F26Dot6>],
    pub flags: &'a mut [PointFlags],
    pub contours: &'a [u16],
    pub phantom: &'a mut [Point<F26Dot6>],
    pub bytecode: &'a [u8],
    pub stack: &'a mut [i32],
    pub cvt: &'a mut [i32],
    pub storage: &'a mut [i32],
    pub twilight_scaled: &'a mut [Point<F26Dot6>],
    pub twilight_original_scaled: &'a mut [Point<F26Dot6>],
    pub twilight_flags: &'a mut [PointFlags],
    pub is_composite: bool,
    pub coords: &'a [F2Dot14],
}
