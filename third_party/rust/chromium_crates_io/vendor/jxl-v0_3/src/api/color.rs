// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{borrow::Cow, fmt};

use crate::{
    color::tf::{hlg_to_scene, linear_to_pq_precise, pq_to_linear_precise},
    error::{Error, Result},
    headers::color_encoding::{
        ColorEncoding, ColorSpace, Primaries, RenderingIntent, TransferFunction, WhitePoint,
    },
    util::{Matrix3x3, Vector3, inv_3x3_matrix, mul_3x3_matrix, mul_3x3_vector},
};

// Bradford matrices for chromatic adaptation
const K_BRADFORD: Matrix3x3<f64> = [
    [0.8951, 0.2664, -0.1614],
    [-0.7502, 1.7135, 0.0367],
    [0.0389, -0.0685, 1.0296],
];

const K_BRADFORD_INV: Matrix3x3<f64> = [
    [0.9869929, -0.1470543, 0.1599627],
    [0.4323053, 0.5183603, 0.0492912],
    [-0.0085287, 0.0400428, 0.9684867],
];

pub fn compute_md5(data: &[u8]) -> [u8; 16] {
    let mut sum = [0u8; 16];
    let mut data64 = data.to_vec();
    data64.push(128);

    // Add bytes such that ((size + 8) & 63) == 0
    let extra = (64 - ((data64.len() + 8) & 63)) & 63;
    data64.resize(data64.len() + extra, 0);

    // Append length in bits as 64-bit little-endian
    let bit_len = (data.len() as u64) << 3;
    for i in (0..64).step_by(8) {
        data64.push((bit_len >> i) as u8);
    }

    const SINEPARTS: [u32; 64] = [
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613,
        0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193,
        0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d,
        0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122,
        0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
        0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244,
        0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb,
        0xeb86d391,
    ];

    const SHIFT: [u32; 64] = [
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5,
        9, 14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10,
        15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
    ];

    let mut a0: u32 = 0x67452301;
    let mut b0: u32 = 0xefcdab89;
    let mut c0: u32 = 0x98badcfe;
    let mut d0: u32 = 0x10325476;

    for i in (0..data64.len()).step_by(64) {
        let mut a = a0;
        let mut b = b0;
        let mut c = c0;
        let mut d = d0;

        for j in 0..64 {
            let (f, g) = if j < 16 {
                ((b & c) | ((!b) & d), j)
            } else if j < 32 {
                ((d & b) | ((!d) & c), (5 * j + 1) & 0xf)
            } else if j < 48 {
                (b ^ c ^ d, (3 * j + 5) & 0xf)
            } else {
                (c ^ (b | (!d)), (7 * j) & 0xf)
            };

            let dg0 = data64[i + g * 4] as u32;
            let dg1 = data64[i + g * 4 + 1] as u32;
            let dg2 = data64[i + g * 4 + 2] as u32;
            let dg3 = data64[i + g * 4 + 3] as u32;
            let u = dg0 | (dg1 << 8) | (dg2 << 16) | (dg3 << 24);

            let f = f.wrapping_add(a).wrapping_add(SINEPARTS[j]).wrapping_add(u);
            a = d;
            d = c;
            c = b;
            b = b.wrapping_add(f.rotate_left(SHIFT[j]));
        }

        a0 = a0.wrapping_add(a);
        b0 = b0.wrapping_add(b);
        c0 = c0.wrapping_add(c);
        d0 = d0.wrapping_add(d);
    }

    sum[0] = a0 as u8;
    sum[1] = (a0 >> 8) as u8;
    sum[2] = (a0 >> 16) as u8;
    sum[3] = (a0 >> 24) as u8;
    sum[4] = b0 as u8;
    sum[5] = (b0 >> 8) as u8;
    sum[6] = (b0 >> 16) as u8;
    sum[7] = (b0 >> 24) as u8;
    sum[8] = c0 as u8;
    sum[9] = (c0 >> 8) as u8;
    sum[10] = (c0 >> 16) as u8;
    sum[11] = (c0 >> 24) as u8;
    sum[12] = d0 as u8;
    sum[13] = (d0 >> 8) as u8;
    sum[14] = (d0 >> 16) as u8;
    sum[15] = (d0 >> 24) as u8;
    sum
}

#[allow(clippy::too_many_arguments)]
pub(crate) fn primaries_to_xyz(
    rx: f32,
    ry: f32,
    gx: f32,
    gy: f32,
    bx: f32,
    by: f32,
    wx: f32,
    wy: f32,
) -> Result<Matrix3x3<f64>, Error> {
    // Validate white point coordinates
    if !((0.0..=1.0).contains(&wx) && (wy > 0.0 && wy <= 1.0)) {
        return Err(Error::IccInvalidWhitePoint(
            wx,
            wy,
            "White point coordinates out of range ([0,1] for x, (0,1] for y)".to_string(),
        ));
    }
    // Comment from libjxl:
    // TODO(lode): also require rx, ry, gx, gy, bx, to be in range 0-1? ICC
    // profiles in theory forbid negative XYZ values, but in practice the ACES P0
    // color space uses a negative y for the blue primary.

    // Construct the primaries matrix P. Its columns are the XYZ coordinates
    // of the R, G, B primaries (derived from their chromaticities x, y, z=1-x-y).
    // P = [[xr, xg, xb],
    //      [yr, yg, yb],
    //      [zr, zg, zb]]
    let rz = 1.0 - rx as f64 - ry as f64;
    let gz = 1.0 - gx as f64 - gy as f64;
    let bz = 1.0 - bx as f64 - by as f64;
    let p_matrix = [
        [rx as f64, gx as f64, bx as f64],
        [ry as f64, gy as f64, by as f64],
        [rz, gz, bz],
    ];

    let p_inv_matrix = inv_3x3_matrix(&p_matrix)?;

    // Convert reference white point (wx, wy) to XYZ form with Y=1
    // This is WhitePoint_XYZ_wp = [wx/wy, 1, (1-wx-wy)/wy]
    let x_over_y_wp = wx as f64 / wy as f64;
    let z_over_y_wp = (1.0 - wx as f64 - wy as f64) / wy as f64;

    if !x_over_y_wp.is_finite() || !z_over_y_wp.is_finite() {
        return Err(Error::IccInvalidWhitePoint(
            wx,
            wy,
            "Calculated X/Y or Z/Y for white point is not finite.".to_string(),
        ));
    }
    let white_point_xyz_vec: Vector3<f64> = [x_over_y_wp, 1.0, z_over_y_wp];

    // Calculate scaling factors S = [Sr, Sg, Sb] such that P * S = WhitePoint_XYZ_wp
    // So, S = P_inv * WhitePoint_XYZ_wp
    let s_vec = mul_3x3_vector(&p_inv_matrix, &white_point_xyz_vec);

    // Construct diagonal matrix S_diag from s_vec
    let s_diag_matrix = [
        [s_vec[0], 0.0, 0.0],
        [0.0, s_vec[1], 0.0],
        [0.0, 0.0, s_vec[2]],
    ];
    // The final RGB-to-XYZ matrix is P * S_diag
    let result_matrix = mul_3x3_matrix(&p_matrix, &s_diag_matrix);

    Ok(result_matrix)
}

pub(crate) fn adapt_to_xyz_d50(wx: f32, wy: f32) -> Result<Matrix3x3<f64>, Error> {
    if !((0.0..=1.0).contains(&wx) && (wy > 0.0 && wy <= 1.0)) {
        return Err(Error::IccInvalidWhitePoint(
            wx,
            wy,
            "White point coordinates out of range ([0,1] for x, (0,1] for y)".to_string(),
        ));
    }

    // Convert white point (wx, wy) to XYZ with Y=1
    let x_over_y = wx as f64 / wy as f64;
    let z_over_y = (1.0 - wx as f64 - wy as f64) / wy as f64;

    // Check for finiteness, as 1.0 / tiny float can overflow.
    if !x_over_y.is_finite() || !z_over_y.is_finite() {
        return Err(Error::IccInvalidWhitePoint(
            wx,
            wy,
            "Calculated X/Y or Z/Y for white point is not finite.".to_string(),
        ));
    }
    let w: Vector3<f64> = [x_over_y, 1.0, z_over_y];

    // D50 white point in XYZ (Y=1 form)
    // These are X_D50/Y_D50, 1.0, Z_D50/Y_D50
    let w50: Vector3<f64> = [0.96422, 1.0, 0.82521];

    // Transform to LMS color space
    let lms_source = mul_3x3_vector(&K_BRADFORD, &w);
    let lms_d50 = mul_3x3_vector(&K_BRADFORD, &w50);

    // Check for invalid LMS values which would lead to division by zero
    if lms_source.contains(&0.0) {
        return Err(Error::IccInvalidWhitePoint(
            wx,
            wy,
            "LMS components for source white point are zero, leading to division by zero."
                .to_string(),
        ));
    }

    // Create diagonal scaling matrix in LMS space
    let mut a_diag_matrix: Matrix3x3<f64> = [[0.0; 3]; 3];
    for i in 0..3 {
        a_diag_matrix[i][i] = lms_d50[i] / lms_source[i];
        if !a_diag_matrix[i][i].is_finite() {
            return Err(Error::IccInvalidWhitePoint(
                wx,
                wy,
                format!("Diagonal adaptation matrix component {i} is not finite."),
            ));
        }
    }

    // Combine transformations
    let b_matrix = mul_3x3_matrix(&a_diag_matrix, &K_BRADFORD);
    let final_adaptation_matrix = mul_3x3_matrix(&K_BRADFORD_INV, &b_matrix);

    Ok(final_adaptation_matrix)
}

#[allow(clippy::too_many_arguments)]
pub(crate) fn primaries_to_xyz_d50(
    rx: f32,
    ry: f32,
    gx: f32,
    gy: f32,
    bx: f32,
    by: f32,
    wx: f32,
    wy: f32,
) -> Result<Matrix3x3<f64>, Error> {
    // Get the matrix to convert RGB to XYZ, adapted to its native white point (wx, wy).
    let rgb_to_xyz_native_wp_matrix = primaries_to_xyz(rx, ry, gx, gy, bx, by, wx, wy)?;

    // Get the chromatic adaptation matrix from the native white point (wx, wy) to D50.
    let adaptation_to_d50_matrix = adapt_to_xyz_d50(wx, wy)?;
    // This matrix converts XYZ values relative to white point (wx, wy)
    // to XYZ values relative to D50.

    // Combine the matrices: M_RGBtoD50XYZ = M_AdaptToD50 * M_RGBtoNativeXYZ
    // Applying M_RGBtoNativeXYZ first gives XYZ relative to native white point.
    // Then applying M_AdaptToD50 converts these XYZ values to be relative to D50.
    let result_matrix = mul_3x3_matrix(&adaptation_to_d50_matrix, &rgb_to_xyz_native_wp_matrix);

    Ok(result_matrix)
}

#[allow(clippy::too_many_arguments)]
fn create_icc_rgb_matrix(
    rx: f32,
    ry: f32,
    gx: f32,
    gy: f32,
    bx: f32,
    by: f32,
    wx: f32,
    wy: f32,
) -> Result<Matrix3x3<f32>, Error> {
    // TODO: think about if we need/want to change precision to f64 for some calculations here
    let result_f64 = primaries_to_xyz_d50(rx, ry, gx, gy, bx, by, wx, wy)?;
    Ok(std::array::from_fn(|r_idx| {
        std::array::from_fn(|c_idx| result_f64[r_idx][c_idx] as f32)
    }))
}

#[derive(Clone, Debug, PartialEq)]
pub enum JxlWhitePoint {
    D65,
    E,
    DCI,
    Chromaticity { wx: f32, wy: f32 },
}

impl fmt::Display for JxlWhitePoint {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            JxlWhitePoint::D65 => f.write_str("D65"),
            JxlWhitePoint::E => f.write_str("EER"),
            JxlWhitePoint::DCI => f.write_str("DCI"),
            JxlWhitePoint::Chromaticity { wx, wy } => write!(f, "{wx:.7};{wy:.7}"),
        }
    }
}

impl JxlWhitePoint {
    pub fn to_xy_coords(&self) -> (f32, f32) {
        match self {
            JxlWhitePoint::Chromaticity { wx, wy } => (*wx, *wy),
            JxlWhitePoint::D65 => (0.3127, 0.3290),
            JxlWhitePoint::DCI => (0.314, 0.351),
            JxlWhitePoint::E => (1.0 / 3.0, 1.0 / 3.0),
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum JxlPrimaries {
    SRGB,
    BT2100,
    P3,
    Chromaticities {
        rx: f32,
        ry: f32,
        gx: f32,
        gy: f32,
        bx: f32,
        by: f32,
    },
}

impl fmt::Display for JxlPrimaries {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            JxlPrimaries::SRGB => f.write_str("SRG"),
            JxlPrimaries::BT2100 => f.write_str("202"),
            JxlPrimaries::P3 => f.write_str("DCI"),
            JxlPrimaries::Chromaticities {
                rx,
                ry,
                gx,
                gy,
                bx,
                by,
            } => write!(f, "{rx:.7},{ry:.7};{gx:.7},{gy:.7};{bx:.7},{by:.7}"),
        }
    }
}

impl JxlPrimaries {
    pub fn to_xy_coords(&self) -> [(f32, f32); 3] {
        match self {
            JxlPrimaries::Chromaticities {
                rx,
                ry,
                gx,
                gy,
                bx,
                by,
            } => [(*rx, *ry), (*gx, *gy), (*bx, *by)],
            JxlPrimaries::SRGB => [
                // libjxl has these weird numbers for some reason.
                (0.639_998_7, 0.330_010_15),
                //(0.640, 0.330), // R
                (0.300_003_8, 0.600_003_36),
                //(0.300, 0.600), // G
                (0.150_002_05, 0.059_997_204),
                //(0.150, 0.060), // B
            ],
            JxlPrimaries::BT2100 => [
                (0.708, 0.292), // R
                (0.170, 0.797), // G
                (0.131, 0.046), // B
            ],
            JxlPrimaries::P3 => [
                (0.680, 0.320), // R
                (0.265, 0.690), // G
                (0.150, 0.060), // B
            ],
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum JxlTransferFunction {
    BT709,
    Linear,
    SRGB,
    PQ,
    DCI,
    HLG,
    Gamma(f32),
}

impl fmt::Display for JxlTransferFunction {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            JxlTransferFunction::BT709 => f.write_str("709"),
            JxlTransferFunction::Linear => f.write_str("Lin"),
            JxlTransferFunction::SRGB => f.write_str("SRG"),
            JxlTransferFunction::PQ => f.write_str("PeQ"),
            JxlTransferFunction::DCI => f.write_str("DCI"),
            JxlTransferFunction::HLG => f.write_str("HLG"),
            JxlTransferFunction::Gamma(g) => write!(f, "g{g:.7}"),
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum JxlColorEncoding {
    RgbColorSpace {
        white_point: JxlWhitePoint,
        primaries: JxlPrimaries,
        transfer_function: JxlTransferFunction,
        rendering_intent: RenderingIntent,
    },
    GrayscaleColorSpace {
        white_point: JxlWhitePoint,
        transfer_function: JxlTransferFunction,
        rendering_intent: RenderingIntent,
    },
    XYB {
        rendering_intent: RenderingIntent,
    },
}

impl fmt::Display for JxlColorEncoding {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::RgbColorSpace { .. } => f.write_str("RGB"),
            Self::GrayscaleColorSpace { .. } => f.write_str("Gra"),
            Self::XYB { .. } => f.write_str("XYB"),
        }
    }
}

impl JxlColorEncoding {
    pub fn from_internal(internal: &ColorEncoding) -> Result<Self> {
        let rendering_intent = internal.rendering_intent;
        if internal.color_space == ColorSpace::XYB {
            if rendering_intent != RenderingIntent::Perceptual {
                return Err(Error::InvalidRenderingIntent);
            }
            return Ok(Self::XYB { rendering_intent });
        }

        let white_point = match internal.white_point {
            WhitePoint::D65 => JxlWhitePoint::D65,
            WhitePoint::E => JxlWhitePoint::E,
            WhitePoint::DCI => JxlWhitePoint::DCI,
            WhitePoint::Custom => {
                let (wx, wy) = internal.white.as_f32_coords();
                JxlWhitePoint::Chromaticity { wx, wy }
            }
        };
        let transfer_function = if internal.tf.have_gamma {
            JxlTransferFunction::Gamma(internal.tf.gamma())
        } else {
            match internal.tf.transfer_function {
                TransferFunction::BT709 => JxlTransferFunction::BT709,
                TransferFunction::Linear => JxlTransferFunction::Linear,
                TransferFunction::SRGB => JxlTransferFunction::SRGB,
                TransferFunction::PQ => JxlTransferFunction::PQ,
                TransferFunction::DCI => JxlTransferFunction::DCI,
                TransferFunction::HLG => JxlTransferFunction::HLG,
                TransferFunction::Unknown => {
                    return Err(Error::InvalidColorEncoding);
                }
            }
        };

        if internal.color_space == ColorSpace::Gray {
            return Ok(Self::GrayscaleColorSpace {
                white_point,
                transfer_function,
                rendering_intent,
            });
        }

        let primaries = match internal.primaries {
            Primaries::SRGB => JxlPrimaries::SRGB,
            Primaries::BT2100 => JxlPrimaries::BT2100,
            Primaries::P3 => JxlPrimaries::P3,
            Primaries::Custom => {
                let (rx, ry) = internal.custom_primaries[0].as_f32_coords();
                let (gx, gy) = internal.custom_primaries[1].as_f32_coords();
                let (bx, by) = internal.custom_primaries[2].as_f32_coords();
                JxlPrimaries::Chromaticities {
                    rx,
                    ry,
                    gx,
                    gy,
                    bx,
                    by,
                }
            }
        };

        match internal.color_space {
            ColorSpace::Gray | ColorSpace::XYB => unreachable!(),
            ColorSpace::RGB => Ok(Self::RgbColorSpace {
                white_point,
                primaries,
                transfer_function,
                rendering_intent,
            }),
            ColorSpace::Unknown => Err(Error::InvalidColorSpace),
        }
    }

    fn create_icc_cicp_tag_data(&self, tags_data: &mut Vec<u8>) -> Result<Option<TagInfo>, Error> {
        let JxlColorEncoding::RgbColorSpace {
            white_point,
            primaries,
            transfer_function,
            ..
        } = self
        else {
            return Ok(None);
        };

        // Determine the CICP value for primaries.
        let primaries_val: u8 = match (white_point, primaries) {
            (JxlWhitePoint::D65, JxlPrimaries::SRGB) => 1,
            (JxlWhitePoint::D65, JxlPrimaries::BT2100) => 9,
            (JxlWhitePoint::D65, JxlPrimaries::P3) => 12,
            (JxlWhitePoint::DCI, JxlPrimaries::P3) => 11,
            _ => return Ok(None),
        };

        let tf_val = match transfer_function {
            JxlTransferFunction::BT709 => 1,
            JxlTransferFunction::Linear => 8,
            JxlTransferFunction::SRGB => 13,
            JxlTransferFunction::PQ => 16,
            JxlTransferFunction::DCI => 17,
            JxlTransferFunction::HLG => 18,
            // Custom gamma cannot be represented.
            JxlTransferFunction::Gamma(_) => return Ok(None),
        };

        let signature = b"cicp";
        let start_offset = tags_data.len() as u32;
        tags_data.extend_from_slice(signature);
        let data_len = tags_data.len();
        tags_data.resize(tags_data.len() + 4, 0);
        write_u32_be(tags_data, data_len, 0)?;
        tags_data.push(primaries_val);
        tags_data.push(tf_val);
        // Matrix Coefficients (RGB is non-constant luminance)
        tags_data.push(0);
        // Video Full Range Flag
        tags_data.push(1);

        Ok(Some(TagInfo {
            signature: *signature,
            offset_in_tags_blob: start_offset,
            size_unpadded: 12,
        }))
    }

    fn can_tone_map_for_icc(&self) -> bool {
        let JxlColorEncoding::RgbColorSpace {
            white_point,
            primaries,
            transfer_function,
            ..
        } = self
        else {
            return false;
        };
        // This function determines if an ICC profile can be used for tone mapping.
        // The logic is ported from the libjxl `CanToneMap` function.
        // The core idea is that if the color space can be represented by a CICP tag
        // in the ICC profile, then there's more freedom to use other parts of the
        // profile (like the A2B0 LUT) for tone mapping. Otherwise, the profile must
        // unambiguously describe the color space.

        // The conditions for being able to tone map are:
        // 1. The color space must be RGB.
        // 2. The transfer function must be either PQ (Perceptual Quantizer) or HLG (Hybrid Log-Gamma).
        // 3. The combination of primaries and white point must be one that is commonly
        //    describable by a standard CICP value. This includes:
        //    a) P3 primaries with either a D65 or DCI white point.
        //    b) Any non-custom primaries, as long as the white point is D65.

        if let JxlPrimaries::Chromaticities { .. } = primaries {
            return false;
        }

        matches!(
            transfer_function,
            JxlTransferFunction::PQ | JxlTransferFunction::HLG
        ) && (*white_point == JxlWhitePoint::D65
            || (*white_point == JxlWhitePoint::DCI && *primaries == JxlPrimaries::P3))
    }

    pub fn get_color_encoding_description(&self) -> String {
        // Handle special known color spaces first
        if let Some(common_name) = match self {
            JxlColorEncoding::RgbColorSpace {
                white_point: JxlWhitePoint::D65,
                primaries: JxlPrimaries::SRGB,
                transfer_function: JxlTransferFunction::SRGB,
                rendering_intent: RenderingIntent::Perceptual,
            } => Some("sRGB"),
            JxlColorEncoding::RgbColorSpace {
                white_point: JxlWhitePoint::D65,
                primaries: JxlPrimaries::P3,
                transfer_function: JxlTransferFunction::SRGB,
                rendering_intent: RenderingIntent::Perceptual,
            } => Some("DisplayP3"),
            JxlColorEncoding::RgbColorSpace {
                white_point: JxlWhitePoint::D65,
                primaries: JxlPrimaries::BT2100,
                transfer_function: JxlTransferFunction::PQ,
                rendering_intent: RenderingIntent::Relative,
            } => Some("Rec2100PQ"),
            JxlColorEncoding::RgbColorSpace {
                white_point: JxlWhitePoint::D65,
                primaries: JxlPrimaries::BT2100,
                transfer_function: JxlTransferFunction::HLG,
                rendering_intent: RenderingIntent::Relative,
            } => Some("Rec2100HLG"),
            _ => None,
        } {
            return common_name.to_string();
        }

        // Build the string part by part for other case
        let mut d = String::with_capacity(64);

        match self {
            JxlColorEncoding::RgbColorSpace {
                white_point,
                primaries,
                transfer_function,
                rendering_intent,
            } => {
                d.push_str("RGB_");
                d.push_str(&white_point.to_string());
                d.push('_');
                d.push_str(&primaries.to_string());
                d.push('_');
                d.push_str(&rendering_intent.to_string());
                d.push('_');
                d.push_str(&transfer_function.to_string());
            }
            JxlColorEncoding::GrayscaleColorSpace {
                white_point,
                transfer_function,
                rendering_intent,
            } => {
                d.push_str("Gra_");
                d.push_str(&white_point.to_string());
                d.push('_');
                d.push_str(&rendering_intent.to_string());
                d.push('_');
                d.push_str(&transfer_function.to_string());
            }
            JxlColorEncoding::XYB { rendering_intent } => {
                d.push_str("XYB_");
                d.push_str(&rendering_intent.to_string());
            }
        }

        d
    }

    fn create_icc_header(&self) -> Result<Vec<u8>, Error> {
        let mut header_data = vec![0u8; 128];

        // Profile size - To be filled in at the end of profile creation.
        write_u32_be(&mut header_data, 0, 0)?;
        const CMM_TAG: &str = "jxl ";
        // CMM Type
        write_icc_tag(&mut header_data, 4, CMM_TAG)?;

        // Profile version - ICC v4.4 (0x04400000)
        // Conformance tests have v4.3, libjxl produces v4.4
        write_u32_be(&mut header_data, 8, 0x04400000u32)?;

        let profile_class_str = match self {
            JxlColorEncoding::XYB { .. } => "scnr",
            _ => "mntr",
        };
        write_icc_tag(&mut header_data, 12, profile_class_str)?;

        // Data color space
        let data_color_space_str = match self {
            JxlColorEncoding::GrayscaleColorSpace { .. } => "GRAY",
            _ => "RGB ",
        };
        write_icc_tag(&mut header_data, 16, data_color_space_str)?;

        // PCS - Profile Connection Space
        // Corresponds to: if (kEnable3DToneMapping && CanToneMap(c))
        // Assuming kEnable3DToneMapping is true for this port for now.
        const K_ENABLE_3D_ICC_TONEMAPPING: bool = true;
        if K_ENABLE_3D_ICC_TONEMAPPING && self.can_tone_map_for_icc() {
            write_icc_tag(&mut header_data, 20, "Lab ")?;
        } else {
            write_icc_tag(&mut header_data, 20, "XYZ ")?;
        }

        // Date and Time - Placeholder values from libjxl
        write_u16_be(&mut header_data, 24, 2019)?; // Year
        write_u16_be(&mut header_data, 26, 12)?; // Month
        write_u16_be(&mut header_data, 28, 1)?; // Day
        write_u16_be(&mut header_data, 30, 0)?; // Hours
        write_u16_be(&mut header_data, 32, 0)?; // Minutes
        write_u16_be(&mut header_data, 34, 0)?; // Seconds

        write_icc_tag(&mut header_data, 36, "acsp")?;
        write_icc_tag(&mut header_data, 40, "APPL")?;

        // Profile flags
        write_u32_be(&mut header_data, 44, 0)?;
        // Device manufacturer
        write_u32_be(&mut header_data, 48, 0)?;
        // Device model
        write_u32_be(&mut header_data, 52, 0)?;
        // Device attributes
        write_u32_be(&mut header_data, 56, 0)?;
        write_u32_be(&mut header_data, 60, 0)?;

        // Rendering Intent
        let rendering_intent = match self {
            JxlColorEncoding::RgbColorSpace {
                rendering_intent, ..
            }
            | JxlColorEncoding::GrayscaleColorSpace {
                rendering_intent, ..
            }
            | JxlColorEncoding::XYB { rendering_intent } => rendering_intent,
        };
        write_u32_be(&mut header_data, 64, *rendering_intent as u32)?;

        // Whitepoint is fixed to D50 for ICC.
        write_u32_be(&mut header_data, 68, 0x0000F6D6)?;
        write_u32_be(&mut header_data, 72, 0x00010000)?;
        write_u32_be(&mut header_data, 76, 0x0000D32D)?;

        // Profile Creator
        write_icc_tag(&mut header_data, 80, CMM_TAG)?;

        // Profile ID (MD5 checksum) (offset 84) - 16 bytes.
        // This is calculated at the end of profile creation and written here.

        // Reserved (offset 100-127) - already zeroed here.

        Ok(header_data)
    }

    pub fn maybe_create_profile(&self) -> Result<Option<Vec<u8>>, Error> {
        if let JxlColorEncoding::XYB { rendering_intent } = self
            && *rendering_intent != RenderingIntent::Perceptual
        {
            return Err(Error::InvalidRenderingIntent);
        }
        let header = self.create_icc_header()?;
        let mut tags_data: Vec<u8> = Vec::new();
        let mut collected_tags: Vec<TagInfo> = Vec::new();

        // Create 'desc' (ProfileDescription) tag
        let description_string = self.get_color_encoding_description();

        let desc_tag_start_offset = tags_data.len() as u32; // 0 at this point ...
        create_icc_mluc_tag(&mut tags_data, &description_string)?;
        let desc_tag_unpadded_size = (tags_data.len() as u32) - desc_tag_start_offset;
        pad_to_4_byte_boundary(&mut tags_data);
        collected_tags.push(TagInfo {
            signature: *b"desc",
            offset_in_tags_blob: desc_tag_start_offset,
            size_unpadded: desc_tag_unpadded_size,
        });

        // Create 'cprt' (Copyright) tag
        let copyright_string = "CC0";
        let cprt_tag_start_offset = tags_data.len() as u32;
        create_icc_mluc_tag(&mut tags_data, copyright_string)?;
        let cprt_tag_unpadded_size = (tags_data.len() as u32) - cprt_tag_start_offset;
        pad_to_4_byte_boundary(&mut tags_data);
        collected_tags.push(TagInfo {
            signature: *b"cprt",
            offset_in_tags_blob: cprt_tag_start_offset,
            size_unpadded: cprt_tag_unpadded_size,
        });

        match self {
            JxlColorEncoding::GrayscaleColorSpace { white_point, .. } => {
                let (wx, wy) = white_point.to_xy_coords();
                collected_tags.push(create_icc_xyz_tag(
                    &mut tags_data,
                    &cie_xyz_from_white_cie_xy(wx, wy)?,
                )?);
            }
            _ => {
                // Ok, in this case we will add the chad tag below
                const D50: [f32; 3] = [0.964203f32, 1.0, 0.824905];
                collected_tags.push(create_icc_xyz_tag(&mut tags_data, &D50)?);
            }
        }
        pad_to_4_byte_boundary(&mut tags_data);
        if !matches!(self, JxlColorEncoding::GrayscaleColorSpace { .. }) {
            let (wx, wy) = match self {
                JxlColorEncoding::GrayscaleColorSpace { .. } => unreachable!(),
                JxlColorEncoding::RgbColorSpace { white_point, .. } => white_point.to_xy_coords(),
                JxlColorEncoding::XYB { .. } => JxlWhitePoint::D65.to_xy_coords(),
            };
            let chad_matrix_f64 = adapt_to_xyz_d50(wx, wy)?;
            let chad_matrix = std::array::from_fn(|r_idx| {
                std::array::from_fn(|c_idx| chad_matrix_f64[r_idx][c_idx] as f32)
            });
            collected_tags.push(create_icc_chad_tag(&mut tags_data, &chad_matrix)?);
            pad_to_4_byte_boundary(&mut tags_data);
        }

        if let JxlColorEncoding::RgbColorSpace {
            white_point,
            primaries,
            ..
        } = self
        {
            if let Some(tag_info) = self.create_icc_cicp_tag_data(&mut tags_data)? {
                collected_tags.push(tag_info);
                // Padding here not necessary, since we add 12 bytes to already 4-byte aligned
                // buffer
                // pad_to_4_byte_boundary(&mut tags_data);
            }

            // Get colorant and white point coordinates to build the conversion matrix.
            let primaries_coords = primaries.to_xy_coords();
            let (rx, ry) = primaries_coords[0];
            let (gx, gy) = primaries_coords[1];
            let (bx, by) = primaries_coords[2];
            let (wx, wy) = white_point.to_xy_coords();

            // Calculate the RGB to XYZD50 matrix.
            let m = create_icc_rgb_matrix(rx, ry, gx, gy, bx, by, wx, wy)?;

            // Extract the columns, which are the XYZ values for the R, G, and B primaries.
            let r_xyz = [m[0][0], m[1][0], m[2][0]];
            let g_xyz = [m[0][1], m[1][1], m[2][1]];
            let b_xyz = [m[0][2], m[1][2], m[2][2]];

            // Helper to create the raw data for any 'XYZ ' type tag.
            let create_xyz_type_tag_data =
                |tags: &mut Vec<u8>, xyz: &[f32; 3]| -> Result<u32, Error> {
                    let start_offset = tags.len();
                    // The tag *type* is 'XYZ ' for all three
                    tags.extend_from_slice(b"XYZ ");
                    tags.extend_from_slice(&0u32.to_be_bytes());
                    for &val in xyz {
                        append_s15_fixed_16(tags, val)?;
                    }
                    Ok((tags.len() - start_offset) as u32)
                };

            // Create the 'rXYZ' tag.
            let r_xyz_tag_start_offset = tags_data.len() as u32;
            let r_xyz_tag_unpadded_size = create_xyz_type_tag_data(&mut tags_data, &r_xyz)?;
            pad_to_4_byte_boundary(&mut tags_data);
            collected_tags.push(TagInfo {
                signature: *b"rXYZ", // Making the *signature* is unique.
                offset_in_tags_blob: r_xyz_tag_start_offset,
                size_unpadded: r_xyz_tag_unpadded_size,
            });

            // Create the 'gXYZ' tag.
            let g_xyz_tag_start_offset = tags_data.len() as u32;
            let g_xyz_tag_unpadded_size = create_xyz_type_tag_data(&mut tags_data, &g_xyz)?;
            pad_to_4_byte_boundary(&mut tags_data);
            collected_tags.push(TagInfo {
                signature: *b"gXYZ",
                offset_in_tags_blob: g_xyz_tag_start_offset,
                size_unpadded: g_xyz_tag_unpadded_size,
            });

            // Create the 'bXYZ' tag.
            let b_xyz_tag_start_offset = tags_data.len() as u32;
            let b_xyz_tag_unpadded_size = create_xyz_type_tag_data(&mut tags_data, &b_xyz)?;
            pad_to_4_byte_boundary(&mut tags_data);
            collected_tags.push(TagInfo {
                signature: *b"bXYZ",
                offset_in_tags_blob: b_xyz_tag_start_offset,
                size_unpadded: b_xyz_tag_unpadded_size,
            });
        }
        if self.can_tone_map_for_icc() {
            // Create A2B0 tag for HDR tone mapping
            if let JxlColorEncoding::RgbColorSpace {
                white_point,
                primaries,
                transfer_function,
                ..
            } = self
            {
                let a2b0_start = tags_data.len() as u32;
                create_icc_lut_atob_tag_for_hdr(
                    transfer_function,
                    primaries,
                    white_point,
                    &mut tags_data,
                )?;
                pad_to_4_byte_boundary(&mut tags_data);
                let a2b0_size = (tags_data.len() as u32) - a2b0_start;
                collected_tags.push(TagInfo {
                    signature: *b"A2B0",
                    offset_in_tags_blob: a2b0_start,
                    size_unpadded: a2b0_size,
                });

                // Create B2A0 tag (no-op, required by Apple software including Safari/Preview)
                let b2a0_start = tags_data.len() as u32;
                create_icc_noop_btoa_tag(&mut tags_data)?;
                pad_to_4_byte_boundary(&mut tags_data);
                let b2a0_size = (tags_data.len() as u32) - b2a0_start;
                collected_tags.push(TagInfo {
                    signature: *b"B2A0",
                    offset_in_tags_blob: b2a0_start,
                    size_unpadded: b2a0_size,
                });
            }
        } else {
            match self {
                JxlColorEncoding::XYB { .. } => todo!("implement A2B0 and B2A0 tags"),
                JxlColorEncoding::RgbColorSpace {
                    transfer_function, ..
                }
                | JxlColorEncoding::GrayscaleColorSpace {
                    transfer_function, ..
                } => {
                    let trc_tag_start_offset = tags_data.len() as u32;
                    let trc_tag_unpadded_size = match transfer_function {
                        JxlTransferFunction::Gamma(g) => {
                            // Type 0 parametric curve: Y = X^gamma
                            let gamma = 1.0 / g;
                            create_icc_curv_para_tag(&mut tags_data, &[gamma], 0)?
                        }
                        JxlTransferFunction::SRGB => {
                            // Type 3 parametric curve for sRGB standard.
                            const PARAMS: [f32; 5] =
                                [2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, 0.04045];
                            create_icc_curv_para_tag(&mut tags_data, &PARAMS, 3)?
                        }
                        JxlTransferFunction::BT709 => {
                            // Type 3 parametric curve for BT.709 standard.
                            const PARAMS: [f32; 5] =
                                [1.0 / 0.45, 1.0 / 1.099, 0.099 / 1.099, 1.0 / 4.5, 0.081];
                            create_icc_curv_para_tag(&mut tags_data, &PARAMS, 3)?
                        }
                        JxlTransferFunction::Linear => {
                            // Type 3 can also represent a linear response (gamma=1.0).
                            const PARAMS: [f32; 5] = [1.0, 1.0, 0.0, 1.0, 0.0];
                            create_icc_curv_para_tag(&mut tags_data, &PARAMS, 3)?
                        }
                        JxlTransferFunction::DCI => {
                            // Type 3 can also represent a pure power curve (gamma=2.6).
                            const PARAMS: [f32; 5] = [2.6, 1.0, 0.0, 1.0, 0.0];
                            create_icc_curv_para_tag(&mut tags_data, &PARAMS, 3)?
                        }
                        JxlTransferFunction::HLG | JxlTransferFunction::PQ => {
                            let params = create_table_curve(64, transfer_function, false)?;
                            create_icc_curv_para_tag(&mut tags_data, params.as_slice(), 3)?
                        }
                    };
                    pad_to_4_byte_boundary(&mut tags_data);

                    match self {
                        JxlColorEncoding::GrayscaleColorSpace { .. } => {
                            // Grayscale profiles use a single 'kTRC' tag.
                            collected_tags.push(TagInfo {
                                signature: *b"kTRC",
                                offset_in_tags_blob: trc_tag_start_offset,
                                size_unpadded: trc_tag_unpadded_size,
                            });
                        }
                        _ => {
                            // For RGB, rTRC, gTRC, and bTRC all point to the same curve data,
                            // an optimization to keep the profile size small.
                            collected_tags.push(TagInfo {
                                signature: *b"rTRC",
                                offset_in_tags_blob: trc_tag_start_offset,
                                size_unpadded: trc_tag_unpadded_size,
                            });
                            collected_tags.push(TagInfo {
                                signature: *b"gTRC",
                                offset_in_tags_blob: trc_tag_start_offset, // Same offset
                                size_unpadded: trc_tag_unpadded_size,      // Same size
                            });
                            collected_tags.push(TagInfo {
                                signature: *b"bTRC",
                                offset_in_tags_blob: trc_tag_start_offset, // Same offset
                                size_unpadded: trc_tag_unpadded_size,      // Same size
                            });
                        }
                    }
                }
            }
        }

        // Construct the Tag Table bytes
        let mut tag_table_bytes: Vec<u8> = Vec::new();
        // First, the number of tags (u32)
        tag_table_bytes.extend_from_slice(&(collected_tags.len() as u32).to_be_bytes());

        let header_size = header.len() as u32;
        // Each entry in the tag table on disk is 12 bytes: signature (4), offset (4), size (4)
        let tag_table_on_disk_size = 4 + (collected_tags.len() as u32 * 12);

        for tag_info in &collected_tags {
            tag_table_bytes.extend_from_slice(&tag_info.signature);
            // The offset in the tag table is absolute from the start of the ICC profile file
            let final_profile_offset_for_tag =
                header_size + tag_table_on_disk_size + tag_info.offset_in_tags_blob;
            tag_table_bytes.extend_from_slice(&final_profile_offset_for_tag.to_be_bytes());
            // In https://www.color.org/specification/ICC.1-2022-05.pdf, section 7.3.5 reads:
            //
            // "The value of the tag data element size shall be the number of actual data
            // bytes and shall not include any padding at the end of the tag data element."
            //
            // The reference from conformance tests and libjxl use the padded size here instead.

            tag_table_bytes.extend_from_slice(&tag_info.size_unpadded.to_be_bytes());
            // In order to get byte_exact the same output as libjxl, remove the line above
            // and uncomment the lines below
            // let padded_size = tag_info.size_unpadded.next_multiple_of(4);
            // tag_table_bytes.extend_from_slice(&padded_size.to_be_bytes());
        }

        // Assemble the final ICC profile parts: header + tag_table + tags_data
        let mut final_icc_profile_data: Vec<u8> =
            Vec::with_capacity(header.len() + tag_table_bytes.len() + tags_data.len());
        final_icc_profile_data.extend_from_slice(&header);
        final_icc_profile_data.extend_from_slice(&tag_table_bytes);
        final_icc_profile_data.extend_from_slice(&tags_data);

        // Update the profile size in the header (at offset 0)
        let total_profile_size = final_icc_profile_data.len() as u32;
        write_u32_be(&mut final_icc_profile_data, 0, total_profile_size)?;

        // Assemble the final ICC profile parts: header + tag_table + tags_data
        let mut final_icc_profile_data: Vec<u8> =
            Vec::with_capacity(header.len() + tag_table_bytes.len() + tags_data.len());
        final_icc_profile_data.extend_from_slice(&header);
        final_icc_profile_data.extend_from_slice(&tag_table_bytes);
        final_icc_profile_data.extend_from_slice(&tags_data);

        // Update the profile size in the header (at offset 0)
        let total_profile_size = final_icc_profile_data.len() as u32;
        write_u32_be(&mut final_icc_profile_data, 0, total_profile_size)?;

        // The MD5 checksum (Profile ID) must be computed on the profile with
        // specific header fields zeroed out, as per the ICC specification.
        let mut profile_for_checksum = final_icc_profile_data.clone();

        if profile_for_checksum.len() >= 84 {
            // Zero out Profile Flags at offset 44.
            profile_for_checksum[44..48].fill(0);
            // Zero out Rendering Intent at offset 64.
            profile_for_checksum[64..68].fill(0);
            // The Profile ID field at offset 84 is already zero at this stage.
        }

        // Compute the MD5 hash on the modified profile data.
        let checksum = compute_md5(&profile_for_checksum);

        // Write the 16-byte checksum into the "Profile ID" field of the *original*
        // profile data buffer, starting at offset 84.
        if final_icc_profile_data.len() >= 100 {
            final_icc_profile_data[84..100].copy_from_slice(&checksum);
        }

        Ok(Some(final_icc_profile_data))
    }

    pub fn srgb(grayscale: bool) -> Self {
        if grayscale {
            JxlColorEncoding::GrayscaleColorSpace {
                white_point: JxlWhitePoint::D65,
                transfer_function: JxlTransferFunction::SRGB,
                rendering_intent: RenderingIntent::Relative,
            }
        } else {
            JxlColorEncoding::RgbColorSpace {
                white_point: JxlWhitePoint::D65,
                primaries: JxlPrimaries::SRGB,
                transfer_function: JxlTransferFunction::SRGB,
                rendering_intent: RenderingIntent::Relative,
            }
        }
    }

    /// Creates linear sRGB color encoding (sRGB primaries with linear transfer function).
    /// This is the fallback output color space for XYB images when the embedded
    /// color profile cannot be output to without a CMS.
    pub fn linear_srgb(grayscale: bool) -> Self {
        if grayscale {
            JxlColorEncoding::GrayscaleColorSpace {
                white_point: JxlWhitePoint::D65,
                transfer_function: JxlTransferFunction::Linear,
                rendering_intent: RenderingIntent::Relative,
            }
        } else {
            JxlColorEncoding::RgbColorSpace {
                white_point: JxlWhitePoint::D65,
                primaries: JxlPrimaries::SRGB,
                transfer_function: JxlTransferFunction::Linear,
                rendering_intent: RenderingIntent::Relative,
            }
        }
    }

    /// Returns a copy of this encoding with linear transfer function.
    /// For XYB encoding, returns linear sRGB as fallback.
    pub fn with_linear_tf(&self) -> Self {
        match self {
            JxlColorEncoding::RgbColorSpace {
                white_point,
                primaries,
                rendering_intent,
                ..
            } => JxlColorEncoding::RgbColorSpace {
                white_point: white_point.clone(),
                primaries: primaries.clone(),
                transfer_function: JxlTransferFunction::Linear,
                rendering_intent: *rendering_intent,
            },
            JxlColorEncoding::GrayscaleColorSpace {
                white_point,
                rendering_intent,
                ..
            } => JxlColorEncoding::GrayscaleColorSpace {
                white_point: white_point.clone(),
                transfer_function: JxlTransferFunction::Linear,
                rendering_intent: *rendering_intent,
            },
            JxlColorEncoding::XYB { .. } => Self::linear_srgb(false),
        }
    }

    /// Returns the number of color channels for this encoding.
    /// RGB/XYB = 3, Grayscale = 1.
    pub fn channels(&self) -> usize {
        match self {
            JxlColorEncoding::RgbColorSpace { .. } => 3,
            JxlColorEncoding::GrayscaleColorSpace { .. } => 1,
            JxlColorEncoding::XYB { .. } => 3,
        }
    }
}

#[derive(Clone, PartialEq)]
pub enum JxlColorProfile {
    Icc(Vec<u8>),
    Simple(JxlColorEncoding),
}

impl JxlColorProfile {
    /// Returns the ICC profile, panicking if unavailable.
    ///
    /// # Panics
    /// Panics if the color encoding cannot generate an ICC profile.
    /// Consider using `try_as_icc` for fallible conversion.
    pub fn as_icc(&self) -> Cow<'_, Vec<u8>> {
        match self {
            Self::Icc(x) => Cow::Borrowed(x),
            Self::Simple(encoding) => Cow::Owned(encoding.maybe_create_profile().unwrap().unwrap()),
        }
    }

    /// Attempts to get an ICC profile, returning None if unavailable.
    ///
    /// Returns `None` for color encodings that cannot generate ICC profiles.
    pub fn try_as_icc(&self) -> Option<Cow<'_, Vec<u8>>> {
        match self {
            Self::Icc(x) => Some(Cow::Borrowed(x)),
            Self::Simple(encoding) => encoding
                .maybe_create_profile()
                .ok()
                .flatten()
                .map(Cow::Owned),
        }
    }

    /// Returns true if both profiles represent the same color encoding.
    ///
    /// Two profiles are the same if they are both simple color encodings
    /// with matching color space (primaries, white point) and transfer function.
    /// ICC profiles are never considered the same (even if identical bytes).
    pub fn same_color_encoding(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Simple(a), Self::Simple(b)) => {
                use JxlColorEncoding::*;
                match (a, b) {
                    (
                        RgbColorSpace {
                            white_point: wp_a,
                            primaries: prim_a,
                            transfer_function: tf_a,
                            ..
                        },
                        RgbColorSpace {
                            white_point: wp_b,
                            primaries: prim_b,
                            transfer_function: tf_b,
                            ..
                        },
                    ) => wp_a == wp_b && prim_a == prim_b && tf_a == tf_b,
                    (
                        GrayscaleColorSpace {
                            white_point: wp_a,
                            transfer_function: tf_a,
                            ..
                        },
                        GrayscaleColorSpace {
                            white_point: wp_b,
                            transfer_function: tf_b,
                            ..
                        },
                    ) => wp_a == wp_b && tf_a == tf_b,
                    // Different color space types (RGB vs Gray vs XYB)
                    _ => false,
                }
            }
            // ICC profiles require CMS
            _ => false,
        }
    }

    /// Returns the transfer function if this is a simple color profile.
    /// Returns None for ICC profiles or XYB.
    pub fn transfer_function(&self) -> Option<&JxlTransferFunction> {
        match self {
            Self::Simple(JxlColorEncoding::RgbColorSpace {
                transfer_function, ..
            })
            | Self::Simple(JxlColorEncoding::GrayscaleColorSpace {
                transfer_function, ..
            }) => Some(transfer_function),
            _ => None,
        }
    }

    /// Returns true if the decoder can output to this color profile without a CMS.
    ///
    /// This is the equivalent of libjxl's `CanOutputToColorEncoding`. Output is possible
    /// when the profile is a simple encoding (not ICC) with a natively-supported transfer
    /// function. For grayscale, the white point must be D65.
    pub fn can_output_to(&self) -> bool {
        match self {
            Self::Icc(_) => false,
            Self::Simple(JxlColorEncoding::RgbColorSpace { .. }) => true,
            Self::Simple(JxlColorEncoding::GrayscaleColorSpace { white_point, .. }) => {
                // libjxl requires D65 white point for grayscale output without CMS
                *white_point == JxlWhitePoint::D65
            }
            Self::Simple(JxlColorEncoding::XYB { .. }) => {
                // XYB as output doesn't make sense without further conversion
                false
            }
        }
    }

    /// Returns a copy of this profile with linear transfer function.
    /// For ICC profiles, returns None since we can't modify embedded ICC profiles.
    /// This is used to create the CMS input profile for XYB images where XybStage
    /// outputs linear data.
    pub fn with_linear_tf(&self) -> Option<Self> {
        match self {
            Self::Icc(_) => None,
            Self::Simple(encoding) => Some(Self::Simple(encoding.with_linear_tf())),
        }
    }

    /// Returns the number of color channels (1 for grayscale, 3 for RGB, 4 for CMYK).
    ///
    /// For ICC profiles, this parses the profile header to determine the color space.
    /// Falls back to 3 (RGB) if the ICC profile cannot be parsed.
    pub fn channels(&self) -> usize {
        match self {
            Self::Simple(enc) => enc.channels(),
            Self::Icc(icc_data) => {
                // ICC color space signature is at bytes 16-19 in the header
                if icc_data.len() >= 20 {
                    match &icc_data[16..20] {
                        b"GRAY" => 1,
                        b"RGB " => 3,
                        b"CMYK" => 4,
                        _ => 3, // Default to RGB for unknown color spaces
                    }
                } else {
                    3 // Default to RGB if profile is too short
                }
            }
        }
    }

    /// Returns true if this profile is for a CMYK color space.
    ///
    /// For ICC profiles, this parses the profile header to check the color space.
    /// Simple color encodings are never CMYK.
    pub fn is_cmyk(&self) -> bool {
        match self {
            Self::Simple(_) => false, // Simple encodings are never CMYK
            Self::Icc(icc_data) => {
                // ICC color space signature is at bytes 16-19 in the header
                if icc_data.len() >= 20 {
                    &icc_data[16..20] == b"CMYK"
                } else {
                    false
                }
            }
        }
    }
}

impl fmt::Display for JxlColorProfile {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Icc(_) => f.write_str("ICC"),
            Self::Simple(enc) => write!(f, "{}", enc),
        }
    }
}

pub trait JxlCmsTransformer {
    /// Runs a single transform. The buffers each contain `num_pixels` x `num_channels` interleaved
    /// floating point (0..1) samples, where `num_channels` is the number of color channels of
    /// their respective color profiles. For CMYK data, 0 represents the maximum amount of ink
    /// while 1 represents no ink.
    fn do_transform(&mut self, input: &[f32], output: &mut [f32]) -> Result<()>;

    /// Runs a single transform in-place. The buffer contains `num_pixels` x `num_channels`
    /// interleaved floating point (0..1) samples, where `num_channels` is the number of color
    /// channels of the input and output color profiles. For CMYK data, 0 represents the maximum
    /// amount of ink while 1 represents no ink.
    fn do_transform_inplace(&mut self, inout: &mut [f32]) -> Result<()>;
}

pub trait JxlCms {
    /// Initializes `n` transforms (different transforms might be used in parallel) to
    /// convert from color space `input` to colorspace `output`, assuming an intensity of 1.0 for
    /// non-absolute luminance colorspaces of `intensity_target`.
    /// It is an error to not return `n` transforms.
    /// Returns the number of channels the ICC outputs, and the transforms.
    fn initialize_transforms(
        &self,
        n: usize,
        max_pixels_per_transform: usize,
        input: JxlColorProfile,
        output: JxlColorProfile,
        intensity_target: f32,
    ) -> Result<(usize, Vec<Box<dyn JxlCmsTransformer + Send>>)>;
}

/// Writes a u32 value in big-endian format to the slice at the given position.
fn write_u32_be(slice: &mut [u8], pos: usize, value: u32) -> Result<(), Error> {
    if pos.checked_add(4).is_none_or(|end| end > slice.len()) {
        return Err(Error::IccWriteOutOfBounds);
    }
    slice[pos..pos + 4].copy_from_slice(&value.to_be_bytes());
    Ok(())
}

/// Writes a u16 value in big-endian format to the slice at the given position.
fn write_u16_be(slice: &mut [u8], pos: usize, value: u16) -> Result<(), Error> {
    if pos.checked_add(2).is_none_or(|end| end > slice.len()) {
        return Err(Error::IccWriteOutOfBounds);
    }
    slice[pos..pos + 2].copy_from_slice(&value.to_be_bytes());
    Ok(())
}

/// Writes a 4-character ASCII tag string to the slice at the given position.
fn write_icc_tag(slice: &mut [u8], pos: usize, tag_str: &str) -> Result<(), Error> {
    if tag_str.len() != 4 || !tag_str.is_ascii() {
        return Err(Error::IccInvalidTagString(tag_str.to_string()));
    }
    if pos.checked_add(4).is_none_or(|end| end > slice.len()) {
        return Err(Error::IccWriteOutOfBounds);
    }
    slice[pos..pos + 4].copy_from_slice(tag_str.as_bytes());
    Ok(())
}

/// Creates an ICC 'mluc' tag with a single "enUS" record.
///
/// The input `text` must be ASCII, as it will be encoded as UTF-16BE by prepending
/// a null byte to each ASCII character.
fn create_icc_mluc_tag(tags: &mut Vec<u8>, text: &str) -> Result<(), Error> {
    // libjxl comments that "The input text must be ASCII".
    // We enforce this.
    if !text.is_ascii() {
        return Err(Error::IccMlucTextNotAscii(text.to_string()));
    }
    // Tag signature 'mluc' (4 bytes)
    tags.extend_from_slice(b"mluc");
    // Reserved, must be 0 (4 bytes)
    tags.extend_from_slice(&0u32.to_be_bytes());
    // Number of records (u32, 4 bytes) - Hardcoded to 1.
    tags.extend_from_slice(&1u32.to_be_bytes());
    // Record size (u32, 4 bytes) - Each record descriptor is 12 bytes.
    // (Language Code [2] + Country Code [2] + String Length [4] + String Offset [4])
    tags.extend_from_slice(&12u32.to_be_bytes());
    // Language Code (2 bytes) - "en" for English
    tags.extend_from_slice(b"en");
    // Country Code (2 bytes) - "US" for United States
    tags.extend_from_slice(b"US");
    // Length of the string (u32, 4 bytes)
    // For ASCII text encoded as UTF-16BE, each char becomes 2 bytes.
    let string_actual_byte_length = text.len() * 2;
    tags.extend_from_slice(&(string_actual_byte_length as u32).to_be_bytes());
    // Offset of the string (u32, 4 bytes)
    // The string data for this record starts at offset 28.
    tags.extend_from_slice(&28u32.to_be_bytes());
    // The actual string data, encoded as UTF-16BE.
    // For ASCII char 'X', UTF-16BE is 0x00 0x58.
    for ascii_char_code in text.as_bytes() {
        tags.push(0u8);
        tags.push(*ascii_char_code);
    }

    Ok(())
}

struct TagInfo {
    signature: [u8; 4],
    // Offset of this tag's data relative to the START of the `tags_data` block
    offset_in_tags_blob: u32,
    // Unpadded size of this tag's actual data content.
    size_unpadded: u32,
}

fn pad_to_4_byte_boundary(data: &mut Vec<u8>) {
    data.resize(data.len().next_multiple_of(4), 0u8);
}

/// Converts an f32 to s15Fixed16 format and appends it as big-endian bytes.
/// s15Fixed16 is a signed 32-bit number with 1 sign bit, 15 integer bits,
/// and 16 fractional bits.
fn append_s15_fixed_16(tags_data: &mut Vec<u8>, value: f32) -> Result<(), Error> {
    // In libjxl, the following specific range check is used: (-32767.995f <= value) && (value <= 32767.995f)
    // This is slightly tighter than the theoretical max positive s15.16 value.
    // We replicate this for consistency.
    if !(value.is_finite() && (-32767.995..=32767.995).contains(&value)) {
        return Err(Error::IccValueOutOfRangeS15Fixed16(value));
    }

    // Multiply by 2^16 and round to nearest integer
    let scaled_value = (value * 65536.0).round();
    // Cast to i32 for correct two's complement representation
    let int_value = scaled_value as i32;
    tags_data.extend_from_slice(&int_value.to_be_bytes());
    Ok(())
}

/// Creates the data for an ICC 'XYZ ' tag and appends it to `tags_data`.
/// The 'XYZ ' tag contains three s15Fixed16Number values.
fn create_icc_xyz_tag(tags_data: &mut Vec<u8>, xyz_color: &[f32; 3]) -> Result<TagInfo, Error> {
    // Tag signature 'XYZ ' (4 bytes, note the trailing space)
    let start_offset = tags_data.len() as u32;
    let signature = b"XYZ ";
    tags_data.extend_from_slice(signature);

    // Reserved, must be 0 (4 bytes)
    tags_data.extend_from_slice(&0u32.to_be_bytes());

    // XYZ data (3 * s15Fixed16Number = 3 * 4 bytes)
    for &val in xyz_color {
        append_s15_fixed_16(tags_data, val)?;
    }

    Ok(TagInfo {
        signature: *b"wtpt",
        offset_in_tags_blob: start_offset,
        size_unpadded: (tags_data.len() as u32) - start_offset,
    })
}

fn create_icc_chad_tag(
    tags_data: &mut Vec<u8>,
    chad_matrix: &Matrix3x3<f32>,
) -> Result<TagInfo, Error> {
    // The tag type signature "sf32" (4 bytes).
    let signature = b"sf32";
    let start_offset = tags_data.len() as u32;
    tags_data.extend_from_slice(signature);

    // A reserved field (4 bytes), which must be set to 0.
    tags_data.extend_from_slice(&0u32.to_be_bytes());

    // The 9 matrix elements as s15Fixed16Number values.
    // m[0][0], m[0][1], m[0][2], m[1][0], ..., m[2][2]
    for row_array in chad_matrix.iter() {
        for &value in row_array.iter() {
            append_s15_fixed_16(tags_data, value)?;
        }
    }
    Ok(TagInfo {
        signature: *b"chad",
        offset_in_tags_blob: start_offset,
        size_unpadded: (tags_data.len() as u32) - start_offset,
    })
}

/// Converts CIE xy white point coordinates to CIE XYZ values (Y is normalized to 1.0).
fn cie_xyz_from_white_cie_xy(wx: f32, wy: f32) -> Result<[f32; 3], Error> {
    // Check for wy being too close to zero to prevent division by zero or extreme values.
    if wy.abs() < 1e-12 {
        return Err(Error::IccInvalidWhitePointY(wy));
    }
    let factor = 1.0 / wy;
    let x_val = wx * factor;
    let y_val = 1.0f32;
    let z_val = (1.0 - wx - wy) * factor;
    Ok([x_val, y_val, z_val])
}

/// Creates the data for an ICC `para` (parametricCurveType) tag.
/// It writes `12 + 4 * params.len()` bytes.
fn create_icc_curv_para_tag(
    tags_data: &mut Vec<u8>,
    params: &[f32],
    curve_type: u16,
) -> Result<u32, Error> {
    let start_offset = tags_data.len();
    // Tag type 'para' (4 bytes)
    tags_data.extend_from_slice(b"para");
    // Reserved, must be 0 (4 bytes)
    tags_data.extend_from_slice(&0u32.to_be_bytes());
    // Function type (u16, 2 bytes)
    tags_data.extend_from_slice(&curve_type.to_be_bytes());
    // Reserved, must be 0 (u16, 2 bytes)
    tags_data.extend_from_slice(&0u16.to_be_bytes());
    // Parameters (s15Fixed16Number each)
    for &param in params {
        append_s15_fixed_16(tags_data, param)?;
    }
    Ok((tags_data.len() - start_offset) as u32)
}

fn display_from_encoded_pq(display_intensity_target: f32, mut e: f64) -> f64 {
    const M1: f64 = 2610.0 / 16384.0;
    const M2: f64 = (2523.0 / 4096.0) * 128.0;
    const C1: f64 = 3424.0 / 4096.0;
    const C2: f64 = (2413.0 / 4096.0) * 32.0;
    const C3: f64 = (2392.0 / 4096.0) * 32.0;
    // Handle the zero case directly.
    if e == 0.0 {
        return 0.0;
    }

    // Handle negative inputs by using their absolute
    // value for the calculation and reapplying the sign at the end.
    let original_sign = e.signum();
    e = e.abs();

    // Core PQ EOTF formula from ST 2084.
    let xp = e.powf(1.0 / M2);
    let num = (xp - C1).max(0.0);
    let den = C2 - C3 * xp;

    // In release builds, a zero denominator would lead to `inf` or `NaN`,
    // which is handled by the assertion below. For valid inputs (e in [0,1]),
    // the denominator is always positive.
    debug_assert!(den != 0.0, "PQ transfer function denominator is zero.");

    let d = (num / den).powf(1.0 / M1);

    // The result `d` should always be non-negative for non-negative inputs.
    debug_assert!(
        d >= 0.0,
        "PQ intermediate value `d` should not be negative."
    );

    // The libjxl implementation includes a scaling factor. Note that `d` represents
    // a value normalized to a 10,000 nit peak.
    let scaled_d = d * (10000.0 / display_intensity_target as f64);

    // Re-apply the original sign.
    scaled_d.copysign(original_sign)
}

/// TF_HLG_Base class for BT.2100 HLG.
///
/// This struct provides methods to convert between non-linear encoded HLG signals
/// and linear display-referred light, following the definitions in BT.2100-2.
///
/// - **"display"**: linear light, normalized to [0, 1].
/// - **"encoded"**: a non-linear HLG signal, nominally in [0, 1].
/// - **"scene"**: scene-referred linear light, normalized to [0, 1].
///
/// The functions are designed to be unbounded to handle inputs outside the
/// nominal [0, 1] range, which can occur during color space conversions. Negative
/// inputs are handled by mirroring the function (`f(-x) = -f(x)`).
#[allow(non_camel_case_types)]
struct TF_HLG;

impl TF_HLG {
    // Constants for the HLG formula, as defined in BT.2100.
    const A: f64 = 0.17883277;
    const RA: f64 = 1.0 / Self::A;
    const B: f64 = 1.0 - 4.0 * Self::A;
    const C: f64 = 0.5599107295;
    const INV_12: f64 = 1.0 / 12.0;

    /// Converts a non-linear encoded signal to a linear display value (EOTF).
    ///
    /// This corresponds to `DisplayFromEncoded(e) = OOTF(InvOETF(e))`.
    /// Since the OOTF is simplified to an identity function, this is equivalent
    /// to calling `inv_oetf(e)`.
    #[inline]
    fn display_from_encoded(e: f64) -> f64 {
        Self::inv_oetf(e)
    }

    /// Converts a linear display value to a non-linear encoded signal (inverse EOTF).
    ///
    /// This corresponds to `EncodedFromDisplay(d) = OETF(InvOOTF(d))`.
    /// Since the InvOOTF is an identity function, this is equivalent to `oetf(d)`.
    #[inline]
    #[allow(dead_code)]
    fn encoded_from_display(d: f64) -> f64 {
        Self::oetf(d)
    }

    /// The private HLG OETF, converting scene-referred light to a non-linear signal.
    fn oetf(mut s: f64) -> f64 {
        if s == 0.0 {
            return 0.0;
        }
        let original_sign = s.signum();
        s = s.abs();

        let e = if s <= Self::INV_12 {
            (3.0 * s).sqrt()
        } else {
            Self::A * (12.0 * s - Self::B).ln() + Self::C
        };

        // The result should be positive for positive inputs.
        debug_assert!(e > 0.0);

        e.copysign(original_sign)
    }

    /// The private HLG inverse OETF, converting a non-linear signal back to scene-referred light.
    fn inv_oetf(mut e: f64) -> f64 {
        if e == 0.0 {
            return 0.0;
        }
        let original_sign = e.signum();
        e = e.abs();

        let s = if e <= 0.5 {
            // The `* (1.0 / 3.0)` is slightly more efficient than `/ 3.0`.
            e * e * (1.0 / 3.0)
        } else {
            (((e - Self::C) * Self::RA).exp() + Self::B) * Self::INV_12
        };

        // The result should be non-negative for non-negative inputs.
        debug_assert!(s >= 0.0);

        s.copysign(original_sign)
    }
}

/// Creates a lookup table for an ICC `curv` tag from a transfer function.
///
/// This function generates a vector of 16-bit integers representing the response
/// of the HLG or PQ electro-optical transfer functions (EOTF).
///
/// ### Arguments
/// * `n` - The number of entries in the lookup table. Must not exceed 4096.
/// * `tf` - The transfer function to model, either `TransferFunction::HLG` or `TransferFunction::PQ`.
/// * `tone_map` - A boolean to enable tone mapping for PQ curves. Currently a stub.
///
/// ### Returns
/// A `Result` containing the `Vec<f32>` lookup table or an `Error`.
fn create_table_curve(
    n: usize,
    tf: &JxlTransferFunction,
    tone_map: bool,
) -> Result<Vec<f32>, Error> {
    // ICC Specification (v4.4, section 10.6) for `curveType` with `curv`
    // processing elements states the table can have at most 4096 entries.
    if n > 4096 {
        return Err(Error::IccTableSizeExceeded(n));
    }

    if !matches!(tf, JxlTransferFunction::PQ | JxlTransferFunction::HLG) {
        return Err(Error::IccUnsupportedTransferFunction);
    }

    // The peak luminance for PQ decoding, as specified in the original C++ code.
    const PQ_INTENSITY_TARGET: f64 = 10000.0;
    // The target peak luminance for SDR, used if tone mapping is applied.
    const DEFAULT_INTENSITY_TARGET: f64 = 255.0; // Placeholder value

    let mut table = Vec::with_capacity(n);
    for i in 0..n {
        // `x` represents the normalized input signal, from 0.0 to 1.0.
        let x = i as f64 / (n - 1) as f64;

        // Apply the specified EOTF to get the linear light value `y`.
        // The output `y` is normalized to the range [0.0, 1.0].
        let y = match tf {
            JxlTransferFunction::HLG => TF_HLG::display_from_encoded(x),
            JxlTransferFunction::PQ => {
                // For PQ, the output of the EOTF is absolute luminance, so we
                // normalize it back to [0, 1] relative to the peak luminance.
                display_from_encoded_pq(PQ_INTENSITY_TARGET as f32, x) / PQ_INTENSITY_TARGET
            }
            _ => unreachable!(), // Already checked above.
        };

        // Apply tone mapping if requested.
        if tone_map
            && *tf == JxlTransferFunction::PQ
            && PQ_INTENSITY_TARGET > DEFAULT_INTENSITY_TARGET
        {
            // TODO(firsching): add tone mapping here. (make y mutable for this)
            // let linear_luminance = y * PQ_INTENSITY_TARGET;
            // let tone_mapped_luminance = rec2408_tone_map(linear_luminance)?;
            // y = tone_mapped_luminance / DEFAULT_INTENSITY_TARGET;
        }

        // Clamp the final value to the valid range [0.0, 1.0]. This is
        // particularly important for HLG, which can exceed 1.0.
        let y_clamped = y.clamp(0.0, 1.0);

        // table.push((y_clamped * 65535.0).round() as u16);
        table.push(y_clamped as f32);
    }

    Ok(table)
}

// ============================================================================
// HDR Tone Mapping Implementation
// ============================================================================

/// BT.2408 HDR to SDR tone mapper.
/// Maps PQ content from source range (e.g., 0-10000 nits) to target range (e.g., 0-250 nits).
struct Rec2408ToneMapper {
    source_range: (f32, f32), // (min, max) in nits
    target_range: (f32, f32),
    luminances: [f32; 3], // RGB luminance coefficients (Y values)

    // Precomputed values
    pq_mastering_min: f32,
    #[allow(dead_code)] // Stored for potential future use / debugging
    pq_mastering_max: f32,
    pq_mastering_range: f32,
    inv_pq_mastering_range: f32,
    min_lum: f32,
    max_lum: f32,
    ks: f32,
    inv_one_minus_ks: f32,
    normalizer: f32,
    inv_target_peak: f32,
}

impl Rec2408ToneMapper {
    fn new(source_range: (f32, f32), target_range: (f32, f32), luminances: [f32; 3]) -> Self {
        let pq_mastering_min = Self::linear_to_pq(source_range.0);
        let pq_mastering_max = Self::linear_to_pq(source_range.1);
        let pq_mastering_range = pq_mastering_max - pq_mastering_min;
        let inv_pq_mastering_range = 1.0 / pq_mastering_range;

        let min_lum =
            (Self::linear_to_pq(target_range.0) - pq_mastering_min) * inv_pq_mastering_range;
        let max_lum =
            (Self::linear_to_pq(target_range.1) - pq_mastering_min) * inv_pq_mastering_range;
        let ks = 1.5 * max_lum - 0.5;

        Self {
            source_range,
            target_range,
            luminances,
            pq_mastering_min,
            pq_mastering_max,
            pq_mastering_range,
            inv_pq_mastering_range,
            min_lum,
            max_lum,
            ks,
            inv_one_minus_ks: 1.0 / (1.0 - ks).max(1e-6),
            normalizer: source_range.1 / target_range.1,
            inv_target_peak: 1.0 / target_range.1,
        }
    }

    /// PQ inverse EOTF - converts luminance (nits) to PQ encoded value.
    /// Uses the existing `linear_to_pq_precise` from color::tf.
    fn linear_to_pq(luminance: f32) -> f32 {
        let mut val = [luminance / 10000.0]; // Normalize to 0-1 for 10000 nits
        linear_to_pq_precise(10000.0, &mut val);
        val[0]
    }

    /// PQ EOTF - converts PQ encoded value to luminance (nits).
    /// Uses the existing `pq_to_linear_precise` from color::tf.
    fn pq_to_linear(encoded: f32) -> f32 {
        let mut val = [encoded];
        pq_to_linear_precise(10000.0, &mut val);
        val[0] * 10000.0
    }

    fn t(&self, a: f32) -> f32 {
        (a - self.ks) * self.inv_one_minus_ks
    }

    fn p(&self, b: f32) -> f32 {
        let t_b = self.t(b);
        let t_b_2 = t_b * t_b;
        let t_b_3 = t_b_2 * t_b;
        (2.0 * t_b_3 - 3.0 * t_b_2 + 1.0) * self.ks
            + (t_b_3 - 2.0 * t_b_2 + t_b) * (1.0 - self.ks)
            + (-2.0 * t_b_3 + 3.0 * t_b_2) * self.max_lum
    }

    /// Apply tone mapping to RGB values (in-place)
    fn tone_map(&self, rgb: &mut [f32; 3]) {
        let luminance = self.source_range.1
            * (self.luminances[0] * rgb[0]
                + self.luminances[1] * rgb[1]
                + self.luminances[2] * rgb[2]);

        let normalized_pq = ((Self::linear_to_pq(luminance) - self.pq_mastering_min)
            * self.inv_pq_mastering_range)
            .min(1.0);

        let e2 = if normalized_pq < self.ks {
            normalized_pq
        } else {
            self.p(normalized_pq)
        };

        let one_minus_e2 = 1.0 - e2;
        let one_minus_e2_2 = one_minus_e2 * one_minus_e2;
        let one_minus_e2_4 = one_minus_e2_2 * one_minus_e2_2;
        let e3 = self.min_lum * one_minus_e2_4 + e2;
        let e4 = e3 * self.pq_mastering_range + self.pq_mastering_min;
        let d4 = Self::pq_to_linear(e4);
        let new_luminance = d4.clamp(0.0, self.target_range.1);

        let min_luminance = 1e-6;
        let use_cap = luminance <= min_luminance;
        let ratio = new_luminance / luminance.max(min_luminance);
        let cap = new_luminance * self.inv_target_peak;
        let multiplier = ratio * self.normalizer;

        for c in rgb.iter_mut() {
            *c = if use_cap { cap } else { *c * multiplier };
        }
    }
}

/// Apply HLG OOTF for tone mapping HLG content to SDR.
/// This implements the HLG OOTF inline for a single pixel, based on the same math
/// as `color::tf::hlg_scene_to_display` but avoiding the bulk-processing API.
fn apply_hlg_ootf(rgb: &mut [f32; 3], target_luminance: f32, luminances: [f32; 3]) {
    // HLG OOTF: scene-referred to display-referred conversion
    // system_gamma = 1.2 * 1.111^log2(intensity_display / 1000)
    let system_gamma = 1.2_f32 * 1.111_f32.powf((target_luminance / 1e3).log2());
    let exp = system_gamma - 1.0;

    if exp.abs() < 0.1 {
        return;
    }

    // Compute luminance and apply OOTF
    let mixed = rgb[0] * luminances[0] + rgb[1] * luminances[1] + rgb[2] * luminances[2];
    let mult = crate::util::fast_powf(mixed, exp);
    rgb[0] *= mult;
    rgb[1] *= mult;
    rgb[2] *= mult;
}

/// Desaturate out-of-gamut pixels while preserving luminance.
fn gamut_map(rgb: &mut [f32; 3], luminances: &[f32; 3], preserve_saturation: f32) {
    let luminance = luminances[0] * rgb[0] + luminances[1] * rgb[1] + luminances[2] * rgb[2];

    let mut gray_mix_saturation = 0.0_f32;
    let mut gray_mix_luminance = 0.0_f32;

    for &val in rgb.iter() {
        let val_minus_gray = val - luminance;
        let inv_val_minus_gray = if val_minus_gray == 0.0 {
            1.0
        } else {
            1.0 / val_minus_gray
        };
        let val_over_val_minus_gray = val * inv_val_minus_gray;

        if val_minus_gray < 0.0 {
            gray_mix_saturation = gray_mix_saturation.max(val_over_val_minus_gray);
        }

        gray_mix_luminance = gray_mix_luminance.max(if val_minus_gray <= 0.0 {
            gray_mix_saturation
        } else {
            val_over_val_minus_gray - inv_val_minus_gray
        });
    }

    let gray_mix = (preserve_saturation * (gray_mix_saturation - gray_mix_luminance)
        + gray_mix_luminance)
        .clamp(0.0, 1.0);

    for val in rgb.iter_mut() {
        *val = gray_mix * (luminance - *val) + *val;
    }

    let max_clr = rgb[0].max(rgb[1]).max(rgb[2]).max(1.0);
    let normalizer = 1.0 / max_clr;
    for v in rgb.iter_mut() {
        *v *= normalizer;
    }
}

/// Tone map a single pixel and convert to PCS Lab for ICC profile.
fn tone_map_pixel(
    transfer_function: &JxlTransferFunction,
    primaries: &JxlPrimaries,
    white_point: &JxlWhitePoint,
    input: [f32; 3],
) -> Result<[u8; 3], Error> {
    // Get primaries coordinates
    let primaries_coords = primaries.to_xy_coords();
    let (rx, ry) = primaries_coords[0];
    let (gx, gy) = primaries_coords[1];
    let (bx, by) = primaries_coords[2];
    let (wx, wy) = white_point.to_xy_coords();

    // Get the RGB to XYZ matrix (not adapted to D50 yet)
    let primaries_xyz = primaries_to_xyz(rx, ry, gx, gy, bx, by, wx, wy)?;

    // Extract luminances from Y row of the matrix
    let luminances = [
        primaries_xyz[1][0] as f32,
        primaries_xyz[1][1] as f32,
        primaries_xyz[1][2] as f32,
    ];

    // Apply EOTF to get linear values
    let mut linear = match transfer_function {
        JxlTransferFunction::PQ => {
            // PQ EOTF - convert from encoded to linear (normalized to 0-1 range for 10000 nits)
            [
                Rec2408ToneMapper::pq_to_linear(input[0]) / 10000.0,
                Rec2408ToneMapper::pq_to_linear(input[1]) / 10000.0,
                Rec2408ToneMapper::pq_to_linear(input[2]) / 10000.0,
            ]
        }
        JxlTransferFunction::HLG => {
            // Use existing hlg_to_scene from color::tf
            let mut vals = [input[0], input[1], input[2]];
            hlg_to_scene(&mut vals);
            vals
        }
        _ => return Err(Error::IccUnsupportedTransferFunction),
    };

    // Apply tone mapping
    match transfer_function {
        JxlTransferFunction::PQ => {
            let tone_mapper = Rec2408ToneMapper::new(
                (0.0, 10000.0), // PQ source range
                (0.0, 250.0),   // SDR target range
                luminances,
            );
            tone_mapper.tone_map(&mut linear);
        }
        JxlTransferFunction::HLG => {
            // Apply HLG OOTF (80 nit SDR target)
            apply_hlg_ootf(&mut linear, 80.0, luminances);
        }
        _ => {}
    }

    // Gamut map
    gamut_map(&mut linear, &luminances, 0.3);

    // Get chromatic adaptation matrix
    let chad = adapt_to_xyz_d50(wx, wy)?;

    // Combine matrices: to_xyzd50 = chad * primaries_xyz
    // Use mul_3x3_matrix from util which works with f64
    let to_xyzd50_f64 = mul_3x3_matrix(&chad, &primaries_xyz);

    // Convert to f32 for the final calculation
    let to_xyzd50: [[f32; 3]; 3] =
        std::array::from_fn(|r| std::array::from_fn(|c| to_xyzd50_f64[r][c] as f32));

    // Apply matrix to get XYZ D50
    let xyz = [
        linear[0] * to_xyzd50[0][0] + linear[1] * to_xyzd50[0][1] + linear[2] * to_xyzd50[0][2],
        linear[0] * to_xyzd50[1][0] + linear[1] * to_xyzd50[1][1] + linear[2] * to_xyzd50[1][2],
        linear[0] * to_xyzd50[2][0] + linear[1] * to_xyzd50[2][1] + linear[2] * to_xyzd50[2][2],
    ];

    // Convert XYZ to Lab
    // D50 reference white
    const XN: f32 = 0.964212;
    const YN: f32 = 1.0;
    const ZN: f32 = 0.825188;
    const DELTA: f32 = 6.0 / 29.0;

    let lab_f = |x: f32| -> f32 {
        if x <= DELTA * DELTA * DELTA {
            x * (1.0 / (3.0 * DELTA * DELTA)) + 4.0 / 29.0
        } else {
            x.cbrt()
        }
    };

    let f_x = lab_f(xyz[0] / XN);
    let f_y = lab_f(xyz[1] / YN);
    let f_z = lab_f(xyz[2] / ZN);

    // Convert to ICC PCS Lab encoding (8-bit)
    // L* = 116 * f(Y/Yn) - 16, encoded as L* / 100 * 255
    // a* = 500 * (f(X/Xn) - f(Y/Yn)), encoded as (a* + 128) for 8-bit
    // b* = 200 * (f(Y/Yn) - f(Z/Zn)), encoded as (b* + 128) for 8-bit
    Ok([
        (255.0 * (1.16 * f_y - 0.16).clamp(0.0, 1.0)).round() as u8,
        (128.0 + (500.0 * (f_x - f_y)).clamp(-128.0, 127.0)).round() as u8,
        (128.0 + (200.0 * (f_y - f_z)).clamp(-128.0, 127.0)).round() as u8,
    ])
}

/// Create mft1 (8-bit LUT) A2B0 tag for HDR tone mapping.
fn create_icc_lut_atob_tag_for_hdr(
    transfer_function: &JxlTransferFunction,
    primaries: &JxlPrimaries,
    white_point: &JxlWhitePoint,
    tags: &mut Vec<u8>,
) -> Result<(), Error> {
    const LUT_DIM: usize = 9; // 9x9x9 3D LUT

    // Tag signature: 'mft1'
    tags.extend_from_slice(b"mft1");
    // Reserved
    tags.extend_from_slice(&0u32.to_be_bytes());
    // Number of input channels
    tags.push(3);
    // Number of output channels
    tags.push(3);
    // Number of CLUT grid points
    tags.push(LUT_DIM as u8);
    // Padding
    tags.push(0);

    // Identity matrix (3x3, s15Fixed16)
    for i in 0..3 {
        for j in 0..3 {
            let val: f32 = if i == j { 1.0 } else { 0.0 };
            append_s15_fixed_16(tags, val)?;
        }
    }

    // Input tables (identity, 256 entries per channel)
    for _ in 0..3 {
        for i in 0..256 {
            tags.push(i as u8);
        }
    }

    // 3D CLUT
    for ix in 0..LUT_DIM {
        for iy in 0..LUT_DIM {
            for ib in 0..LUT_DIM {
                let input = [
                    ix as f32 / (LUT_DIM - 1) as f32,
                    iy as f32 / (LUT_DIM - 1) as f32,
                    ib as f32 / (LUT_DIM - 1) as f32,
                ];
                let pcslab = tone_map_pixel(transfer_function, primaries, white_point, input)?;
                tags.extend_from_slice(&pcslab);
            }
        }
    }

    // Output tables (identity, 256 entries per channel)
    for _ in 0..3 {
        for i in 0..256 {
            tags.push(i as u8);
        }
    }

    Ok(())
}

/// Create mBA B2A0 tag (no-op, required by some software like Safari).
fn create_icc_noop_btoa_tag(tags: &mut Vec<u8>) -> Result<(), Error> {
    // Tag signature: 'mBA '
    tags.extend_from_slice(b"mBA ");
    // Reserved
    tags.extend_from_slice(&0u32.to_be_bytes());
    // Number of input channels
    tags.push(3);
    // Number of output channels
    tags.push(3);
    // Padding
    tags.extend_from_slice(&0u16.to_be_bytes());
    // Offset to first B curve
    tags.extend_from_slice(&32u32.to_be_bytes());
    // Offset to matrix (0 = none)
    tags.extend_from_slice(&0u32.to_be_bytes());
    // Offset to first M curve (0 = none)
    tags.extend_from_slice(&0u32.to_be_bytes());
    // Offset to CLUT (0 = none)
    tags.extend_from_slice(&0u32.to_be_bytes());
    // Offset to first A curve (0 = none)
    tags.extend_from_slice(&0u32.to_be_bytes());

    // Three identity parametric curves (gamma = 1.0)
    // Each curve is a 'para' type with function type 0 (simple gamma)
    for _ in 0..3 {
        create_icc_curv_para_tag(tags, &[1.0], 0)?;
    }

    Ok(())
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_md5() {
        // Test vectors
        let test_cases = vec![
            ("", "d41d8cd98f00b204e9800998ecf8427e"),
            (
                "The quick brown fox jumps over the lazy dog",
                "9e107d9d372bb6826bd81d3542a419d6",
            ),
            ("abc", "900150983cd24fb0d6963f7d28e17f72"),
            ("message digest", "f96b697d7cb7938d525a2f31aaf161d0"),
            (
                "abcdefghijklmnopqrstuvwxyz",
                "c3fcd3d76192e4007dfb496cca67e13b",
            ),
            (
                "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
                "57edf4a22be3c955ac49da2e2107b67a",
            ),
        ];

        for (input, expected) in test_cases {
            let hash = compute_md5(input.as_bytes());
            let hex: String = hash.iter().map(|e| format!("{:02x}", e)).collect();
            assert_eq!(hex, expected, "Failed for input: '{}'", input);
        }
    }

    #[test]
    fn test_description() {
        assert_eq!(
            JxlColorEncoding::srgb(false).get_color_encoding_description(),
            "RGB_D65_SRG_Rel_SRG"
        );
        assert_eq!(
            JxlColorEncoding::srgb(true).get_color_encoding_description(),
            "Gra_D65_Rel_SRG"
        );
        assert_eq!(
            JxlColorEncoding::RgbColorSpace {
                white_point: JxlWhitePoint::D65,
                primaries: JxlPrimaries::BT2100,
                transfer_function: JxlTransferFunction::Gamma(1.7),
                rendering_intent: RenderingIntent::Relative
            }
            .get_color_encoding_description(),
            "RGB_D65_202_Rel_g1.7000000"
        );
        assert_eq!(
            JxlColorEncoding::RgbColorSpace {
                white_point: JxlWhitePoint::D65,
                primaries: JxlPrimaries::P3,
                transfer_function: JxlTransferFunction::SRGB,
                rendering_intent: RenderingIntent::Perceptual
            }
            .get_color_encoding_description(),
            "DisplayP3"
        );
    }

    #[test]
    fn test_rec2408_tone_mapper() {
        // Test the Rec2408ToneMapper with BT.2100 luminances
        let luminances = [0.2627, 0.6780, 0.0593]; // BT.2100/BT.2020
        let tone_mapper = Rec2408ToneMapper::new((0.0, 10000.0), (0.0, 250.0), luminances);

        // Test with a bright HDR pixel (should be compressed)
        let mut rgb = [0.8, 0.8, 0.8]; // High values in PQ space = very bright
        tone_mapper.tone_map(&mut rgb);
        // Result should be within valid range
        assert!(rgb[0] >= 0.0 && rgb[0] <= 1.0, "R out of range: {}", rgb[0]);
        assert!(rgb[1] >= 0.0 && rgb[1] <= 1.0, "G out of range: {}", rgb[1]);
        assert!(rgb[2] >= 0.0 && rgb[2] <= 1.0, "B out of range: {}", rgb[2]);

        // Test with a dark pixel (should not be affected much)
        let mut rgb_dark = [0.1, 0.1, 0.1];
        tone_mapper.tone_map(&mut rgb_dark);
        assert!(
            rgb_dark[0] >= 0.0 && rgb_dark[0] <= 1.0,
            "R out of range: {}",
            rgb_dark[0]
        );
    }

    #[test]
    fn test_hlg_ootf() {
        let luminances = [0.2627, 0.6780, 0.0593];

        let mut rgb = [0.5, 0.5, 0.5];
        apply_hlg_ootf(&mut rgb, 80.0, luminances);
        // Result should be in valid range
        assert!(rgb[0] >= 0.0, "R should be non-negative");
        assert!(rgb[1] >= 0.0, "G should be non-negative");
        assert!(rgb[2] >= 0.0, "B should be non-negative");
    }

    #[test]
    fn test_gamut_map() {
        let luminances = [0.2627, 0.6780, 0.0593];

        // Test out-of-gamut pixel (negative value)
        let mut rgb = [-0.1, 0.5, 0.5];
        gamut_map(&mut rgb, &luminances, 0.3);
        // All values should be non-negative after gamut mapping
        assert!(rgb[0] >= 0.0, "R should be non-negative after gamut map");
        assert!(rgb[1] >= 0.0, "G should be non-negative after gamut map");
        assert!(rgb[2] >= 0.0, "B should be non-negative after gamut map");

        // Test in-gamut pixel (should not change much)
        let mut rgb_valid = [0.5, 0.3, 0.2];
        gamut_map(&mut rgb_valid, &luminances, 0.3);
        assert!(rgb_valid[0] >= 0.0 && rgb_valid[0] <= 1.0);
        assert!(rgb_valid[1] >= 0.0 && rgb_valid[1] <= 1.0);
        assert!(rgb_valid[2] >= 0.0 && rgb_valid[2] <= 1.0);
    }

    #[test]
    fn test_tone_map_pixel_pq() {
        let result = tone_map_pixel(
            &JxlTransferFunction::PQ,
            &JxlPrimaries::BT2100,
            &JxlWhitePoint::D65,
            [0.5, 0.5, 0.5],
        );
        assert!(result.is_ok());
        let lab = result.unwrap();
        // Lab L* should be in reasonable range for mid-gray after tone mapping
        assert!(lab[0] > 0, "L* should be positive for non-black input");
        // a* and b* should be near neutral (128) for achromatic input
        assert!(
            (lab[1] as i32 - 128).abs() < 10,
            "a* should be near neutral"
        );
        assert!(
            (lab[2] as i32 - 128).abs() < 10,
            "b* should be near neutral"
        );
    }

    #[test]
    fn test_tone_map_pixel_hlg() {
        let result = tone_map_pixel(
            &JxlTransferFunction::HLG,
            &JxlPrimaries::BT2100,
            &JxlWhitePoint::D65,
            [0.5, 0.5, 0.5],
        );
        assert!(result.is_ok());
        let lab = result.unwrap();
        // Lab L* should be in reasonable range
        assert!(lab[0] > 0, "L* should be positive for non-black input");
        // a* and b* should be near neutral (128) for achromatic input
        assert!(
            (lab[1] as i32 - 128).abs() < 10,
            "a* should be near neutral"
        );
        assert!(
            (lab[2] as i32 - 128).abs() < 10,
            "b* should be near neutral"
        );
    }

    #[test]
    fn test_hdr_icc_profile_generation_pq() {
        // Test that PQ HDR color encoding generates an ICC profile with A2B0/B2A0 tags.
        // This tests the complete HDR tone mapping pipeline without needing an actual
        // HDR JXL file - the color encoding is constructed programmatically.
        let encoding = JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::BT2100,
            transfer_function: JxlTransferFunction::PQ,
            rendering_intent: RenderingIntent::Relative,
        };

        assert!(encoding.can_tone_map_for_icc());

        let result = encoding.maybe_create_profile();
        assert!(result.is_ok(), "Profile creation should succeed");
        let profile_opt = result.unwrap();
        assert!(profile_opt.is_some(), "Profile should be generated for PQ");

        let profile = profile_opt.unwrap();
        // Profile should be a valid ICC profile (starts with profile size, then signature)
        assert!(profile.len() > 128, "Profile should have header + tags");

        // Verify header has Lab PCS (bytes 20-23 should be "Lab ")
        assert_eq!(
            &profile[20..24],
            b"Lab ",
            "PCS should be Lab for HDR profiles"
        );

        // Check for 'mft1' (A2B0 tag type) somewhere in the profile
        assert!(
            profile.windows(4).any(|w| w == b"mft1"),
            "Profile should contain mft1 tag (A2B0)"
        );
        assert!(
            profile.windows(4).any(|w| w == b"mBA "),
            "Profile should contain mBA tag (B2A0)"
        );

        // Verify CICP tag is present (for standard primaries)
        assert!(
            profile.windows(4).any(|w| w == b"cicp"),
            "Profile should contain cicp tag"
        );
    }

    #[test]
    fn test_hdr_icc_profile_generation_hlg() {
        // Test that HLG HDR color encoding generates an ICC profile with A2B0/B2A0 tags
        let encoding = JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::BT2100,
            transfer_function: JxlTransferFunction::HLG,
            rendering_intent: RenderingIntent::Relative,
        };

        assert!(encoding.can_tone_map_for_icc());

        let result = encoding.maybe_create_profile();
        assert!(result.is_ok(), "Profile creation should succeed");
        let profile_opt = result.unwrap();
        assert!(profile_opt.is_some(), "Profile should be generated for HLG");

        let profile = profile_opt.unwrap();
        assert!(profile.len() > 128, "Profile should have header + tags");
    }

    #[test]
    fn test_pq_eotf_inv_eotf_roundtrip() {
        // Test that linear_to_pq and pq_to_linear are inverses
        let test_values: [f32; 5] = [0.0, 100.0, 1000.0, 5000.0, 10000.0];
        for &luminance in &test_values {
            let encoded = Rec2408ToneMapper::linear_to_pq(luminance);
            let decoded = Rec2408ToneMapper::pq_to_linear(encoded);
            let diff = (luminance - decoded).abs();
            assert!(
                diff < 1.0,
                "Roundtrip failed for {}: got {}, diff {}",
                luminance,
                decoded,
                diff
            );
        }
    }

    #[test]
    fn test_can_tone_map_for_icc() {
        // PQ with D65 and standard primaries should be able to tone map
        let pq_bt2100 = JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::BT2100,
            transfer_function: JxlTransferFunction::PQ,
            rendering_intent: RenderingIntent::Relative,
        };
        assert!(pq_bt2100.can_tone_map_for_icc());

        // HLG with D65 and standard primaries should be able to tone map
        let hlg_bt2100 = JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::BT2100,
            transfer_function: JxlTransferFunction::HLG,
            rendering_intent: RenderingIntent::Relative,
        };
        assert!(hlg_bt2100.can_tone_map_for_icc());

        // sRGB should NOT be able to tone map (not HDR)
        let srgb = JxlColorEncoding::srgb(false);
        assert!(!srgb.can_tone_map_for_icc());

        // Custom primaries should NOT be able to tone map
        let custom_pq = JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::Chromaticities {
                rx: 0.7,
                ry: 0.3,
                gx: 0.2,
                gy: 0.8,
                bx: 0.15,
                by: 0.05,
            },
            transfer_function: JxlTransferFunction::PQ,
            rendering_intent: RenderingIntent::Relative,
        };
        assert!(!custom_pq.can_tone_map_for_icc());
    }

    /// Integration test: decode actual HDR PQ test file and verify ICC profile
    #[test]
    fn test_hdr_pq_file_icc_profile() {
        use crate::api::{JxlDecoder, JxlDecoderOptions, ProcessingResult};

        let data = std::fs::read("resources/test/hdr_pq_test.jxl")
            .expect("Failed to read hdr_pq_test.jxl - run from jxl crate directory");

        let options = JxlDecoderOptions::default();
        let decoder = JxlDecoder::new(options);
        let mut input: &[u8] = &data;

        let decoder_info = match decoder.process(&mut input).unwrap() {
            ProcessingResult::Complete { result } => result,
            _ => panic!("Expected complete decoding"),
        };

        // Get the color profile
        let color_profile = decoder_info.output_color_profile();

        // For HDR PQ content, we should be able to generate an ICC profile
        let icc = color_profile.try_as_icc();
        assert!(
            icc.is_some(),
            "Should generate ICC profile for HDR PQ content"
        );

        let profile = icc.unwrap();
        // Verify it's an HDR profile with Lab PCS
        assert_eq!(&profile[20..24], b"Lab ", "PCS should be Lab for HDR");
        // Verify A2B0 tag exists
        assert!(
            profile.windows(4).any(|w| w == b"A2B0"),
            "Should have A2B0 tag"
        );
        // Verify B2A0 tag exists
        assert!(
            profile.windows(4).any(|w| w == b"B2A0"),
            "Should have B2A0 tag"
        );
    }

    /// Integration test: decode actual HDR HLG test file and verify ICC profile
    #[test]
    fn test_hdr_hlg_file_icc_profile() {
        use crate::api::{JxlDecoder, JxlDecoderOptions, ProcessingResult};

        let data = std::fs::read("resources/test/hdr_hlg_test.jxl")
            .expect("Failed to read hdr_hlg_test.jxl - run from jxl crate directory");

        let options = JxlDecoderOptions::default();
        let decoder = JxlDecoder::new(options);
        let mut input: &[u8] = &data;

        let decoder_info = match decoder.process(&mut input).unwrap() {
            ProcessingResult::Complete { result } => result,
            _ => panic!("Expected complete decoding"),
        };

        // Get the color profile
        let color_profile = decoder_info.output_color_profile();

        // For HDR HLG content, we should be able to generate an ICC profile
        let icc = color_profile.try_as_icc();
        assert!(
            icc.is_some(),
            "Should generate ICC profile for HDR HLG content"
        );

        let profile = icc.unwrap();
        // Verify it's an HDR profile with Lab PCS
        assert_eq!(&profile[20..24], b"Lab ", "PCS should be Lab for HDR");
        // Verify A2B0 tag exists
        assert!(
            profile.windows(4).any(|w| w == b"A2B0"),
            "Should have A2B0 tag"
        );
    }

    #[test]
    fn test_same_color_encoding_identical() {
        // Identical encodings → same
        let srgb1 = JxlColorProfile::Simple(JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::SRGB,
            transfer_function: JxlTransferFunction::SRGB,
            rendering_intent: RenderingIntent::Relative,
        });
        let srgb2 = JxlColorProfile::Simple(JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::SRGB,
            transfer_function: JxlTransferFunction::SRGB,
            rendering_intent: RenderingIntent::Relative,
        });
        assert!(srgb1.same_color_encoding(&srgb2));
    }

    #[test]
    fn test_same_color_encoding_different_transfer() {
        // Same primaries and white point, different transfer function → NOT same
        let srgb_gamma = JxlColorProfile::Simple(JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::SRGB,
            transfer_function: JxlTransferFunction::SRGB,
            rendering_intent: RenderingIntent::Relative,
        });
        let srgb_linear = JxlColorProfile::Simple(JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::SRGB,
            transfer_function: JxlTransferFunction::Linear,
            rendering_intent: RenderingIntent::Relative,
        });
        assert!(!srgb_gamma.same_color_encoding(&srgb_linear));
        assert!(!srgb_linear.same_color_encoding(&srgb_gamma));
    }

    #[test]
    fn test_same_color_encoding_different_primaries() {
        // Different primaries → NOT same
        let srgb = JxlColorProfile::Simple(JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::SRGB,
            transfer_function: JxlTransferFunction::SRGB,
            rendering_intent: RenderingIntent::Relative,
        });
        let p3 = JxlColorProfile::Simple(JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::P3,
            transfer_function: JxlTransferFunction::SRGB,
            rendering_intent: RenderingIntent::Relative,
        });
        assert!(!srgb.same_color_encoding(&p3));
        assert!(!p3.same_color_encoding(&srgb));
    }

    #[test]
    fn test_same_color_encoding_different_white_point() {
        // Different white point → NOT same
        let d65 = JxlColorProfile::Simple(JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::SRGB,
            transfer_function: JxlTransferFunction::SRGB,
            rendering_intent: RenderingIntent::Relative,
        });
        let dci = JxlColorProfile::Simple(JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::DCI,
            primaries: JxlPrimaries::SRGB,
            transfer_function: JxlTransferFunction::SRGB,
            rendering_intent: RenderingIntent::Relative,
        });
        assert!(!d65.same_color_encoding(&dci));
    }

    #[test]
    fn test_same_color_encoding_grayscale() {
        // Grayscale with same white point, different transfer → NOT same
        let gray_srgb = JxlColorProfile::Simple(JxlColorEncoding::GrayscaleColorSpace {
            white_point: JxlWhitePoint::D65,
            transfer_function: JxlTransferFunction::SRGB,
            rendering_intent: RenderingIntent::Relative,
        });
        let gray_linear = JxlColorProfile::Simple(JxlColorEncoding::GrayscaleColorSpace {
            white_point: JxlWhitePoint::D65,
            transfer_function: JxlTransferFunction::Linear,
            rendering_intent: RenderingIntent::Relative,
        });
        assert!(!gray_srgb.same_color_encoding(&gray_linear));
        // But identical grayscale encodings are same
        let gray_srgb2 = JxlColorProfile::Simple(JxlColorEncoding::GrayscaleColorSpace {
            white_point: JxlWhitePoint::D65,
            transfer_function: JxlTransferFunction::SRGB,
            rendering_intent: RenderingIntent::Relative,
        });
        assert!(gray_srgb.same_color_encoding(&gray_srgb2));
    }

    #[test]
    fn test_same_color_encoding_icc_profile() {
        // ICC profiles are never considered same (even with themselves)
        let srgb = JxlColorProfile::Simple(JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::SRGB,
            transfer_function: JxlTransferFunction::SRGB,
            rendering_intent: RenderingIntent::Relative,
        });
        let icc = JxlColorProfile::Icc(vec![0u8; 100]); // Dummy ICC profile
        assert!(!srgb.same_color_encoding(&icc));
        assert!(!icc.same_color_encoding(&srgb));
        assert!(!icc.same_color_encoding(&icc));
    }

    #[test]
    fn test_same_color_encoding_rgb_vs_grayscale() {
        // RGB vs Grayscale → NOT same
        let rgb = JxlColorProfile::Simple(JxlColorEncoding::RgbColorSpace {
            white_point: JxlWhitePoint::D65,
            primaries: JxlPrimaries::SRGB,
            transfer_function: JxlTransferFunction::SRGB,
            rendering_intent: RenderingIntent::Relative,
        });
        let gray = JxlColorProfile::Simple(JxlColorEncoding::GrayscaleColorSpace {
            white_point: JxlWhitePoint::D65,
            transfer_function: JxlTransferFunction::SRGB,
            rendering_intent: RenderingIntent::Relative,
        });
        assert!(!rgb.same_color_encoding(&gray));
        assert!(!gray.same_color_encoding(&rgb));
    }
}
