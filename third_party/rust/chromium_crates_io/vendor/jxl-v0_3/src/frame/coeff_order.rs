// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    BLOCK_DIM, BLOCK_SIZE,
    bit_reader::BitReader,
    entropy_coding::decode::SymbolReader,
    error::Result,
    frame::Histograms,
    headers::permutation::Permutation,
    util::{CeilLog2, tracing_wrappers::*},
};

use jxl_transforms::transform_map::*;

use std::borrow::Cow;
use std::mem;
use std::sync::OnceLock;

pub const NUM_ORDERS: usize = 13;

pub const TRANSFORM_TYPE_LUT: [HfTransformType; NUM_ORDERS] = [
    HfTransformType::DCT,
    HfTransformType::IDENTITY, // a.k.a. "Hornuss"
    HfTransformType::DCT16X16,
    HfTransformType::DCT32X32,
    HfTransformType::DCT8X16,
    HfTransformType::DCT8X32,
    HfTransformType::DCT16X32,
    HfTransformType::DCT64X64,
    HfTransformType::DCT32X64,
    HfTransformType::DCT128X128,
    HfTransformType::DCT64X128,
    HfTransformType::DCT256X256,
    HfTransformType::DCT128X256,
];

pub const NUM_PERMUTATION_CONTEXTS: usize = 8;

/// Cached natural coefficient orders per transform type.
/// Each entry is computed lazily on first access, avoiding computation
/// of orders for large transforms that are never used.
static NATURAL_COEFF_ORDERS: [OnceLock<Vec<u32>>; NUM_ORDERS] = [
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

/// Get cached natural coefficient order for a transform type index.
/// Computes the order lazily on first access.
fn get_natural_coeff_order(idx: usize) -> &'static Vec<u32> {
    NATURAL_COEFF_ORDERS[idx].get_or_init(|| natural_coeff_order(TRANSFORM_TYPE_LUT[idx]))
}

pub fn natural_coeff_order(transform: HfTransformType) -> Vec<u32> {
    let cx = covered_blocks_x(transform) as usize;
    let cy = covered_blocks_y(transform) as usize;
    let xsize: usize = cx * BLOCK_DIM;
    assert!(cx >= cy);
    // We compute the zigzag order for a cx x cx block, then discard all the
    // lines that are not multiple of the ratio between cx and cy.
    let xs = cx / cy;
    let xsm = xs - 1;
    let xss = xs.ceil_log2();
    let mut out: Vec<u32> = vec![0; cx * cy * BLOCK_SIZE];
    // First half of the block
    let mut cur = cx * cy;
    for i in 0..xsize {
        for j in 0..(i + 1) {
            let mut x = j;
            let mut y = i - j;
            if i % 2 != 0 {
                mem::swap(&mut x, &mut y);
            }
            if (y & xsm) != 0 {
                continue;
            }
            y >>= xss;
            let val;
            if x < cx && y < cy {
                val = y * cx + x;
            } else {
                val = cur;
                cur += 1;
            }
            out[val] = (y * xsize + x) as u32;
        }
    }
    // Second half
    for ir in 1..xsize {
        let ip = xsize - ir;
        let i = ip - 1;
        for j in 0..(i + 1) {
            let mut x = xsize - 1 - (i - j);
            let mut y = xsize - 1 - j;
            if !i.is_multiple_of(2) {
                mem::swap(&mut x, &mut y);
            }
            if (y & xsm) != 0 {
                continue;
            }
            y >>= xss;
            let val = cur;
            cur += 1;
            out[val] = (y * xsize + x) as u32;
        }
    }
    out
}

pub fn decode_coeff_orders(used_orders: u32, br: &mut BitReader) -> Result<Vec<Permutation>> {
    // Use cached natural coefficient orders instead of recomputing
    let all_component_orders = 3 * NUM_ORDERS;
    let mut permutations: Vec<Permutation> = (0..all_component_orders)
        .map(|o| Permutation(Cow::Borrowed(get_natural_coeff_order(o / 3))))
        .collect();
    if used_orders == 0 {
        return Ok(permutations);
    }
    let histograms = Histograms::decode(NUM_PERMUTATION_CONTEXTS, br, true)?;
    let mut reader = SymbolReader::new(&histograms, br, None)?;
    for (ord, transform_type) in TRANSFORM_TYPE_LUT.iter().enumerate() {
        if used_orders & (1 << ord) == 0 {
            continue;
        }
        debug!(?transform_type);
        let num_blocks = covered_blocks_x(*transform_type) * covered_blocks_y(*transform_type);
        for c in 0..3 {
            let size = num_blocks * BLOCK_SIZE as u32;
            let permutation = Permutation::decode(size, num_blocks, &histograms, br, &mut reader)?;
            let index = 3 * ord + c;
            permutations[index].compose(&permutation);
        }
    }
    reader.check_final_state(&histograms, br)?;
    Ok(permutations)
}

#[cfg(test)]
mod tests {
    use super::*;

    // Golden data generated from libjxl's `ComputeNaturalCoeffOrder` for DCT (8x8).
    const COEFF_ORDER_1X1: [u32; 64] = [
        0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27,
        20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
    ];

    // Golden data generated from libjxl's `ComputeNaturalCoeffOrder` for DCT8X16 (16x8).
    const COEFF_ORDER_2X1: [u32; 128] = [
        0, 1, 16, 2, 3, 17, 32, 18, 4, 5, 19, 33, 48, 34, 20, 6, 7, 21, 35, 49, 64, 50, 36, 22, 8,
        9, 23, 37, 51, 65, 80, 66, 52, 38, 24, 10, 11, 25, 39, 53, 67, 81, 96, 82, 68, 54, 40, 26,
        12, 13, 27, 41, 55, 69, 83, 97, 112, 98, 84, 70, 56, 42, 28, 14, 15, 29, 43, 57, 71, 85,
        99, 113, 114, 100, 86, 72, 58, 44, 30, 31, 45, 59, 73, 87, 101, 115, 116, 102, 88, 74, 60,
        46, 47, 61, 75, 89, 103, 117, 118, 104, 90, 76, 62, 63, 77, 91, 105, 119, 120, 106, 92, 78,
        79, 93, 107, 121, 122, 108, 94, 95, 109, 123, 124, 110, 111, 125, 126, 127,
    ];

    #[test]
    fn test_natural_coeff_order() {
        let order_1x1 = natural_coeff_order(HfTransformType::DCT);
        assert_eq!(order_1x1, COEFF_ORDER_1X1);

        let order_2x1 = natural_coeff_order(HfTransformType::DCT8X16);
        assert_eq!(order_2x1, COEFF_ORDER_2X1);
    }
}
