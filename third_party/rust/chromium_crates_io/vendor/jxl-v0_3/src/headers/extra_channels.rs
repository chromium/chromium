// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    bit_reader::BitReader,
    error::Error,
    headers::{bit_depth::BitDepth, encodings::*},
};
use jxl_macros::UnconditionalCoder;
use num_derive::FromPrimitive;

#[allow(clippy::upper_case_acronyms)]
#[derive(UnconditionalCoder, Copy, Clone, PartialEq, Debug, FromPrimitive, Eq)]
pub enum ExtraChannel {
    Alpha,
    Depth,
    SpotColor,
    SelectionMask,
    Black,
    CFA,
    Thermal,
    Reserved0,
    Reserved1,
    Reserved2,
    Reserved3,
    Reserved4,
    Reserved5,
    Reserved6,
    Reserved7,
    Unknown,
    Optional,
}

// TODO(veluca): figure out if these fields should be unused.
#[allow(dead_code)]
#[derive(UnconditionalCoder, Debug, Clone)]
#[validate]
pub struct ExtraChannelInfo {
    #[all_default]
    all_default: bool,
    #[default(ExtraChannel::Alpha)]
    pub ec_type: ExtraChannel,
    #[default(BitDepth::default(&field_nonserialized))]
    bit_depth: BitDepth,
    #[coder(u2S(0, 3, 4, Bits(3) + 1))]
    #[default(0)]
    dim_shift: u32,
    name: String,
    // TODO(veluca93): if using Option<bool>, this is None when all_default.
    #[condition(ec_type == ExtraChannel::Alpha)]
    #[default(false)]
    alpha_associated: bool,
    #[condition(ec_type == ExtraChannel::SpotColor)]
    pub spot_color: Option<[f32; 4]>,
    #[condition(ec_type == ExtraChannel::CFA)]
    #[coder(u2S(1, Bits(2), Bits(4) + 3, Bits(8) + 19))]
    cfa_channel: Option<u32>,
}

impl ExtraChannelInfo {
    #[cfg(test)]
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        all_default: bool,
        ec_type: ExtraChannel,
        bit_depth: BitDepth,
        dim_shift: u32,
        name: String,
        alpha_associated: bool,
        spot_color: Option<[f32; 4]>,
        cfa_channel: Option<u32>,
    ) -> ExtraChannelInfo {
        ExtraChannelInfo {
            all_default,
            ec_type,
            bit_depth,
            dim_shift,
            name,
            alpha_associated,
            spot_color,
            cfa_channel,
        }
    }
    pub fn dim_shift(&self) -> u32 {
        self.dim_shift
    }
    pub fn alpha_associated(&self) -> bool {
        self.alpha_associated
    }
    pub fn bit_depth(&self) -> BitDepth {
        self.bit_depth
    }
    fn check(&self, _: &Empty) -> Result<(), Error> {
        if self.dim_shift > 3 {
            Err(Error::DimShiftTooLarge(self.dim_shift))
        } else {
            Ok(())
        }
    }
}
