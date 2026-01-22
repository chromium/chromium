// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    bit_reader::BitReader,
    error::{Error, Result},
    frame::quant_weights,
    headers::encodings::{Empty, UnconditionalCoder},
};

pub const NUM_QUANT_TABLES: usize = 17;
pub const GLOBAL_SCALE_DENOM: usize = 1 << 16;

#[derive(Debug)]
pub struct LfQuantFactors {
    pub quant_factors: [f32; 3],
    pub inv_quant_factors: [f32; 3],
}

impl LfQuantFactors {
    pub fn new(br: &mut BitReader) -> Result<LfQuantFactors> {
        let mut quant_factors = [0.0f32; 3];
        if br.read(1)? == 1 {
            quant_factors = quant_weights::LF_QUANT;
        } else {
            for qf in quant_factors.iter_mut() {
                *qf = f32::read_unconditional(&(), br, &Empty {})? / 128.0;
                if *qf < 1e-8 {
                    return Err(Error::LfQuantFactorTooSmall(*qf));
                }
            }
        }

        let inv_quant_factors = quant_factors.map(f32::recip);

        Ok(LfQuantFactors {
            quant_factors,
            inv_quant_factors,
        })
    }
}

#[derive(Debug)]
pub struct QuantizerParams {
    pub global_scale: u32,
    pub quant_lf: u32,
}

impl QuantizerParams {
    pub fn read(br: &mut BitReader) -> Result<QuantizerParams> {
        let global_scale = match br.read(2)? {
            0 => br.read(11)? + 1,
            1 => br.read(11)? + 2049,
            2 => br.read(12)? + 4097,
            _ => br.read(16)? + 8193,
        };
        let quant_lf = match br.read(2)? {
            0 => 16,
            1 => br.read(5)? + 1,
            2 => br.read(8)? + 1,
            _ => br.read(16)? + 1,
        };
        Ok(QuantizerParams {
            global_scale: global_scale as u32,
            quant_lf: quant_lf as u32,
        })
    }
    pub fn inv_global_scale(&self) -> f32 {
        GLOBAL_SCALE_DENOM as f32 / self.global_scale as f32
    }
    pub fn inv_quant_lf(&self) -> f32 {
        self.inv_global_scale() / self.quant_lf as f32
    }
}
