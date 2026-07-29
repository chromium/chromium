// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::excessive_precision)]
#![allow(clippy::approx_constant)]

use crate::{transform_map::HfTransformType, *};
use jxl_simd::{simd_function, SimdDescriptor};

fn idct2_top_block(s: usize, block_in: &[f32], block_out: &mut [f32]) {
    let num_2x2 = s / 2;
    for y in 0..num_2x2 {
        for x in 0..num_2x2 {
            let c00 = block_in[y * 8 + x];
            let c01 = block_in[y * 8 + num_2x2 + x];
            let c10 = block_in[(y + num_2x2) * 8 + x];
            let c11 = block_in[(y + num_2x2) * 8 + num_2x2 + x];
            let r00 = c00 + c01 + c10 + c11;
            let r01 = c00 + c01 - c10 - c11;
            let r10 = c00 - c01 + c10 - c11;
            let r11 = c00 - c01 - c10 + c11;
            block_out[y * 2 * 8 + x * 2] = r00;
            block_out[y * 2 * 8 + x * 2 + 1] = r01;
            block_out[(y * 2 + 1) * 8 + x * 2] = r10;
            block_out[(y * 2 + 1) * 8 + x * 2 + 1] = r11;
        }
    }
}

const AFV4X4BASIS: [f32; 256] = [
    0.25,
    0.25,
    0.25,
    0.25,
    0.25,
    0.25,
    0.25,
    0.25,
    0.25,
    0.25,
    0.25,
    0.25,
    0.25,
    0.25,
    0.25,
    0.25,
    0.876902929799142,
    0.2206518106944235,
    -0.10140050393753763,
    -0.1014005039375375,
    0.2206518106944236,
    -0.10140050393753777,
    -0.10140050393753772,
    -0.10140050393753763,
    -0.10140050393753758,
    -0.10140050393753769,
    -0.1014005039375375,
    -0.10140050393753768,
    -0.10140050393753768,
    -0.10140050393753759,
    -0.10140050393753763,
    -0.10140050393753741,
    0.0,
    0.0,
    0.40670075830260755,
    0.44444816619734445,
    0.0,
    0.0,
    0.19574399372042936,
    0.2929100136981264,
    -0.40670075830260716,
    -0.19574399372042872,
    0.0,
    0.11379074460448091,
    -0.44444816619734384,
    -0.29291001369812636,
    -0.1137907446044814,
    0.0,
    0.0,
    0.0,
    -0.21255748058288748,
    0.3085497062849767,
    0.0,
    0.4706702258572536,
    -0.1621205195722993,
    0.0,
    -0.21255748058287047,
    -0.16212051957228327,
    -0.47067022585725277,
    -0.1464291867126764,
    0.3085497062849487,
    0.0,
    -0.14642918671266536,
    0.4251149611657548,
    0.0,
    -0.7071067811865474,
    0.0,
    0.0,
    0.7071067811865476,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    -0.4105377591765233,
    0.6235485373547691,
    -0.06435071657946274,
    -0.06435071657946266,
    0.6235485373547694,
    -0.06435071657946284,
    -0.0643507165794628,
    -0.06435071657946274,
    -0.06435071657946272,
    -0.06435071657946279,
    -0.06435071657946266,
    -0.06435071657946277,
    -0.06435071657946277,
    -0.06435071657946273,
    -0.06435071657946274,
    -0.0643507165794626,
    0.0,
    0.0,
    -0.4517556589999482,
    0.15854503551840063,
    0.0,
    -0.04038515160822202,
    0.0074182263792423875,
    0.39351034269210167,
    -0.45175565899994635,
    0.007418226379244351,
    0.1107416575309343,
    0.08298163094882051,
    0.15854503551839705,
    0.3935103426921022,
    0.0829816309488214,
    -0.45175565899994796,
    0.0,
    0.0,
    -0.304684750724869,
    0.5112616136591823,
    0.0,
    0.0,
    -0.290480129728998,
    -0.06578701549142804,
    0.304684750724884,
    0.2904801297290076,
    0.0,
    -0.23889773523344604,
    -0.5112616136592012,
    0.06578701549142545,
    0.23889773523345467,
    0.0,
    0.0,
    0.0,
    0.3017929516615495,
    0.25792362796341184,
    0.0,
    0.16272340142866204,
    0.09520022653475037,
    0.0,
    0.3017929516615503,
    0.09520022653475055,
    -0.16272340142866173,
    -0.35312385449816297,
    0.25792362796341295,
    0.0,
    -0.3531238544981624,
    -0.6035859033230976,
    0.0,
    0.0,
    0.40824829046386274,
    0.0,
    0.0,
    0.0,
    0.0,
    -0.4082482904638628,
    -0.4082482904638635,
    0.0,
    0.0,
    -0.40824829046386296,
    0.0,
    0.4082482904638634,
    0.408248290463863,
    0.0,
    0.0,
    0.0,
    0.1747866975480809,
    0.0812611176717539,
    0.0,
    0.0,
    -0.3675398009862027,
    -0.307882213957909,
    -0.17478669754808135,
    0.3675398009862011,
    0.0,
    0.4826689115059883,
    -0.08126111767175039,
    0.30788221395790305,
    -0.48266891150598584,
    0.0,
    0.0,
    0.0,
    -0.21105601049335784,
    0.18567180916109802,
    0.0,
    0.0,
    0.49215859013738733,
    -0.38525013709251915,
    0.21105601049335806,
    -0.49215859013738905,
    0.0,
    0.17419412659916217,
    -0.18567180916109904,
    0.3852501370925211,
    -0.1741941265991621,
    0.0,
    0.0,
    0.0,
    -0.14266084808807264,
    -0.3416446842253372,
    0.0,
    0.7367497537172237,
    0.24627107722075148,
    -0.08574019035519306,
    -0.14266084808807344,
    0.24627107722075137,
    0.14883399227113567,
    -0.04768680350229251,
    -0.3416446842253373,
    -0.08574019035519267,
    -0.047686803502292804,
    -0.14266084808807242,
    0.0,
    0.0,
    -0.13813540350758585,
    0.3302282550303788,
    0.0,
    0.08755115000587084,
    -0.07946706605909573,
    -0.4613374887461511,
    -0.13813540350758294,
    -0.07946706605910261,
    0.49724647109535086,
    0.12538059448563663,
    0.3302282550303805,
    -0.4613374887461554,
    0.12538059448564315,
    -0.13813540350758452,
    0.0,
    0.0,
    -0.17437602599651067,
    0.0702790691196284,
    0.0,
    -0.2921026642334881,
    0.3623817333531167,
    0.0,
    -0.1743760259965108,
    0.36238173335311646,
    0.29210266423348785,
    -0.4326608024727445,
    0.07027906911962818,
    0.0,
    -0.4326608024727457,
    0.34875205199302267,
    0.0,
    0.0,
    0.11354987314994337,
    -0.07417504595810355,
    0.0,
    0.19402893032594343,
    -0.435190496523228,
    0.21918684838857466,
    0.11354987314994257,
    -0.4351904965232251,
    0.5550443808910661,
    -0.25468277124066463,
    -0.07417504595810233,
    0.2191868483885728,
    -0.25468277124066413,
    0.1135498731499429,
];

#[allow(clippy::excessive_precision)]
#[allow(clippy::approx_constant)]
fn avfidct4x4(coeffs: &[f32], pixels: &mut [f32]) {
    for i in 0..16 {
        let mut pixel: f32 = 0.0;
        for j in 0..16 {
            pixel += coeffs[j] * AFV4X4BASIS[j * 16 + i];
        }
        pixels[i] = pixel;
    }
}

#[inline(always)]
fn afv_transform_to_pixels<D: SimdDescriptor>(
    d: D,
    afv_kind: usize,
    coefficients: &[f32],
    pixels: &mut [f32],
) {
    let afv_x = afv_kind & 1;
    let afv_y = afv_kind / 2;
    let block00 = coefficients[0];
    let block01 = coefficients[1];
    let block10 = coefficients[8];
    let dcs: [f32; 3] = [
        (block00 + block10 + block01) * 4.0,
        block00 + block10 - block01,
        block00 - block10,
    ];
    // IAFV: (even, even) positions.
    let mut coeff: Vec<f32> = vec![0.0; 4 * 4];
    for iy in 0..4 {
        for ix in 0..4 {
            coeff[iy * 4 + ix] = if ix == 0 && iy == 0 {
                dcs[0]
            } else {
                coefficients[iy * 2 * 8 + ix * 2]
            };
        }
    }
    let mut block: Vec<f32> = vec![0.0; 4 * 8];
    avfidct4x4(&coeff, &mut block);
    for iy in 0..4 {
        let block_y = if afv_y == 1 { 3 - iy } else { iy };
        for ix in 0..4 {
            let block_x = if afv_x == 1 { 3 - ix } else { ix };
            pixels[(iy + afv_y * 4) * 8 + afv_x * 4 + ix] = block[block_y * 4 + block_x];
        }
    }
    // IDCT4x4 in (odd, even) positions.
    for iy in 0..4 {
        for ix in 0..4 {
            block[iy * 4 + ix] = if ix == 0 && iy == 0 {
                dcs[1]
            } else {
                coefficients[iy * 2 * 8 + ix * 2 + 1]
            };
        }
    }
    idct2d_4_4(d, &mut block[0..16]);
    for iy in 0..4 {
        for ix in 0..4 {
            pixels[(iy + afv_y * 4) * 8 + (1 - afv_x) * 4 + ix] = block[iy * 4 + ix];
        }
    }
    // IDCT4x8.
    for iy in 0..4 {
        for ix in 0..8 {
            block[iy * 8 + ix] = if ix == 0 && iy == 0 {
                dcs[2]
            } else {
                coefficients[(1 + iy * 2) * 8 + ix]
            };
        }
    }
    idct2d_4_8(d, &mut block);
    for iy in 0..4 {
        for ix in 0..8 {
            pixels[(iy + (1 - afv_y) * 4) * 8 + ix] = block[iy * 8 + ix];
        }
    }
}

#[inline(always)]
pub fn transform_to_pixels_impl<D: SimdDescriptor>(
    d: D,
    transform_type: HfTransformType,
    lf: &mut [f32],
    transform_buffer: &mut [f32],
) {
    match transform_type {
        HfTransformType::DCT => d.call(
            #[inline(always)]
            |_| {
                transform_buffer[0] = lf[0];
                idct2d_8_8(d, &mut transform_buffer[0..64]);
            },
        ),
        HfTransformType::DCT16X16 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_2_2(d, &mut lf[..4], transform_buffer);
                idct2d_16_16(d, &mut transform_buffer[0..256]);
            },
        ),
        HfTransformType::DCT32X32 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_4_4(d, &mut lf[..16], transform_buffer);
                idct2d_32_32(d, &mut transform_buffer[0..1024]);
            },
        ),
        HfTransformType::DCT16X8 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_2_1(d, &mut lf[..2], transform_buffer);
                idct2d_16_8(d, &mut transform_buffer[0..128]);
            },
        ),
        HfTransformType::DCT8X16 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_1_2(d, &mut lf[..2], transform_buffer);
                idct2d_8_16(d, &mut transform_buffer[0..128]);
            },
        ),
        HfTransformType::DCT32X8 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_4_1(d, &mut lf[..4], transform_buffer);
                idct2d_32_8(d, &mut transform_buffer[0..256]);
            },
        ),
        HfTransformType::DCT8X32 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_1_4(d, &mut lf[..4], transform_buffer);
                idct2d_8_32(d, &mut transform_buffer[0..256]);
            },
        ),
        HfTransformType::DCT32X16 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_4_2(d, &mut lf[..8], transform_buffer);
                idct2d_32_16(d, &mut transform_buffer[0..512]);
            },
        ),
        HfTransformType::DCT16X32 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_2_4(d, &mut lf[..8], transform_buffer);
                idct2d_16_32(d, &mut transform_buffer[0..512]);
            },
        ),
        HfTransformType::DCT64X64 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_8_8(d, &mut lf[..64], transform_buffer);
                idct2d_64_64(d, &mut transform_buffer[0..4096]);
            },
        ),
        HfTransformType::DCT64X32 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_8_4(d, &mut lf[..32], transform_buffer);
                idct2d_64_32(d, &mut transform_buffer[0..2048]);
            },
        ),
        HfTransformType::DCT32X64 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_4_8(d, &mut lf[..32], transform_buffer);
                idct2d_32_64(d, &mut transform_buffer[0..2048]);
            },
        ),
        HfTransformType::DCT128X128 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_16_16(d, &mut lf[..256], transform_buffer);
                idct2d_128_128(d, &mut transform_buffer[0..16384]);
            },
        ),
        HfTransformType::DCT128X64 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_16_8(d, &mut lf[..128], transform_buffer);
                idct2d_128_64(d, &mut transform_buffer[0..8192]);
            },
        ),
        HfTransformType::DCT64X128 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_8_16(d, &mut lf[..128], transform_buffer);
                idct2d_64_128(d, &mut transform_buffer[0..8192]);
            },
        ),
        HfTransformType::DCT256X256 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_32_32(d, &mut lf[..1024], transform_buffer);
                idct2d_256_256(d, &mut transform_buffer[0..65536]);
            },
        ),
        HfTransformType::DCT256X128 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_32_16(d, &mut lf[..512], transform_buffer);
                idct2d_256_128(d, &mut transform_buffer[0..32768]);
            },
        ),
        HfTransformType::DCT128X256 => d.call(
            #[inline(always)]
            |_| {
                reinterpreting_dct2d_16_32(d, &mut lf[..512], transform_buffer);
                idct2d_128_256(d, &mut transform_buffer[0..32768]);
            },
        ),
        HfTransformType::AFV0 => {
            transform_buffer[0] = lf[0];
            let block: Vec<f32> = transform_buffer[0..64].to_vec();
            afv_transform_to_pixels::<D>(d, 0, &block, &mut transform_buffer[0..64]);
        }
        HfTransformType::AFV1 => {
            transform_buffer[0] = lf[0];
            let block: Vec<f32> = transform_buffer[0..64].to_vec();
            afv_transform_to_pixels::<D>(d, 1, &block, &mut transform_buffer[0..64]);
        }
        HfTransformType::AFV2 => {
            transform_buffer[0] = lf[0];
            let block: Vec<f32> = transform_buffer[0..64].to_vec();
            afv_transform_to_pixels::<D>(d, 2, &block, &mut transform_buffer[0..64]);
        }
        HfTransformType::AFV3 => {
            transform_buffer[0] = lf[0];
            let block: Vec<f32> = transform_buffer[0..64].to_vec();
            afv_transform_to_pixels::<D>(d, 3, &block, &mut transform_buffer[0..64]);
        }
        HfTransformType::IDENTITY => {
            transform_buffer[0] = lf[0];
            let coefficients: [f32; 64] = transform_buffer[0..64].try_into().unwrap();
            let block00 = coefficients[0];
            let block01 = coefficients[1];
            let block10 = coefficients[8];
            let block11 = coefficients[9];
            let dcs: [f32; 4] = [
                block00 + block01 + block10 + block11,
                block00 + block01 - block10 - block11,
                block00 - block01 + block10 - block11,
                block00 - block01 - block10 + block11,
            ];
            for y in 0..2 {
                for x in 0..2 {
                    let block_dc = dcs[y * 2 + x];
                    let mut residual_sum = 0.0;
                    for iy in 0..4 {
                        for ix in 0..4 {
                            if ix == 0 && iy == 0 {
                                continue;
                            }
                            residual_sum += coefficients[(y + iy * 2) * 8 + x + ix * 2];
                        }
                    }
                    transform_buffer[(4 * y + 1) * 8 + 4 * x + 1] =
                        block_dc - residual_sum * (1.0 / 16.0);
                    for iy in 0..4 {
                        for ix in 0..4 {
                            if ix == 1 && iy == 1 {
                                continue;
                            }
                            transform_buffer[(y * 4 + iy) * 8 + x * 4 + ix] = coefficients
                                [(y + iy * 2) * 8 + x + ix * 2]
                                + transform_buffer[(4 * y + 1) * 8 + 4 * x + 1];
                        }
                    }
                    transform_buffer[y * 4 * 8 + x * 4] = coefficients[(y + 2) * 8 + x + 2]
                        + transform_buffer[(4 * y + 1) * 8 + 4 * x + 1];
                }
            }
        }
        HfTransformType::DCT2X2 => {
            transform_buffer[0] = lf[0];
            let mut tmp: [f32; 64] = transform_buffer[0..64].try_into().unwrap();
            idct2_top_block(2, &tmp, &mut transform_buffer[0..64]);
            idct2_top_block(4, &transform_buffer[0..64], &mut tmp);
            idct2_top_block(8, &tmp, &mut transform_buffer[0..64]);
        }
        HfTransformType::DCT4X4 => {
            transform_buffer[0] = lf[0];
            let coefficients: [f32; 64] = transform_buffer[0..64].try_into().unwrap();
            let block00 = coefficients[0];
            let block01 = coefficients[1];
            let block10 = coefficients[8];
            let block11 = coefficients[9];
            let dcs: [f32; 4] = [
                block00 + block01 + block10 + block11,
                block00 + block01 - block10 - block11,
                block00 - block01 + block10 - block11,
                block00 - block01 - block10 + block11,
            ];
            for y in 0..2 {
                for x in 0..2 {
                    let mut block = [0.0; 4 * 4];
                    block[0] = dcs[y * 2 + x];
                    for iy in 0..4 {
                        for ix in 0..4 {
                            if ix == 0 && iy == 0 {
                                continue;
                            }
                            block[iy * 4 + ix] = coefficients[(y + iy * 2) * 8 + x + ix * 2];
                        }
                    }
                    idct2d_4_4(d, &mut block);
                    for iy in 0..4 {
                        for ix in 0..4 {
                            transform_buffer[(y * 4 + iy) * 8 + x * 4 + ix] = block[iy * 4 + ix];
                        }
                    }
                }
            }
        }
        HfTransformType::DCT8X4 => {
            transform_buffer[0] = lf[0];
            let coefficients: [f32; 64] = transform_buffer[0..64].try_into().unwrap();
            let block0 = coefficients[0];
            let block1 = coefficients[8];
            let dcs: [f32; 2] = [block0 + block1, block0 - block1];
            for x in 0..2 {
                let mut block = [0.0; 8 * 4];
                for iy in 0..4 {
                    for ix in 0..8 {
                        block[iy * 8 + ix] = if ix == 0 && iy == 0 {
                            dcs[x]
                        } else {
                            coefficients[(x + iy * 2) * 8 + ix]
                        }
                    }
                }
                idct2d_8_4(d, &mut block);
                for iy in 0..8 {
                    for ix in 0..4 {
                        transform_buffer[iy * 8 + x * 4 + ix] = block[iy * 4 + ix];
                    }
                }
            }
        }
        HfTransformType::DCT4X8 => {
            transform_buffer[0] = lf[0];
            let coefficients: [f32; 64] = transform_buffer[0..64].try_into().unwrap();
            let block0 = coefficients[0];
            let block1 = coefficients[8];
            let dcs: [f32; 2] = [block0 + block1, block0 - block1];
            for y in 0..2 {
                let mut block = [0.0; 4 * 8];
                for iy in 0..4 {
                    for ix in 0..8 {
                        block[iy * 8 + ix] = if ix == 0 && iy == 0 {
                            dcs[y]
                        } else {
                            coefficients[(y + iy * 2) * 8 + ix]
                        }
                    }
                }
                idct2d_4_8(d, &mut block);
                for iy in 0..4 {
                    for ix in 0..8 {
                        transform_buffer[(y * 4 + iy) * 8 + ix] = block[iy * 8 + ix];
                    }
                }
            }
        }
    };
}

simd_function!(
    transform_to_pixels,
    d: D,
    /// This includes a call to what libjxl calls lowest_frequencies_from_lf.
    pub fn transform_to_pixels_trampoline(
        transform_type: HfTransformType,
        lf: &mut [f32],
        transform_buffer: &mut [f32],
    ) {
        transform_to_pixels_impl(d, transform_type, lf, transform_buffer);
    }
);
