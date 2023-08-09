#![allow(dead_code)]

use super::scaler::ScalerFont;
use crate::scale::Hinting;

use read_fonts::{
    tables::glyf::PointFlags,
    types::{F26Dot6, Point},
};

/// Slot for the hinting cache.
#[derive(Copy, Clone, Default, Debug)]
enum Slot {
    /// Uncached font.
    #[default]
    Uncached,
    /// Font and size cache indices.
    Cached {
        font_index: usize,
        size_index: usize,
    },
}

#[derive(Copy, Clone, Default, Debug)]
pub struct HintConfig {
    hinting: Option<Hinting>,
    is_enabled: bool,
    slot: Option<Slot>,
}

impl HintConfig {
    pub fn new(hinting: Option<Hinting>) -> Self {
        Self {
            hinting,
            is_enabled: hinting.is_some(),
            slot: None,
        }
    }

    pub fn is_enabled(&self) -> bool {
        self.is_enabled
    }

    pub fn reset(&mut self) {
        self.is_enabled = self.hinting.is_some();
    }
}

/// Aggregate state from the scaler that is necessary for hinting
/// a glyph.
pub struct HintGlyph<'a> {
    pub font: &'a ScalerFont<'a>,
    pub config: &'a mut HintConfig,
    pub points: &'a mut [Point<F26Dot6>],
    pub original: &'a mut [Point<F26Dot6>],
    pub unscaled: &'a mut [Point<i32>],
    pub flags: &'a mut [PointFlags],
    pub contours: &'a mut [u16],
    pub point_base: usize,
    pub contour_base: usize,
    pub phantom: &'a mut [Point<F26Dot6>],
    pub ins: &'a [u8],
    pub is_composite: bool,
}

#[derive(Clone, Default, Debug)]
pub struct HintContext {
    // TODO
}

impl HintContext {
    pub fn hint(&mut self, _glyph: HintGlyph) -> bool {
        true
    }
}
