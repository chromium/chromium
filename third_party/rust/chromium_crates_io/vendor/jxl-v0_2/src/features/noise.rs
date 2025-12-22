// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{bit_reader::BitReader, error::Result};
#[derive(Debug, PartialEq, Default, Clone, Copy)]
pub struct Noise {
    pub lut: [f32; 8],
}

impl Noise {
    pub fn read(br: &mut BitReader) -> Result<Noise> {
        let mut noise = Noise::default();
        for l in &mut noise.lut {
            *l = (br.read(10)? as f32) / ((1 << 10) as f32);
        }
        Ok(noise)
    }
    pub fn strength(&self, vx: f32) -> f32 {
        let k_scale = (self.lut.len() - 2) as f32;
        let scaled_vx = f32::max(0.0, vx * k_scale);
        let pre_floor_x = scaled_vx.floor();
        let pre_frac_x = scaled_vx - pre_floor_x;
        let floor_x = if scaled_vx >= k_scale + 1.0 {
            k_scale
        } else {
            pre_floor_x
        };
        let frac_x = if scaled_vx >= k_scale + 1.0 {
            1.0
        } else {
            pre_frac_x
        };
        let floor_x_int = floor_x as usize;
        let low = self.lut[floor_x_int];
        let hi = self.lut[floor_x_int + 1];
        ((hi - low) * frac_x + low).clamp(0.0, 1.0)
    }
}
