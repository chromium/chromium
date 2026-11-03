/*
 * Copyright (c) 2026.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

//! Q13 cosine-basis coefficients matching native scaled-IDCT integer rounding.
//! Evaluated before the first-pass descale of the corresponding native kernels.

pub(super) const BASIS_3: [[i32; 8]; 3] = [
    [8192, 10033, 5793, 0, 0, 0, 0, 0],
    [8192, 0, -11586, 0, 0, 0, 0, 0],
    [8192, -10033, 5793, 0, 0, 0, 0, 0]
];

pub(super) const BASIS_5: [[i32; 8]; 5] = [
    [8192, 11019, 9372, 6810, 3580, 0, 0, 0],
    [8192, 6810, -3580, -11018, -9372, 0, 0, 0],
    [8192, 0, -11584, 0, 11584, 0, 0, 0],
    [8192, -6810, -3580, 11018, -9372, 0, 0, 0],
    [8192, -11019, 9372, -6810, 3580, 0, 0, 0]
];

pub(super) const BASIS_6: [[i32; 8]; 6] = [
    [8192, 11190, 10033, 8192, 5793, 2998, 0, 0],
    [8192, 8192, 0, -8192, -11586, -8192, 0, 0],
    [8192, 2998, -10033, -8192, 5793, 11190, 0, 0],
    [8192, -2998, -10033, 8192, 5793, -11190, 0, 0],
    [8192, -8192, 0, 8192, -11586, 8192, 0, 0],
    [8192, -11190, 10033, -8192, 5793, -2998, 0, 0]
];

pub(super) const BASIS_7: [[i32; 8]; 7] = [
    [8192, 11295, 10438, 9058, 7223, 5027, 2578, 0],
    [8192, 9058, 2578, -5027, -10438, -11295, -7223, 0],
    [8192, 5027, -7223, -11295, -2578, 9058, 10438, 0],
    [8192, 0, -11585, 0, 11585, 0, -11585, 0],
    [8192, -5027, -7223, 11295, -2578, -9058, 10438, 0],
    [8192, -9058, 2578, 5027, -10438, 11295, -7223, 0],
    [8192, -11295, 10438, -9058, 7223, -5027, 2578, 0]
];

pub(super) const BASIS_10: [[i32; 8]; 10] = [
    [8192, 11443, 11019, 10322, 9373, 8192, 6810, 5260],
    [8192, 10323, 6810, 1812, -3580, -8192, -11018, -11442],
    [8192, 8192, 0, -8192, -11586, -8192, 0, 8192],
    [8192, 5260, -6810, -11442, -3580, 8192, 11018, 1812],
    [8192, 1812, -11019, -5260, 9373, 8192, -6810, -10322],
    [8192, -1812, -11019, 5260, 9373, -8192, -6810, 10322],
    [8192, -5260, -6810, 11442, -3580, -8192, 11018, -1812],
    [8192, -8192, 0, 8192, -11586, 8192, 0, -8192],
    [8192, -10323, 6810, -1812, -3580, 8192, -11018, 11442],
    [8192, -11443, 11019, -10322, 9373, -8192, 6810, -5260]
];

pub(super) const BASIS_12: [[i32; 8]; 12] = [
    [8192, 11487, 11190, 10703, 10033, 9192, 8192, 7053],
    [8192, 10703, 8192, 4433, 0, -4433, -8192, -10703],
    [8192, 9192, 2998, -4433, -10033, -11485, -8192, -1512],
    [8192, 7053, -2998, -10703, -10033, -1512, 8192, 11486],
    [8192, 4433, -8192, -10704, 0, 10704, 8192, -4433],
    [8192, 1513, -11190, -4433, 10033, 7053, -8192, -9191],
    [8192, -1513, -11190, 4433, 10033, -7053, -8192, 9191],
    [8192, -4433, -8192, 10704, 0, -10704, 8192, 4433],
    [8192, -7053, -2998, 10703, -10033, 1512, 8192, -11486],
    [8192, -9192, 2998, 4433, -10033, 11485, -8192, 1512],
    [8192, -10703, 8192, -4433, 0, 4433, -8192, 10703],
    [8192, -11487, 11190, -10703, 10033, -9192, 8192, -7053]
];

pub(super) const BASIS_14: [[i32; 8]; 14] = [
    [8192, 11513, 11295, 10935, 10438, 9810, 9058, 8192],
    [8192, 10935, 9058, 6164, 2578, -1297, -5026, -8192],
    [8192, 9810, 5027, -1297, -7223, -10934, -11295, -8192],
    [8192, 8192, 0, -8192, -11586, -8192, 0, 8192],
    [8192, 6164, -5027, -11512, -7223, 3826, 11295, 8192],
    [8192, 3826, -9058, -9809, 2578, 11512, 5026, -8192],
    [8192, 1297, -11295, -3826, 10438, 6164, -9058, -8192],
    [8192, -1297, -11295, 3826, 10438, -6164, -9058, 8192],
    [8192, -3826, -9058, 9809, 2578, -11512, 5026, 8192],
    [8192, -6164, -5027, 11512, -7223, -3826, 11295, -8192],
    [8192, -8192, 0, 8192, -11586, 8192, 0, -8192],
    [8192, -9810, 5027, 1297, -7223, 10934, -11295, 8192],
    [8192, -10935, 9058, -6164, 2578, 1297, -5026, 8192],
    [8192, -11513, 11295, -10935, 10438, -9810, 9058, -8192]
];

#[cfg(test)]
mod tests {
    use alloc::vec;

    use crate::decoder::IDCTPtr;
    use crate::idct::scalar::{
        idct_scaled_10x10, idct_scaled_12x12, idct_scaled_14x14, idct_scaled_3x3, idct_scaled_5x5,
        idct_scaled_6x6, idct_scaled_7x7
    };

    #[test]
    fn native_basis_pixels_coefficients_and_padding() {
        // Native JDCT_ISLOW digests include nonzero AC coefficients in every
        // frequency, including frequencies omitted by the smaller transforms.
        let cases: [(usize, IDCTPtr, u64); 7] = [
            (3, idct_scaled_3x3, 0x1ec5e9af714517d2),
            (5, idct_scaled_5x5, 0x6dd4ce36356afd58),
            (6, idct_scaled_6x6, 0x2b33f734cdf926ce),
            (7, idct_scaled_7x7, 0x95c8123e6da8e64e),
            (10, idct_scaled_10x10, 0x1767d731c035ae8c),
            (12, idct_scaled_12x12, 0x74e15d7e20e75cf6),
            (14, idct_scaled_14x14, 0xccb72e6863a96da9)
        ];
        let mut original = core::array::from_fn(|i| (i as i32 * 37 % 65) - 32);
        original[0] = 311;
        for (side, transform, expected) in cases {
            let stride = side + 5;
            let mut output = vec![-321; stride * side];
            let mut coefficients = original;
            transform(&mut coefficients, &mut output, stride);
            assert_eq!(coefficients, original);
            let actual = output
                .chunks_exact(stride)
                .flat_map(|row| row[..side].iter())
                .fold(0xcbf29ce484222325_u64, |hash, &sample| {
                    (hash ^ u64::try_from(sample).unwrap()).wrapping_mul(0x100000001b3)
                });
            assert_eq!(actual, expected, "native pixels for {side}x{side}");
            for row in output.chunks_exact(stride) {
                assert!(row[side..].iter().all(|&sample| sample == -321));
            }
            for (dc, sample) in [(-4096, 0), (0, 128), (4096, 255)] {
                let mut coefficients = [0; 64];
                coefficients[0] = dc;
                transform(&mut coefficients, &mut output, stride);
                for row in output.chunks_exact(stride) {
                    assert!(row[..side].iter().all(|&value| value == sample));
                    assert!(row[side..].iter().all(|&value| value == -321));
                }
            }
        }
    }
}
