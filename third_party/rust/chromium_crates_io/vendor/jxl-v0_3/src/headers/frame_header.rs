// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::excessive_precision)]

use crate::{
    BLOCK_DIM, GROUP_DIM,
    bit_reader::BitReader,
    error::Error,
    headers::{encodings::*, extra_channels::ExtraChannelInfo},
    image::Rect,
    util::FloorLog2,
};

use jxl_macros::UnconditionalCoder;
use num_derive::FromPrimitive;
use std::cmp::min;

use super::Animation;

#[derive(UnconditionalCoder, Copy, Clone, PartialEq, Debug, FromPrimitive)]
pub enum FrameType {
    RegularFrame = 0,
    LFFrame = 1,
    ReferenceOnly = 2,
    SkipProgressive = 3,
}

#[derive(UnconditionalCoder, Copy, Clone, PartialEq, Debug, FromPrimitive)]
pub enum Encoding {
    VarDCT = 0,
    Modular = 1,
}

struct Flags;

impl Flags {
    pub const ENABLE_NOISE: u64 = 1;
    pub const ENABLE_PATCHES: u64 = 2;
    pub const ENABLE_SPLINES: u64 = 0x10;
    pub const USE_LF_FRAME: u64 = 0x20;
    pub const SKIP_ADAPTIVE_LF_SMOOTHING: u64 = 0x80;
}

#[derive(UnconditionalCoder, Debug, PartialEq)]
pub struct Passes {
    #[coder(u2S(1, 2, 3, Bits(3) + 4))]
    #[default(1)]
    pub num_passes: u32,

    #[coder(u2S(0, 1, 2, Bits(1) + 3))]
    #[default(0)]
    #[condition(num_passes != 1)]
    num_ds: u32,

    #[size_coder(explicit(num_passes - 1))]
    #[coder(Bits(2))]
    #[default_element(0)]
    #[condition(num_passes != 1)]
    pub shift: Vec<u32>,

    #[size_coder(explicit(num_ds))]
    #[coder(u2S(1, 2, 4, 8))]
    #[default_element(1)]
    #[condition(num_passes != 1)]
    downsample: Vec<u32>,

    #[size_coder(explicit(num_ds))]
    #[coder(u2S(0, 1, 2, Bits(3)))]
    #[default_element(0)]
    #[condition(num_passes != 1)]
    last_pass: Vec<u32>,
}

impl Passes {
    pub fn downsampling_bracket(&self, pass: usize) -> (usize, usize) {
        let mut max_shift = 2;
        let mut min_shift = 3;
        for i in 0..pass + 1 {
            for j in 0..self.num_ds as usize {
                if i == self.last_pass[j] as usize {
                    min_shift = self.downsample[j].floor_log2();
                }
            }
            if i + 1 == self.num_passes as usize {
                min_shift = 0;
            }
            if i != pass {
                max_shift = min_shift.saturating_sub(1);
            }
        }
        (min_shift as usize, max_shift as usize)
    }
}

#[derive(UnconditionalCoder, Copy, Clone, PartialEq, Debug, FromPrimitive)]
pub enum BlendingMode {
    Replace = 0,
    Add = 1,
    Blend = 2,
    AlphaWeightedAdd = 3,
    Mul = 4,
}

#[derive(Default)]
pub struct BlendingInfoNonserialized {
    num_extra_channels: u32,
    full_frame: bool,
}

#[derive(UnconditionalCoder, Debug, PartialEq, Clone)]
#[nonserialized(BlendingInfoNonserialized)]
pub struct BlendingInfo {
    #[coder(u2S(0, 1, 2, Bits(2) + 3))]
    #[default(BlendingMode::Replace)]
    pub mode: BlendingMode,

    /* Spec: "Let multi_extra be true if and only if and the number of extra channels is at least two."
    libjxl condition is num_extra_channels > 0 */
    #[coder(u2S(0, 1, 2, Bits(3) + 3))]
    #[default(0)]
    #[condition(nonserialized.num_extra_channels > 0 &&
        (mode == BlendingMode::Blend || mode == BlendingMode::AlphaWeightedAdd))]
    pub alpha_channel: u32,

    #[default(false)]
    #[condition(nonserialized.num_extra_channels > 0 &&
        (mode == BlendingMode::Blend || mode == BlendingMode::AlphaWeightedAdd || mode == BlendingMode::Mul))]
    pub clamp: bool,

    #[coder(u2S(0, 1, 2, 3))]
    #[default(0)]
    // This condition is called `resets_canvas` in the spec
    #[condition(!(nonserialized.full_frame && mode == BlendingMode::Replace))]
    pub source: u32,
}

pub struct RestorationFilterNonserialized {
    encoding: Encoding,
}

#[derive(UnconditionalCoder, Debug, PartialEq)]
#[nonserialized(RestorationFilterNonserialized)]
pub struct RestorationFilter {
    #[all_default]
    all_default: bool,

    #[default(true)]
    pub gab: bool,

    #[default(false)]
    #[condition(gab)]
    gab_custom: bool,

    #[default(0.115169525)]
    #[condition(gab_custom)]
    pub gab_x_weight1: f32,

    #[default(0.061248592)]
    #[condition(gab_custom)]
    pub gab_x_weight2: f32,

    #[default(0.115169525)]
    #[condition(gab_custom)]
    pub gab_y_weight1: f32,

    #[default(0.061248592)]
    #[condition(gab_custom)]
    pub gab_y_weight2: f32,

    #[default(0.115169525)]
    #[condition(gab_custom)]
    pub gab_b_weight1: f32,

    #[default(0.061248592)]
    #[condition(gab_custom)]
    pub gab_b_weight2: f32,

    #[coder(Bits(2))]
    #[default(2)]
    pub epf_iters: u32,

    #[default(false)]
    #[condition(epf_iters > 0 && nonserialized.encoding == Encoding::VarDCT)]
    epf_sharp_custom: bool,

    #[default([0.0, 1.0 / 7.0, 2.0 / 7.0, 3.0 / 7.0, 4.0 / 7.0, 5.0 / 7.0, 6.0 / 7.0, 1.0])]
    #[condition(epf_sharp_custom)]
    pub epf_sharp_lut: [f32; 8],

    #[default(false)]
    #[condition(epf_iters > 0)]
    epf_weight_custom: bool,

    #[default([40.0, 5.0, 3.5])]
    #[condition(epf_weight_custom)]
    pub epf_channel_scale: [f32; 3],

    #[default(0.45)]
    #[condition(epf_weight_custom)]
    epf_pass1_zeroflush: f32,

    #[default(0.6)]
    #[condition(epf_weight_custom)]
    epf_pass2_zeroflush: f32,

    #[default(false)]
    #[condition(epf_iters > 0)]
    epf_sigma_custom: bool,

    #[default(0.46)]
    #[condition(epf_sigma_custom && nonserialized.encoding == Encoding::VarDCT)]
    pub epf_quant_mul: f32,

    #[default(0.9)]
    #[condition(epf_sigma_custom)]
    pub epf_pass0_sigma_scale: f32,

    #[default(6.5)]
    #[condition(epf_sigma_custom)]
    pub epf_pass2_sigma_scale: f32,

    #[default(2.0 / 3.0)]
    #[condition(epf_sigma_custom)]
    pub epf_border_sad_mul: f32,

    #[default(1.0)]
    #[condition(epf_iters > 0 && nonserialized.encoding == Encoding::Modular)]
    pub epf_sigma_for_modular: f32,

    #[default(Extensions::default())]
    extensions: Extensions,
}

pub struct PermutationNonserialized {
    pub num_entries: u32,
    pub permuted: bool,
}

pub struct FrameHeaderNonserialized {
    pub xyb_encoded: bool,
    pub num_extra_channels: u32,
    pub extra_channel_info: Vec<ExtraChannelInfo>,
    pub have_animation: bool,
    pub have_timecode: bool,
    pub img_width: u32,
    pub img_height: u32,
}

const H_SHIFT: [usize; 4] = [0, 1, 1, 0];
const V_SHIFT: [usize; 4] = [0, 1, 0, 1];

fn compute_jpeg_shift(jpeg_upsampling: &[u32], shift_table: &[usize]) -> u32 {
    jpeg_upsampling
        .iter()
        .map(|&ch| shift_table[ch as usize])
        .max()
        .unwrap_or(0) as u32
}

#[derive(UnconditionalCoder, Debug, PartialEq)]
#[nonserialized(FrameHeaderNonserialized)]
#[aligned]
#[validate]
pub struct FrameHeader {
    #[all_default]
    all_default: bool,

    #[coder(Bits(2))]
    #[default(FrameType::RegularFrame)]
    pub frame_type: FrameType,

    #[coder(Bits(1))]
    #[default(Encoding::VarDCT)]
    pub encoding: Encoding,

    #[default(0)]
    flags: u64,

    #[default(false)]
    #[condition(!nonserialized.xyb_encoded)]
    pub do_ycbcr: bool,

    #[coder(Bits(2))]
    #[default([0, 0, 0])]
    #[condition(do_ycbcr && flags & Flags::USE_LF_FRAME == 0)]
    jpeg_upsampling: [u32; 3],

    #[coder(u2S(1, 2, 4, 8))]
    #[default(1)]
    #[condition(flags & Flags::USE_LF_FRAME == 0)]
    pub upsampling: u32,

    #[size_coder(explicit(nonserialized.num_extra_channels))]
    #[coder(u2S(1, 2, 4, 8))]
    #[default_element(1)]
    #[condition(flags & Flags::USE_LF_FRAME == 0)]
    pub ec_upsampling: Vec<u32>,

    #[coder(Bits(2))]
    #[default(1)]
    #[condition(encoding == Encoding::Modular)]
    group_size_shift: u32,

    #[coder(Bits(3))]
    #[default(3)]
    #[condition(encoding == Encoding::VarDCT && nonserialized.xyb_encoded)]
    pub x_qm_scale: u32,

    #[coder(Bits(3))]
    #[default(2)]
    #[condition(encoding == Encoding::VarDCT && nonserialized.xyb_encoded)]
    pub b_qm_scale: u32,

    #[condition(frame_type != FrameType::ReferenceOnly)]
    #[default(Passes::default(&field_nonserialized))]
    pub passes: Passes,

    #[coder(u2S(1, 2, 3, 4))]
    #[default(0)]
    #[condition(frame_type == FrameType::LFFrame)]
    pub lf_level: u32,

    #[default(false)]
    #[condition(frame_type != FrameType::LFFrame)]
    have_crop: bool,

    #[coder(u2S(Bits(8), Bits(11) + 256, Bits(14) + 2304, Bits(30) + 18688))]
    #[default(0)]
    #[condition(have_crop && frame_type != FrameType::ReferenceOnly)]
    pub x0: i32,

    #[coder(u2S(Bits(8), Bits(11) + 256, Bits(14) + 2304, Bits(30) + 18688))]
    #[default(0)]
    #[condition(have_crop && frame_type != FrameType::ReferenceOnly)]
    pub y0: i32,

    #[coder(u2S(Bits(8), Bits(11) + 256, Bits(14) + 2304, Bits(30) + 18688))]
    #[default(0)]
    #[condition(have_crop)]
    frame_width: u32,

    #[coder(u2S(Bits(8), Bits(11) + 256, Bits(14) + 2304, Bits(30) + 18688))]
    #[default(0)]
    #[condition(have_crop)]
    frame_height: u32,

    // The following 2 fields are not actually serialized, but just used as variables to help with
    // defining later conditions.
    #[default(x0 <= 0 && y0 <= 0 && (frame_width as i64 + x0 as i64) >= nonserialized.img_width as i64 &&
        (frame_height as i64 + y0 as i64) >= nonserialized.img_height as i64)]
    #[condition(false)]
    completely_covers: bool,

    #[default(!have_crop || completely_covers)]
    #[condition(false)]
    full_frame: bool,

    /* "normal_frame" denotes the condition !all_default
    && (frame_type == kRegularFrame || frame_type == kSkipProgressive) */
    #[default(BlendingInfo::default(&field_nonserialized))]
    #[condition(frame_type == FrameType::RegularFrame || frame_type == FrameType::SkipProgressive)]
    #[nonserialized(num_extra_channels : nonserialized.num_extra_channels, full_frame : full_frame)]
    pub blending_info: BlendingInfo,

    #[size_coder(explicit(nonserialized.num_extra_channels))]
    #[condition(frame_type == FrameType::RegularFrame || frame_type == FrameType::SkipProgressive)]
    #[default_element(BlendingInfo::default(&field_nonserialized))]
    #[nonserialized(num_extra_channels : nonserialized.num_extra_channels, full_frame: full_frame)]
    pub ec_blending_info: Vec<BlendingInfo>,

    #[coder(u2S(0, 1, Bits(8), Bits(32)))]
    #[default(0)]
    #[condition((frame_type == FrameType::RegularFrame ||
        frame_type == FrameType::SkipProgressive) && nonserialized.have_animation)]
    pub duration: u32,

    #[coder(Bits(32))]
    #[default(0)]
    #[condition((frame_type == FrameType::RegularFrame ||
        frame_type == FrameType::SkipProgressive) && nonserialized.have_timecode)]
    timecode: u32,

    #[default(frame_type == FrameType::RegularFrame)]
    #[condition(frame_type == FrameType::RegularFrame || frame_type == FrameType::SkipProgressive)]
    pub is_last: bool,

    #[coder(Bits(2))]
    #[default(0)]
    #[condition(frame_type != FrameType::LFFrame && !is_last)]
    pub save_as_reference: u32,

    // The following 2 fields are not actually serialized, but just used as variables to help with
    // defining later conditions.
    #[default(!is_last && frame_type != FrameType::LFFrame && (duration == 0 || save_as_reference != 0))]
    #[condition(false)]
    pub can_be_referenced: bool,

    #[default(can_be_referenced && blending_info.mode == BlendingMode::Replace && full_frame &&
              (frame_type == FrameType::RegularFrame || frame_type == FrameType::SkipProgressive))]
    #[condition(false)]
    save_before_ct_def_false: bool,

    #[default(frame_type == FrameType::LFFrame)]
    #[condition(frame_type == FrameType::ReferenceOnly || save_before_ct_def_false)]
    pub save_before_ct: bool,

    pub name: String,

    #[default(RestorationFilter::default(&field_nonserialized))]
    #[nonserialized(encoding : encoding)]
    pub restoration_filter: RestorationFilter,

    #[default(Extensions::default())]
    extensions: Extensions,

    // The following fields are not actually serialized, but just used as variables for
    // implementing the methods below.
    #[coder(Bits(0))]
    #[default(if frame_width == 0 { nonserialized.img_width } else { frame_width })]
    #[condition(false)]
    pub width: u32,

    #[coder(Bits(0))]
    #[default(if frame_height == 0 { nonserialized.img_height } else { frame_height })]
    #[condition(false)]
    pub height: u32,

    #[coder(Bits(0))]
    #[default(compute_jpeg_shift(&jpeg_upsampling, &H_SHIFT))]
    #[condition(false)]
    pub maxhs: u32,

    #[coder(Bits(0))]
    #[default(compute_jpeg_shift(&jpeg_upsampling, &V_SHIFT))]
    #[condition(false)]
    pub maxvs: u32,

    #[coder(Bits(0))]
    #[default(nonserialized.num_extra_channels)]
    #[condition(false)]
    pub num_extra_channels: u32,
}

impl FrameHeader {
    pub fn log_group_dim(&self) -> usize {
        (GROUP_DIM.ilog2() - 1 + self.group_size_shift) as usize
    }
    pub fn group_dim(&self) -> usize {
        1 << self.log_group_dim()
    }
    pub fn lf_group_dim(&self) -> usize {
        self.group_dim() * BLOCK_DIM
    }

    pub fn num_groups(&self) -> usize {
        self.size_groups().0 * self.size_groups().1
    }

    pub fn num_lf_groups(&self) -> usize {
        self.size_lf_groups().0 * self.size_lf_groups().1
    }

    pub fn num_toc_entries(&self) -> usize {
        let num_groups = self.num_groups();
        let num_dc_groups = self.num_lf_groups();

        if num_groups == 1 && self.passes.num_passes == 1 {
            1
        } else {
            2 + num_dc_groups + num_groups * self.passes.num_passes as usize
        }
    }

    pub fn duration(&self, animation: &Animation) -> f64 {
        (self.duration as f64) * 1000.0 * (animation.tps_denominator as f64)
            / (animation.tps_numerator as f64)
    }

    pub fn has_patches(&self) -> bool {
        self.flags & Flags::ENABLE_PATCHES != 0
    }

    pub fn has_noise(&self) -> bool {
        self.flags & Flags::ENABLE_NOISE != 0
    }

    pub fn has_splines(&self) -> bool {
        self.flags & Flags::ENABLE_SPLINES != 0
    }
    pub fn has_lf_frame(&self) -> bool {
        self.flags & Flags::USE_LF_FRAME != 0
    }
    pub fn should_do_adaptive_lf_smoothing(&self) -> bool {
        self.flags & Flags::SKIP_ADAPTIVE_LF_SMOOTHING == 0
            && !self.has_lf_frame()
            && self.encoding == Encoding::VarDCT
    }
    pub fn raw_hshift(&self, c: usize) -> usize {
        H_SHIFT[self.jpeg_upsampling[c] as usize]
    }
    pub fn hshift(&self, c: usize) -> usize {
        (self.maxhs as usize) - self.raw_hshift(c)
    }
    pub fn raw_vshift(&self, c: usize) -> usize {
        V_SHIFT[self.jpeg_upsampling[c] as usize]
    }
    pub fn vshift(&self, c: usize) -> usize {
        (self.maxvs as usize) - self.raw_vshift(c)
    }
    pub fn is444(&self) -> bool {
        self.hshift(0) == 0 && self.vshift(0) == 0 &&  // Cb
        self.hshift(2) == 0 && self.vshift(2) == 0 &&  // Cr
        self.hshift(1) == 0 && self.vshift(1) == 0 // Y
    }
    pub fn is420(&self) -> bool {
        self.hshift(0) == 1 && self.vshift(0) == 1 &&  // Cb
        self.hshift(2) == 1 && self.vshift(2) == 1 &&  // Cr
        self.hshift(1) == 0 && self.vshift(1) == 0 // Y
    }
    pub fn is422(&self) -> bool {
        self.hshift(0) == 1 && self.vshift(0) == 0 &&  // Cb
        self.hshift(2) == 1 && self.vshift(2) == 0 &&  // Cr
        self.hshift(1) == 0 && self.vshift(1) == 0 // Y
    }
    pub fn is440(&self) -> bool {
        self.hshift(0) == 0 && self.vshift(0) == 1 &&  // Cb
        self.hshift(2) == 0 && self.vshift(2) == 1 &&  // Cr
        self.hshift(1) == 0 && self.vshift(1) == 0 // Y
    }

    pub fn is_visible(&self) -> bool {
        (self.is_last || self.duration > 0)
            && (self.frame_type == FrameType::RegularFrame
                || self.frame_type == FrameType::SkipProgressive)
    }

    pub fn needs_blending(&self) -> bool {
        if !(self.frame_type == FrameType::RegularFrame
            || self.frame_type == FrameType::SkipProgressive)
        {
            return false;
        }
        let replace_all = self.blending_info.mode == BlendingMode::Replace
            && self
                .ec_blending_info
                .iter()
                .all(|x| x.mode == BlendingMode::Replace);
        self.have_crop || !replace_all
    }

    /// The dimensions of this frame, as coded in the codestream, excluding padding pixels.
    pub fn size(&self) -> (usize, usize) {
        let (width, height) = self.size_upsampled();
        (
            width.div_ceil(self.upsampling as usize),
            height.div_ceil(self.upsampling as usize),
        )
    }

    /// The dimensions of this frame, as coded in the codestream, in 8x8 blocks.
    pub fn size_blocks(&self) -> (usize, usize) {
        (
            self.size().0.div_ceil(BLOCK_DIM << self.maxhs) << self.maxhs,
            self.size().1.div_ceil(BLOCK_DIM << self.maxvs) << self.maxvs,
        )
    }

    /// The dimensions of this frame, as coded in the codestream but including padding pixels.
    pub fn size_padded(&self) -> (usize, usize) {
        if self.encoding == Encoding::Modular {
            self.size()
        } else {
            (
                self.size_blocks().0 * BLOCK_DIM,
                self.size_blocks().1 * BLOCK_DIM,
            )
        }
    }

    /// The dimensions of this frame, after upsampling.
    pub fn size_upsampled(&self) -> (usize, usize) {
        (
            self.width.div_ceil(1 << (3 * self.lf_level)) as usize,
            self.height.div_ceil(1 << (3 * self.lf_level)) as usize,
        )
    }

    pub fn size_padded_upsampled(&self) -> (usize, usize) {
        let (xsize, ysize) = self.size_padded();
        (
            xsize * self.upsampling as usize,
            ysize * self.upsampling as usize,
        )
    }

    /// The dimensions of this frame, in groups.
    pub fn size_groups(&self) -> (usize, usize) {
        (
            self.size().0.div_ceil(self.group_dim()),
            self.size().1.div_ceil(self.group_dim()),
        )
    }

    /// The dimensions of this frame, in LF groups.
    pub fn size_lf_groups(&self) -> (usize, usize) {
        (
            self.size_blocks().0.div_ceil(self.group_dim()),
            self.size_blocks().1.div_ceil(self.group_dim()),
        )
    }

    pub fn block_group_rect(&self, group: usize) -> Rect {
        let group_dims = self.size_groups();
        let block_dims = self.size_blocks();
        let group_dim_in_blocks = self.group_dim() >> 3;
        let gx = group % group_dims.0;
        let gy = group / group_dims.0;
        let origin = (gx * group_dim_in_blocks, gy * group_dim_in_blocks);
        let size = (
            min(
                block_dims.0.checked_sub(origin.0).unwrap(),
                group_dim_in_blocks,
            ),
            min(
                block_dims.1.checked_sub(origin.1).unwrap(),
                group_dim_in_blocks,
            ),
        );
        Rect { origin, size }
    }

    pub fn lf_group_rect(&self, group: usize) -> Rect {
        let lf_dims = self.size_lf_groups();
        let block_dims = self.size_blocks();
        let gx = group % lf_dims.0;
        let gy = group / lf_dims.0;
        let origin = (gx * self.group_dim(), gy * self.group_dim());
        let size = (
            min(
                block_dims.0.checked_sub(origin.0).unwrap(),
                self.group_dim(),
            ),
            min(
                block_dims.1.checked_sub(origin.1).unwrap(),
                self.group_dim(),
            ),
        );
        Rect { origin, size }
    }

    pub fn postprocess(&mut self, nonserialized: &FrameHeaderNonserialized) {
        if self.upsampling > 1 {
            for i in 0..nonserialized.extra_channel_info.len() {
                let dim_shift = nonserialized.extra_channel_info[i].dim_shift();
                self.ec_upsampling[i] <<= dim_shift;
            }
        }
        if self.encoding != Encoding::VarDCT || !nonserialized.xyb_encoded {
            self.x_qm_scale = 2;
        }
    }

    fn check(&self, nonserialized: &FrameHeaderNonserialized) -> Result<(), Error> {
        if self.upsampling > 1
            && let Some((info, upsampling)) = nonserialized
                .extra_channel_info
                .iter()
                .zip(&self.ec_upsampling)
                .find(|(info, ec_upsampling)| {
                    ((*ec_upsampling << info.dim_shift()) < self.upsampling)
                        || (**ec_upsampling > 8)
                })
        {
            return Err(Error::InvalidEcUpsampling(
                self.upsampling,
                info.dim_shift(),
                *upsampling,
            ));
        }

        let num_extra_channels = nonserialized.extra_channel_info.len();
        let uses_alpha =
            |mode| matches!(mode, BlendingMode::Blend | BlendingMode::AlphaWeightedAdd);

        debug_assert_eq!(self.ec_blending_info.len(), num_extra_channels);

        if num_extra_channels > 0
            && uses_alpha(self.blending_info.mode)
            && self.blending_info.alpha_channel as usize >= num_extra_channels
        {
            return Err(Error::InvalidBlendingAlphaChannel(
                self.blending_info.alpha_channel as usize,
                num_extra_channels,
            ));
        }

        for info in &self.ec_blending_info {
            if num_extra_channels > 0
                && uses_alpha(info.mode)
                && info.alpha_channel as usize >= num_extra_channels
            {
                return Err(Error::InvalidBlendingAlphaChannel(
                    info.alpha_channel as usize,
                    num_extra_channels,
                ));
            }
        }

        if self.has_lf_frame() && self.lf_level >= 4 {
            return Err(Error::InvalidLfLevel(self.lf_level));
        }

        if self.passes.num_ds >= self.passes.num_passes {
            return Err(Error::NumPassesTooLarge(
                self.passes.num_ds,
                self.passes.num_passes,
            ));
        }

        for w in self.passes.downsample.windows(2) {
            let [last_ds, ds] = w else { unreachable!() };
            if ds >= last_ds {
                return Err(Error::PassesDownsampleNonDecreasing);
            }
        }

        for w in self.passes.last_pass.windows(2) {
            let [last_lp, lp] = w else { unreachable!() };
            if lp <= last_lp {
                return Err(Error::PassesLastPassNonIncreasing);
            }
        }

        for &lp in self.passes.last_pass.iter() {
            if lp >= self.passes.num_passes {
                return Err(Error::PassesLastPassTooLarge);
            }
        }

        if !self.save_before_ct && !self.full_frame && self.frame_type == FrameType::ReferenceOnly {
            return Err(Error::NonPatchReferenceWithCrop);
        }
        if !self.is444()
            && ((self.flags & Flags::SKIP_ADAPTIVE_LF_SMOOTHING) == 0)
            && self.encoding == Encoding::VarDCT
        {
            return Err(Error::Non444ChromaSubsampling);
        }
        Ok(())
    }
}

#[cfg(test)]
mod test_frame_header {
    use super::super::permutation::Permutation;
    use super::super::toc::Toc;
    use super::*;
    use crate::util::test::read_headers_and_toc;
    use test_log::test;

    #[test]
    fn test_basic() {
        let (_, frame_header, toc) =
            read_headers_and_toc(include_bytes!("../../resources/test/basic.jxl")).unwrap();
        assert_eq!(frame_header.frame_type, FrameType::RegularFrame);
        assert_eq!(frame_header.encoding, Encoding::VarDCT);
        assert_eq!(frame_header.flags, 0);
        assert_eq!(frame_header.upsampling, 1);
        assert_eq!(frame_header.x_qm_scale, 2);
        assert_eq!(frame_header.b_qm_scale, 2);
        assert!(!frame_header.have_crop);
        assert!(!frame_header.save_before_ct);
        assert_eq!(frame_header.name, String::from(""));
        assert_eq!(frame_header.restoration_filter.epf_iters, 1);
        assert_eq!(
            toc,
            Toc {
                permuted: false,
                permutation: Permutation::default(),
                entries: [53].to_vec(),
            }
        )
    }

    #[test]
    fn test_extra_channel() {
        let frame_header =
            read_headers_and_toc(include_bytes!("../../resources/test/extra_channels.jxl"))
                .unwrap()
                .1;
        assert_eq!(frame_header.frame_type, FrameType::RegularFrame);
        assert_eq!(frame_header.encoding, Encoding::Modular);
        assert_eq!(frame_header.flags, 0);
        assert_eq!(frame_header.upsampling, 1);
        assert_eq!(frame_header.ec_upsampling, vec![1]);
        // libjxl x_qm_scale = 2, but condition is false (should be 3 according to the draft)
        // Doesn't actually matter since this is modular mode and the value doesn't get used.
        assert_eq!(frame_header.x_qm_scale, 3);
        assert_eq!(frame_header.b_qm_scale, 2);
        assert!(!frame_header.have_crop);
        assert!(!frame_header.save_before_ct);
        assert_eq!(frame_header.name, String::from(""));
        assert_eq!(frame_header.restoration_filter.epf_iters, 0);
        assert!(!frame_header.restoration_filter.gab);
    }

    #[test]
    fn test_invalid_blending_alpha_channel() {
        let (file_header, mut frame_header, _) =
            read_headers_and_toc(include_bytes!("../../resources/test/extra_channels.jxl"))
                .unwrap();
        let nonserialized = file_header.frame_header_nonserialized();
        frame_header.blending_info.mode = BlendingMode::Blend;
        frame_header.blending_info.alpha_channel = nonserialized.extra_channel_info.len() as u32;

        let err = frame_header.check(&nonserialized).unwrap_err();
        assert!(matches!(err, Error::InvalidBlendingAlphaChannel(_, _)));
    }

    #[test]
    fn test_has_permutation() {
        let (_, frame_header, toc) =
            read_headers_and_toc(include_bytes!("../../resources/test/has_permutation.jxl"))
                .unwrap();
        assert_eq!(frame_header.frame_type, FrameType::RegularFrame);
        assert_eq!(frame_header.encoding, Encoding::VarDCT);
        assert_eq!(frame_header.flags, 0);
        assert_eq!(frame_header.upsampling, 1);
        assert_eq!(frame_header.x_qm_scale, 3);
        assert_eq!(frame_header.b_qm_scale, 2);
        assert!(!frame_header.have_crop);
        assert!(!frame_header.save_before_ct);
        assert_eq!(frame_header.name, String::from(""));
        assert_eq!(frame_header.restoration_filter.epf_iters, 1);
        assert_eq!(
            toc,
            Toc {
                permuted: true,
                permutation: Permutation(std::borrow::Cow::Owned(vec![
                    0u32, 1, 42, 48, 2, 3, 4, 5, 6, 7, 8, 9, 43, 10, 11, 12, 13, 14, 15, 16, 17,
                    44, 18, 19, 20, 21, 22, 23, 24, 25, 45, 26, 27, 28, 29, 30, 31, 32, 33, 46, 34,
                    35, 36, 37, 38, 39, 40, 41, 47,
                ])),
                entries: vec![
                    155, 992, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 9, 9, 9, 9, 9, 9, 9,
                    9, 9, 9, 9, 9, 9, 9, 9, 9, 5, 5, 5, 5, 5, 5, 5, 5, 697, 5, 5, 5, 5, 5, 60,
                ],
            },
        )
    }
}
