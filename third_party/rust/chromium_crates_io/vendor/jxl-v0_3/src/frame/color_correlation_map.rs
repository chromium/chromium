// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    BLOCK_DIM,
    bit_reader::BitReader,
    error::{Error, Result},
};
use std::default::Default;

pub const COLOR_TILE_DIM: usize = 64;

const _: () = assert!(COLOR_TILE_DIM.is_multiple_of(BLOCK_DIM));

pub const COLOR_TILE_DIM_IN_BLOCKS: usize = COLOR_TILE_DIM / BLOCK_DIM;

pub const DEFAULT_COLOR_FACTOR: u32 = 84;

#[derive(Debug, Copy, Clone)]
pub struct ColorCorrelationParams {
    pub color_factor: u32,
    pub base_correlation_x: f32,
    pub base_correlation_b: f32,
    pub ytox_lf: i32,
    pub ytob_lf: i32,
}

impl Default for ColorCorrelationParams {
    fn default() -> Self {
        Self {
            color_factor: DEFAULT_COLOR_FACTOR,
            base_correlation_x: 0.0,
            base_correlation_b: 1.0,
            ytox_lf: 0,
            ytob_lf: 0,
        }
    }
}

impl ColorCorrelationParams {
    pub fn read(br: &mut BitReader) -> Result<ColorCorrelationParams, Error> {
        if br.read(1)? == 1 {
            Ok(Self::default())
        } else {
            let color_factor = match br.read(2)? {
                0 => DEFAULT_COLOR_FACTOR,
                1 => 256,
                2 => (br.read(8)? + 2) as u32,
                _ => (br.read(16)? + 258) as u32,
            };
            use crate::util::f16;
            let val_x = f16::from_bits(br.read(16)? as u16);
            let val_b = f16::from_bits(br.read(16)? as u16);
            if !val_x.is_finite() || !val_b.is_finite() {
                return Err(Error::FloatNaNOrInf);
            }
            let base_correlation_x = val_x.to_f32();
            let base_correlation_b = val_b.to_f32();
            if base_correlation_x > 4.0 || base_correlation_b > 4.0 {
                return Err(Error::BaseColorCorrelationOutOfRange);
            }
            let ytox_lf = br.read(8)? as i32 - 128;
            let ytob_lf = br.read(8)? as i32 - 128;
            Ok(ColorCorrelationParams {
                color_factor,
                base_correlation_x,
                base_correlation_b,
                ytox_lf,
                ytob_lf,
            })
        }
    }

    pub fn y_to_x(&self, factor: i32) -> f32 {
        self.base_correlation_x + (factor as f32) / (self.color_factor as f32)
    }

    pub fn y_to_x_lf(&self) -> f32 {
        self.y_to_x(self.ytox_lf)
    }

    pub fn y_to_b(&self, factor: i32) -> f32 {
        self.base_correlation_b + (factor as f32) / (self.color_factor as f32)
    }

    pub fn y_to_b_lf(&self) -> f32 {
        self.y_to_b(self.ytob_lf)
    }
}
