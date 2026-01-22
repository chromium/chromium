// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{borrow::Cow, f32::consts::SQRT_2, sync::OnceLock};

use crate::util::f16;

use crate::{
    BLOCK_DIM, BLOCK_SIZE,
    bit_reader::BitReader,
    error::{
        Error::{
            HfQuantFactorTooSmall, InvalidDistanceBand, InvalidQuantEncoding,
            InvalidQuantEncodingMode, InvalidQuantizationTableWeight, InvalidRawQuantTable,
        },
        Result,
    },
    frame::{
        LfGlobalState,
        modular::{ModularChannel, ModularStreamId, decode::decode_modular_subbitstream},
    },
    headers::{bit_depth::BitDepth, frame_header::FrameHeader},
    image::Rect,
};
use jxl_transforms::transform_map::*;

pub const INV_LF_QUANT: [f32; 3] = [4096.0, 512.0, 256.0];

pub const LF_QUANT: [f32; 3] = [
    1.0 / INV_LF_QUANT[0],
    1.0 / INV_LF_QUANT[1],
    1.0 / INV_LF_QUANT[2],
];

const ALMOST_ZERO: f32 = 1e-8;

const LOG2_NUM_QUANT_MODES: usize = 3;

#[derive(Debug)]
pub struct DctQuantWeightParams {
    params: [[f32; Self::MAX_DISTANCE_BANDS]; 3],
    num_bands: usize,
}
impl DctQuantWeightParams {
    const LOG2_MAX_DISTANCE_BANDS: usize = 4;
    const MAX_DISTANCE_BANDS: usize = 1 + (1 << Self::LOG2_MAX_DISTANCE_BANDS);

    #[inline(never)]
    pub fn from_array<const N: usize>(values: &[[f32; N]; 3]) -> Self {
        let mut result = Self {
            params: [[0.0; Self::MAX_DISTANCE_BANDS]; 3],
            num_bands: N,
        };
        for (params, values) in result.params.iter_mut().zip(values) {
            params[..values.len()].copy_from_slice(values);
        }
        result
    }

    #[inline(never)]
    pub fn decode(br: &mut BitReader) -> Result<Self> {
        let num_bands = br.read(Self::LOG2_MAX_DISTANCE_BANDS)? as usize + 1;
        let mut params = [[0.0; Self::MAX_DISTANCE_BANDS]; 3];
        for row in params.iter_mut() {
            for item in row[..num_bands].iter_mut() {
                *item = f32::from(f16::from_bits(br.read(16)? as u16));
            }
            if row[0] < ALMOST_ZERO {
                return Err(HfQuantFactorTooSmall(row[0]));
            }
            row[0] *= 64.0;
        }
        Ok(DctQuantWeightParams { params, num_bands })
    }
}

#[allow(clippy::large_enum_variant)]
#[derive(Debug)]
pub enum QuantEncoding {
    Library,
    // a.k.a. "Hornuss"
    Identity {
        xyb_weights: [[f32; 3]; 3],
    },
    Dct2 {
        xyb_weights: [[f32; 6]; 3],
    },
    Dct4 {
        params: DctQuantWeightParams,
        xyb_mul: [[f32; 2]; 3],
    },
    Dct4x8 {
        params: DctQuantWeightParams,
        xyb_mul: [f32; 3],
    },
    Afv {
        params4x8: DctQuantWeightParams,
        params4x4: DctQuantWeightParams,
        weights: [[f32; 9]; 3],
    },
    Dct {
        params: DctQuantWeightParams,
    },
    Raw {
        qtable: Vec<i32>,
        qtable_den: f32,
    },
}

impl QuantEncoding {
    // TODO(veluca): figure out if this should actually be unused.
    #[allow(dead_code)]
    pub fn raw_from_qtable(qtable: Vec<i32>, shift: i32) -> Self {
        Self::Raw {
            qtable,
            qtable_den: (1 << shift) as f32 * (1.0 / (8.0 * 255.0)),
        }
    }

    pub fn decode(
        mut required_size_x: usize,
        mut required_size_y: usize,
        index: usize,
        header: &FrameHeader,
        lf_global: &LfGlobalState,
        br: &mut BitReader,
    ) -> Result<Self> {
        let required_size = required_size_x * required_size_y;
        required_size_x *= BLOCK_DIM;
        required_size_y *= BLOCK_DIM;
        let mode = br.read(LOG2_NUM_QUANT_MODES)? as u8;
        match mode {
            0 => Ok(Self::Library),
            1 => {
                if required_size != 1 {
                    return Err(InvalidQuantEncoding {
                        mode,
                        required_size,
                    });
                }
                let mut xyb_weights = [[0.0; 3]; 3];
                for row in xyb_weights.iter_mut() {
                    for item in row.iter_mut() {
                        *item = f32::from(f16::from_bits(br.read(16)? as u16));
                        if item.abs() < ALMOST_ZERO {
                            return Err(HfQuantFactorTooSmall(*item));
                        }
                        *item *= 64.0;
                    }
                }
                Ok(Self::Identity { xyb_weights })
            }
            2 => {
                if required_size != 1 {
                    return Err(InvalidQuantEncoding {
                        mode,
                        required_size,
                    });
                }
                let mut xyb_weights = [[0.0; 6]; 3];
                for row in xyb_weights.iter_mut() {
                    for item in row.iter_mut() {
                        *item = f32::from(f16::from_bits(br.read(16)? as u16));
                        if item.abs() < ALMOST_ZERO {
                            return Err(HfQuantFactorTooSmall(*item));
                        }
                        *item *= 64.0;
                    }
                }
                Ok(Self::Dct2 { xyb_weights })
            }
            3 => {
                if required_size != 1 {
                    return Err(InvalidQuantEncoding {
                        mode,
                        required_size,
                    });
                }
                let mut xyb_mul = [[0.0; 2]; 3];
                for row in xyb_mul.iter_mut() {
                    for item in row.iter_mut() {
                        *item = f32::from(f16::from_bits(br.read(16)? as u16));
                        if item.abs() < ALMOST_ZERO {
                            return Err(HfQuantFactorTooSmall(*item));
                        }
                    }
                }
                let params = DctQuantWeightParams::decode(br)?;
                Ok(Self::Dct4 { params, xyb_mul })
            }
            4 => {
                if required_size != 1 {
                    return Err(InvalidQuantEncoding {
                        mode,
                        required_size,
                    });
                }
                let mut xyb_mul = [0.0; 3];
                for item in xyb_mul.iter_mut() {
                    *item = f32::from(f16::from_bits(br.read(16)? as u16));
                    if item.abs() < ALMOST_ZERO {
                        return Err(HfQuantFactorTooSmall(*item));
                    }
                }
                let params = DctQuantWeightParams::decode(br)?;
                Ok(Self::Dct4x8 { params, xyb_mul })
            }
            5 => {
                if required_size != 1 {
                    return Err(InvalidQuantEncoding {
                        mode,
                        required_size,
                    });
                }
                let mut weights = [[0.0; 9]; 3];
                for row in weights.iter_mut() {
                    for item in row.iter_mut() {
                        *item = f32::from(f16::from_bits(br.read(16)? as u16));
                    }
                    for item in row[0..6].iter_mut() {
                        *item *= 64.0;
                    }
                }
                let params4x8 = DctQuantWeightParams::decode(br)?;
                let params4x4 = DctQuantWeightParams::decode(br)?;
                Ok(Self::Afv {
                    params4x8,
                    params4x4,
                    weights,
                })
            }
            6 => {
                let params = DctQuantWeightParams::decode(br)?;
                Ok(Self::Dct { params })
            }
            7 => {
                let qtable_den = f32::from(f16::from_bits(br.read(16)? as u16));
                if qtable_den < ALMOST_ZERO {
                    // qtable[] values are already checked for <= 0 so the denominator may not be negative.
                    return Err(InvalidRawQuantTable);
                }
                let bit_depth = BitDepth::integer_samples(8);
                let mut image = [
                    ModularChannel::new((required_size_x, required_size_y), bit_depth)?,
                    ModularChannel::new((required_size_x, required_size_y), bit_depth)?,
                    ModularChannel::new((required_size_x, required_size_y), bit_depth)?,
                ];
                let stream_id = ModularStreamId::QuantTable(index).get_id(header);
                decode_modular_subbitstream(
                    image.iter_mut().collect(),
                    stream_id,
                    None,
                    &lf_global.tree,
                    br,
                )?;
                let mut qtable = Vec::with_capacity(required_size_x * required_size_y * 3);
                for channel in image.iter_mut() {
                    for entry in channel
                        .data
                        .get_rect(Rect {
                            size: (required_size_x, required_size_y),
                            origin: (0, 0),
                        })
                        .iter()
                    {
                        qtable.push(entry);
                        if entry <= 0 {
                            return Err(InvalidRawQuantTable);
                        }
                    }
                }
                Ok(Self::Raw { qtable, qtable_den })
            }
            _ => Err(InvalidQuantEncoding {
                mode,
                required_size,
            }),
        }
    }
}

#[derive(Clone, Copy, Debug)]
enum QuantTable {
    // Update QuantTable::VALUES when changing this!
    Dct,
    Identity,
    Dct2x2,
    Dct4x4,
    Dct16x16,
    Dct32x32,
    // Dct16x8
    Dct8x16,
    // Dct32x8
    Dct8x32,
    // Dct32x16
    Dct16x32,
    Dct4x8,
    // Dct8x4
    Afv0,
    // Afv1
    // Afv2
    // Afv3
    Dct64x64,
    // Dct64x32,
    Dct32x64,
    Dct128x128,
    // Dct128x64,
    Dct64x128,
    Dct256x256,
    // Dct256x128,
    Dct128x256,
}

impl QuantTable {
    pub const CARDINALITY: usize = Self::VALUES.len();
    pub const VALUES: [QuantTable; 17] = [
        QuantTable::Dct,
        QuantTable::Identity,
        QuantTable::Dct2x2,
        QuantTable::Dct4x4,
        QuantTable::Dct16x16,
        QuantTable::Dct32x32,
        // QuantTable::Dct16x8
        QuantTable::Dct8x16,
        // QuantTable::Dct32x8
        QuantTable::Dct8x32,
        // QuantTable::Dct32x16
        QuantTable::Dct16x32,
        QuantTable::Dct4x8,
        // QuantTable::Dct8x4
        QuantTable::Afv0,
        // QuantTable::Afv1
        // QuantTable::Afv2
        // QuantTable::Afv3
        QuantTable::Dct64x64,
        // QuantTable::Dct64x32,
        QuantTable::Dct32x64,
        QuantTable::Dct128x128,
        // QuantTable::Dct128x64,
        QuantTable::Dct64x128,
        QuantTable::Dct256x256,
        // QuantTable::Dct256x128,
        QuantTable::Dct128x256,
    ];
    fn for_strategy(strategy: HfTransformType) -> QuantTable {
        match strategy {
            HfTransformType::DCT => QuantTable::Dct,
            HfTransformType::IDENTITY => QuantTable::Identity,
            HfTransformType::DCT2X2 => QuantTable::Dct2x2,
            HfTransformType::DCT4X4 => QuantTable::Dct4x4,
            HfTransformType::DCT16X16 => QuantTable::Dct16x16,
            HfTransformType::DCT32X32 => QuantTable::Dct32x32,
            HfTransformType::DCT16X8 | HfTransformType::DCT8X16 => QuantTable::Dct8x16,
            HfTransformType::DCT32X8 | HfTransformType::DCT8X32 => QuantTable::Dct8x32,
            HfTransformType::DCT32X16 | HfTransformType::DCT16X32 => QuantTable::Dct16x32,
            HfTransformType::DCT4X8 | HfTransformType::DCT8X4 => QuantTable::Dct4x8,
            HfTransformType::AFV0
            | HfTransformType::AFV1
            | HfTransformType::AFV2
            | HfTransformType::AFV3 => QuantTable::Afv0,
            HfTransformType::DCT64X64 => QuantTable::Dct64x64,
            HfTransformType::DCT64X32 | HfTransformType::DCT32X64 => QuantTable::Dct32x64,
            HfTransformType::DCT128X128 => QuantTable::Dct128x128,
            HfTransformType::DCT128X64 | HfTransformType::DCT64X128 => QuantTable::Dct64x128,
            HfTransformType::DCT256X256 => QuantTable::Dct256x256,
            HfTransformType::DCT256X128 | HfTransformType::DCT128X256 => QuantTable::Dct128x256,
        }
    }
}

pub struct DequantMatrices {
    /// 17 separate tables, one per QuantTable type.
    /// Uses Cow to allow zero-copy borrowing from static cache for library tables.
    tables: [Cow<'static, [f32]>; QuantTable::CARDINALITY],
}

/// Cached computed library tables per QuantTable type.
/// Each entry contains the computed f32 weights for all 3 channels.
/// Computed lazily on first access.
static LIBRARY_TABLES: [OnceLock<Box<[f32]>>; QuantTable::CARDINALITY] = [
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
    OnceLock::new(),
];

#[allow(clippy::excessive_precision)]
impl DequantMatrices {
    fn dct() -> QuantEncoding {
        QuantEncoding::Dct {
            params: DctQuantWeightParams::from_array(&[
                [3150.0, 0.0, -0.4, -0.4, -0.4, -2.0],
                [560.0, 0.0, -0.3, -0.3, -0.3, -0.3],
                [512.0, -2.0, -1.0, 0.0, -1.0, -2.0],
            ]),
        }
    }
    fn id() -> QuantEncoding {
        QuantEncoding::Identity {
            xyb_weights: [
                [280.0, 3160.0, 3160.0],
                [60.0, 864.0, 864.0],
                [18.0, 200.0, 200.0],
            ],
        }
    }
    fn dct2x2() -> QuantEncoding {
        QuantEncoding::Dct2 {
            xyb_weights: [
                [3840.0, 2560.0, 1280.0, 640.0, 480.0, 300.0],
                [960.0, 640.0, 320.0, 180.0, 140.0, 120.0],
                [640.0, 320.0, 128.0, 64.0, 32.0, 16.0],
            ],
        }
    }
    fn dct4x4() -> QuantEncoding {
        QuantEncoding::Dct4 {
            params: DctQuantWeightParams::from_array(&[
                [2200.0, 0.0, 0.0, 0.0],
                [392.0, 0.0, 0.0, 0.0],
                [112.0, -0.25, -0.25, -0.5],
            ]),
            xyb_mul: [[1.0, 1.0], [1.0, 1.0], [1.0, 1.0]],
        }
    }
    fn dct16x16() -> QuantEncoding {
        QuantEncoding::Dct {
            params: DctQuantWeightParams::from_array(&[
                [
                    8996.8725711814115328,
                    -1.3000777393353804,
                    -0.49424529824571225,
                    -0.439093774457103443,
                    -0.6350101832695744,
                    -0.90177264050827612,
                    -1.6162099239887414,
                ],
                [
                    3191.48366296844234752,
                    -0.67424582104194355,
                    -0.80745813428471001,
                    -0.44925837484843441,
                    -0.35865440981033403,
                    -0.31322389111877305,
                    -0.37615025315725483,
                ],
                [
                    1157.50408145487200256,
                    -2.0531423165804414,
                    -1.4,
                    -0.50687130033378396,
                    -0.42708730624733904,
                    -1.4856834539296244,
                    -4.9209142884401604,
                ],
            ]),
        }
    }
    fn dct32x32() -> QuantEncoding {
        QuantEncoding::Dct {
            params: DctQuantWeightParams::from_array(&[
                [
                    15718.40830982518931456,
                    -1.025,
                    -0.98,
                    -0.9012,
                    -0.4,
                    -0.48819395464,
                    -0.421064,
                    -0.27,
                ],
                [
                    7305.7636810695983104,
                    -0.8041958212306401,
                    -0.7633036457487539,
                    -0.55660379990111464,
                    -0.49785304658857626,
                    -0.43699592683512467,
                    -0.40180866526242109,
                    -0.27321683125358037,
                ],
                [
                    3803.53173721215041536,
                    -3.060733579805728,
                    -2.0413270132490346,
                    -2.0235650159727417,
                    -0.5495389509954993,
                    -0.4,
                    -0.4,
                    -0.3,
                ],
            ]),
        }
    }

    // dct16x8
    fn dct8x16() -> QuantEncoding {
        QuantEncoding::Dct {
            params: DctQuantWeightParams::from_array(&[
                [7240.7734393502, -0.7, -0.7, -0.2, -0.2, -0.2, -0.5],
                [1448.15468787004, -0.5, -0.5, -0.5, -0.2, -0.2, -0.2],
                [506.854140754517, -1.4, -0.2, -0.5, -0.5, -1.5, -3.6],
            ]),
        }
    }

    // dct32x8
    fn dct8x32() -> QuantEncoding {
        QuantEncoding::Dct {
            params: DctQuantWeightParams::from_array(&[
                [
                    16283.2494710648897,
                    -1.7812845336559429,
                    -1.6309059012653515,
                    -1.0382179034313539,
                    -0.85,
                    -0.7,
                    -0.9,
                    -1.2360638576849587,
                ],
                [
                    5089.15750884921511936,
                    -0.320049391452786891,
                    -0.35362849922161446,
                    -0.30340000000000003,
                    -0.61,
                    -0.5,
                    -0.5,
                    -0.6,
                ],
                [
                    3397.77603275308720128,
                    -0.321327362693153371,
                    -0.34507619223117997,
                    -0.70340000000000003,
                    -0.9,
                    -1.0,
                    -1.0,
                    -1.1754605576265209,
                ],
            ]),
        }
    }

    // dct32x16
    fn dct16x32() -> QuantEncoding {
        QuantEncoding::Dct {
            params: DctQuantWeightParams::from_array(&[
                [
                    13844.97076442300573,
                    -0.97113799999999995,
                    -0.658,
                    -0.42026,
                    -0.22712,
                    -0.2206,
                    -0.226,
                    -0.6,
                ],
                [
                    4798.964084220744293,
                    -0.61125308982767057,
                    -0.83770786552491361,
                    -0.79014862079498627,
                    -0.2692727459704829,
                    -0.38272769465388551,
                    -0.22924222653091453,
                    -0.20719098826199578,
                ],
                [
                    1807.236946760964614,
                    -1.2,
                    -1.2,
                    -0.7,
                    -0.7,
                    -0.7,
                    -0.4,
                    -0.5,
                ],
            ]),
        }
    }

    // dct8x4
    fn dct4x8() -> QuantEncoding {
        QuantEncoding::Dct4x8 {
            params: DctQuantWeightParams::from_array(&[
                [
                    2198.050556016380522,
                    -0.96269623020744692,
                    -0.76194253026666783,
                    -0.6551140670773547,
                ],
                [
                    764.3655248643528689,
                    -0.92630200888366945,
                    -0.9675229603596517,
                    -0.27845290869168118,
                ],
                [
                    527.107573587542228,
                    -1.4594385811273854,
                    -1.450082094097871593,
                    -1.5843722511996204,
                ],
            ]),
            xyb_mul: [1.0, 1.0, 1.0],
        }
    }
    // AFV
    fn afv0() -> QuantEncoding {
        let QuantEncoding::Dct4x8 {
            params: params4x8, ..
        } = Self::dct4x8()
        else {
            unreachable!();
        };
        let QuantEncoding::Dct4 {
            params: params4x4, ..
        } = Self::dct4x4()
        else {
            unreachable!()
        };
        QuantEncoding::Afv {
            params4x8,
            params4x4,
            weights: [
                [
                    3072.0, 3072.0, // 4x4/4x8 DC tendency.
                    256.0, 256.0, 256.0, // AFV corner.
                    414.0, 0.0, 0.0, 0.0, // AFV high freqs.
                ],
                [
                    1024.0, 1024.0, // 4x4/4x8 DC tendency.
                    50.0, 50.0, 50.0, // AFV corner.
                    58.0, 0.0, 0.0, 0.0, // AFV high freqs.
                ],
                [
                    384.0, 384.0, // 4x4/4x8 DC tendency.
                    12.0, 12.0, 12.0, // AFV corner.
                    22.0, -0.25, -0.25, -0.25, // AFV high freqs.
                ],
            ],
        }
    }

    fn dct64x64() -> QuantEncoding {
        QuantEncoding::Dct {
            params: DctQuantWeightParams::from_array(&[
                [
                    0.9 * 26629.073922049845,
                    -1.025,
                    -0.78,
                    -0.65012,
                    -0.19041574084286472,
                    -0.20819395464,
                    -0.421064,
                    -0.32733845535848671,
                ],
                [
                    0.9 * 9311.3238710010046,
                    -0.3041958212306401,
                    -0.3633036457487539,
                    -0.35660379990111464,
                    -0.3443074455424403,
                    -0.33699592683512467,
                    -0.30180866526242109,
                    -0.27321683125358037,
                ],
                [
                    0.9 * 4992.2486445538634,
                    -1.2,
                    -1.2,
                    -0.8,
                    -0.7,
                    -0.7,
                    -0.4,
                    -0.5,
                ],
            ]),
        }
    }

    // dct64x32
    fn dct32x64() -> QuantEncoding {
        QuantEncoding::Dct {
            params: DctQuantWeightParams::from_array(&[
                [
                    0.65 * 23629.073922049845,
                    -1.025,
                    -0.78,
                    -0.65012,
                    -0.19041574084286472,
                    -0.20819395464,
                    -0.421064,
                    -0.32733845535848671,
                ],
                [
                    0.65 * 8611.3238710010046,
                    -0.3041958212306401,
                    -0.3633036457487539,
                    -0.35660379990111464,
                    -0.3443074455424403,
                    -0.33699592683512467,
                    -0.30180866526242109,
                    -0.27321683125358037,
                ],
                [
                    0.65 * 4492.2486445538634,
                    -1.2,
                    -1.2,
                    -0.8,
                    -0.7,
                    -0.7,
                    -0.4,
                    -0.5,
                ],
            ]),
        }
    }
    fn dct128x128() -> QuantEncoding {
        QuantEncoding::Dct {
            params: DctQuantWeightParams::from_array(&[
                [
                    1.8 * 26629.073922049845,
                    -1.025,
                    -0.78,
                    -0.65012,
                    -0.19041574084286472,
                    -0.20819395464,
                    -0.421064,
                    -0.32733845535848671,
                ],
                [
                    1.8 * 9311.3238710010046,
                    -0.3041958212306401,
                    -0.3633036457487539,
                    -0.35660379990111464,
                    -0.3443074455424403,
                    -0.33699592683512467,
                    -0.30180866526242109,
                    -0.27321683125358037,
                ],
                [
                    1.8 * 4992.2486445538634,
                    -1.2,
                    -1.2,
                    -0.8,
                    -0.7,
                    -0.7,
                    -0.4,
                    -0.5,
                ],
            ]),
        }
    }

    // dct128x64
    fn dct64x128() -> QuantEncoding {
        QuantEncoding::Dct {
            params: DctQuantWeightParams::from_array(&[
                [
                    1.3 * 23629.073922049845,
                    -1.025,
                    -0.78,
                    -0.65012,
                    -0.19041574084286472,
                    -0.20819395464,
                    -0.421064,
                    -0.32733845535848671,
                ],
                [
                    1.3 * 8611.3238710010046,
                    -0.3041958212306401,
                    -0.3633036457487539,
                    -0.35660379990111464,
                    -0.3443074455424403,
                    -0.33699592683512467,
                    -0.30180866526242109,
                    -0.27321683125358037,
                ],
                [
                    1.3 * 4492.2486445538634,
                    -1.2,
                    -1.2,
                    -0.8,
                    -0.7,
                    -0.7,
                    -0.4,
                    -0.5,
                ],
            ]),
        }
    }
    fn dct256x256() -> QuantEncoding {
        QuantEncoding::Dct {
            params: DctQuantWeightParams::from_array(&[
                [
                    3.6 * 26629.073922049845,
                    -1.025,
                    -0.78,
                    -0.65012,
                    -0.19041574084286472,
                    -0.20819395464,
                    -0.421064,
                    -0.32733845535848671,
                ],
                [
                    3.6 * 9311.3238710010046,
                    -0.3041958212306401,
                    -0.3633036457487539,
                    -0.35660379990111464,
                    -0.3443074455424403,
                    -0.33699592683512467,
                    -0.30180866526242109,
                    -0.27321683125358037,
                ],
                [
                    3.6 * 4992.2486445538634,
                    -1.2,
                    -1.2,
                    -0.8,
                    -0.7,
                    -0.7,
                    -0.4,
                    -0.5,
                ],
            ]),
        }
    }

    // dct256x128
    fn dct128x256() -> QuantEncoding {
        QuantEncoding::Dct {
            params: DctQuantWeightParams::from_array(&[
                [
                    2.6 * 23629.073922049845,
                    -1.025,
                    -0.78,
                    -0.65012,
                    -0.19041574084286472,
                    -0.20819395464,
                    -0.421064,
                    -0.32733845535848671,
                ],
                [
                    2.6 * 8611.3238710010046,
                    -0.3041958212306401,
                    -0.3633036457487539,
                    -0.35660379990111464,
                    -0.3443074455424403,
                    -0.33699592683512467,
                    -0.30180866526242109,
                    -0.27321683125358037,
                ],
                [
                    2.6 * 4492.2486445538634,
                    -1.2,
                    -1.2,
                    -0.8,
                    -0.7,
                    -0.7,
                    -0.4,
                    -0.5,
                ],
            ]),
        }
    }

    /// Get library quantization encoding for a table type index.
    fn get_library_encoding(idx: usize) -> QuantEncoding {
        match idx {
            0 => Self::dct(),
            1 => Self::id(),
            2 => Self::dct2x2(),
            3 => Self::dct4x4(),
            4 => Self::dct16x16(),
            5 => Self::dct32x32(),
            6 => Self::dct8x16(),
            7 => Self::dct8x32(),
            8 => Self::dct16x32(),
            9 => Self::dct4x8(),
            10 => Self::afv0(),
            11 => Self::dct64x64(),
            12 => Self::dct32x64(),
            // Same default for large transforms (128+) as for 64x* transforms.
            13 => Self::dct128x128(),
            14 => Self::dct64x128(),
            15 => Self::dct256x256(),
            16 => Self::dct128x256(),
            _ => unreachable!(),
        }
    }

    /// Get cached computed library table for a QuantTable type index.
    /// Computes the table lazily on first access.
    fn get_library_table(idx: usize) -> &'static [f32] {
        LIBRARY_TABLES[idx].get_or_init(|| {
            let encoding = Self::get_library_encoding(idx);
            Self::compute_table(&encoding, idx).expect("library table computation should not fail")
        })
    }

    /// Compute a single quant table from an encoding.
    /// Returns the computed weights as a boxed slice for all 3 channels.
    fn compute_table(encoding: &QuantEncoding, table_idx: usize) -> Result<Box<[f32]>> {
        let wrows = 8 * Self::REQUIRED_SIZE_X[table_idx];
        let wcols = 8 * Self::REQUIRED_SIZE_Y[table_idx];
        let num = wrows * wcols;
        let mut weights = vec![0f32; 3 * num];
        match encoding {
            QuantEncoding::Library => {
                // Library encoding should be resolved by the caller.
                return Err(InvalidQuantEncodingMode);
            }
            QuantEncoding::Identity { xyb_weights } => {
                for c in 0..3 {
                    for i in 0..64 {
                        weights[64 * c + i] = xyb_weights[c][0];
                    }
                    weights[64 * c + 1] = xyb_weights[c][1];
                    weights[64 * c + 8] = xyb_weights[c][1];
                    weights[64 * c + 9] = xyb_weights[c][2];
                }
            }
            QuantEncoding::Dct2 { xyb_weights } => {
                for (c, xyb_weight) in xyb_weights.iter().enumerate() {
                    let start = c * 64;
                    weights[start] = 0xBAD as f32;
                    weights[start + 1] = xyb_weight[0];
                    weights[start + 8] = xyb_weight[0];
                    weights[start + 9] = xyb_weight[1];
                    for y in 0..2 {
                        for x in 0..2 {
                            weights[start + y * 8 + x + 2] = xyb_weight[2];
                            weights[start + (y + 2) * 8 + x] = xyb_weight[2];
                        }
                    }
                    for y in 0..2 {
                        for x in 0..2 {
                            weights[start + (y + 2) * 8 + x + 2] = xyb_weight[3];
                        }
                    }
                    for y in 0..4 {
                        for x in 0..4 {
                            weights[start + y * 8 + x + 4] = xyb_weight[4];
                            weights[start + (y + 4) * 8 + x] = xyb_weight[4];
                        }
                    }
                    for y in 0..4 {
                        for x in 0..4 {
                            weights[start + (y + 4) * 8 + x + 4] = xyb_weight[5];
                        }
                    }
                }
            }
            QuantEncoding::Dct4 { params, xyb_mul } => {
                let mut weights4x4 = [0f32; 3 * 4 * 4];
                get_quant_weights(4, 4, params, &mut weights4x4)?;
                for c in 0..3 {
                    for y in 0..BLOCK_DIM {
                        for x in 0..BLOCK_DIM {
                            weights[c * num + y * BLOCK_DIM + x] =
                                weights4x4[c * 16 + (y / 2) * 4 + (x / 2)];
                        }
                    }
                    weights[c * num + 1] /= xyb_mul[c][0];
                    weights[c * num + BLOCK_DIM] /= xyb_mul[c][0];
                    weights[c * num + BLOCK_DIM + 1] /= xyb_mul[c][1];
                }
            }
            QuantEncoding::Dct4x8 { params, xyb_mul } => {
                let mut weights4x8 = [0f32; 3 * 4 * 8];
                get_quant_weights(4, 8, params, &mut weights4x8)?;
                for c in 0..3 {
                    for y in 0..BLOCK_DIM {
                        for x in 0..BLOCK_DIM {
                            weights[c * num + y * BLOCK_DIM + x] =
                                weights4x8[c * 32 + (y / 2) * 8 + x];
                        }
                    }
                    weights[c * num + BLOCK_DIM] /= xyb_mul[c];
                }
            }
            QuantEncoding::Dct { params } => {
                get_quant_weights(wrows, wcols, params, &mut weights)?;
            }
            QuantEncoding::Raw { qtable, qtable_den } => {
                if qtable.len() != 3 * num {
                    return Err(InvalidRawQuantTable);
                }
                for i in 0..3 * num {
                    weights[i] = 1f32 / (qtable_den * qtable[i] as f32);
                }
            }
            QuantEncoding::Afv {
                params4x8,
                params4x4,
                weights: afv_weights,
            } => {
                const FREQS: [f32; 16] = [
                    0xBAD as f32,
                    0xBAD as f32,
                    0.8517778890324296,
                    5.37778436506804,
                    0xBAD as f32,
                    0xBAD as f32,
                    4.734747904497923,
                    5.449245381693219,
                    1.6598270267479331,
                    4f32,
                    7.275749096817861,
                    10.423227632456525,
                    2.662932286148962,
                    7.630657783650829,
                    8.962388608184032,
                    12.97166202570235,
                ];
                let mut weights4x8 = [0f32; 3 * 4 * 8];
                get_quant_weights(4, 8, params4x8, &mut weights4x8)?;
                let mut weights4x4 = [0f32; 3 * 4 * 4];
                get_quant_weights(4, 4, params4x4, &mut weights4x4)?;
                const LO: f32 = 0.8517778890324296;
                const HI: f32 = 12.97166202570235f32 - LO + 1e-6f32;
                for c in 0..3 {
                    let mut bands = [0f32; 4];
                    bands[0] = afv_weights[c][5];
                    if bands[0] < ALMOST_ZERO {
                        return Err(InvalidDistanceBand(0, bands[0]));
                    }
                    for i in 1..4 {
                        bands[i] = bands[i - 1] * mult(afv_weights[c][i + 5]);
                        if bands[i] < ALMOST_ZERO {
                            return Err(InvalidDistanceBand(i, bands[i]));
                        }
                    }

                    {
                        let start = c * 64;
                        weights[start] = 1f32;
                        let mut set = |x, y, val| {
                            weights[start + y * 8 + x] = val;
                        };
                        set(0, 1, afv_weights[c][0]);
                        set(1, 0, afv_weights[c][1]);
                        set(0, 2, afv_weights[c][2]);
                        set(2, 0, afv_weights[c][3]);
                        set(2, 2, afv_weights[c][4]);

                        for y in 0..4 {
                            for x in 0..4 {
                                if x < 2 && y < 2 {
                                    continue;
                                }
                                let val = interpolate(FREQS[y * 4 + x] - LO, HI, &bands);
                                set(2 * x, 2 * y, val);
                            }
                        }
                    }

                    for y in 0..BLOCK_DIM / 2 {
                        for x in 0..BLOCK_DIM {
                            if x == 0 && y == 0 {
                                continue;
                            }
                            weights[c * num + (2 * y + 1) * BLOCK_DIM + x] =
                                weights4x8[c * 32 + y * 8 + x];
                        }
                    }

                    for y in 0..BLOCK_DIM / 2 {
                        for x in 0..BLOCK_DIM / 2 {
                            if x == 0 && y == 0 {
                                continue;
                            }
                            weights[c * num + (2 * y) * BLOCK_DIM + 2 * x + 1] =
                                weights4x4[c * 16 + y * 4 + x];
                        }
                    }
                }
            }
        }
        // Convert weights in place to the final table format (1.0 / weight)
        for weight in &mut weights {
            if !(ALMOST_ZERO..=1.0 / ALMOST_ZERO).contains(weight) {
                return Err(InvalidQuantizationTableWeight(*weight));
            }
            *weight = 1f32 / *weight;
        }
        Ok(weights.into_boxed_slice())
    }

    pub fn matrix(&self, quant_kind: HfTransformType, c: usize) -> &[f32] {
        let qt_idx = QuantTable::for_strategy(quant_kind) as usize;
        let table = &self.tables[qt_idx];
        let num = Self::REQUIRED_SIZE_X[qt_idx] * Self::REQUIRED_SIZE_Y[qt_idx] * BLOCK_SIZE;
        &table[c * num..]
    }

    pub fn decode(
        header: &FrameHeader,
        lf_global: &LfGlobalState,
        br: &mut BitReader,
    ) -> Result<Self> {
        let all_default = br.read(1)? == 1;

        // Compute all tables during decode
        let tables: [Cow<'static, [f32]>; QuantTable::CARDINALITY] = if all_default {
            // All library tables - borrow from static cache (zero-copy)
            std::array::from_fn(|idx| Cow::Borrowed(Self::get_library_table(idx)))
        } else {
            // Decode and compute each table
            let mut tables_vec: Vec<Cow<'static, [f32]>> =
                Vec::with_capacity(QuantTable::CARDINALITY);
            for (i, (&required_size_x, required_size_y)) in Self::REQUIRED_SIZE_X
                .iter()
                .zip(Self::REQUIRED_SIZE_Y)
                .enumerate()
            {
                let encoding = QuantEncoding::decode(
                    required_size_x,
                    required_size_y,
                    i,
                    header,
                    lf_global,
                    br,
                )?;
                let table = match encoding {
                    QuantEncoding::Library => Cow::Borrowed(Self::get_library_table(i)),
                    _ => Cow::Owned(Self::compute_table(&encoding, i)?.into_vec()),
                };
                tables_vec.push(table);
            }
            tables_vec.try_into().unwrap()
        };

        Ok(Self { tables })
    }

    pub const REQUIRED_SIZE_X: [usize; QuantTable::CARDINALITY] =
        [1, 1, 1, 1, 2, 4, 1, 1, 2, 1, 1, 8, 4, 16, 8, 32, 16];

    pub const REQUIRED_SIZE_Y: [usize; QuantTable::CARDINALITY] =
        [1, 1, 1, 1, 2, 4, 2, 4, 4, 1, 1, 8, 8, 16, 16, 32, 32];

    #[cfg(test)]
    const SUM_REQUIRED_X_Y: usize = 2056;
}

fn get_quant_weights(
    rows: usize,
    cols: usize,
    distance_bands: &DctQuantWeightParams,
    out: &mut [f32],
) -> Result<()> {
    for c in 0..3 {
        let mut bands = [0f32; DctQuantWeightParams::MAX_DISTANCE_BANDS];
        bands[0] = distance_bands.params[c][0];
        if bands[0] < ALMOST_ZERO {
            return Err(InvalidDistanceBand(0, bands[0]));
        }
        for i in 1..distance_bands.num_bands {
            bands[i] = bands[i - 1] * mult(distance_bands.params[c][i]);
            if bands[i] < ALMOST_ZERO {
                return Err(InvalidDistanceBand(i, bands[i]));
            }
        }
        let scale = (distance_bands.num_bands - 1) as f32 / (SQRT_2 + 1e-6);
        let rcpcol = scale / (cols - 1) as f32;
        let rcprow = scale / (rows - 1) as f32;
        for y in 0..rows {
            let dy = y as f32 * rcprow;
            let dy2 = dy * dy;
            for x in 0..cols {
                let dx = x as f32 * rcpcol;
                let scaled_distance = (dx * dx + dy2).sqrt();
                let weight = if distance_bands.num_bands == 1 {
                    bands[0]
                } else {
                    interpolate_vec(scaled_distance, &bands)
                };
                out[c * cols * rows + y * cols + x] = weight;
            }
        }
    }
    Ok(())
}

fn interpolate_vec(scaled_pos: f32, array: &[f32]) -> f32 {
    let idxf32 = scaled_pos.floor();
    let frac = scaled_pos - idxf32;
    let idx = idxf32 as usize;
    let a = array[idx];
    let b = array[1..][idx];
    (b / a).powf(frac) * a
}

fn interpolate(pos: f32, max: f32, array: &[f32]) -> f32 {
    let scaled_pos = pos * (array.len() - 1) as f32 / max;
    let idx = scaled_pos as usize;
    let a = array[idx];
    let b = array[idx + 1];
    a * (b / a).powf(scaled_pos - idx as f32)
}

fn mult(v: f32) -> f32 {
    if v > 0f32 {
        1f32 + v
    } else {
        1f32 / (1f32 - v)
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::error::Result;
    use crate::frame::quant_weights::DequantMatrices;
    use crate::util::test::assert_almost_abs_eq;

    #[test]
    fn check_required_x_y() {
        assert_eq!(
            DequantMatrices::SUM_REQUIRED_X_Y,
            DequantMatrices::REQUIRED_SIZE_X
                .iter()
                .zip(DequantMatrices::REQUIRED_SIZE_Y)
                .map(|(&x, y)| x * y)
                .sum()
        );
    }

    #[test]
    fn check_dequant_matrix_correctness() -> Result<()> {
        // All library tables
        let matrices = DequantMatrices {
            tables: std::array::from_fn(|idx| {
                Cow::Borrowed(DequantMatrices::get_library_table(idx))
            }),
        };

        // Golden data produced by libjxl.
        let target_table = [
            0.000317f32,
            0.000629f32,
            0.000457f32,
            0.000367f32,
            0.000378f32,
            0.000709f32,
            0.000593f32,
            0.000566f32,
            0.000629f32,
            0.001192f32,
            0.000943f32,
            0.001786f32,
            0.003042f32,
            0.002372f32,
            0.001998f32,
            0.002044f32,
            0.003341f32,
            0.002907f32,
            0.002804f32,
            0.003042f32,
            0.004229f32,
            0.003998f32,
            0.001953f32,
            0.011969f32,
            0.011719f32,
            0.007886f32,
            0.008374f32,
            0.015337f32,
            0.011719f32,
            0.011719f32,
            0.011969f32,
            0.032080f32,
            0.025368f32,
            0.003571f32,
            0.003571f32,
            0.003571f32,
            0.003571f32,
            0.003571f32,
            0.003571f32,
            0.003571f32,
            0.003571f32,
            0.003571f32,
            0.003571f32,
            0.003571f32,
            0.016667f32,
            0.016667f32,
            0.016667f32,
            0.016667f32,
            0.016667f32,
            0.016667f32,
            0.016667f32,
            0.016667f32,
            0.016667f32,
            0.016667f32,
            0.016667f32,
            0.055556f32,
            0.055556f32,
            0.055556f32,
            0.055556f32,
            0.055556f32,
            0.055556f32,
            0.055556f32,
            0.055556f32,
            0.055556f32,
            0.055556f32,
            0.055556f32,
            0.000335f32,
            0.002083f32,
            0.002083f32,
            0.001563f32,
            0.000781f32,
            0.002083f32,
            0.003333f32,
            0.002083f32,
            0.002083f32,
            0.003333f32,
            0.003333f32,
            0.000335f32,
            0.007143f32,
            0.007143f32,
            0.005556f32,
            0.003125f32,
            0.007143f32,
            0.008333f32,
            0.007143f32,
            0.007143f32,
            0.008333f32,
            0.008333f32,
            0.000335f32,
            0.031250f32,
            0.031250f32,
            0.015625f32,
            0.007812f32,
            0.031250f32,
            0.062500f32,
            0.031250f32,
            0.031250f32,
            0.062500f32,
            0.062500f32,
            0.000455f32,
            0.000455f32,
            0.000455f32,
            0.000455f32,
            0.000455f32,
            0.000455f32,
            0.000455f32,
            0.000455f32,
            0.000455f32,
            0.000455f32,
            0.000455f32,
            0.002551f32,
            0.002551f32,
            0.002551f32,
            0.002551f32,
            0.002551f32,
            0.002551f32,
            0.002551f32,
            0.002551f32,
            0.002551f32,
            0.002551f32,
            0.002551f32,
            0.008929f32,
            0.014654f32,
            0.012241f32,
            0.011161f32,
            0.010455f32,
            0.015352f32,
            0.013951f32,
            0.012706f32,
            0.014654f32,
            0.020926f32,
            0.017433f32,
            0.000111f32,
            0.000469f32,
            0.000258f32,
            0.000640f32,
            0.000388f32,
            0.001007f32,
            0.000566f32,
            0.001880f32,
            0.000946f32,
            0.000886f32,
            0.001880f32,
            0.000313f32,
            0.001168f32,
            0.000531f32,
            0.001511f32,
            0.000962f32,
            0.001959f32,
            0.001399f32,
            0.002531f32,
            0.001908f32,
            0.001850f32,
            0.002531f32,
            0.000864f32,
            0.007969f32,
            0.002684f32,
            0.010653f32,
            0.006434f32,
            0.015981f32,
            0.009743f32,
            0.040354f32,
            0.014631f32,
            0.013468f32,
            0.040354f32,
            0.000064f32,
            0.000135f32,
            0.000279f32,
            0.000521f32,
            0.000760f32,
            0.001145f32,
            0.000502f32,
            0.000647f32,
            0.000911f32,
            0.001286f32,
            0.001685f32,
            0.000137f32,
            0.000257f32,
            0.000464f32,
            0.000739f32,
            0.001126f32,
            0.001645f32,
            0.000706f32,
            0.000959f32,
            0.001327f32,
            0.001839f32,
            0.002404f32,
            0.000263f32,
            0.001155f32,
            0.003800f32,
            0.010779f32,
            0.016740f32,
            0.024003f32,
            0.010258f32,
            0.014299f32,
            0.019509f32,
            0.026824f32,
            0.035546f32,
            0.000138f32,
            0.000515f32,
            0.000425f32,
            0.000333f32,
            0.000362f32,
            0.000559f32,
            0.000507f32,
            0.000500f32,
            0.000538f32,
            0.000686f32,
            0.000666f32,
            0.000691f32,
            0.002504f32,
            0.001785f32,
            0.001353f32,
            0.001443f32,
            0.002721f32,
            0.002469f32,
            0.002432f32,
            0.002617f32,
            0.003340f32,
            0.003241f32,
            0.001973f32,
            0.010000f32,
            0.006529f32,
            0.005339f32,
            0.005497f32,
            0.012033f32,
            0.009689f32,
            0.009374f32,
            0.011033f32,
            0.031220f32,
            0.026814f32,
            0.000138f32,
            0.000515f32,
            0.000425f32,
            0.000333f32,
            0.000362f32,
            0.000559f32,
            0.000507f32,
            0.000500f32,
            0.000538f32,
            0.000686f32,
            0.000666f32,
            0.000691f32,
            0.002504f32,
            0.001785f32,
            0.001353f32,
            0.001443f32,
            0.002721f32,
            0.002469f32,
            0.002432f32,
            0.002617f32,
            0.003340f32,
            0.003241f32,
            0.001973f32,
            0.010000f32,
            0.006529f32,
            0.005339f32,
            0.005497f32,
            0.012033f32,
            0.009689f32,
            0.009374f32,
            0.011033f32,
            0.031220f32,
            0.026814f32,
            0.000061f32,
            0.001686f32,
            0.000890f32,
            0.000539f32,
            0.000524f32,
            0.003058f32,
            0.002221f32,
            0.001956f32,
            0.002130f32,
            0.002809f32,
            0.007926f32,
            0.000196f32,
            0.000734f32,
            0.000453f32,
            0.000376f32,
            0.000372f32,
            0.001148f32,
            0.000906f32,
            0.000822f32,
            0.000877f32,
            0.001084f32,
            0.002058f32,
            0.000294f32,
            0.001684f32,
            0.000872f32,
            0.000599f32,
            0.000587f32,
            0.003612f32,
            0.002411f32,
            0.002042f32,
            0.002282f32,
            0.003276f32,
            0.009683f32,
            0.000061f32,
            0.001686f32,
            0.000890f32,
            0.000539f32,
            0.000524f32,
            0.003058f32,
            0.002221f32,
            0.001956f32,
            0.002130f32,
            0.002809f32,
            0.007926f32,
            0.000196f32,
            0.000734f32,
            0.000453f32,
            0.000376f32,
            0.000372f32,
            0.001148f32,
            0.000906f32,
            0.000822f32,
            0.000877f32,
            0.001084f32,
            0.002058f32,
            0.000294f32,
            0.001684f32,
            0.000872f32,
            0.000599f32,
            0.000587f32,
            0.003612f32,
            0.002411f32,
            0.002042f32,
            0.002282f32,
            0.003276f32,
            0.009683f32,
            0.000072f32,
            0.000339f32,
            0.000172f32,
            0.000429f32,
            0.000308f32,
            0.000552f32,
            0.000422f32,
            0.000388f32,
            0.000557f32,
            0.000496f32,
            0.000935f32,
            0.000208f32,
            0.001118f32,
            0.000422f32,
            0.001498f32,
            0.000958f32,
            0.002133f32,
            0.001464f32,
            0.001310f32,
            0.002154f32,
            0.001903f32,
            0.002817f32,
            0.000553f32,
            0.004679f32,
            0.001639f32,
            0.008626f32,
            0.003998f32,
            0.015372f32,
            0.008305f32,
            0.006659f32,
            0.015623f32,
            0.012761f32,
            0.026404f32,
            0.000072f32,
            0.000339f32,
            0.000172f32,
            0.000429f32,
            0.000308f32,
            0.000552f32,
            0.000422f32,
            0.000388f32,
            0.000557f32,
            0.000496f32,
            0.000935f32,
            0.000208f32,
            0.001118f32,
            0.000422f32,
            0.001498f32,
            0.000958f32,
            0.002133f32,
            0.001464f32,
            0.001310f32,
            0.002154f32,
            0.001903f32,
            0.002817f32,
            0.000553f32,
            0.004679f32,
            0.001639f32,
            0.008626f32,
            0.003998f32,
            0.015372f32,
            0.008305f32,
            0.006659f32,
            0.015623f32,
            0.012761f32,
            0.026404f32,
            0.000455f32,
            0.001419f32,
            0.001007f32,
            0.000853f32,
            0.000733f32,
            0.001530f32,
            0.001456f32,
            0.001211f32,
            0.001672f32,
            0.002347f32,
            0.001967f32,
            0.001308f32,
            0.004385f32,
            0.002909f32,
            0.002409f32,
            0.002080f32,
            0.004796f32,
            0.004518f32,
            0.003629f32,
            0.005108f32,
            0.006026f32,
            0.005529f32,
            0.001897f32,
            0.009714f32,
            0.005643f32,
            0.004386f32,
            0.003585f32,
            0.010940f32,
            0.010108f32,
            0.007561f32,
            0.012828f32,
            0.024294f32,
            0.017414f32,
            0.000455f32,
            0.001419f32,
            0.001007f32,
            0.000853f32,
            0.000733f32,
            0.001530f32,
            0.001456f32,
            0.001211f32,
            0.001672f32,
            0.002347f32,
            0.001967f32,
            0.001308f32,
            0.004385f32,
            0.002909f32,
            0.002409f32,
            0.002080f32,
            0.004796f32,
            0.004518f32,
            0.003629f32,
            0.005108f32,
            0.006026f32,
            0.005529f32,
            0.001897f32,
            0.009714f32,
            0.005643f32,
            0.004386f32,
            0.003585f32,
            0.010940f32,
            0.010108f32,
            0.007561f32,
            0.012828f32,
            0.024294f32,
            0.017414f32,
            1.000000f32,
            0.002415f32,
            0.001007f32,
            0.003906f32,
            0.000733f32,
            0.001530f32,
            0.002415f32,
            0.001211f32,
            0.002415f32,
            0.002415f32,
            0.001967f32,
            1.000000f32,
            0.017241f32,
            0.002909f32,
            0.020000f32,
            0.002080f32,
            0.004796f32,
            0.017241f32,
            0.003629f32,
            0.017241f32,
            0.017241f32,
            0.005529f32,
            1.000000f32,
            0.058364f32,
            0.005643f32,
            0.083333f32,
            0.003585f32,
            0.010940f32,
            0.064815f32,
            0.007561f32,
            0.050237f32,
            0.088778f32,
            0.017414f32,
            1.000000f32,
            0.002415f32,
            0.001007f32,
            0.003906f32,
            0.000733f32,
            0.001530f32,
            0.002415f32,
            0.001211f32,
            0.002415f32,
            0.002415f32,
            0.001967f32,
            1.000000f32,
            0.017241f32,
            0.002909f32,
            0.020000f32,
            0.002080f32,
            0.004796f32,
            0.017241f32,
            0.003629f32,
            0.017241f32,
            0.017241f32,
            0.005529f32,
            1.000000f32,
            0.058364f32,
            0.005643f32,
            0.083333f32,
            0.003585f32,
            0.010940f32,
            0.064815f32,
            0.007561f32,
            0.050237f32,
            0.088778f32,
            0.017414f32,
            1.000000f32,
            0.002415f32,
            0.001007f32,
            0.003906f32,
            0.000733f32,
            0.001530f32,
            0.002415f32,
            0.001211f32,
            0.002415f32,
            0.002415f32,
            0.001967f32,
            1.000000f32,
            0.017241f32,
            0.002909f32,
            0.020000f32,
            0.002080f32,
            0.004796f32,
            0.017241f32,
            0.003629f32,
            0.017241f32,
            0.017241f32,
            0.005529f32,
            1.000000f32,
            0.058364f32,
            0.005643f32,
            0.083333f32,
            0.003585f32,
            0.010940f32,
            0.064815f32,
            0.007561f32,
            0.050237f32,
            0.088778f32,
            0.017414f32,
            1.000000f32,
            0.002415f32,
            0.001007f32,
            0.003906f32,
            0.000733f32,
            0.001530f32,
            0.002415f32,
            0.001211f32,
            0.002415f32,
            0.002415f32,
            0.001967f32,
            1.000000f32,
            0.017241f32,
            0.002909f32,
            0.020000f32,
            0.002080f32,
            0.004796f32,
            0.017241f32,
            0.003629f32,
            0.017241f32,
            0.017241f32,
            0.005529f32,
            1.000000f32,
            0.058364f32,
            0.005643f32,
            0.083333f32,
            0.003585f32,
            0.010940f32,
            0.064815f32,
            0.007561f32,
            0.050237f32,
            0.088778f32,
            0.017414f32,
            0.000042f32,
            0.000152f32,
            0.000298f32,
            0.000128f32,
            0.000268f32,
            0.000407f32,
            0.000268f32,
            0.000364f32,
            0.000299f32,
            0.000380f32,
            0.000623f32,
            0.000119f32,
            0.000213f32,
            0.000391f32,
            0.000195f32,
            0.000328f32,
            0.000571f32,
            0.000329f32,
            0.000525f32,
            0.000393f32,
            0.000542f32,
            0.000803f32,
            0.000223f32,
            0.001090f32,
            0.003367f32,
            0.000867f32,
            0.002454f32,
            0.006359f32,
            0.002462f32,
            0.005715f32,
            0.003396f32,
            0.005943f32,
            0.010539f32,
            0.000065f32,
            0.000136f32,
            0.000249f32,
            0.000399f32,
            0.000481f32,
            0.000616f32,
            0.000394f32,
            0.000449f32,
            0.000528f32,
            0.000700f32,
            0.000944f32,
            0.000179f32,
            0.000237f32,
            0.000329f32,
            0.000453f32,
            0.000619f32,
            0.000836f32,
            0.000444f32,
            0.000554f32,
            0.000714f32,
            0.000920f32,
            0.001172f32,
            0.000342f32,
            0.000788f32,
            0.001774f32,
            0.003270f32,
            0.005731f32,
            0.009499f32,
            0.003143f32,
            0.004679f32,
            0.007422f32,
            0.010736f32,
            0.015538f32,
            0.000065f32,
            0.000136f32,
            0.000249f32,
            0.000399f32,
            0.000481f32,
            0.000616f32,
            0.000394f32,
            0.000449f32,
            0.000528f32,
            0.000700f32,
            0.000944f32,
            0.000179f32,
            0.000237f32,
            0.000329f32,
            0.000453f32,
            0.000619f32,
            0.000836f32,
            0.000444f32,
            0.000554f32,
            0.000714f32,
            0.000920f32,
            0.001172f32,
            0.000342f32,
            0.000788f32,
            0.001774f32,
            0.003270f32,
            0.005731f32,
            0.009499f32,
            0.003143f32,
            0.004679f32,
            0.007422f32,
            0.010736f32,
            0.015538f32,
            0.000021f32,
            0.000148f32,
            0.000127f32,
            0.000094f32,
            0.000083f32,
            0.000212f32,
            0.000175f32,
            0.000163f32,
            0.000159f32,
            0.000164f32,
            0.000329f32,
            0.000060f32,
            0.000194f32,
            0.000149f32,
            0.000122f32,
            0.000113f32,
            0.000294f32,
            0.000251f32,
            0.000224f32,
            0.000217f32,
            0.000228f32,
            0.000420f32,
            0.000111f32,
            0.001651f32,
            0.001032f32,
            0.000701f32,
            0.000605f32,
            0.003305f32,
            0.002650f32,
            0.002162f32,
            0.002031f32,
            0.002222f32,
            0.005691f32,
            0.000033f32,
            0.000120f32,
            0.000234f32,
            0.000104f32,
            0.000213f32,
            0.000334f32,
            0.000214f32,
            0.000303f32,
            0.000236f32,
            0.000315f32,
            0.000521f32,
            0.000089f32,
            0.000161f32,
            0.000297f32,
            0.000148f32,
            0.000254f32,
            0.000444f32,
            0.000255f32,
            0.000412f32,
            0.000299f32,
            0.000424f32,
            0.000638f32,
            0.000171f32,
            0.000850f32,
            0.002654f32,
            0.000698f32,
            0.002002f32,
            0.005130f32,
            0.002014f32,
            0.004672f32,
            0.002695f32,
            0.004847f32,
            0.008953f32,
            0.000033f32,
            0.000120f32,
            0.000234f32,
            0.000104f32,
            0.000213f32,
            0.000334f32,
            0.000214f32,
            0.000303f32,
            0.000236f32,
            0.000315f32,
            0.000521f32,
            0.000089f32,
            0.000161f32,
            0.000297f32,
            0.000148f32,
            0.000254f32,
            0.000444f32,
            0.000255f32,
            0.000412f32,
            0.000299f32,
            0.000424f32,
            0.000638f32,
            0.000171f32,
            0.000850f32,
            0.002654f32,
            0.000698f32,
            0.002002f32,
            0.005130f32,
            0.002014f32,
            0.004672f32,
            0.002695f32,
            0.004847f32,
            0.008953f32,
            0.000010f32,
            0.000062f32,
            0.000026f32,
            0.000077f32,
            0.000055f32,
            0.000106f32,
            0.000076f32,
            0.000069f32,
            0.000108f32,
            0.000087f32,
            0.000165f32,
            0.000030f32,
            0.000072f32,
            0.000044f32,
            0.000103f32,
            0.000067f32,
            0.000147f32,
            0.000101f32,
            0.000086f32,
            0.000149f32,
            0.000124f32,
            0.000211f32,
            0.000056f32,
            0.000487f32,
            0.000166f32,
            0.000920f32,
            0.000424f32,
            0.001655f32,
            0.000897f32,
            0.000664f32,
            0.001683f32,
            0.001291f32,
            0.002862f32,
            0.000016f32,
            0.000115f32,
            0.000099f32,
            0.000073f32,
            0.000065f32,
            0.000164f32,
            0.000136f32,
            0.000127f32,
            0.000124f32,
            0.000128f32,
            0.000256f32,
            0.000045f32,
            0.000144f32,
            0.000111f32,
            0.000091f32,
            0.000084f32,
            0.000219f32,
            0.000187f32,
            0.000168f32,
            0.000162f32,
            0.000171f32,
            0.000314f32,
            0.000086f32,
            0.001260f32,
            0.000790f32,
            0.000537f32,
            0.000465f32,
            0.002528f32,
            0.002026f32,
            0.001657f32,
            0.001560f32,
            0.001709f32,
            0.004355f32,
            0.000016f32,
            0.000115f32,
            0.000099f32,
            0.000073f32,
            0.000065f32,
            0.000164f32,
            0.000136f32,
            0.000127f32,
            0.000124f32,
            0.000128f32,
            0.000256f32,
            0.000045f32,
            0.000144f32,
            0.000111f32,
            0.000091f32,
            0.000084f32,
            0.000219f32,
            0.000187f32,
            0.000168f32,
            0.000162f32,
            0.000171f32,
            0.000314f32,
            0.000086f32,
            0.001260f32,
            0.000790f32,
            0.000537f32,
            0.000465f32,
            0.002528f32,
            0.002026f32,
            0.001657f32,
            0.001560f32,
            0.001709f32,
            0.004355f32,
        ];
        let mut target_table_index = 0;
        for i in 0..HfTransformType::CARDINALITY {
            let hf_type = HfTransformType::from_usize(i).unwrap();
            let qt_idx = QuantTable::for_strategy(hf_type) as usize;
            let size = DequantMatrices::REQUIRED_SIZE_X[qt_idx]
                * DequantMatrices::REQUIRED_SIZE_Y[qt_idx]
                * BLOCK_SIZE;
            for c in 0..3 {
                let table = matrices.matrix(hf_type, c);
                for j in (0..size).step_by(size / 10) {
                    assert_almost_abs_eq(table[j], target_table[target_table_index], 1e-5);
                    target_table_index += 1;
                }
            }
        }
        Ok(())
    }
}
