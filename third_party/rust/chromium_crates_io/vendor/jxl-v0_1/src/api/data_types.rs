// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{headers::extra_channels::ExtraChannel, image::DataTypeTag};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum JxlColorType {
    Grayscale,
    GrayscaleAlpha,
    Rgb,
    Rgba,
    Bgr,
    Bgra,
}

impl JxlColorType {
    pub fn has_alpha(&self) -> bool {
        match self {
            Self::Grayscale => false,
            Self::GrayscaleAlpha => true,
            Self::Rgb | Self::Bgr => false,
            Self::Rgba | Self::Bgra => true,
        }
    }
    pub fn samples_per_pixel(&self) -> usize {
        match self {
            Self::Grayscale => 1,
            Self::GrayscaleAlpha => 2,
            Self::Rgb | Self::Bgr => 3,
            Self::Rgba | Self::Bgra => 4,
        }
    }
    pub fn is_grayscale(&self) -> bool {
        match self {
            Self::Grayscale => true,
            Self::GrayscaleAlpha => true,
            Self::Rgb | Self::Bgr => false,
            Self::Rgba | Self::Bgra => false,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Endianness {
    LittleEndian,
    BigEndian,
}

impl Endianness {
    pub fn native() -> Self {
        #[cfg(target_endian = "little")]
        {
            Endianness::LittleEndian
        }
        #[cfg(target_endian = "big")]
        {
            Endianness::BigEndian
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum JxlDataFormat {
    U8 {
        bit_depth: u8,
    },
    U16 {
        endianness: Endianness,
        bit_depth: u8,
    },
    F16 {
        endianness: Endianness,
    },
    F32 {
        endianness: Endianness,
    },
}

impl JxlDataFormat {
    pub fn bytes_per_sample(&self) -> usize {
        match self {
            Self::U8 { .. } => 1,
            Self::U16 { .. } | Self::F16 { .. } => 2,
            Self::F32 { .. } => 4,
        }
    }

    pub fn f32() -> Self {
        Self::F32 {
            endianness: Endianness::native(),
        }
    }

    pub(crate) fn data_type(&self) -> DataTypeTag {
        match self {
            JxlDataFormat::U8 { .. } => DataTypeTag::U8,
            JxlDataFormat::U16 { .. } => DataTypeTag::U16,
            JxlDataFormat::F16 { .. } => DataTypeTag::F16,
            JxlDataFormat::F32 { .. } => DataTypeTag::F32,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct JxlPixelFormat {
    pub color_type: JxlColorType,
    // None -> ignore
    pub color_data_format: Option<JxlDataFormat>,
    pub extra_channel_format: Vec<Option<JxlDataFormat>>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum JxlBitDepth {
    Int {
        bits_per_sample: u32,
    },
    Float {
        bits_per_sample: u32,
        exponent_bits_per_sample: u32,
    },
}

impl JxlBitDepth {
    pub fn bits_per_sample(&self) -> u32 {
        match self {
            JxlBitDepth::Int { bits_per_sample: b } => *b,
            JxlBitDepth::Float {
                bits_per_sample: b, ..
            } => *b,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct JxlExtraChannel {
    pub ec_type: ExtraChannel,
    pub alpha_associated: bool,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct JxlAnimation {
    pub tps_numerator: u32,
    pub tps_denominator: u32,
    pub num_loops: u32,
    pub have_timecodes: bool,
}

#[derive(Clone, Debug)]
pub struct JxlFrameHeader {
    pub name: String,
    pub duration: Option<f64>,
    /// Frame size (width, height)
    pub size: (usize, usize),
}
