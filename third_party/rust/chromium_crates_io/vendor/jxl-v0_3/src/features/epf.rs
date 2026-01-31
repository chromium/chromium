// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::sync::Arc;

use crate::{
    error::{Error, Result},
    frame::{HfMetadata, LfGlobalState},
    headers::frame_header::{Encoding, FrameHeader},
    image::Image,
};

use jxl_transforms::transform_map::*;

/// Source of sigma values for EPF (Edge Preserving Filter).
/// For VarDCT encoding, sigma varies per-block based on quantization.
/// For Modular encoding, sigma is constant across the entire image.
#[derive(Clone)]
pub enum SigmaSource {
    /// Variable sigma per block (VarDCT encoding)
    Variable(Arc<Image<f32>>),
    /// Constant sigma for entire image (Modular encoding)
    Constant(f32),
}

impl SigmaSource {
    pub fn new(
        frame_header: &FrameHeader,
        lf_global: &LfGlobalState,
        hf_meta: &Option<HfMetadata>,
    ) -> Result<Self> {
        let rf = &frame_header.restoration_filter;
        #[allow(clippy::excessive_precision)]
        const INV_SIGMA_NUM: f32 = -1.1715728752538099024;

        if frame_header.encoding == Encoding::VarDCT {
            let size_blocks = frame_header.size_blocks();
            let sigma_xsize = size_blocks.0;
            let sigma_ysize = size_blocks.1;
            // We might over-read the sigma row slightly when applying EPF, so ensure that there is enough
            // space to avoid having the out-of-bounds read from the row causing a panic (the value does
            // not affect any pixels that are actually visualized, so we don't need to set it to anything
            // special below).
            let mut sigma_image = Image::<f32>::new((sigma_xsize + 2, sigma_ysize))?;

            let hf_meta = hf_meta.as_ref().unwrap();
            let quant_params = lf_global.quant_params.as_ref().unwrap();
            let quant_scale = 1.0 / quant_params.inv_global_scale();
            for by in 0..size_blocks.1 {
                let raw_quant_row = hf_meta.raw_quant_map.row(by);
                let transform_row = hf_meta.transform_map.row(by);
                for bx in 0..size_blocks.0 {
                    let raw_quant = raw_quant_row[bx];
                    let raw_transform_id = transform_row[bx];
                    let transform_id = raw_transform_id & 127;
                    let is_first_block = raw_transform_id >= 128;
                    if !is_first_block {
                        continue;
                    }
                    let transform_type = HfTransformType::from_usize(transform_id as usize)
                        .ok_or(Error::InvalidVarDCTTransform(transform_id as usize))?;
                    let cx = covered_blocks_x(transform_type) as usize;
                    let cy = covered_blocks_y(transform_type) as usize;
                    let sigma_quant =
                        rf.epf_quant_mul / (quant_scale * raw_quant as f32 * INV_SIGMA_NUM);
                    for iy in 0..cy {
                        for ix in 0..cx {
                            let sharpness = hf_meta.epf_map.row(by + iy)[bx + ix] as usize;
                            let sigma = (sigma_quant * rf.epf_sharp_lut[sharpness]).min(-1e-4);
                            sigma_image.row_mut(by + iy)[bx + ix] = 1.0 / sigma;
                        }
                    }
                }
            }
            Ok(SigmaSource::Variable(Arc::new(sigma_image)))
        } else {
            // For Modular encoding, sigma is constant - no need to allocate an image
            let sigma = INV_SIGMA_NUM / rf.epf_sigma_for_modular;
            Ok(SigmaSource::Constant(sigma))
        }
    }
}
