// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::bit_reader::BitReader;
use crate::error::Result;
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
}
