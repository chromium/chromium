// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::simd::prelude::*;
use std::simd::Simd;

use crate::{Reg, Reg32, UReg};

/// Subblock sums and averages, used in eval_quant_err.
#[derive(Default, Copy, Clone)]
pub struct SubblockStats {
    pub sum: [Reg; 3],
    pub avg: [Reg; 3],
}

#[inline]
pub fn fast_div_255_round(x: Reg) -> Reg {
    let r = x + Simd::splat(128);
    (r + ((r + Simd::splat(257)) >> 8)) >> 8
}

/// Compute subblock (2x4 or 4x2) sums and averages.
///
/// Returns: subblock averages in order of top, bottom, left, right.
#[inline]
pub fn prepare_averages(data: &[[[Reg; 3]; 4]; 4]) -> [SubblockStats; 4] {
    let mut sum_2x2 = [[Reg::default(); 3]; 4];
    for y in 0..2 {
        for x in 0..2 {
            for ch in 0..3 {
                sum_2x2[y * 2 + x][ch] = data[y * 2][x * 2][ch]
                    + data[y * 2 + 1][x * 2][ch]
                    + data[y * 2][x * 2 + 1][ch]
                    + data[y * 2 + 1][x * 2 + 1][ch];
            }
        }
    }
    let mut out = [SubblockStats::default(); 4];
    for (i, (s0, s1)) in [(0, 1), (2, 3), (0, 2), (1, 3)].into_iter().enumerate() {
        for ch in 0..3 {
            out[i].sum[ch] = sum_2x2[s0][ch] + sum_2x2[s1][ch];
            out[i].avg[ch] = (out[i].sum[ch] + Simd::splat(4)) >> 3;
        }
    }
    out
}

struct QuantResultWithErr {
    /// Bit 47..32 of ETC1 codeword
    lo: UReg,
    /// Bit 63..48 of ETC1 codeword
    hi: UReg,
    /// Value of endpoint 0, scaled to `0..=255``
    scaled0: [Reg; 3],
    /// Value of endpoint 1, scaled to `0..=255``
    scaled1: [Reg; 3],
    /// Error metric, see [`eval_quant_err`]
    err: Reg32,
}

pub struct QuantResult {
    /// Bit 47..32 of ETC1 codeword
    pub lo: UReg,
    /// Bit 63..48 of ETC1 codeword
    pub hi: UReg,
    /// Value of endpoint 0, scaled to `0..=255``
    pub scaled0: [Reg; 3],
    /// Value of endpoint 1, scaled to `0..=255``
    pub scaled1: [Reg; 3],
}

#[inline]
fn quant_444(
    avg0: [Reg; 3],
    avg1: [Reg; 3],
    sum0: [Reg; 3],
    sum1: [Reg; 3],
    flip: bool,
) -> QuantResultWithErr {
    #[inline]
    fn quant(avg: [Reg; 3]) -> [Reg; 3] {
        avg.map(|x| fast_div_255_round(x * Simd::splat(15)))
    }

    #[inline]
    fn scale(q: [Reg; 3]) -> [Reg; 3] {
        q.map(|x| (x << 4) | x)
    }

    #[inline]
    fn encode(q0: [Reg; 3], q1: [Reg; 3], flip: bool) -> (UReg, UReg) {
        let flip = if flip { UReg::splat(1) } else { UReg::splat(0) };
        let diff = UReg::splat(0);
        let base1_b = q1[2].cast::<u16>() << 8;
        let base0_b = q0[2].cast::<u16>() << 12;
        let lo = flip | diff | base1_b | base0_b;

        let base1_g = q1[1].cast::<u16>();
        let base0_g = q0[1].cast::<u16>() << 4;
        let base1_r = q1[0].cast::<u16>() << 8;
        let base0_r = q0[0].cast::<u16>() << 12;
        let hi = base1_g | base0_g | base1_r | base0_r;

        (lo, hi)
    }

    let q0 = quant(avg0);
    let q1 = quant(avg1);
    let scaled0 = scale(q0);
    let scaled1 = scale(q1);
    let err = eval_quant_err(scaled0, scaled1, sum0, sum1);
    let (lo, hi) = encode(q0, q1, flip);
    QuantResultWithErr { lo, hi, scaled0, scaled1, err }
}

#[inline]
fn quant_555(
    avg0: [Reg; 3],
    avg1: [Reg; 3],
    sum0: [Reg; 3],
    sum1: [Reg; 3],
    flip: bool,
) -> QuantResultWithErr {
    #[inline]
    fn quant(avg: [Reg; 3]) -> [Reg; 3] {
        avg.map(|x| fast_div_255_round(x * Simd::splat(31)))
    }
    #[inline]
    fn scale(q: [Reg; 3]) -> [Reg; 3] {
        // Per ETC1 spec, the "five-bit codewords are extended to eight bits by
        // replicating the top three highest-order bits to the three lowest
        // order bits".
        q.map(|x| (x << 3) | (x >> 2))
    }
    #[inline]
    fn encode(q0: [Reg; 3], delta: [Reg; 3], flip: bool) -> (UReg, UReg) {
        #[inline]
        fn encode_delta(d: Reg) -> UReg {
            d.cast::<u16>() & Simd::splat(0b111)
        }

        let flip = if flip { UReg::splat(1) } else { UReg::splat(0) };
        let diff = UReg::splat(1 << 1);
        let delta_b = encode_delta(delta[2]) << 8;
        let base_b = q0[2].cast::<u16>() << 11;
        let lo = flip | diff | delta_b | base_b;

        let delta_g = encode_delta(delta[1]);
        let base_g = q0[1].cast::<u16>() << 3;
        let delta_r = encode_delta(delta[0]) << 8;
        let base_r = q0[0].cast::<u16>() << 11;
        let hi = delta_g | base_g | delta_r | base_r;

        (lo, hi)
    }

    let q0 = quant(avg0);
    let q1 = quant(avg1);
    let delta = [0, 1, 2].map(|i| (q1[i] - q0[i]).simd_clamp(Simd::splat(-4), Simd::splat(3)));
    let q1 = [0, 1, 2].map(|i| q0[i] + delta[i]);

    let scaled0 = scale(q0);
    let scaled1 = scale(q1);
    let err = eval_quant_err(scaled0, scaled1, sum0, sum1);
    let (lo, hi) = encode(q0, delta, flip);
    QuantResultWithErr { lo, hi, scaled0, scaled1, err }
}

#[inline]
fn eval_quant_err(q0: [Reg; 3], q1: [Reg; 3], sum0: [Reg; 3], sum1: [Reg; 3]) -> Reg32 {
    // Target error metric:
    //   sum((x - q) ** 2)  (for each pixel)
    //   where x is the original pixel value, and
    //         q is the quantized average of the block
    // This can be rewritten as:
    //   sum(x ** 2) - 2 * sum(x * q) + sum(q ** 2)
    // For relative comparisons, sum(x ** 2) is constant and can be omitted.
    // With this and more simplification:
    //   q * sum(q - 2 * x)
    // Assuming that we are computing the sum for 8 pixels within a subblock:
    //   q * (8 * q - 2 * sum(x))
    // Dividing by 2:
    //   q * ((q << 2) - sum(x))
    (0..3).fold(Reg32::splat(0), |mut acc, i| {
        let q0 = q0[i].cast::<i32>();
        let q1 = q1[i].cast::<i32>();
        let sum0 = sum0[i].cast::<i32>();
        let sum1 = sum1[i].cast::<i32>();
        acc += q0 * ((q0 << 2) - sum0);
        acc += q1 * ((q1 << 2) - sum1);
        acc
    })
}

#[inline]
fn quantize_endpoint_pairs(
    avg0: [Reg; 3],
    avg1: [Reg; 3],
    sum0: [Reg; 3],
    sum1: [Reg; 3],
    flip: bool,
) -> QuantResultWithErr {
    let q444 = quant_444(avg0, avg1, sum0, sum1, flip);
    let q555 = quant_555(avg0, avg1, sum0, sum1, flip);

    let prefer555_32 = q555.err.simd_lt(q444.err);
    let prefer555 = prefer555_32.cast::<i16>();
    QuantResultWithErr {
        lo: prefer555.select(q555.lo, q444.lo),
        hi: prefer555.select(q555.hi, q444.hi),
        scaled0: [0, 1, 2].map(|i| prefer555.select(q555.scaled0[i], q444.scaled0[i])),
        scaled1: [0, 1, 2].map(|i| prefer555.select(q555.scaled1[i], q444.scaled1[i])),
        err: prefer555_32.select(q555.err, q444.err),
    }
}

#[inline]
/// Search through flip / no-flip and individual / differential modes, and
/// return the result with the least MSE from original pixels.
pub fn quantize_averages(data: &[[[Reg; 3]; 4]; 4]) -> QuantResult {
    let stats = prepare_averages(&data);

    let flip =
        quantize_endpoint_pairs(stats[0].avg, stats[1].avg, stats[0].sum, stats[1].sum, true);
    let no_flip =
        quantize_endpoint_pairs(stats[2].avg, stats[3].avg, stats[2].sum, stats[3].sum, false);

    let prefer_flip = flip.err.simd_lt(no_flip.err).cast::<i16>();
    QuantResult {
        lo: prefer_flip.select(flip.lo, no_flip.lo),
        hi: prefer_flip.select(flip.hi, no_flip.hi),
        scaled0: [0, 1, 2].map(|i| prefer_flip.select(flip.scaled0[i], no_flip.scaled0[i])),
        scaled1: [0, 1, 2].map(|i| prefer_flip.select(flip.scaled1[i], no_flip.scaled1[i])),
    }
}
