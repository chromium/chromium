// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::fmt;

use jxl_macros::UnconditionalCoder;
use num_derive::FromPrimitive;

use crate::api::{adapt_to_xyz_d50, primaries_to_xyz};
use crate::bit_reader::BitReader;
use crate::error::Error;
use crate::headers::encodings::*;

#[allow(clippy::upper_case_acronyms)]
#[derive(UnconditionalCoder, Copy, Clone, PartialEq, Debug, FromPrimitive)]
pub enum ColorSpace {
    RGB,
    Gray,
    XYB,
    Unknown,
}

impl fmt::Display for ColorSpace {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                ColorSpace::RGB => "RGB",
                ColorSpace::Gray => "Gra",
                ColorSpace::XYB => "XYB",
                ColorSpace::Unknown => "CS?",
            }
        )
    }
}

#[allow(clippy::upper_case_acronyms)]
#[derive(UnconditionalCoder, Copy, Clone, PartialEq, Debug, FromPrimitive)]
pub enum WhitePoint {
    D65 = 1,
    Custom = 2,
    E = 10,
    DCI = 11,
}

impl WhitePoint {
    pub fn to_xy_coords(&self, custom: &CustomXY) -> (f32, f32) {
        match self {
            WhitePoint::D65 => (0.3127, 0.3290),
            WhitePoint::E => (1.0 / 3.0, 1.0 / 3.0),
            WhitePoint::DCI => (0.314, 0.351),
            WhitePoint::Custom => custom.as_f32_coords(),
        }
    }
}

#[allow(clippy::upper_case_acronyms)]
#[derive(UnconditionalCoder, Copy, Clone, PartialEq, Debug, FromPrimitive)]
pub enum Primaries {
    SRGB = 1,
    Custom = 2,
    BT2100 = 9,
    P3 = 11,
}

#[allow(clippy::upper_case_acronyms)]
#[derive(UnconditionalCoder, Copy, Clone, PartialEq, Debug, FromPrimitive)]
pub enum TransferFunction {
    BT709 = 1,
    Unknown = 2,
    Linear = 8,
    SRGB = 13,
    PQ = 16,
    DCI = 17,
    HLG = 18,
}

#[derive(UnconditionalCoder, Copy, Clone, PartialEq, Debug, FromPrimitive)]
pub enum RenderingIntent {
    Perceptual = 0,
    Relative,
    Saturation,
    Absolute,
}

impl fmt::Display for RenderingIntent {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                RenderingIntent::Perceptual => "Per",
                RenderingIntent::Relative => "Rel",
                RenderingIntent::Saturation => "Sat",
                RenderingIntent::Absolute => "Abs",
            }
        )
    }
}

#[derive(UnconditionalCoder, Debug, Clone)]
pub struct CustomXY {
    #[default(0)]
    #[coder(u2S(Bits(19), Bits(19) + 524288, Bits(20) + 1048576, Bits(21) + 2097152))]
    pub x: i32,
    #[default(0)]
    #[coder(u2S(Bits(19), Bits(19) + 524288, Bits(20) + 1048576, Bits(21) + 2097152))]
    pub y: i32,
}

impl CustomXY {
    /// Converts the stored scaled integer coordinates to f32 (x, y) values.
    pub fn as_f32_coords(&self) -> (f32, f32) {
        (self.x as f32 / 1_000_000.0, self.y as f32 / 1_000_000.0)
    }

    pub fn from_f32_coords(x: f32, y: f32) -> Self {
        Self {
            x: (x * 1_000_000.0).round() as i32,
            y: (y * 1_000_000.0).round() as i32,
        }
    }
}

pub struct CustomTransferFunctionNonserialized {
    color_space: ColorSpace,
}

#[derive(UnconditionalCoder, Debug, Clone)]
#[nonserialized(CustomTransferFunctionNonserialized)]
#[validate]
pub struct CustomTransferFunction {
    #[condition(nonserialized.color_space != ColorSpace::XYB)]
    #[default(false)]
    pub have_gamma: bool,
    #[condition(have_gamma)]
    #[default(3333333)] // XYB gamma
    #[coder(Bits(24))]
    pub gamma: u32,
    #[condition(!have_gamma && nonserialized.color_space != ColorSpace::XYB)]
    #[default(TransferFunction::SRGB)]
    pub transfer_function: TransferFunction,
}

impl CustomTransferFunction {
    #[cfg(test)]
    pub fn empty() -> CustomTransferFunction {
        CustomTransferFunction {
            have_gamma: false,
            gamma: 0,
            transfer_function: TransferFunction::Unknown,
        }
    }
    pub fn gamma(&self) -> f32 {
        assert!(self.have_gamma);
        self.gamma as f32 * 0.0000001
    }

    pub fn check(&self, _: &CustomTransferFunctionNonserialized) -> Result<(), Error> {
        if self.have_gamma {
            let gamma = self.gamma();
            if gamma > 1.0 || gamma * 8192.0 < 1.0 {
                Err(Error::InvalidGamma(gamma))
            } else {
                Ok(())
            }
        } else {
            Ok(())
        }
    }
}

#[derive(UnconditionalCoder, Debug, Clone)]
#[validate]
pub struct ColorEncoding {
    // all_default is never read.
    #[allow(dead_code)]
    #[all_default]
    all_default: bool,
    #[default(false)]
    pub want_icc: bool,
    #[default(ColorSpace::RGB)]
    pub color_space: ColorSpace,
    #[condition(!want_icc && color_space != ColorSpace::XYB)]
    #[default(WhitePoint::D65)]
    pub white_point: WhitePoint,
    // TODO(veluca): can this be merged in the enum?!
    #[condition(white_point == WhitePoint::Custom)]
    #[default(CustomXY::default(&field_nonserialized))]
    pub white: CustomXY,
    #[condition(!want_icc && color_space != ColorSpace::XYB && color_space != ColorSpace::Gray)]
    #[default(Primaries::SRGB)]
    pub primaries: Primaries,
    #[condition(primaries == Primaries::Custom)]
    #[default([CustomXY::default(&field_nonserialized), CustomXY::default(&field_nonserialized), CustomXY::default(&field_nonserialized)])]
    pub custom_primaries: [CustomXY; 3],
    #[condition(!want_icc)]
    #[default(CustomTransferFunction::default(&field_nonserialized))]
    #[nonserialized(color_space: color_space)]
    pub tf: CustomTransferFunction,
    #[condition(!want_icc)]
    #[default(RenderingIntent::Relative)]
    pub rendering_intent: RenderingIntent,
}

impl ColorEncoding {
    pub fn check(&self, _: &Empty) -> Result<(), Error> {
        if self.color_space == ColorSpace::Unknown
            || self.tf.transfer_function == TransferFunction::Unknown
            || self.color_space == ColorSpace::XYB
        {
            return Err(Error::InvalidColorEncoding);
        }
        let (wx, wy) = self.white_point.to_xy_coords(&self.white);
        if self.white_point == WhitePoint::Custom {
            adapt_to_xyz_d50(wx, wy)?;
        }
        if self.primaries == Primaries::Custom {
            primaries_to_xyz(
                self.custom_primaries[0].as_f32_coords().0,
                self.custom_primaries[0].as_f32_coords().1,
                self.custom_primaries[1].as_f32_coords().0,
                self.custom_primaries[1].as_f32_coords().1,
                self.custom_primaries[2].as_f32_coords().0,
                self.custom_primaries[2].as_f32_coords().1,
                wx,
                wy,
            )?;
        }
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_custom_primaries_with_standard_white_point() {
        let mut encoding = ColorEncoding::default(&Empty {});
        encoding.primaries = Primaries::Custom;
        encoding.white_point = WhitePoint::D65;
        // Set standard sRGB-like primaries
        encoding.custom_primaries = [
            CustomXY::from_f32_coords(0.64, 0.33),
            CustomXY::from_f32_coords(0.30, 0.60),
            CustomXY::from_f32_coords(0.15, 0.06),
        ];
        assert!(encoding.check(&Empty {}).is_ok());
    }

    #[test]
    fn test_custom_white_point_invalid() {
        let mut encoding = ColorEncoding::default(&Empty {});
        encoding.white_point = WhitePoint::Custom;
        encoding.white = CustomXY::from_f32_coords(0.3127, 0.0);
        assert!(encoding.check(&Empty {}).is_err());
    }
}
