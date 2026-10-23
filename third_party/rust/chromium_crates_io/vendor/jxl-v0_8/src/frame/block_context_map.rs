// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::bit_reader::BitReader;
use crate::entropy_coding::context_map::*;
use crate::entropy_coding::decode::unpack_signed;
use crate::error::{Error, Result};
use crate::frame::coeff_order::NUM_ORDERS;

pub const NON_ZERO_BUCKETS: usize = 37;

// Supremum of zero_density_context(x, y) + 1, when x + y <= 64.
pub const ZERO_DENSITY_CONTEXT_COUNT: usize = 458;
// Supremum of zero_density_context(x, y) + 1.
pub const ZERO_DENSITY_CONTEXT_LIMIT: usize = 474;

// Both tables below have their entries pre-multiplied by 2 so that the zero-density
// context can be computed directly as `COEFF_NUM_NONZERO_CONTEXT[nonzeros] + COEFF_FREQ_CONTEXT[k] + prev`.
pub const COEFF_FREQ_CONTEXT: [usize; 64] = [
    0xBAD, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 30, 32, 32, 34, 34, 36, 36,
    38, 38, 40, 40, 42, 42, 44, 44, 46, 46, 46, 46, 48, 48, 48, 48, 50, 50, 50, 50, 52, 52, 52, 52,
    54, 54, 54, 54, 56, 56, 56, 56, 58, 58, 58, 58, 60, 60, 60, 60,
];

pub const COEFF_NUM_NONZERO_CONTEXT: [usize; 64] = [
    0xBAD, 0, 62, 124, 124, 186, 186, 186, 186, 246, 246, 246, 246, 304, 304, 304, 304, 304, 304,
    304, 304, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 412, 412, 412, 412, 412,
    412, 412, 412, 412, 412, 412, 412, 412, 412, 412, 412, 412, 412, 412, 412, 412, 412, 412, 412,
    412, 412, 412, 412, 412, 412, 412,
];

#[derive(Debug)]
pub struct BlockContextMap {
    pub lf_thresholds: [Vec<i32>; 3],
    pub qf_thresholds: Vec<u32>,
    pub context_map: Vec<u8>,
    pub num_lf_contexts: usize,
    pub num_contexts: usize,
}

impl BlockContextMap {
    pub fn num_ac_contexts(&self) -> usize {
        self.num_contexts * (NON_ZERO_BUCKETS + ZERO_DENSITY_CONTEXT_COUNT)
    }
    pub fn read(br: &mut BitReader, lz77_scratch: &mut Vec<u32>) -> Result<BlockContextMap, Error> {
        if br.read(1)? == 1 {
            Ok(BlockContextMap {
                lf_thresholds: [vec![], vec![], vec![]],
                qf_thresholds: vec![],
                context_map: vec![
                    0, 1, 2, 2, 3, 3, 4, 5, 6, 6, 6, 6, 6, //
                    7, 8, 9, 9, 10, 11, 12, 13, 14, 14, 14, 14, 14, //
                    7, 8, 9, 9, 10, 11, 12, 13, 14, 14, 14, 14, 14, //
                ],
                num_lf_contexts: 1,
                num_contexts: 15,
            })
        } else {
            let mut num_lf_contexts: usize = 1;
            let mut lf_thresholds: [Vec<i32>; 3] = [vec![], vec![], vec![]];
            for thr in lf_thresholds.iter_mut() {
                let num_lf_thresholds = br.read(4)? as usize;
                let mut v: Vec<i32> = vec![0; num_lf_thresholds];
                for val in v.iter_mut() {
                    let uval = match br.read(2)? {
                        0 => br.read(4)?,
                        1 => br.read(8)? + 16,
                        2 => br.read(16)? + 272,
                        _ => br.read(32)? + 65808,
                    };
                    *val = unpack_signed(uval as u32)
                }
                *thr = v;
                num_lf_contexts *= num_lf_thresholds + 1;
            }
            let num_qf_thresholds = br.read(4)? as usize;
            let mut qf_thresholds: Vec<u32> = vec![0; num_qf_thresholds];
            for val in qf_thresholds.iter_mut() {
                *val = match br.read(2)? {
                    0 => br.read(2)?,
                    1 => br.read(3)? + 4,
                    2 => br.read(5)? + 12,
                    _ => br.read(8)? + 44,
                } as u32
                    + 1;
            }
            if num_lf_contexts * (num_qf_thresholds + 1) > 64 {
                return Err(Error::BlockContextMapSizeTooBig(
                    num_lf_contexts,
                    num_qf_thresholds,
                ));
            }
            let context_map_size = 3 * NUM_ORDERS * num_lf_contexts * (num_qf_thresholds + 1);
            let context_map = decode_context_map(context_map_size, br, lz77_scratch)?;
            assert_eq!(context_map.len(), context_map_size);
            let num_contexts = *context_map.iter().max().unwrap() as usize + 1;
            if num_contexts > 16 {
                Err(Error::TooManyBlockContexts)
            } else {
                Ok(BlockContextMap {
                    lf_thresholds,
                    qf_thresholds,
                    context_map,
                    num_lf_contexts,
                    num_contexts,
                })
            }
        }
    }
    #[inline(always)]
    pub fn block_context(&self, lf_idx: usize, qf: u32, shape_id: usize, c: usize) -> usize {
        let mut qf_idx: usize = 0;
        for t in &self.qf_thresholds {
            if qf > *t {
                qf_idx += 1;
            }
        }
        let mut idx = if c < 2 { c ^ 1 } else { 2 };
        idx = idx * NUM_ORDERS + shape_id;
        idx = idx * (self.qf_thresholds.len() + 1) + qf_idx;
        idx = idx * self.num_lf_contexts + lf_idx;
        self.context_map[idx] as usize
    }
    #[inline(always)]
    pub fn nonzero_context(&self, nonzeros: usize, block_context: usize) -> usize {
        let context: usize = if nonzeros < 8 {
            nonzeros
        } else if nonzeros < 64 {
            4 + nonzeros / 2
        } else {
            36
        };
        context * self.num_contexts + block_context
    }
    #[inline(always)]
    pub fn zero_density_context_offset(&self, block_context: usize) -> usize {
        self.num_contexts * NON_ZERO_BUCKETS + ZERO_DENSITY_CONTEXT_COUNT * block_context
    }
}
