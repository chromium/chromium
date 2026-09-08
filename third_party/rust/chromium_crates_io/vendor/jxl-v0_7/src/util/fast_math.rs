// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::excessive_precision)]

use std::f32::consts::{PI, SQRT_2};

use jxl_simd::{F32SimdVec, I32SimdVec, ScalarDescriptor, SimdDescriptor, SimdMask, shl, shr};

use super::{eval_rational_poly, eval_rational_poly_simd};

const POW2F_NUMER_COEFFS: [f32; 3] = [1.01749063e1, 4.88687798e1, 9.85506591e1];
const POW2F_DENOM_COEFFS: [f32; 4] = [2.10242958e-1, -2.22328856e-2, -1.94414990e1, 9.85506633e1];

#[inline(always)]
pub fn fast_cos(x: f32) -> f32 {
    // Step 1: range reduction to [0, 2pi)
    let pi2 = PI * 2.0;
    let pi2_inv = 0.5 / PI;
    let npi2 = (x * pi2_inv).floor() * pi2;
    let xmodpi2 = x - npi2;
    // Step 2: range reduction to [0, pi]
    let x_pi = xmodpi2.min(pi2 - xmodpi2);
    // Step 3: range reduction to [0, pi/2]
    let above_pihalf = x_pi >= PI / 2.0;
    let x_pihalf = if above_pihalf { PI - x_pi } else { x_pi };
    // Step 4: Taylor-like approximation, scaled by 2**0.75 to make angle
    // duplication steps faster, on x/4.
    let xs = x_pihalf * 0.25;
    let x2 = xs * xs;
    let x4 = x2 * x2;
    let cosx_prescaling = x4 * 0.06960438 + (x2 * -0.84087373 + 1.68179268);
    // Step 5: angle duplication.
    let cosx_scale1 = cosx_prescaling * cosx_prescaling - SQRT_2;
    let cosx_scale2 = cosx_scale1 * cosx_scale1 - 1.0;
    // Step 6: change sign if needed.
    if above_pihalf {
        -cosx_scale2
    } else {
        cosx_scale2
    }
}

#[inline(always)]
pub fn fast_erff(x: f32) -> f32 {
    // Formula from
    // https://en.wikipedia.org/wiki/Error_function#Numerical_approximations
    // but constants have been recomputed.
    let absx = x.abs();
    // Compute 1 - 1 / ((((x * a + b) * x + c) * x + d) * x + 1)**4
    let denom1 = absx * 7.77394369e-02 + 2.05260015e-04;
    let denom2 = denom1 * absx + 2.32120216e-01;
    let denom3 = denom2 * absx + 2.77820801e-01;
    let denom4 = denom3 * absx + 1.0;
    let denom5 = denom4 * denom4;
    let inv_denom5 = 1.0 / denom5;
    let result = -inv_denom5 * inv_denom5 + 1.0;
    result.copysign(x)
}

#[inline(always)]
pub fn fast_erff_simd<D: SimdDescriptor>(d: D, x: D::F32Vec) -> D::F32Vec {
    let absx = x.abs();
    let denom1 = absx.mul_add(
        D::F32Vec::splat(d, 7.77394369e-02),
        D::F32Vec::splat(d, 2.05260015e-04),
    );
    let denom2 = denom1.mul_add(absx, D::F32Vec::splat(d, 2.32120216e-01));
    let denom3 = denom2.mul_add(absx, D::F32Vec::splat(d, 2.77820801e-01));
    let denom4 = denom3.mul_add(absx, D::F32Vec::splat(d, 1.0));
    let denom5 = denom4 * denom4;
    let inv_denom5 = D::F32Vec::splat(d, 1.0) / denom5;
    let result = D::F32Vec::splat(d, 1.0) - inv_denom5 * inv_denom5;
    result.copysign(x)
}

#[inline(always)]
pub fn fast_pow2f(x: f32) -> f32 {
    let x_floor = x.floor();
    let exp = f32::from_bits((((x_floor as i32).wrapping_add(127)) as u32) << 23);
    let frac = x - x_floor;

    let num = frac + POW2F_NUMER_COEFFS[0];
    let num = num * frac + POW2F_NUMER_COEFFS[1];
    let num = num * frac + POW2F_NUMER_COEFFS[2];
    let num = num * exp;

    let den = POW2F_DENOM_COEFFS[0] * frac + POW2F_DENOM_COEFFS[1];
    let den = den * frac + POW2F_DENOM_COEFFS[2];
    let den = den * frac + POW2F_DENOM_COEFFS[3];

    num / den
}

#[inline(always)]
pub fn fast_pow2f_simd<D: SimdDescriptor>(d: D, x: D::F32Vec) -> D::F32Vec {
    let x_floor = x.floor();
    let exp = shl!(x_floor.as_i32() + D::I32Vec::splat(d, 127), 23).bitcast_to_f32();
    let frac = x - x_floor;

    let num = frac + D::F32Vec::splat(d, POW2F_NUMER_COEFFS[0]);
    let num = num.mul_add(frac, D::F32Vec::splat(d, POW2F_NUMER_COEFFS[1]));
    let num = num.mul_add(frac, D::F32Vec::splat(d, POW2F_NUMER_COEFFS[2]));
    let num = num * exp;

    let den = D::F32Vec::splat(d, POW2F_DENOM_COEFFS[0])
        .mul_add(frac, D::F32Vec::splat(d, POW2F_DENOM_COEFFS[1]));
    let den = den.mul_add(frac, D::F32Vec::splat(d, POW2F_DENOM_COEFFS[2]));
    let den = den.mul_add(frac, D::F32Vec::splat(d, POW2F_DENOM_COEFFS[3]));

    num / den
}

const LOG2F_P: [f32; 3] = [
    -1.8503833400518310e-6,
    1.4287160470083755,
    7.4245873327820566e-1,
];
const LOG2F_Q: [f32; 3] = [
    9.9032814277590719e-1,
    1.0096718572241148,
    1.7409343003366853e-1,
];

#[inline(always)]
pub fn fast_log2f(x: f32) -> f32 {
    let x_bits = x.to_bits() as i32;
    let exp_bits = x_bits.wrapping_sub(0x3f2aaaab);
    let exp_shifted = exp_bits >> 23;
    let mantissa = f32::from_bits((x_bits.wrapping_sub(exp_shifted << 23)) as u32);
    let exp_val = exp_shifted as f32;

    let x = mantissa - 1.0;
    eval_rational_poly(x, LOG2F_P, LOG2F_Q) + exp_val
}

#[inline(always)]
pub fn fast_log2f_simd<D: SimdDescriptor>(d: D, x: D::F32Vec) -> D::F32Vec {
    let x_bits = x.bitcast_to_i32();
    let exp_bits = x_bits - D::I32Vec::splat(d, 0x3f2aaaab);
    let exp_shifted = shr!(exp_bits, 23);
    let mantissa = (x_bits - shl!(exp_shifted, 23)).bitcast_to_f32();
    let exp_val = exp_shifted.as_f32();

    let x = mantissa - D::F32Vec::splat(d, 1.0);
    eval_rational_poly_simd(d, x, LOG2F_P, LOG2F_Q) + exp_val
}

// Max relative error: ~3e-5
#[inline(always)]
pub fn fast_powf(base: f32, exp: f32) -> f32 {
    fast_pow2f(fast_log2f(base) * exp)
}

#[inline(always)]
pub fn fast_powf_simd<D: SimdDescriptor>(d: D, base: D::F32Vec, exp: D::F32Vec) -> D::F32Vec {
    fast_pow2f_simd(d, fast_log2f_simd(d, base) * exp)
}

#[inline(always)]
pub fn floor_log2_nonzero(x: u64) -> u32 {
    (u64::BITS as usize - 1) as u32 ^ x.leading_zeros()
}

const JINC_WINDOWED_SQ_P: [f32; 6] = [
    1.0,
    -9.49473905e-01,
    3.03599826e-01,
    -4.37817437e-02,
    2.93920389e-03,
    -7.56152766e-05,
];
const JINC_WINDOWED_SQ_Q: [f32; 5] = [
    1.0,
    2.35513458e-01,
    2.73748942e-02,
    1.99519538e-03,
    9.81907926e-05,
];

/// Rational approximation of f(x) = jinc(pi * sqrt(x) * 0.85) * jinc(j_{1,1} * sqrt(x) / 2.5)
/// for x = r^2 in [0, 6.25]. Returns 0.0 for x >= 6.25.
/// Max absolute error < 6e-8.
#[inline(always)]
pub fn fast_jinc_windowed_sq(r2: f32) -> f32 {
    fast_jinc_windowed_sq_simd(ScalarDescriptor::new().unwrap(), r2)
}

#[inline(always)]
pub fn fast_jinc_windowed_sq_simd<D: SimdDescriptor>(d: D, r2: D::F32Vec) -> D::F32Vec {
    let poly = eval_rational_poly_simd(d, r2, JINC_WINDOWED_SQ_P, JINC_WINDOWED_SQ_Q);
    let mask = D::F32Vec::splat(d, 6.25).gt(r2);
    mask.if_then_else_f32(poly, D::F32Vec::zero(d))
}

#[cfg(test)]
mod test {
    use test_log::test;

    use super::*;
    use crate::tests::assert_close;

    #[test]
    fn test_fast_erff() {
        // Golden data copied from https://en.wikipedia.org/wiki/Error_function#Table_of_values.
        let golden = [
            (0.0, 0.0),
            (0.02, 0.022564575),
            (0.04, 0.045111106),
            (0.06, 0.067621594),
            (0.08, 0.090078126),
            (0.1, 0.112462916),
            (0.2, 0.222702589),
            (0.3, 0.328626759),
            (0.4, 0.428392355),
            (0.5, 0.520499878),
            (0.6, 0.603856091),
            (0.7, 0.677801194),
            (0.8, 0.742100965),
            (0.9, 0.796908212),
            (1.0, 0.842700793),
            (1.1, 0.880205070),
            (1.2, 0.910313978),
            (1.3, 0.934007945),
            (1.4, 0.952285120),
            (1.5, 0.966105146),
            (1.6, 0.976348383),
            (1.7, 0.983790459),
            (1.8, 0.989090502),
            (1.9, 0.992790429),
            (2.0, 0.995322265),
            (2.1, 0.997020533),
            (2.2, 0.998137154),
            (2.3, 0.998856823),
            (2.4, 0.999311486),
            (2.5, 0.999593048),
            (3.0, 0.999977910),
            (3.5, 0.999999257),
        ];
        for (x, erf_x) in golden {
            assert_close!(fast_erff(x), erf_x, 6e-4);
            assert_close!(fast_erff(-x), -erf_x, 6e-4);
        }
    }

    #[test]
    fn test_fast_cos() {
        for i in 0..100 {
            let x = i as f32 / 100.0 * (5.0 * PI) - (2.5 * PI);
            assert_close!(fast_cos(x), x.cos(), 1e-4);
        }
    }

    #[test]
    fn fast_powf_arb() {
        arbtest::arbtest(|u| {
            // (0.0, 128.0]
            let base = u.int_in_range(1..=1 << 24)? as f32 / (1 << 17) as f32;
            // [-4.0, 4.0]
            let exp = u.int_in_range(-(1i32 << 22)..=1 << 22)? as f32 / (1 << 20) as f32;

            let expected = base.powf(exp);
            let actual = fast_powf(base, exp);
            let abs_error = (actual - expected).abs();
            let rel_error = abs_error / expected;
            assert!(
                rel_error < 3e-5,
                "base: {base}, exp: {exp}, rel_error: {rel_error}, expected: {expected}, \
		 actual: {actual}",
            );
            Ok(())
        });
    }

    fn bessel_j1(x: f64) -> f64 {
        if x == 0.0 {
            return 0.0;
        }
        let z = x.abs();
        let z2_4 = -(z * z) * 0.25;
        let mut term = 1.0;
        let mut sum = 1.0;
        for k in 1..=30 {
            term *= z2_4 / (k as f64 * (k + 1) as f64);
            sum += term;
            if term.abs() < 1e-16 {
                break;
            }
        }
        let res = (z * 0.5) * sum;
        if x < 0.0 { -res } else { res }
    }

    fn jinc_windowed_sq(r2: f64) -> f64 {
        if r2 >= 6.25 {
            return 0.0;
        }
        let r = r2.sqrt();
        if r == 0.0 {
            return 1.0;
        }
        const J1_ZERO_1: f64 = 3.8317059702075126;
        const CUTOFF_SCALE: f64 = 0.85;
        let pi_r = std::f64::consts::PI * r * CUTOFF_SCALE;
        let jinc = 2.0 * bessel_j1(pi_r) / pi_r;
        let pi_r_w = J1_ZERO_1 * r / 2.5;
        let window = 2.0 * bessel_j1(pi_r_w) / pi_r_w;
        jinc * window
    }

    #[test]
    fn test_fast_jinc_windowed_sq_accuracy() {
        for i in 0..=10000 {
            let r2 = (i as f64) * 6.25 / 10000.0;
            let expected = jinc_windowed_sq(r2) as f32;
            let actual = fast_jinc_windowed_sq(r2 as f32);
            let diff = (actual - expected).abs();
            assert!(
                diff < 1e-6,
                "Accuracy test failed at r2 = {r2}: expected {expected}, got {actual}, diff {diff}"
            );
        }
    }

    fn fast_jinc_windowed_sq_simd_arb<D: SimdDescriptor>(d: D) {
        let lanes = D::F32Vec::LEN;
        let mut input = vec![0.0f32; lanes];
        let mut output = vec![0.0f32; lanes];
        arbtest::arbtest(|u| {
            for v in &mut input {
                *v = (u.int_in_range(0..=(1i32 << 24))? as f64 / (1 << 24) as f64 * 10.0) as f32;
            }
            let vec = D::F32Vec::load(d, &input);
            fast_jinc_windowed_sq_simd(d, vec).store(&mut output);
            for i in 0..lanes {
                let expected = jinc_windowed_sq(input[i] as f64);
                let actual = output[i] as f64;
                let abs_error = (actual - expected).abs();
                assert!(
                    abs_error < 1e-6,
                    "input: {}, expected: {}, actual: {}, abs_error: {}",
                    input[i],
                    expected,
                    actual,
                    abs_error
                );
            }
            Ok(())
        });
    }

    jxl_simd::test_all_instruction_sets!(fast_jinc_windowed_sq_simd_arb);
}
