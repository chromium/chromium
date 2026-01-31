// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    bit_reader::BitReader,
    error::{Error, Result},
    frame::modular::Predictor,
    headers::encodings::*,
};
use jxl_macros::UnconditionalCoder;
use num_derive::FromPrimitive;

use super::encodings;

#[derive(UnconditionalCoder, Debug, PartialEq, Clone)]
pub struct WeightedHeader {
    #[all_default]
    pub all_default: bool,

    #[coder(Bits(5))]
    #[default(16)]
    pub p1c: u32,

    #[coder(Bits(5))]
    #[default(10)]
    pub p2c: u32,

    #[coder(Bits(5))]
    #[default(7)]
    pub p3ca: u32,

    #[coder(Bits(5))]
    #[default(7)]
    pub p3cb: u32,

    #[coder(Bits(5))]
    #[default(7)]
    pub p3cc: u32,

    #[coder(Bits(5))]
    #[default(0)]
    pub p3cd: u32,

    #[coder(Bits(5))]
    #[default(0)]
    pub p3ce: u32,

    #[coder(Bits(4))]
    #[default(0xd)]
    pub w0: u32,

    #[coder(Bits(4))]
    #[default(0xc)]
    pub w1: u32,

    #[coder(Bits(4))]
    #[default(0xc)]
    pub w2: u32,

    #[coder(Bits(4))]
    #[default(0xc)]
    pub w3: u32,
}

impl WeightedHeader {
    pub fn w(&self, i: usize) -> Result<u32> {
        match i {
            0 => Ok(self.w0),
            1 => Ok(self.w1),
            2 => Ok(self.w2),
            3 => Ok(self.w3),
            _ => unreachable!(
            "WeightedHeader::w called with an out-of-bounds index: {}.
            This indicates a logical error in the calling code, which should ensure 'i' is within 0..=3.",
            i),
        }
    }
}

#[derive(UnconditionalCoder, Debug, PartialEq, Clone, Copy)]
pub struct SqueezeParams {
    pub horizontal: bool,
    pub in_place: bool,
    #[coder(u2S(Bits(3), Bits(6) + 8, Bits(10) + 72, Bits(13) + 1096))]
    pub begin_channel: u32,
    #[coder(u2S(1, 2, 3, Bits(4) + 4))]
    pub num_channels: u32,
}

#[derive(UnconditionalCoder, Copy, Clone, PartialEq, Debug, FromPrimitive)]
pub enum TransformId {
    Rct = 0,
    Palette = 1,
    Squeeze = 2,
    Invalid = 3,
}

#[derive(UnconditionalCoder, Debug, PartialEq)]
#[validate]
pub struct Transform {
    #[coder(Bits(2))]
    pub id: TransformId,

    #[condition(id == TransformId::Rct || id == TransformId::Palette)]
    #[coder(u2S(Bits(3), Bits(6) + 8, Bits(10) + 72, Bits(13) + 1096))]
    #[default(0)]
    pub begin_channel: u32,

    #[condition(id == TransformId::Rct)]
    #[coder(u2S(6, Bits(2), Bits(4) + 2, Bits(6) + 10))]
    #[default(6)]
    pub rct_type: u32,

    #[condition(id == TransformId::Palette)]
    #[coder(u2S(1, 3, 4, Bits(13) + 1))]
    #[default(3)]
    pub num_channels: u32,

    #[condition(id == TransformId::Palette)]
    #[coder(u2S(Bits(8), Bits(10) + 256, Bits(12) + 1280, Bits(16)+5376))]
    #[default(256)]
    pub num_colors: u32,

    #[condition(id == TransformId::Palette)]
    #[coder(u2S(0, Bits(8)+1, Bits(10) + 257, Bits(16)+1281))]
    #[default(0)]
    pub num_deltas: u32,

    #[condition(id == TransformId::Palette)]
    #[coder(Bits(4))]
    #[default(0)]
    pub predictor_id: u32,

    #[condition(id == TransformId::Squeeze)]
    #[size_coder(implicit(u2S(0, Bits(4) + 1, Bits(6) + 9, Bits(8) + 41)))]
    #[default(Vec::new())]
    pub squeezes: Vec<SqueezeParams>,
}

impl Transform {
    fn check(&self, _: &encodings::Empty) -> Result<()> {
        if self.id == TransformId::Invalid {
            return Err(Error::InvalidTransformId);
        }

        if self.rct_type >= 42 {
            return Err(Error::InvalidRCT(self.rct_type));
        }

        if self.predictor_id >= Predictor::NUM_PREDICTORS {
            return Err(Error::InvalidPredictor(self.predictor_id));
        }

        Ok(())
    }
}

#[derive(UnconditionalCoder, Debug, PartialEq)]
pub struct GroupHeader {
    pub use_global_tree: bool,
    pub wp_header: WeightedHeader,
    #[size_coder(implicit(u2S(0, 1, Bits(4) + 2, Bits(8) + 18)))]
    pub transforms: Vec<Transform>,
}
