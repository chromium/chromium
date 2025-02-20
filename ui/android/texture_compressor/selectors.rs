// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This file refers to modifiers in ETC1 spec as "selectors". The jargon
//       was inherited from etcpak.

use std::simd::prelude::*;
use std::simd::{Mask, Simd};

use crate::{Reg, Reg32, UReg, SIMD_WIDTH};

// Selector tables from ETC1 spec. The negative part is omitted due to symmetry.
pub const TABLES: [[i16; 2]; 8] =
    [[2, 8], [5, 17], [9, 29], [13, 42], [18, 60], [24, 80], [33, 106], [47, 183]];

/// Conditionally exchange the bottom left 2x2 block with top right 2x2 block,
/// if `flip` for that lane is true.
///
/// i.e. the goal is to flip from:
/// ```text
/// aeim
/// bfjn
/// cgko
/// dhlp
/// ```
/// to:
/// ```text
/// aecg
/// bfdh
/// imko
/// jnlp
/// ```
#[inline]
pub fn flip_pixels(d: &[[[Reg; 3]; 4]; 4], flip: Mask<i16, SIMD_WIDTH>) -> [[[Reg; 3]; 4]; 4] {
    let mut o = [[[Reg::default(); 3]; 4]; 4];
    for y0 in [0, 2] {
        for x0 in [0, 2] {
            for y1 in 0..2 {
                for x1 in 0..2 {
                    for ch in 0..3 {
                        if y0 == x0 {
                            o[y0 + y1][x0 + x1][ch] = d[y0 + y1][x0 + x1][ch];
                        } else {
                            o[y0 + y1][x0 + x1][ch] =
                                flip.select(d[x0 + y1][y0 + x1][ch], d[y0 + y1][x0 + x1][ch]);
                        }
                    }
                }
            }
        }
    }
    o
}

/// Flip the selector codeword if `flip` for that lane is true.
///
/// See [`flip_pixels`] for a description of the flip operation.
#[inline]
pub fn flip_selectors(x: UReg, flip: Mask<i16, SIMD_WIDTH>) -> UReg {
    let keep = x & Simd::splat(0xCC33);
    let bottom_left = x & Simd::splat(0x00CC);
    let top_right = x & Simd::splat(0x3300);

    let flipped = keep | (bottom_left << 6) | (top_right >> 6);
    flip.select(flipped, x)
}

pub struct Fit {
    pub err: Reg32,
    pub table_idx: UReg,
    pub selector_lo: UReg,
    pub selector_hi: UReg,
}

/// Search for the optimal table and selectors for a subblock.
///
/// `data` should be in flipped layout, i.e. 4x2.
///
/// The error function used here is a bit quirky, see code comment for details.
#[inline]
pub fn search_table_and_selectors_subblock(data: &[[[Reg; 3]; 4]], base_color: [Reg; 3]) -> Fit {
    assert_eq!(data.len(), 2);
    // Use fold to compute minimum. Essentially a vector version of min_by_key.
    TABLES
        .iter()
        .enumerate()
        .fold(None, |best_fit, (table_idx, sel_table)| {
            let mut outer_err = Reg32::splat(0);
            let mut selector_lo = UReg::splat(0);
            let mut selector_hi = UReg::splat(0);
            for y in 0..2 {
                for x in 0..4 {
                    // Below, we search for the optimal selector among [-lg, -sm, sm, lg] (sm
                    // and lg is from the selector table).
                    //
                    // We use the error metric:
                    //   abs(gray(q + s - x))
                    //   where q = quantized average, s = selector, x = pixel before compression
                    //         gray(p) = 19*p.r + 38*p.g + 7*p.b  (cf. rec601)
                    //
                    // Note that this is abs(gray(..)) not gray(abs(..)), i.e. the absolute
                    // is taken after computing to grayscale. This allows precomputing
                    // gray(q-x), then exploiting the fact that the selector is same for all
                    // three channels to calculate the final error with a single addition.
                    //
                    // We will first precompute gray(q - x).
                    let mut base_err = Reg::splat(0);
                    let rgb_weight = [19, 38, 7];
                    for ch in 0..3 {
                        base_err += (base_color[ch] - data[y][x][ch]) * Simd::splat(rgb_weight[ch]);
                    }

                    // Now, the sign of selector can be easily decided. To minimize the
                    // absolute value, the selector should be the opposite sign of
                    // gray(q - x).
                    let prefer_neg = base_err.simd_gt(Simd::splat(0));

                    // Finally, we compute the error metric for both sm and lg and decide the
                    // winner.
                    let base_err_abs = base_err.abs();
                    // Subtract in the direction that the final error metric is smaller.
                    // The selector is same for all three channels, so just multiply it by the
                    // total weight.
                    let weight_sum = 64;
                    let err_sm = (base_err_abs - Reg::splat(sel_table[0] * weight_sum)).abs();
                    let err_lg = (base_err_abs - Reg::splat(sel_table[1] * weight_sum)).abs();
                    let prefer_lg = err_lg.simd_lt(err_sm);

                    // The error can be fairly large (a crude upper bound is 255*64). To avoid
                    // overflow after squaring, we use widening multiply and accumulate. This
                    // is somewhat expensive.
                    let best_err = prefer_lg.select(err_lg, err_sm).cast::<i32>();
                    outer_err += best_err * best_err;

                    let pixel_idx = (y + x * 4) as u16;
                    selector_lo |= prefer_lg.select(UReg::splat(1 << pixel_idx), UReg::splat(0));
                    selector_hi |= prefer_neg.select(UReg::splat(1 << pixel_idx), UReg::splat(0));
                }
            }

            let table_idx = UReg::splat(table_idx as u16);
            match best_fit {
                None => Some(Fit { err: outer_err, table_idx, selector_lo, selector_hi }),
                Some(best) => {
                    let lt_32 = outer_err.simd_lt(best.err);
                    let lt = lt_32.cast::<i16>();
                    Some(Fit {
                        err: lt_32.select(outer_err, best.err),
                        table_idx: lt.select(table_idx, best.table_idx),
                        selector_lo: lt.select(selector_lo, best.selector_lo),
                        selector_hi: lt.select(selector_hi, best.selector_hi),
                    })
                }
            }
        })
        .unwrap()
}

/// Search through possible selector tables and selector values for each
/// subblock.
///
/// Returns: Four 16-bit codewords coding the optimal coefficients.
#[inline]
pub fn search_table_and_selectors(
    mut hdr0: UReg,
    hdr1: UReg,
    data: &[[[Reg; 3]; 4]; 4],
    base_color: [[Reg; 3]; 2],
) -> [UReg; 4] {
    // We need to work on pixels in the first subblock, then the second. To allow
    // uniform indices, the flip functions takes care of moving the first
    // subblock to the top half and the second to bottom half. We will fix up
    // the shuffled results in the end.
    let flip = (hdr0 & (UReg::splat(1))).simd_ne(UReg::splat(0));
    let permuted_data = flip_pixels(&data, !flip);

    let mut selector_lo = UReg::splat(0);
    let mut selector_hi = UReg::splat(0);

    for subblock in 0..2 {
        let best_fit = search_table_and_selectors_subblock(
            &permuted_data[subblock * 2..subblock * 2 + 2],
            base_color[subblock],
        );
        let subblock_bit = match subblock {
            0 => 5,
            1 => 2,
            _ => unreachable!(),
        };
        hdr0 |= best_fit.table_idx << subblock_bit;
        selector_lo |= best_fit.selector_lo << (subblock as u16 * 2);
        selector_hi |= best_fit.selector_hi << (subblock as u16 * 2);
    }
    selector_lo = flip_selectors(selector_lo, !flip);
    selector_hi = flip_selectors(selector_hi, !flip);
    [selector_lo, selector_hi, hdr0, hdr1]
}
