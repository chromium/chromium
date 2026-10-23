// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

mod common;
mod epf0;
mod epf1;
mod epf2;

use common::{SigmaRow, prepare_sad_mul_storage};

use crate::BLOCK_DIM;
use crate::features::epf::SigmaSource;
use crate::render::stages::epf::epf0::epf0_process_row_chunk_dispatch;
use crate::render::stages::epf::epf1::epf1_process_row_chunk_dispatch;
use crate::render::stages::epf::epf2::epf2_process_row_chunk_dispatch;
use crate::render::{Channels, ChannelsMut, ErasedLocalState, RenderPipelineInOutStage};
use crate::util::sync::{Arc, RwLock};

/// Edge-preserving filter stage for step `STEP` (`0..=2`) with border radius `BORDER` (`3 - STEP`).
pub struct EpfStage<const STEP: u8, const BORDER: u8> {
    /// Multiplier for sigma in pass `STEP`
    sigma_scale: f32,
    /// (inverse) multiplier for sigma on borders
    border_sad_mul: f32,
    channel_scale: [f32; 3],
    sigma: Arc<RwLock<SigmaSource>>,
}

impl<const STEP: u8, const BORDER: u8> std::fmt::Display for EpfStage<STEP, BORDER> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "EPF stage {STEP} with sigma scale: {}, border_sad_mul: {}",
            self.sigma_scale, self.border_sad_mul
        )
    }
}

impl<const STEP: u8, const BORDER: u8> EpfStage<STEP, BORDER> {
    pub fn new(
        sigma_scale: f32,
        border_sad_mul: f32,
        channel_scale: [f32; 3],
        sigma: Arc<RwLock<SigmaSource>>,
    ) -> Self {
        const { assert!(STEP <= 2) };
        const { assert!(BORDER == 3 - STEP) };
        Self {
            sigma,
            sigma_scale,
            channel_scale,
            border_sad_mul,
        }
    }
}

macro_rules! stage {
    ($step: literal, $border: literal, $fun: path) => {
        impl RenderPipelineInOutStage for EpfStage<$step, $border> {
            type InputT = f32;
            type OutputT = f32;
            const SHIFT: (u8, u8) = (0, 0);
            const BORDER: (u8, u8) = ($border, $border);

            fn uses_channel(&self, c: usize) -> bool {
                c < 3
            }

            fn process_row_chunk(
                &self,
                pos: (usize, usize),
                xsize: usize,
                input_rows: &Channels<f32>,
                output_rows: &mut ChannelsMut<f32>,
                _state: Option<&mut ErasedLocalState>,
                _previous_call_was_previous_row: bool,
            ) {
                if xsize == 0 {
                    return;
                }
                let (xpos, ypos) = pos;
                let sigma = self.sigma.try_read().unwrap();
                let row_sigma = sigma.row(ypos / BLOCK_DIM);
                let start = xpos / BLOCK_DIM;
                let max_needed = (xsize - 1) / BLOCK_DIM + 3;
                let const_sigma_stack;
                let const_sigma_vec;
                let row_sigma = match row_sigma {
                    SigmaRow::Variable(s) => &s[start..],
                    SigmaRow::Constant(c) if max_needed <= 128 => {
                        const_sigma_stack = [c; 128];
                        &const_sigma_stack[..max_needed]
                    }
                    SigmaRow::Constant(c) => {
                        const_sigma_vec = vec![c; max_needed];
                        &const_sigma_vec[..]
                    }
                };
                let sm = self.sigma_scale * 1.65;
                let bsm = sm * self.border_sad_mul;
                let sad_mul_storage = prepare_sad_mul_storage(xpos, ypos, sm, bsm);
                $fun(
                    self,
                    xpos,
                    xsize,
                    row_sigma,
                    &sad_mul_storage,
                    input_rows,
                    output_rows,
                );
            }
        }
    };
}

stage!(0, 3, epf0_process_row_chunk_dispatch);
stage!(1, 2, epf1_process_row_chunk_dispatch);
stage!(2, 1, epf2_process_row_chunk_dispatch);

/// 5x5 plus-shaped kernel with 5 SADs per pixel (3x3 plus-shaped).
/// So this makes this filter a 7x7 filter.
pub type Epf0Stage = EpfStage<0, 3>;
/// 3x3 plus-shaped kernel with 5 SADs per pixel (3x3 plus-shaped).
/// So this makes this filter a 5x5 filter.
pub type Epf1Stage = EpfStage<1, 2>;
/// 3x3 plus-shaped kernel with 1 SAD per pixel.
/// So this makes this filter a 3x3 filter.
pub type Epf2Stage = EpfStage<2, 1>;

#[cfg(test)]
mod test;
