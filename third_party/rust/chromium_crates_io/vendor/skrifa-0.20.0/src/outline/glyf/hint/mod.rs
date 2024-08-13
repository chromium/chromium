//! TrueType hinting.

mod call_stack;
mod cow_slice;
mod cvt;
mod definition;
mod engine;
mod error;
mod graphics;
mod instance;
mod math;
mod program;
mod projection;
mod round;
mod storage;
mod value_stack;
mod zone;

use super::super::{HintingMode, LcdLayout};

use read_fonts::{
    tables::glyf::PointFlags,
    types::{F26Dot6, F2Dot14, GlyphId, Point},
};

pub use error::HintError;
pub use instance::HintInstance;

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

// Helpers for deriving various flags from the mode which
// change the behavior of certain instructions.
// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/truetype/ttgload.c#L2222>
impl HintingMode {
    fn is_smooth(&self) -> bool {
        matches!(self, Self::Smooth { .. })
    }

    fn is_grayscale_cleartype(&self) -> bool {
        matches!(
            self,
            Self::Smooth {
                lcd_subpixel: None,
                ..
            }
        )
    }

    fn is_vertical_lcd(&self) -> bool {
        matches!(
            self,
            Self::Smooth {
                lcd_subpixel: Some(LcdLayout::Vertical),
                ..
            }
        )
    }

    fn preserve_linear_metrics(&self) -> bool {
        matches!(
            self,
            Self::Smooth {
                preserve_linear_metrics: true,
                ..
            }
        )
    }
}
