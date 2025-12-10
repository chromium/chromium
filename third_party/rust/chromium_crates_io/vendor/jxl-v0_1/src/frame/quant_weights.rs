// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{f32::consts::SQRT_2, sync::OnceLock};

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
    pub fn from_usize(idx: usize) -> Result<QuantTable> {
        match QuantTable::VALUES.get(idx) {
            Some(table) => Ok(*table),
            None => Err(InvalidQuantEncodingMode),
        }
    }
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
    computed_mask: u32,
    table: Vec<f32>,
    inv_table: Vec<f32>,
    table_offsets: [usize; HfTransformType::CARDINALITY * 3],
    encodings: Vec<QuantEncoding>,
}

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

    pub fn library() -> &'static [QuantEncoding; QuantTable::CARDINALITY] {
        static QUANTS: OnceLock<[QuantEncoding; QuantTable::CARDINALITY]> = OnceLock::new();
        QUANTS.get_or_init(|| {
            [
                DequantMatrices::dct(),
                DequantMatrices::id(),
                DequantMatrices::dct2x2(),
                DequantMatrices::dct4x4(),
                DequantMatrices::dct16x16(),
                DequantMatrices::dct32x32(),
                DequantMatrices::dct8x16(),
                DequantMatrices::dct8x32(),
                DequantMatrices::dct16x32(),
                DequantMatrices::dct4x8(),
                DequantMatrices::afv0(),
                DequantMatrices::dct64x64(),
                DequantMatrices::dct32x64(),
                // Same default for large transforms (128+) as for 64x* transforms.
                DequantMatrices::dct128x128(),
                DequantMatrices::dct64x128(),
                DequantMatrices::dct256x256(),
                DequantMatrices::dct128x256(),
            ]
        })
    }

    pub fn matrix(&self, quant_kind: HfTransformType, c: usize) -> &[f32] {
        assert_ne!((1 << quant_kind as u32) & self.computed_mask, 0);
        &self.table[self.table_offsets[quant_kind as usize * 3 + c]..]
    }

    // TODO(veluca): figure out if this should actually be unused.
    #[allow(dead_code)]
    pub fn inv_matrix(&self, quant_kind: HfTransformType, c: usize) -> &[f32] {
        assert_ne!((1 << quant_kind as u32) & self.computed_mask, 0);
        &self.inv_table[self.table_offsets[quant_kind as usize * 3 + c]..]
    }

    pub fn decode(
        header: &FrameHeader,
        lf_global: &LfGlobalState,
        br: &mut BitReader,
    ) -> Result<Self> {
        let all_default = br.read(1)? == 1;
        let mut encodings = Vec::with_capacity(QuantTable::CARDINALITY);
        if all_default {
            for _ in 0..QuantTable::CARDINALITY {
                encodings.push(QuantEncoding::Library)
            }
        } else {
            for (i, (&required_size_x, required_size_y)) in Self::REQUIRED_SIZE_X
                .iter()
                .zip(Self::REQUIRED_SIZE_Y)
                .enumerate()
            {
                encodings.push(QuantEncoding::decode(
                    required_size_x,
                    required_size_y,
                    i,
                    header,
                    lf_global,
                    br,
                )?);
            }
        }
        Ok(Self {
            computed_mask: 0,
            table: vec![0.0; Self::TOTAL_TABLE_SIZE],
            inv_table: vec![0.0; Self::TOTAL_TABLE_SIZE],
            table_offsets: [0; HfTransformType::CARDINALITY * 3],
            encodings,
        })
    }

    pub const REQUIRED_SIZE_X: [usize; QuantTable::CARDINALITY] =
        [1, 1, 1, 1, 2, 4, 1, 1, 2, 1, 1, 8, 4, 16, 8, 32, 16];

    pub const REQUIRED_SIZE_Y: [usize; QuantTable::CARDINALITY] =
        [1, 1, 1, 1, 2, 4, 2, 4, 4, 1, 1, 8, 8, 16, 16, 32, 32];

    pub const SUM_REQUIRED_X_Y: usize = 2056;

    pub const TOTAL_TABLE_SIZE: usize = Self::SUM_REQUIRED_X_Y * BLOCK_SIZE * 3;

    pub fn ensure_computed(&mut self, acs_mask: u32) -> Result<()> {
        let mut offsets = [0usize; QuantTable::CARDINALITY * 3];
        let mut pos = 0usize;
        for i in 0..QuantTable::CARDINALITY {
            let num = DequantMatrices::REQUIRED_SIZE_X[i]
                * DequantMatrices::REQUIRED_SIZE_Y[i]
                * BLOCK_SIZE;
            for c in 0..3 {
                offsets[3 * i + c] = pos + c * num;
            }
            pos += 3 * num;
        }
        for i in 0..HfTransformType::CARDINALITY {
            for c in 0..3 {
                self.table_offsets[i * 3 + c] =
                    offsets[QuantTable::for_strategy(HfTransformType::from_usize(i).unwrap())
                        as usize
                        * 3
                        + c];
            }
        }
        let mut kind_mask = 0u32;
        for i in 0..HfTransformType::CARDINALITY {
            if acs_mask & (1u32 << i) != 0 {
                kind_mask |= 1u32 << QuantTable::for_strategy(HfTransformType::VALUES[i]) as u32;
            }
        }
        let mut computed_kind_mask = 0u32;
        for i in 0..HfTransformType::CARDINALITY {
            if self.computed_mask & (1u32 << i) != 0 {
                computed_kind_mask |=
                    1u32 << QuantTable::for_strategy(HfTransformType::VALUES[i]) as u32;
            }
        }
        for table in 0..QuantTable::CARDINALITY {
            if (1u32 << table) & computed_kind_mask != 0 {
                continue;
            }
            if (1u32 << table) & !kind_mask != 0 {
                continue;
            }
            match self.encodings[table] {
                QuantEncoding::Library => {
                    self.compute_quant_table(true, table, offsets[table * 3])?
                }
                _ => self.compute_quant_table(false, table, offsets[table * 3])?,
            };
        }
        self.computed_mask |= acs_mask;
        Ok(())
    }
    fn compute_quant_table(
        &mut self,
        library: bool,
        table_num: usize,
        offset: usize,
    ) -> Result<usize> {
        let encoding = if library {
            &DequantMatrices::library()[table_num]
        } else {
            &self.encodings[table_num]
        };
        let quant_table_idx = QuantTable::from_usize(table_num)? as usize;
        let wrows = 8 * DequantMatrices::REQUIRED_SIZE_X[quant_table_idx];
        let wcols = 8 * DequantMatrices::REQUIRED_SIZE_Y[quant_table_idx];
        let num = wrows * wcols;
        let mut weights = vec![0f32; 3 * num];
        match encoding {
            QuantEncoding::Library => {
                // Library and copy quant encoding should get replaced by the actual
                // parameters by the caller.
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
        for (i, weight) in weights.iter().enumerate() {
            if !(ALMOST_ZERO..=1.0 / ALMOST_ZERO).contains(weight) {
                return Err(InvalidQuantizationTableWeight(*weight));
            }
            self.table[offset + i] = 1f32 / weight;
            self.inv_table[offset + i] = *weight;
        }
        let (xs, ys) = coefficient_layout(
            DequantMatrices::REQUIRED_SIZE_X[quant_table_idx],
            DequantMatrices::REQUIRED_SIZE_Y[quant_table_idx],
        );
        for c in 0..3 {
            for y in 0..ys {
                for x in 0..xs {
                    self.inv_table[offset + c * ys * xs * BLOCK_SIZE + y * BLOCK_DIM * xs + x] =
                        0f32;
                }
            }
        }
        Ok(0)
    }
}

fn coefficient_layout(rows: usize, cols: usize) -> (usize, usize) {
    (
        if rows < cols { rows } else { cols },
        if rows < cols { cols } else { rows },
    )
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
    use crate::util::test::{assert_all_almost_abs_eq, assert_almost_abs_eq, assert_almost_eq};

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
        let mut matrices = DequantMatrices {
            computed_mask: 0,
            table: vec![0.0; DequantMatrices::TOTAL_TABLE_SIZE],
            inv_table: vec![0.0; DequantMatrices::TOTAL_TABLE_SIZE],
            table_offsets: [0; HfTransformType::CARDINALITY * 3],
            encodings: (0..QuantTable::CARDINALITY)
                .map(|_| QuantEncoding::Library)
                .collect(),
        };
        matrices.ensure_computed(!0)?;

        // Golden data produced by libjxl.
        let target_offsets: [usize; 81] = [
            0, 64, 128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 1024, 1280, 1536, 2560,
            3584, 4608, 4736, 4864, 4608, 4736, 4864, 4992, 5248, 5504, 4992, 5248, 5504, 5760,
            6272, 6784, 5760, 6272, 6784, 7296, 7360, 7424, 7296, 7360, 7424, 7488, 7552, 7616,
            7488, 7552, 7616, 7488, 7552, 7616, 7488, 7552, 7616, 7680, 11776, 15872, 19968, 22016,
            24064, 19968, 22016, 24064, 26112, 42496, 58880, 75264, 83456, 91648, 75264, 83456,
            91648, 99840, 165376, 230912, 296448, 329216, 361984, 296448, 329216, 361984,
        ];
        assert_all_almost_abs_eq(matrices.table_offsets, target_offsets, 0);

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
            let qt_idx = QuantTable::for_strategy(HfTransformType::from_usize(i).unwrap()) as usize;
            let size = DequantMatrices::REQUIRED_SIZE_X[qt_idx]
                * DequantMatrices::REQUIRED_SIZE_Y[qt_idx]
                * BLOCK_SIZE;
            for c in 0..3 {
                let start = matrices.table_offsets[3 * i + c];
                for j in (start..start + size).step_by(size / 10) {
                    assert_almost_abs_eq(matrices.table[j], target_table[target_table_index], 1e-5);
                    target_table_index += 1;
                }
            }
        }
        // Golden data produced by libjxl.
        let target_inv_table = [
            0.000000f32,
            1_590.757_8_f32,
            2_188.414_6_f32,
            2_726.993_f32,
            2_648.627_7_f32,
            1_410.374_f32,
            1_686.279_4_f32,
            1_765.964_1_f32,
            1_590.757_8_f32,
            838.702_15_f32,
            1_060.592_9_f32,
            0.000000f32,
            328.723_8_f32,
            421.547_42_f32,
            500.443_73_f32,
            489.194_1_f32,
            299.277_44_f32,
            344.016_4_f32,
            356.627_56_f32,
            328.723_8_f32,
            236.484_7_f32,
            250.119_8_f32,
            0.000000f32,
            83.550804f32,
            85.333_32_f32,
            126.804_84_f32,
            119.412384f32,
            65.203_81_f32,
            85.333_23_f32,
            85.333244f32,
            83.550804f32,
            31.172384f32,
            39.419487f32,
            0.000000f32,
            280.000000f32,
            280.000000f32,
            280.000000f32,
            280.000000f32,
            280.000000f32,
            280.000000f32,
            280.000000f32,
            280.000000f32,
            280.000000f32,
            280.000000f32,
            0.000000f32,
            60.000000f32,
            60.000000f32,
            60.000000f32,
            60.000000f32,
            60.000000f32,
            60.000000f32,
            60.000000f32,
            60.000000f32,
            60.000000f32,
            60.000000f32,
            0.000000f32,
            18.000000f32,
            18.000000f32,
            18.000000f32,
            18.000000f32,
            18.000000f32,
            18.000000f32,
            18.000000f32,
            18.000000f32,
            18.000000f32,
            18.000000f32,
            0.000000f32,
            480.000000f32,
            480.000000f32,
            640.000000f32,
            1280.000000f32,
            480.000000f32,
            300.000000f32,
            480.000000f32,
            480.000000f32,
            300.000000f32,
            300.000000f32,
            0.000000f32,
            140.000000f32,
            140.000000f32,
            180.000000f32,
            320.000000f32,
            140.000000f32,
            120.000000f32,
            140.000000f32,
            140.000000f32,
            120.000000f32,
            120.000000f32,
            0.000000f32,
            32.000000f32,
            32.000000f32,
            64.000000f32,
            128.000000f32,
            32.000000f32,
            16.000000f32,
            32.000000f32,
            32.000000f32,
            16.000000f32,
            16.000000f32,
            0.000000f32,
            2_199.999_5_f32,
            2_199.998_8_f32,
            2_199.997_f32,
            2_199.997_8_f32,
            2_199.999_3_f32,
            2_199.997_f32,
            2_199.998_3_f32,
            2_199.999_5_f32,
            2_199.997_f32,
            2_199.998_3_f32,
            0.000000f32,
            391.999_94_f32,
            391.999_76_f32,
            391.999_45_f32,
            391.999_63_f32,
            391.999_85_f32,
            391.999_45_f32,
            391.999_7_f32,
            391.999_94_f32,
            391.999_45_f32,
            391.999_7_f32,
            0.000000f32,
            68.239_35_f32,
            81.689606f32,
            89.600_1_f32,
            95.651_69_f32,
            65.137_18_f32,
            71.680_09_f32,
            78.702_8_f32,
            68.239_35_f32,
            47.786777f32,
            57.363_38_f32,
            0.000000f32,
            2_134.025_f32,
            3_880.564_5_f32,
            1_561.426_1_f32,
            2_580.273_2_f32,
            993.463_3_f32,
            1_766.659_2_f32,
            531.866_15_f32,
            1_057.315_1_f32,
            1_129.141_7_f32,
            531.866_15_f32,
            0.000000f32,
            856.371_03_f32,
            1_884.007_1_f32,
            661.633_f32,
            1_039.256_2_f32,
            510.514_92_f32,
            714.580_6_f32,
            395.167_5_f32,
            524.174_9_f32,
            540.578_f32,
            395.167_5_f32,
            0.000000f32,
            125.492_86_f32,
            372.602_72_f32,
            93.867_99_f32,
            155.421_52_f32,
            62.573_67_f32,
            102.638_92_f32,
            24.780632f32,
            68.345_99_f32,
            74.248_6_f32,
            24.780632f32,
            0.000000f32,
            7_394.222_7_f32,
            3_578.031_f32,
            1_919.217_f32,
            1_315.414_7_f32,
            873.481_14_f32,
            1_993.637_6_f32,
            1_544.639_8_f32,
            1_097.804_9_f32,
            777.788_7_f32,
            593.406_25_f32,
            0.000000f32,
            3_889.284_f32,
            2_156.401_9_f32,
            1_353.482_9_f32,
            888.446_17_f32,
            607.867_1_f32,
            1_416.747_f32,
            1_042.852_7_f32,
            753.371_15_f32,
            543.717_3_f32,
            415.901_12_f32,
            0.000000f32,
            865.445_74_f32,
            263.145_42_f32,
            92.775_6_f32,
            59.736_86_f32,
            41.660606f32,
            97.485_29_f32,
            69.935_29_f32,
            51.259308f32,
            37.279_94_f32,
            28.132723f32,
            0.000000f32,
            1_943.121_3_f32,
            2_353.786_9_f32,
            3_003.803_7_f32,
            2_759.090_6_f32,
            1_787.990_7_f32,
            1_970.900_4_f32,
            2_000.403_2_f32,
            1_859.104_f32,
            1_456.708_3_f32,
            1_501.485_4_f32,
            0.000000f32,
            399.333_1_f32,
            560.170_4_f32,
            739.322_3_f32,
            692.840_9_f32,
            367.452_f32,
            405.042_f32,
            411.105_13_f32,
            382.066_6_f32,
            299.369_78_f32,
            308.571_96_f32,
            0.000000f32,
            100.000015f32,
            153.171_57_f32,
            187.309_95_f32,
            181.919_97_f32,
            83.107_42_f32,
            103.207_18_f32,
            106.674_48_f32,
            90.637794f32,
            32.030476f32,
            37.294_44_f32,
            0.000000f32,
            1_943.121_3_f32,
            2_353.786_9_f32,
            3_003.803_7_f32,
            2_759.090_6_f32,
            1_787.990_7_f32,
            1_970.900_4_f32,
            2_000.403_2_f32,
            1_859.104_f32,
            1_456.708_3_f32,
            1_501.485_4_f32,
            0.000000f32,
            399.333_1_f32,
            560.170_4_f32,
            739.322_3_f32,
            692.840_9_f32,
            367.452_f32,
            405.042_f32,
            411.105_13_f32,
            382.066_6_f32,
            299.369_78_f32,
            308.571_96_f32,
            0.000000f32,
            100.000015f32,
            153.171_57_f32,
            187.309_95_f32,
            181.919_97_f32,
            83.107_42_f32,
            103.207_18_f32,
            106.674_48_f32,
            90.637794f32,
            32.030476f32,
            37.294_44_f32,
            0.000000f32,
            593.167_f32,
            1_123.533_1_f32,
            1_855.866_1_f32,
            1_908.905_2_f32,
            326.994_14_f32,
            450.258_58_f32,
            511.279_08_f32,
            469.570_34_f32,
            356.047_f32,
            126.164_13_f32,
            0.000000f32,
            1_362.585_8_f32,
            2_208.564_f32,
            2_662.055_2_f32,
            2_690.115_7_f32,
            871.264_95_f32,
            1_103.732_8_f32,
            1_216.298_7_f32,
            1_139.726_f32,
            922.482_9_f32,
            485.887_88_f32,
            0.000000f32,
            593.843_3_f32,
            1_146.650_3_f32,
            1_669.026_5_f32,
            1_704.578_1_f32,
            276.873_93_f32,
            414.831_3_f32,
            489.748_35_f32,
            438.224_15_f32,
            305.274_23_f32,
            103.268776f32,
            0.000000f32,
            593.167_f32,
            1_123.533_1_f32,
            1_855.866_1_f32,
            1_908.905_2_f32,
            326.994_14_f32,
            450.258_58_f32,
            511.279_08_f32,
            469.570_34_f32,
            356.047_f32,
            126.164_13_f32,
            0.000000f32,
            1_362.585_8_f32,
            2_208.564_f32,
            2_662.055_2_f32,
            2_690.115_7_f32,
            871.264_95_f32,
            1_103.732_8_f32,
            1_216.298_7_f32,
            1_139.726_f32,
            922.482_9_f32,
            485.887_88_f32,
            0.000000f32,
            593.843_3_f32,
            1_146.650_3_f32,
            1_669.026_5_f32,
            1_704.578_1_f32,
            276.873_93_f32,
            414.831_3_f32,
            489.748_35_f32,
            438.224_15_f32,
            305.274_23_f32,
            103.268776f32,
            0.000000f32,
            2_951.45_f32,
            5_803.090_3_f32,
            2_333.720_7_f32,
            3_250.279_3_f32,
            1_812.437_7_f32,
            2_367.215_3_f32,
            2_575.901_6_f32,
            1_794.716_f32,
            2_014.430_4_f32,
            1_070.065_2_f32,
            0.000000f32,
            894.280_7_f32,
            2_366.964_4_f32,
            667.591_43_f32,
            1_044.054_9_f32,
            468.918_46_f32,
            683.237_6_f32,
            763.157_1_f32,
            464.274_26_f32,
            525.577_7_f32,
            355.034_94_f32,
            0.000000f32,
            213.711_44_f32,
            609.947_6_f32,
            115.928_83_f32,
            250.111_22_f32,
            65.055_44_f32,
            120.410_9_f32,
            150.171_69_f32,
            64.008_35_f32,
            78.361_82_f32,
            37.872444f32,
            0.000000f32,
            2_951.45_f32,
            5_803.090_3_f32,
            2_333.720_7_f32,
            3_250.279_3_f32,
            1_812.437_7_f32,
            2_367.215_3_f32,
            2_575.901_6_f32,
            1_794.716_f32,
            2_014.430_4_f32,
            1_070.065_2_f32,
            0.000000f32,
            894.280_7_f32,
            2_366.964_4_f32,
            667.591_43_f32,
            1_044.054_9_f32,
            468.918_46_f32,
            683.237_6_f32,
            763.157_1_f32,
            464.274_26_f32,
            525.577_7_f32,
            355.034_94_f32,
            0.000000f32,
            213.711_44_f32,
            609.947_6_f32,
            115.928_83_f32,
            250.111_22_f32,
            65.055_44_f32,
            120.410_9_f32,
            150.171_69_f32,
            64.008_35_f32,
            78.361_82_f32,
            37.872444f32,
            0.000000f32,
            704.524_2_f32,
            993.091_86_f32,
            1_173.002_2_f32,
            1_364.454_1_f32,
            653.527_9_f32,
            687.044_7_f32,
            825.446_53_f32,
            597.922_5_f32,
            426.046_05_f32,
            508.395_4_f32,
            0.000000f32,
            228.070_65_f32,
            343.725_7_f32,
            415.080_72_f32,
            480.806_3_f32,
            208.487_34_f32,
            221.326_13_f32,
            275.591_58_f32,
            195.755_52_f32,
            165.941_71_f32,
            180.871_83_f32,
            0.000000f32,
            102.945_56_f32,
            177.209_15_f32,
            227.986_3_f32,
            278.956_88_f32,
            91.407_4_f32,
            98.934_02_f32,
            132.264_72_f32,
            77.957184f32,
            41.162014f32,
            57.426_7_f32,
            0.000000f32,
            704.524_2_f32,
            993.091_86_f32,
            1_173.002_2_f32,
            1_364.454_1_f32,
            653.527_9_f32,
            687.044_7_f32,
            825.446_53_f32,
            597.922_5_f32,
            426.046_05_f32,
            508.395_4_f32,
            0.000000f32,
            228.070_65_f32,
            343.725_7_f32,
            415.080_72_f32,
            480.806_3_f32,
            208.487_34_f32,
            221.326_13_f32,
            275.591_58_f32,
            195.755_52_f32,
            165.941_71_f32,
            180.871_83_f32,
            0.000000f32,
            102.945_56_f32,
            177.209_15_f32,
            227.986_3_f32,
            278.956_88_f32,
            91.407_4_f32,
            98.934_02_f32,
            132.264_72_f32,
            77.957184f32,
            41.162014f32,
            57.426_7_f32,
            0.000000f32,
            413.999_94_f32,
            993.091_86_f32,
            256.000000f32,
            1_364.454_1_f32,
            653.527_9_f32,
            413.999_66_f32,
            825.446_53_f32,
            413.999_73_f32,
            413.999_42_f32,
            508.395_4_f32,
            0.000000f32,
            57.999_99_f32,
            343.725_7_f32,
            50.000000f32,
            480.806_3_f32,
            208.487_34_f32,
            57.999954f32,
            275.591_58_f32,
            57.999_96_f32,
            57.999_92_f32,
            180.871_83_f32,
            0.000000f32,
            17.133793f32,
            177.209_15_f32,
            12.000000f32,
            278.956_88_f32,
            91.407_4_f32,
            15.428568f32,
            132.264_72_f32,
            19.905685f32,
            11.264012f32,
            57.426_7_f32,
            0.000000f32,
            413.999_94_f32,
            993.091_86_f32,
            256.000000f32,
            1_364.454_1_f32,
            653.527_9_f32,
            413.999_66_f32,
            825.446_53_f32,
            413.999_73_f32,
            413.999_42_f32,
            508.395_4_f32,
            0.000000f32,
            57.999_99_f32,
            343.725_7_f32,
            50.000000f32,
            480.806_3_f32,
            208.487_34_f32,
            57.999954f32,
            275.591_58_f32,
            57.999_96_f32,
            57.999_92_f32,
            180.871_83_f32,
            0.000000f32,
            17.133793f32,
            177.209_15_f32,
            12.000000f32,
            278.956_88_f32,
            91.407_4_f32,
            15.428568f32,
            132.264_72_f32,
            19.905685f32,
            11.264012f32,
            57.426_7_f32,
            0.000000f32,
            413.999_94_f32,
            993.091_86_f32,
            256.000000f32,
            1_364.454_1_f32,
            653.527_9_f32,
            413.999_66_f32,
            825.446_53_f32,
            413.999_73_f32,
            413.999_42_f32,
            508.395_4_f32,
            0.000000f32,
            57.999_99_f32,
            343.725_7_f32,
            50.000000f32,
            480.806_3_f32,
            208.487_34_f32,
            57.999954f32,
            275.591_58_f32,
            57.999_96_f32,
            57.999_92_f32,
            180.871_83_f32,
            0.000000f32,
            17.133793f32,
            177.209_15_f32,
            12.000000f32,
            278.956_88_f32,
            91.407_4_f32,
            15.428568f32,
            132.264_72_f32,
            19.905685f32,
            11.264012f32,
            57.426_7_f32,
            0.000000f32,
            413.999_94_f32,
            993.091_86_f32,
            256.000000f32,
            1_364.454_1_f32,
            653.527_9_f32,
            413.999_66_f32,
            825.446_53_f32,
            413.999_73_f32,
            413.999_42_f32,
            508.395_4_f32,
            0.000000f32,
            57.999_99_f32,
            343.725_7_f32,
            50.000000f32,
            480.806_3_f32,
            208.487_34_f32,
            57.999954f32,
            275.591_58_f32,
            57.999_96_f32,
            57.999_92_f32,
            180.871_83_f32,
            0.000000f32,
            17.133793f32,
            177.209_15_f32,
            12.000000f32,
            278.956_88_f32,
            91.407_4_f32,
            15.428568f32,
            132.264_72_f32,
            19.905685f32,
            11.264012f32,
            57.426_7_f32,
            0.000000f32,
            6_582.816_4_f32,
            3_359.388_7_f32,
            7_791.875_5_f32,
            3_729.601_3_f32,
            2_454.834_f32,
            3_725.529_f32,
            2_744.766_4_f32,
            3_349.231_4_f32,
            2_634.738_8_f32,
            1_604.219_2_f32,
            0.000000f32,
            4_684.622_6_f32,
            2_554.650_4_f32,
            5_132.672_f32,
            3_046.984_1_f32,
            1_750.527_1_f32,
            3_041.338_1_f32,
            1_903.525_6_f32,
            2_542.798_3_f32,
            1_845.962_4_f32,
            1_245.448_2_f32,
            0.000000f32,
            917.482_67_f32,
            297.010_62_f32,
            1_153.165_4_f32,
            407.573_73_f32,
            157.246_46_f32,
            406.220_34_f32,
            174.986_19_f32,
            294.497_8_f32,
            168.263_95_f32,
            94.887_52_f32,
            0.000000f32,
            7_337.233_f32,
            4_022.487_3_f32,
            2_505.753_7_f32,
            2_076.849_f32,
            1_622.844_2_f32,
            2_538.46_f32,
            2_227.385_5_f32,
            1_893.945_9_f32,
            1_428.085_7_f32,
            1_059.229_1_f32,
            0.000000f32,
            4_215.989_f32,
            3_039.568_4_f32,
            2_205.074_5_f32,
            1_614.652_6_f32,
            1_196.811_4_f32,
            2_254.153_8_f32,
            1_805.539_f32,
            1_401.511_f32,
            1_087.307_5_f32,
            853.323_6_f32,
            0.000000f32,
            1_268.412_f32,
            563.855_9_f32,
            305.841_95_f32,
            174.499_47_f32,
            105.278_36_f32,
            318.157_75_f32,
            213.698_53_f32,
            134.729_1_f32,
            93.148544f32,
            64.359_23_f32,
            0.000000f32,
            7_337.233_f32,
            4_022.487_3_f32,
            2_505.753_7_f32,
            2_076.849_f32,
            1_622.844_2_f32,
            2_538.46_f32,
            2_227.385_5_f32,
            1_893.945_9_f32,
            1_428.085_7_f32,
            1_059.229_1_f32,
            0.000000f32,
            4_215.989_f32,
            3_039.568_4_f32,
            2_205.074_5_f32,
            1_614.652_6_f32,
            1_196.811_4_f32,
            2_254.153_8_f32,
            1_805.539_f32,
            1_401.511_f32,
            1_087.307_5_f32,
            853.323_6_f32,
            0.000000f32,
            1_268.412_f32,
            563.855_9_f32,
            305.841_95_f32,
            174.499_47_f32,
            105.278_36_f32,
            318.157_75_f32,
            213.698_53_f32,
            134.729_1_f32,
            93.148544f32,
            64.359_23_f32,
            0.000000f32,
            6_766.112_f32,
            7_894.433_6_f32,
            10_627.11_f32,
            12_049.799_f32,
            4_716.169_f32,
            5_715.242_f32,
            6_145.953_6_f32,
            6_284.098_6_f32,
            6_085.547_f32,
            3_040.497_3_f32,
            0.000000f32,
            5_164.680_7_f32,
            6_709.773_f32,
            8_223.506_f32,
            8_877.354_5_f32,
            3_396.971_2_f32,
            3_985.426_5_f32,
            4_455.855_5_f32,
            4_610.580_6_f32,
            4_388.779_f32,
            2_379.244_9_f32,
            0.000000f32,
            605.837_65_f32,
            968.753_9_f32,
            1_427.095_5_f32,
            1_653.824_2_f32,
            302.614_8_f32,
            377.299_22_f32,
            462.613_9_f32,
            492.384_12_f32,
            449.969_45_f32,
            175.714_2_f32,
            0.000000f32,
            8_341.217_f32,
            4_268.698_f32,
            9_660.034_f32,
            4_689.038_f32,
            2_994.779_5_f32,
            4_679.943_f32,
            3_301.692_6_f32,
            4_245.340_3_f32,
            3_177.615_5_f32,
            1_918.587_6_f32,
            0.000000f32,
            6_214.485_4_f32,
            3_367.615_5_f32,
            6_734.941_4_f32,
            3_939.316_2_f32,
            2_253.354_2_f32,
            3_926.354_7_f32,
            2_424.556_6_f32,
            3_339.36_f32,
            2_355.843_5_f32,
            1_568.312_6_f32,
            0.000000f32,
            1_176.600_6_f32,
            376.791_66_f32,
            1_432.170_2_f32,
            499.566_1_f32,
            194.944_96_f32,
            496.622_07_f32,
            214.034_21_f32,
            371.035_58_f32,
            206.326_42_f32,
            111.690674f32,
            0.000000f32,
            8_341.217_f32,
            4_268.698_f32,
            9_660.034_f32,
            4_689.038_f32,
            2_994.779_5_f32,
            4_679.943_f32,
            3_301.692_6_f32,
            4_245.340_3_f32,
            3_177.615_5_f32,
            1_918.587_6_f32,
            0.000000f32,
            6_214.485_4_f32,
            3_367.615_5_f32,
            6_734.941_4_f32,
            3_939.316_2_f32,
            2_253.354_2_f32,
            3_926.354_7_f32,
            2_424.556_6_f32,
            3_339.36_f32,
            2_355.843_5_f32,
            1_568.312_6_f32,
            0.000000f32,
            1_176.600_6_f32,
            376.791_66_f32,
            1_432.170_2_f32,
            499.566_1_f32,
            194.944_96_f32,
            496.622_07_f32,
            214.034_21_f32,
            371.035_58_f32,
            206.326_42_f32,
            111.690674f32,
            0.000000f32,
            16_091.596_f32,
            37_886.605_f32,
            13_018.401_f32,
            18_061.066_f32,
            9_417.376_f32,
            13_138.253_f32,
            14_536.568_f32,
            9_251.921_f32,
            11_539.108_f32,
            6_057.114_3_f32,
            0.000000f32,
            13_859.232_f32,
            22_801.957_f32,
            9_733.233_f32,
            14_894.771_f32,
            6_785.853_f32,
            9_871.179_f32,
            11_663.131_f32,
            6_696.171_4_f32,
            8_087.468_3_f32,
            4_742.546_f32,
            0.000000f32,
            2_052.832_f32,
            6_023.987_f32,
            1_086.971_4_f32,
            2_357.806_6_f32,
            604.310_36_f32,
            1_115.282_2_f32,
            1_506.556_9_f32,
            594.140_56_f32,
            774.890_87_f32,
            349.454_04_f32,
            0.000000f32,
            8_696.038_f32,
            10_137.892_f32,
            13_662.467_f32,
            15_456.473_f32,
            6_081.469_f32,
            7_342.178_f32,
            7_888.129_4_f32,
            8_059.175_3_f32,
            7_800.867_f32,
            3_911.675_f32,
            0.000000f32,
            6_930.828_6_f32,
            8_992.589_f32,
            11_005.801_f32,
            11_864.501_f32,
            4_558.516_6_f32,
            5_342.787_6_f32,
            5_964.875_5_f32,
            6_164.648_f32,
            5_863.846_7_f32,
            3_188.496_8_f32,
            0.000000f32,
            793.949_34_f32,
            1_266.555_7_f32,
            1_861.544_6_f32,
            2_151.571_5_f32,
            395.616_76_f32,
            493.578_8_f32,
            603.603_2_f32,
            641.048_8_f32,
            585.055_24_f32,
            229.617_22_f32,
            0.000000f32,
            8_696.038_f32,
            10_137.892_f32,
            13_662.467_f32,
            15_456.473_f32,
            6_081.469_f32,
            7_342.178_f32,
            7_888.129_4_f32,
            8_059.175_3_f32,
            7_800.867_f32,
            3_911.675_f32,
            0.000000f32,
            6_930.828_6_f32,
            8_992.589_f32,
            11_005.801_f32,
            11_864.501_f32,
            4_558.516_6_f32,
            5_342.787_6_f32,
            5_964.875_5_f32,
            6_164.648_f32,
            5_863.846_7_f32,
            3_188.496_8_f32,
            0.000000f32,
            793.949_34_f32,
            1_266.555_7_f32,
            1_861.544_6_f32,
            2_151.571_5_f32,
            395.616_76_f32,
            493.578_8_f32,
            603.603_2_f32,
            641.048_8_f32,
            585.055_24_f32,
            229.617_22_f32,
        ];
        let mut target_inv_table_index = 0;
        for i in 0..HfTransformType::CARDINALITY {
            let qt_idx = QuantTable::for_strategy(HfTransformType::from_usize(i).unwrap()) as usize;
            let size = DequantMatrices::REQUIRED_SIZE_X[qt_idx]
                * DequantMatrices::REQUIRED_SIZE_Y[qt_idx]
                * BLOCK_SIZE;
            for c in 0..3 {
                let start = matrices.table_offsets[3 * i + c];
                for j in (start..start + size).step_by(size / 10) {
                    assert_almost_eq(
                        matrices.inv_table[j],
                        target_inv_table[target_inv_table_index],
                        1f32,
                        1e-3,
                    );
                    target_inv_table_index += 1;
                }
            }
        }
        Ok(())
    }
}
