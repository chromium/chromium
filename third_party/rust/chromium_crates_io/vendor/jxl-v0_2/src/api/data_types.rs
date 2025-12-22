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

    /// Returns the byte representation of opaque alpha (1.0) for this format.
    pub(crate) fn opaque_alpha_bytes(&self) -> Vec<u8> {
        match self {
            JxlDataFormat::U8 { bit_depth } => {
                let val = (1u16 << bit_depth) - 1;
                vec![val as u8]
            }
            JxlDataFormat::U16 {
                endianness,
                bit_depth,
            } => {
                let val = (1u32 << bit_depth) - 1;
                let val = val as u16;
                if *endianness == Endianness::LittleEndian {
                    val.to_le_bytes().to_vec()
                } else {
                    val.to_be_bytes().to_vec()
                }
            }
            JxlDataFormat::F16 { endianness } => {
                // 1.0 in f16 is 0x3C00
                let val: u16 = 0x3C00;
                if *endianness == Endianness::LittleEndian {
                    val.to_le_bytes().to_vec()
                } else {
                    val.to_be_bytes().to_vec()
                }
            }
            JxlDataFormat::F32 { endianness } => {
                let val: f32 = 1.0;
                if *endianness == Endianness::LittleEndian {
                    val.to_le_bytes().to_vec()
                } else {
                    val.to_be_bytes().to_vec()
                }
            }
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

impl JxlPixelFormat {
    /// Creates an RGBA 8-bit pixel format.
    pub fn rgba8(num_extra_channels: usize) -> Self {
        Self {
            color_type: JxlColorType::Rgba,
            color_data_format: Some(JxlDataFormat::U8 { bit_depth: 8 }),
            extra_channel_format: vec![
                Some(JxlDataFormat::U8 { bit_depth: 8 });
                num_extra_channels
            ],
        }
    }

    /// Creates an RGBA 16-bit pixel format.
    pub fn rgba16(num_extra_channels: usize) -> Self {
        Self {
            color_type: JxlColorType::Rgba,
            color_data_format: Some(JxlDataFormat::U16 {
                endianness: Endianness::native(),
                bit_depth: 16,
            }),
            extra_channel_format: vec![
                Some(JxlDataFormat::U16 {
                    endianness: Endianness::native(),
                    bit_depth: 16,
                });
                num_extra_channels
            ],
        }
    }

    /// Creates an RGBA f16 pixel format.
    pub fn rgba_f16(num_extra_channels: usize) -> Self {
        Self {
            color_type: JxlColorType::Rgba,
            color_data_format: Some(JxlDataFormat::F16 {
                endianness: Endianness::native(),
            }),
            extra_channel_format: vec![
                Some(JxlDataFormat::F16 {
                    endianness: Endianness::native(),
                });
                num_extra_channels
            ],
        }
    }

    /// Creates an RGBA f32 pixel format.
    pub fn rgba_f32(num_extra_channels: usize) -> Self {
        Self {
            color_type: JxlColorType::Rgba,
            color_data_format: Some(JxlDataFormat::F32 {
                endianness: Endianness::native(),
            }),
            extra_channel_format: vec![
                Some(JxlDataFormat::F32 {
                    endianness: Endianness::native(),
                });
                num_extra_channels
            ],
        }
    }
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
