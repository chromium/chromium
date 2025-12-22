// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::util::{eval_rational_poly, eval_rational_poly_simd};
use jxl_simd::{F32SimdVec, SimdDescriptor, SimdMask};

/// Converts the linear samples with the sRGB transfer curve (SIMD version).
// Max error ~5e-7
#[inline(always)]
pub fn linear_to_srgb_simd<D: SimdDescriptor>(d: D, samples: &mut [f32]) {
    #[allow(clippy::excessive_precision)]
    const P: [f32; 5] = [
        -5.135152395e-4,
        5.287254571e-3,
        3.903842876e-1,
        1.474205315,
        7.352629620e-1,
    ];

    #[allow(clippy::excessive_precision)]
    const Q: [f32; 5] = [
        1.004519624e-2,
        3.036675394e-1,
        1.340816930,
        9.258482155e-1,
        2.424867759e-2,
    ];

    for vec in samples.chunks_exact_mut(D::F32Vec::LEN) {
        let x = D::F32Vec::load(d, vec);
        let a = x.abs();
        D::F32Vec::splat(d, 0.0031308)
            .gt(a)
            .if_then_else_f32(
                a * D::F32Vec::splat(d, 12.92),
                eval_rational_poly_simd(d, a.sqrt(), P, Q),
            )
            .copysign(x)
            .store(vec);
    }
}

/// Converts samples in sRGB transfer curve to linear. Inverse of `linear_to_srgb`.
pub fn srgb_to_linear(samples: &mut [f32]) {
    #[allow(clippy::excessive_precision)]
    const P: [f32; 5] = [
        2.200248328e-4,
        1.043637593e-2,
        1.624820318e-1,
        7.961564959e-1,
        8.210152774e-1,
    ];

    #[allow(clippy::excessive_precision)]
    const Q: [f32; 5] = [
        2.631846970e-1,
        1.076976492,
        4.987528350e-1,
        -5.512498495e-2,
        6.521209011e-3,
    ];

    for x in samples {
        let a = x.abs();
        *x = if a <= 0.04045 {
            a / 12.92
        } else {
            eval_rational_poly(a, P, Q)
        }
        .copysign(*x);
    }
}

#[inline(always)]
pub fn srgb_to_linear_simd<D: SimdDescriptor>(d: D, samples: &mut [f32]) {
    #[allow(clippy::excessive_precision)]
    const P: [f32; 5] = [
        2.200248328e-4,
        1.043637593e-2,
        1.624820318e-1,
        7.961564959e-1,
        8.210152774e-1,
    ];

    #[allow(clippy::excessive_precision)]
    const Q: [f32; 5] = [
        2.631846970e-1,
        1.076976492,
        4.987528350e-1,
        -5.512498495e-2,
        6.521209011e-3,
    ];

    for vec in samples.chunks_exact_mut(D::F32Vec::LEN) {
        let x = D::F32Vec::load(d, vec);
        let a = x.abs();
        D::F32Vec::splat(d, 0.04045)
            .gt(a)
            .if_then_else_f32(
                a / D::F32Vec::splat(d, 12.92),
                eval_rational_poly_simd(d, a, P, Q),
            )
            .copysign(x)
            .store(vec);
    }
}

/// Converts the linear samples with the BT.709 transfer curve (SIMD version).
// Rational polynomial approximation of 1.099 * x^0.45 - 0.099 on sqrt(x).
// Max error ~3e-7
#[inline(always)]
pub fn linear_to_bt709_simd<D: SimdDescriptor>(d: D, samples: &mut [f32]) {
    // Coefficients for rational polynomial P(y)/Q(y) where y = sqrt(x)
    // Approximates 1.099 * y^0.9 - 0.099 on [sqrt(0.018), 1]
    #[allow(clippy::excessive_precision)]
    const P: [f32; 5] = [
        -9.625309705734253e-2,
        -2.2635456919670105e-1,
        1.935774803161621e1,
        5.897886276245117e1,
        2.3947298049926758e1,
    ];

    #[allow(clippy::excessive_precision)]
    const Q: [f32; 5] = [
        1.0,
        1.877663230895996e1,
        5.5292449951171875e1,
        2.6565317153930664e1,
        3.269049823284149e-1,
    ];

    for vec in samples.chunks_exact_mut(D::F32Vec::LEN) {
        let x = D::F32Vec::load(d, vec);
        let a = x.abs();
        D::F32Vec::splat(d, 0.018)
            .gt(a)
            .if_then_else_f32(
                a * D::F32Vec::splat(d, 4.5),
                eval_rational_poly_simd(d, a.sqrt(), P, Q),
            )
            .copysign(x)
            .store(vec);
    }
}

/// Converts samples in BT.709 transfer curve to linear. Inverse of `linear_to_bt709_simd`.
pub fn bt709_to_linear(samples: &mut [f32]) {
    for s in samples {
        let a = s.abs();
        *s = if a <= 0.081 {
            a / 4.5
        } else {
            crate::util::fast_powf(a.mul_add(1.0 / 1.099, 0.099 / 1.099), 1.0 / 0.45)
        }
        .copysign(*s);
    }
}

/// Converts samples in BT.709 transfer curve to linear (SIMD version).
#[inline(always)]
pub fn bt709_to_linear_simd<D: SimdDescriptor>(d: D, xsize: usize, samples: &mut [f32]) {
    let threshold = D::F32Vec::splat(d, 0.081);
    let inv_4_5 = D::F32Vec::splat(d, 1.0 / 4.5);
    let scale = D::F32Vec::splat(d, 1.0 / 1.099);
    let offset = D::F32Vec::splat(d, 0.099 / 1.099);
    let exp = D::F32Vec::splat(d, 1.0 / 0.45);

    for vec in samples
        .chunks_exact_mut(D::F32Vec::LEN)
        .take(xsize.div_ceil(D::F32Vec::LEN))
    {
        let x = D::F32Vec::load(d, vec);
        let a = x.abs();
        let linear_part = a * inv_4_5;
        let gamma_part = crate::util::fast_powf_simd(d, a.mul_add(scale, offset), exp);
        threshold
            .gt(a)
            .if_then_else_f32(linear_part, gamma_part)
            .copysign(x)
            .store(vec);
    }
}

const PQ_M1: f64 = 2610.0 / 16384.0;
const PQ_M2: f64 = (2523.0 / 4096.0) * 128.0;
const PQ_C1: f64 = 3424.0 / 4096.0;
const PQ_C2: f64 = (2413.0 / 4096.0) * 32.0;
const PQ_C3: f64 = (2392.0 / 4096.0) * 32.0;

/// Converts linear sample to PQ signal using PQ inverse EOTF, where linear sample value of 1.0
/// represents `intensity_target` display nits.
///
/// This version uses original EOTF using double precision arithmetic internally.
pub fn linear_to_pq_precise(intensity_target: f32, samples: &mut [f32]) {
    let mult = intensity_target as f64 * 10000f64.recip();

    for s in samples {
        if *s == 0.0 {
            continue;
        }

        let a = s.abs() as f64;
        let xp = (a * mult).powf(PQ_M1);
        let num = PQ_C1 + xp * PQ_C2;
        let den = 1.0 + xp * PQ_C3;
        let e = (num / den).powf(PQ_M2);
        *s = (e as f32).copysign(*s);
    }
}

/// Converts PQ signal to linear sample using PQ EOTF, where linear sample value of 1.0 represents
/// `intensity_target` display nits.
///
/// This version uses original EOTF using double precision arithmetic internally.
pub fn pq_to_linear_precise(intensity_target: f32, samples: &mut [f32]) {
    let mult = 10000.0 / intensity_target as f64;

    for s in samples {
        if *s == 0.0 {
            continue;
        }

        let a = s.abs() as f64;
        let xp = a.powf(PQ_M2.recip());
        let num = (xp - PQ_C1).max(0.0);
        let den = PQ_C2 - PQ_C3 * xp;
        let y = (num / den).powf(PQ_M1.recip());
        *s = ((y * mult) as f32).copysign(*s);
    }
}

const PQ_EOTF_P: [f32; 5] = [
    2.6297566e-4,
    -6.235531e-3,
    7.386023e-1,
    2.6455317,
    5.500349e-1,
];
const PQ_EOTF_Q: [f32; 5] = [
    4.213501e2,
    -4.2873682e2,
    1.7436467e2,
    -3.3907887e1,
    2.6771877,
];

const PQ_INV_EOTF_P: [f32; 5] = [1.351392e-2, -1.095778, 5.522776e1, 1.492516e2, 4.838434e1];
const PQ_INV_EOTF_Q: [f32; 5] = [1.012416, 2.016708e1, 9.26371e1, 1.120607e2, 2.590418e1];
const PQ_INV_EOTF_P_SMALL: [f32; 5] = [
    9.863406e-6,
    3.881234e-1,
    1.352821e2,
    6.889862e4,
    -2.864824e5,
];
const PQ_INV_EOTF_Q_SMALL: [f32; 5] =
    [3.371868e1, 1.477719e3, 1.608477e4, -4.389884e4, -2.072546e5];

/// Converts linear sample to PQ signal using PQ inverse EOTF, where linear sample value of 1.0
/// represents `intensity_target` display nits.
///
/// This version uses approximate curve using rational polynomial.
// Max error: ~7e-7 at intensity_target = 10000
pub fn linear_to_pq(intensity_target: f32, samples: &mut [f32]) {
    let y_mult = intensity_target * 10000f32.recip();

    for s in samples {
        let a = s.abs();
        let a_scaled = a * y_mult;
        let a_1_4 = a_scaled.sqrt().sqrt();

        let y = if a < 1e-4 {
            eval_rational_poly(a_1_4, PQ_INV_EOTF_P_SMALL, PQ_INV_EOTF_Q_SMALL)
        } else {
            eval_rational_poly(a_1_4, PQ_INV_EOTF_P, PQ_INV_EOTF_Q)
        };

        *s = y.copysign(*s);
    }
}

/// Converts linear sample to PQ signal using PQ inverse EOTF (SIMD version).
#[inline(always)]
pub fn linear_to_pq_simd<D: SimdDescriptor>(
    d: D,
    intensity_target: f32,
    xsize: usize,
    samples: &mut [f32],
) {
    let y_mult = D::F32Vec::splat(d, intensity_target * 10000f32.recip());
    let threshold = D::F32Vec::splat(d, 1e-4);

    for vec in samples
        .chunks_exact_mut(D::F32Vec::LEN)
        .take(xsize.div_ceil(D::F32Vec::LEN))
    {
        let s = D::F32Vec::load(d, vec);
        let a = s.abs();
        let a_scaled = a * y_mult;
        let a_1_4 = a_scaled.sqrt().sqrt();

        // Use small polynomial for a < 1e-4, regular polynomial otherwise
        let y_small = eval_rational_poly_simd(d, a_1_4, PQ_INV_EOTF_P_SMALL, PQ_INV_EOTF_Q_SMALL);
        let y_large = eval_rational_poly_simd(d, a_1_4, PQ_INV_EOTF_P, PQ_INV_EOTF_Q);
        let y = threshold.gt(a).if_then_else_f32(y_small, y_large);

        y.copysign(s).store(vec);
    }
}

/// Converts PQ signal to linear sample using PQ EOTF, where linear sample value of 1.0 represents
/// `intensity_target` display nits.
///
/// This version uses approximate curve using rational polynomial.
// Max error: ~3e-6 at intensity_target = 10000
pub fn pq_to_linear(intensity_target: f32, samples: &mut [f32]) {
    let y_mult = 10000.0 / intensity_target;

    for s in samples {
        let a = s.abs();
        // a + a * a
        let x = a.mul_add(a, a);
        let y = eval_rational_poly(x, PQ_EOTF_P, PQ_EOTF_Q);
        *s = (y * y_mult).copysign(*s);
    }
}

/// Converts PQ signal to linear sample using PQ EOTF (SIMD version).
#[inline(always)]
pub fn pq_to_linear_simd<D: SimdDescriptor>(
    d: D,
    intensity_target: f32,
    xsize: usize,
    samples: &mut [f32],
) {
    let y_mult = D::F32Vec::splat(d, 10000.0 / intensity_target);

    for vec in samples
        .chunks_exact_mut(D::F32Vec::LEN)
        .take(xsize.div_ceil(D::F32Vec::LEN))
    {
        let s = D::F32Vec::load(d, vec);
        let a = s.abs();
        // a + a * a
        let x = a.mul_add(a, a);
        let y = eval_rational_poly_simd(d, x, PQ_EOTF_P, PQ_EOTF_Q);
        (y * y_mult).copysign(s).store(vec);
    }
}

const HLG_A: f64 = 0.17883277;
const HLG_B: f64 = 1.0 - 4.0 * HLG_A;
const HLG_C: f64 = 0.5599107295;

fn hlg_ootf_inner_precise(exp: f64, [lr, lg, lb]: [f32; 3], [sr, sg, sb]: [&mut [f32]; 3]) {
    if exp.abs() < 0.1 {
        return;
    }

    let lr = lr as f64;
    let lg = lg as f64;
    let lb = lb as f64;
    for ((r, g), b) in std::iter::zip(sr, sg).zip(sb) {
        let dr = *r as f64;
        let dg = *g as f64;
        let db = *b as f64;
        let mixed = dr.mul_add(lr, dg.mul_add(lg, db * lb));
        let mult = mixed.powf(exp);
        *r = (dr * mult) as f32;
        *g = (dg * mult) as f32;
        *b = (db * mult) as f32;
    }
}

fn hlg_ootf_inner(exp: f32, [lr, lg, lb]: [f32; 3], [sr, sg, sb]: [&mut [f32]; 3]) {
    if exp.abs() < 0.1 {
        return;
    }

    for ((r, g), b) in std::iter::zip(sr, sg).zip(sb) {
        let mixed = r.mul_add(lr, g.mul_add(lg, *b * lb));
        let mult = crate::util::fast_powf(mixed, exp);
        *r *= mult;
        *g *= mult;
        *b *= mult;
    }
}

/// Converts scene-referred linear samples to display-referred linear samples using HLG OOTF.
///
/// This version uses double precision arithmetic internally.
pub fn hlg_scene_to_display_precise(
    intensity_display: f32,
    luminance_rgb: [f32; 3],
    samples_rgb: [&mut [f32]; 3],
) {
    let system_gamma = 1.2f64 * 1.111f64.powf((intensity_display as f64 / 1e3).log2());
    let gamma_sub_one = system_gamma - 1.0;
    hlg_ootf_inner_precise(gamma_sub_one, luminance_rgb, samples_rgb);
}

/// Converts display-referred linear samples to scene-referred linear samples using HLG inverse
/// OOTF.
///
/// This version uses double precision arithmetic internally.
pub fn hlg_display_to_scene_precise(
    intensity_display: f32,
    luminance_rgb: [f32; 3],
    samples_rgb: [&mut [f32]; 3],
) {
    let system_gamma = 1.2f64 * 1.111f64.powf((intensity_display as f64 / 1e3).log2());
    let one_sub_gamma = 1.0 - system_gamma;
    hlg_ootf_inner_precise(one_sub_gamma / system_gamma, luminance_rgb, samples_rgb);
}

/// Converts scene-referred linear samples to display-referred linear samples using HLG OOTF.
///
/// This version uses `fast_powf` to compute power function.
pub fn hlg_scene_to_display(
    intensity_display: f32,
    luminance_rgb: [f32; 3],
    samples_rgb: [&mut [f32]; 3],
) {
    let system_gamma = 1.2f32 * 1.111f32.powf((intensity_display / 1e3).log2());
    let gamma_sub_one = system_gamma - 1.0;
    hlg_ootf_inner(gamma_sub_one, luminance_rgb, samples_rgb);
}

/// Converts display-referred linear samples to scene-referred linear samples using HLG inverse
/// OOTF.
///
/// This version uses `fast_powf` to compute power function.
pub fn hlg_display_to_scene(
    intensity_display: f32,
    luminance_rgb: [f32; 3],
    samples_rgb: [&mut [f32]; 3],
) {
    let system_gamma = 1.2f32 * 1.111f32.powf((intensity_display / 1e3).log2());
    let one_sub_gamma = 1.0 - system_gamma;
    hlg_ootf_inner(one_sub_gamma / system_gamma, luminance_rgb, samples_rgb);
}

/// Converts scene-referred linear sample to HLG signal.
///
/// This version uses double precision arithmetic internally.
pub fn scene_to_hlg_precise(samples: &mut [f32]) {
    for s in samples {
        let a = s.abs() as f64;
        let y = if a <= 1.0 / 12.0 {
            (3.0 * a).sqrt()
        } else {
            // TODO(tirr-c): maybe use mul_add?
            HLG_A * (12.0 * a - HLG_B).ln() + HLG_C
        };
        *s = (y as f32).copysign(*s);
    }
}

/// Converts HLG signal to scene-referred linear sample.
///
/// This version uses double precision arithmetic internally.
pub fn hlg_to_scene_precise(samples: &mut [f32]) {
    for s in samples {
        let a = s.abs() as f64;
        let y = if a <= 0.5 {
            a * a / 3.0
        } else {
            (((a - HLG_C) / HLG_A).exp() + HLG_B) / 12.0
        };
        *s = (y as f32).copysign(*s);
    }
}

/// Converts scene-referred linear sample to HLG signal.
///
/// This version uses `fast_log2f` to apply logarithmic function.
// Max error: ~5e-7
pub fn scene_to_hlg(samples: &mut [f32]) {
    for s in samples {
        let a = s.abs();
        let y = if a <= 1.0 / 12.0 {
            (3.0 * a).sqrt()
        } else {
            // TODO(tirr-c): maybe use mul_add?
            let log = crate::util::fast_log2f(12.0 * a - HLG_B as f32);
            // log2 x = ln x / ln 2, therefore ln x = (ln 2)(log2 x)
            (HLG_A * std::f64::consts::LN_2) as f32 * log + HLG_C as f32
        };
        *s = y.copysign(*s);
    }
}

/// Converts HLG signal to scene-referred linear sample.
///
/// This version uses `fast_pow2f` to apply logarithmic function.
// Max error: ~5e-6
pub fn hlg_to_scene(samples: &mut [f32]) {
    for s in samples {
        let a = s.abs();
        let y = if a <= 0.5 {
            a * a / 3.0
        } else {
            const POW: f32 = (std::f64::consts::LOG2_E / HLG_A) as f32;
            const ADD: f32 = (HLG_B / 12.0) as f32;
            // TODO(OneDeuxTriSeiGo): replace raw constant with the below equation
            // when std::f64::exp() can is available as a const fn.
            //
            // Equation: ((-HLG_B / HLG_A).exp() / 12.0)
            // Constant: 0.003_639_807_079_052_639
            const MUL: f32 = 0.003_639_807;

            // TODO(OneDeuxTriSeiGo): maybe use mul_add?
            crate::util::fast_pow2f(a * POW) * MUL + ADD
        };
        *s = y.copysign(*s);
    }
}

#[cfg(test)]
mod test {
    use test_log::test;

    use super::*;
    use crate::util::test::assert_all_almost_abs_eq;

    fn arb_samples(
        u: &mut arbtest::arbitrary::Unstructured,
    ) -> arbtest::arbitrary::Result<Vec<f32>> {
        const DENOM: u32 = 1 << 24;

        let mut samples = Vec::new();

        // uniform distribution in [-1.0, 1.0]
        while !u.is_empty() {
            let a: u32 = u.int_in_range(0..=DENOM)?;
            let signed: bool = u.arbitrary()?;
            let x = a as f32 / DENOM as f32;
            samples.push(if signed { -x } else { x });
        }

        Ok(samples)
    }

    /// Naive linear to sRGB using actual pow for testing.
    fn linear_to_srgb_naive(samples: &mut [f32]) {
        for x in samples {
            let a = x.abs();
            *x = if a <= 0.0031308 {
                a * 12.92
            } else {
                a.powf(1.0 / 2.4).mul_add(1.055, -0.055)
            }
            .copysign(*x);
        }
    }

    /// Naive linear to BT.709 using actual pow for testing.
    fn linear_to_bt709_naive(samples: &mut [f32]) {
        for x in samples {
            let a = x.abs();
            *x = if a <= 0.018 {
                a * 4.5
            } else {
                a.powf(0.45).mul_add(1.099, -0.099)
            }
            .copysign(*x);
        }
    }

    #[test]
    fn srgb_roundtrip_arb() {
        arbtest::arbtest(|u| {
            let samples = arb_samples(u)?;
            let mut output = samples.clone();

            linear_to_srgb_simd(jxl_simd::ScalarDescriptor::new().unwrap(), &mut output);
            srgb_to_linear(&mut output);
            assert_all_almost_abs_eq(&output, &samples, 2e-6);
            Ok(())
        });
    }

    #[test]
    fn bt709_roundtrip_arb() {
        arbtest::arbtest(|u| {
            let samples = arb_samples(u)?;
            let mut output = samples.clone();

            linear_to_bt709_simd(jxl_simd::ScalarDescriptor::new().unwrap(), &mut output);
            bt709_to_linear(&mut output);
            assert_all_almost_abs_eq(&output, &samples, 5e-6);
            Ok(())
        });
    }

    #[test]
    fn linear_to_srgb_simd_arb() {
        arbtest::arbtest(|u| {
            let mut samples = arb_samples(u)?;
            let mut simd = samples.clone();

            linear_to_srgb_naive(&mut samples);
            linear_to_srgb_simd(jxl_simd::ScalarDescriptor::new().unwrap(), &mut simd);
            assert_all_almost_abs_eq(&samples, &simd, 1e-6);
            Ok(())
        });
    }

    #[test]
    fn linear_to_bt709_simd_arb() {
        arbtest::arbtest(|u| {
            let mut samples = arb_samples(u)?;
            let mut simd = samples.clone();

            linear_to_bt709_naive(&mut samples);
            linear_to_bt709_simd(jxl_simd::ScalarDescriptor::new().unwrap(), &mut simd);
            assert_all_almost_abs_eq(&samples, &simd, 1e-6);
            Ok(())
        });
    }

    #[test]
    fn linear_to_pq_arb() {
        arbtest::arbtest(|u| {
            let intensity_target = u.int_in_range(9900..=10100)? as f32;
            let mut samples = arb_samples(u)?;
            let mut precise = samples.clone();

            linear_to_pq(intensity_target, &mut samples);
            linear_to_pq_precise(intensity_target, &mut precise);
            // Error seems to increase at intensity_target < 10000
            assert_all_almost_abs_eq(&samples, &precise, 8e-7);
            Ok(())
        });
    }

    #[test]
    fn pq_to_linear_arb() {
        arbtest::arbtest(|u| {
            let intensity_target = u.int_in_range(9900..=10100)? as f32;
            let mut samples = arb_samples(u)?;
            let mut precise = samples.clone();

            pq_to_linear(intensity_target, &mut samples);
            pq_to_linear_precise(intensity_target, &mut precise);
            assert_all_almost_abs_eq(&samples, &precise, 3e-6);
            Ok(())
        });
    }

    #[test]
    fn bt709_to_linear_simd_arb() {
        arbtest::arbtest(|u| {
            let mut samples = arb_samples(u)?;
            let mut simd = samples.clone();
            let xsize = samples.len();

            bt709_to_linear(&mut samples);
            bt709_to_linear_simd(jxl_simd::ScalarDescriptor::new().unwrap(), xsize, &mut simd);
            assert_all_almost_abs_eq(&samples, &simd, 1e-5);
            Ok(())
        });
    }

    #[test]
    fn pq_to_linear_simd_arb() {
        arbtest::arbtest(|u| {
            let intensity_target = u.int_in_range(9900..=10100)? as f32;
            let mut samples = arb_samples(u)?;
            let mut simd = samples.clone();
            let xsize = samples.len();

            pq_to_linear(intensity_target, &mut samples);
            pq_to_linear_simd(
                jxl_simd::ScalarDescriptor::new().unwrap(),
                intensity_target,
                xsize,
                &mut simd,
            );
            assert_all_almost_abs_eq(&samples, &simd, 2e-5);
            Ok(())
        });
    }

    #[test]
    fn linear_to_pq_simd_arb() {
        arbtest::arbtest(|u| {
            let intensity_target = u.int_in_range(9900..=10100)? as f32;
            let mut samples = arb_samples(u)?;
            let mut simd = samples.clone();
            let xsize = samples.len();

            linear_to_pq(intensity_target, &mut samples);
            linear_to_pq_simd(
                jxl_simd::ScalarDescriptor::new().unwrap(),
                intensity_target,
                xsize,
                &mut simd,
            );
            assert_all_almost_abs_eq(&samples, &simd, 2e-5);
            Ok(())
        });
    }

    #[test]
    fn hlg_ootf_arb() {
        arbtest::arbtest(|u| {
            let intensity_target = u.int_in_range(900..=1100)? as f32;

            let lr = 0.2 + u.int_in_range(0..=255)? as f32 / 255.0;
            let lb = 0.2 + u.int_in_range(0..=255)? as f32 / 255.0;
            let lg = 1.0 - lr - lb;
            let luminance_rgb = [lr, lg, lb];

            let r = u.int_in_range(0u32..=(1 << 24))? as f32 / (1 << 24) as f32;
            let g = u.int_in_range(0u32..=(1 << 24))? as f32 / (1 << 24) as f32;
            let b = u.int_in_range(0u32..=(1 << 24))? as f32 / (1 << 24) as f32;

            let mut fast_r = r;
            let mut fast_g = g;
            let mut fast_b = b;
            let fast = [
                std::slice::from_mut(&mut fast_r),
                std::slice::from_mut(&mut fast_g),
                std::slice::from_mut(&mut fast_b),
            ];
            hlg_display_to_scene(intensity_target, luminance_rgb, fast);

            let mut precise_r = r;
            let mut precise_g = g;
            let mut precise_b = b;
            let precise = [
                std::slice::from_mut(&mut precise_r),
                std::slice::from_mut(&mut precise_g),
                std::slice::from_mut(&mut precise_b),
            ];
            hlg_display_to_scene(intensity_target, luminance_rgb, precise);

            assert_all_almost_abs_eq(
                &[fast_r, fast_g, fast_b],
                &[precise_r, precise_g, precise_b],
                7.2e-7,
            );

            let mut fast_r = r;
            let mut fast_g = g;
            let mut fast_b = b;
            let fast = [
                std::slice::from_mut(&mut fast_r),
                std::slice::from_mut(&mut fast_g),
                std::slice::from_mut(&mut fast_b),
            ];
            hlg_scene_to_display(intensity_target, luminance_rgb, fast);

            let mut precise_r = r;
            let mut precise_g = g;
            let mut precise_b = b;
            let precise = [
                std::slice::from_mut(&mut precise_r),
                std::slice::from_mut(&mut precise_g),
                std::slice::from_mut(&mut precise_b),
            ];
            hlg_scene_to_display(intensity_target, luminance_rgb, precise);

            assert_all_almost_abs_eq(
                &[fast_r, fast_g, fast_b],
                &[precise_r, precise_g, precise_b],
                7.2e-7,
            );

            Ok(())
        });
    }

    #[test]
    fn scene_to_hlg_arb() {
        arbtest::arbtest(|u| {
            let mut samples = arb_samples(u)?;
            let mut precise = samples.clone();

            scene_to_hlg(&mut samples);
            scene_to_hlg_precise(&mut precise);
            assert_all_almost_abs_eq(&samples, &precise, 5e-7);
            Ok(())
        });
    }

    #[test]
    fn hlg_to_scene_arb() {
        arbtest::arbtest(|u| {
            let mut samples = arb_samples(u)?;
            let mut precise = samples.clone();

            hlg_to_scene(&mut samples);
            hlg_to_scene_precise(&mut precise);
            assert_all_almost_abs_eq(&samples, &precise, 5e-6);
            Ok(())
        });
    }
}
