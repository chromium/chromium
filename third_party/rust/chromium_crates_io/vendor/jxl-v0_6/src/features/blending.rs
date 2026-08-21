// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::needless_range_loop)]

use jxl_simd::{F32SimdVec, SimdDescriptor, SimdMask, simd_function};

use crate::headers::extra_channels::{ExtraChannel, ExtraChannelInfo};

use super::patches::{PatchBlendMode, PatchBlending};

const MAX_F32_LANES: usize = 16;

#[inline(always)]
fn maybe_clamp(v: f32, clamp: bool) -> f32 {
    if clamp { v.clamp(0.0, 1.0) } else { v }
}

/// Which layer is placed on top: `Above` blends fg over bg, `Below` blends bg
/// over fg. The top layer's alpha is the one that is clamped and drives the blend.
#[derive(Copy, Clone, PartialEq, Eq)]
enum BlendOrder {
    Above,
    Below,
}

#[derive(Copy, Clone)]
struct BlendConfig {
    clamp: bool,
    alpha_associated: bool,
    order: BlendOrder,
}

#[inline(always)]
fn load_vec<D: SimdDescriptor>(d: D, s: &[f32], x: usize) -> D::F32Vec {
    let lanes = D::F32Vec::LEN;
    if x + lanes <= s.len() {
        D::F32Vec::load(d, &s[x..x + lanes])
    } else {
        let mut buf = [0.0; MAX_F32_LANES];
        buf[..s.len() - x].copy_from_slice(&s[x..]);
        D::F32Vec::load(d, &buf[..lanes])
    }
}

#[inline(always)]
fn store_vec<D: SimdDescriptor>(_d: D, v: D::F32Vec, s: &mut [f32], x: usize) {
    let lanes = D::F32Vec::LEN;
    if x + lanes <= s.len() {
        v.store(&mut s[x..x + lanes]);
    } else {
        let mut buf = [0.0; MAX_F32_LANES];
        v.store(&mut buf[..lanes]);
        let rem = s.len() - x;
        s[x..].copy_from_slice(&buf[..rem]);
    }
}

simd_function!(
    blend,
    d: D,
    #[allow(clippy::too_many_arguments)]
    fn blend_impl(
        bg_color: &mut [&mut [f32]],
        bg_alpha: &mut [f32],
        bg_alpha_old: &[f32],
        fg_color: &[&[f32]],
        fg_alpha: &[f32],
        xsize: usize,
        cfg: BlendConfig,
    ) {
        let BlendConfig { clamp, alpha_associated, order } = cfg;
        let fg_on_top = order == BlendOrder::Above;

        let lanes = D::F32Vec::LEN;
        let one = D::F32Vec::splat(d, 1.0);
        let zero = D::F32Vec::zero(d);

        let [bg_c0, bg_c1, bg_c2] = bg_color else { unreachable!() };
        let bg_c = [&mut bg_c0[..xsize], &mut bg_c1[..xsize], &mut bg_c2[..xsize]];
        let bg_alpha = &mut bg_alpha[..xsize];
        let fg_c = [&fg_color[0][..xsize], &fg_color[1][..xsize], &fg_color[2][..xsize]];

        let (top_alpha, bottom_alpha) = if fg_on_top {
            (&fg_alpha[..xsize], &bg_alpha_old[..xsize])
        } else {
            (&bg_alpha_old[..xsize], &fg_alpha[..xsize])
        };
        let maybe_clamp_vec = |v: D::F32Vec| if clamp { v.max(zero).min(one) } else { v };

        for k in 0..xsize.div_ceil(lanes) {
            let x = k * lanes;
            let top_a = maybe_clamp_vec(load_vec(d, top_alpha, x));
            let bottom_a = load_vec(d, bottom_alpha, x);
            let one_minus_top_a = one - top_a;
            let new_a = one - one_minus_top_a * (one - bottom_a);
            let reciprocal_a = new_a.gt(zero).if_then_else_f32(one / new_a, zero);
            for c in 0..3 {
                let bg_v = load_vec(d, bg_c[c], x);
                let fg_v = load_vec(d, fg_c[c], x);
                let (top_c, bottom_c) = if fg_on_top { (fg_v, bg_v) } else { (bg_v, fg_v) };
                let out = if alpha_associated {
                    top_c + bottom_c * one_minus_top_a
                } else {
                    (top_c * top_a + bottom_c * bottom_a * one_minus_top_a) * reciprocal_a
                };
                store_vec(d, out, bg_c[c], x);
            }
            store_vec(d, new_a, bg_alpha, x);
        }
    }
);

simd_function!(
    blend_alpha,
    d: D,
    fn blend_alpha_impl(bg_alpha: &mut [f32], fg_alpha: &[f32], xsize: usize, cfg: BlendConfig) {
        let clamp = cfg.clamp;
        let fg_on_top = cfg.order == BlendOrder::Above;
        let lanes = D::F32Vec::LEN;
        let one = D::F32Vec::splat(d, 1.0);
        let zero = D::F32Vec::zero(d);

        let bg_alpha = &mut bg_alpha[..xsize];
        let fg_alpha = &fg_alpha[..xsize];
        let maybe_clamp_vec = |v: D::F32Vec| if clamp { v.max(zero).min(one) } else { v };

        for k in 0..xsize.div_ceil(lanes) {
            let x = k * lanes;
            let fg_a = load_vec(d, fg_alpha, x);
            let bg_a = load_vec(d, bg_alpha, x);
            let (top_a, bottom_a) = if fg_on_top { (fg_a, bg_a) } else { (bg_a, fg_a) };
            let top_a = maybe_clamp_vec(top_a);
            store_vec(d, one - (one - top_a) * (one - bottom_a), bg_alpha, x);
        }
    }
);

simd_function!(
    add,
    d: D,
    fn add_impl(dst: &mut [f32], src: &[f32], xsize: usize) {
        let lanes = D::F32Vec::LEN;
        let dst = &mut dst[..xsize];
        let src = &src[..xsize];
        for k in 0..xsize.div_ceil(lanes) {
            let x = k * lanes;
            store_vec(d, load_vec(d, dst, x) + load_vec(d, src, x), dst, x);
        }
    }
);

simd_function!(
    mul,
    d: D,
    fn mul_impl(dst: &mut [f32], src: &[f32], xsize: usize, clamp: bool) {
        let lanes = D::F32Vec::LEN;
        let one = D::F32Vec::splat(d, 1.0);
        let zero = D::F32Vec::zero(d);
        let dst = &mut dst[..xsize];
        let src = &src[..xsize];
        let maybe_clamp_vec = |v: D::F32Vec| if clamp { v.max(zero).min(one) } else { v };
        for k in 0..xsize.div_ceil(lanes) {
            let x = k * lanes;
            let s = maybe_clamp_vec(load_vec(d, src, x));
            store_vec(d, load_vec(d, dst, x) * s, dst, x);
        }
    }
);

// Above: dst = dst + src * weight (weight is the newly composited layer's own alpha).
// Below: dst = src + dst * weight (weight is the old background's alpha).
simd_function!(
    alpha_weighted_add,
    d: D,
    fn alpha_weighted_add_impl(dst: &mut [f32], src: &[f32], weight: &[f32], xsize: usize, cfg: BlendConfig) {
        let clamp = cfg.clamp;
        let fg_on_top = cfg.order == BlendOrder::Above;
        let lanes = D::F32Vec::LEN;
        let one = D::F32Vec::splat(d, 1.0);
        let zero = D::F32Vec::zero(d);
        let dst = &mut dst[..xsize];
        let src = &src[..xsize];
        let weight = &weight[..xsize];
        let maybe_clamp_vec = |v: D::F32Vec| if clamp { v.max(zero).min(one) } else { v };
        for k in 0..xsize.div_ceil(lanes) {
            let x = k * lanes;
            let w = maybe_clamp_vec(load_vec(d, weight, x));
            let dst_v = load_vec(d, dst, x);
            let src_v = load_vec(d, src, x);
            let (unweighted, weighted) = if fg_on_top { (dst_v, src_v) } else { (src_v, dst_v) };
            store_vec(d, unweighted + weighted * w, dst, x);
        }
    }
);

/// Blend `fg` onto `bg` in place.
pub fn perform_blending(
    bg: &mut [&mut [f32]],
    fg: &[&[f32]],
    color_blending: &PatchBlending,
    ec_blending: &[PatchBlending],
    extra_channel_info: &[ExtraChannelInfo],
    tmp: &mut Vec<f32>,
) {
    // TODO(veluca): in many cases, the copy to temporary space seems redundant. Investigate more.
    let num_ec = extra_channel_info.len();
    let xsize = bg[0].len();

    // Fast path: if color is None (keep bg) and all ec are None, nothing to do.
    if color_blending.mode == PatchBlendMode::None
        && ec_blending.iter().all(|b| b.mode == PatchBlendMode::None)
    {
        return;
    }

    // Fast path: Replace color + Replace/None ec -> copy fg directly to bg, no tmp needed.
    if color_blending.mode == PatchBlendMode::Replace {
        let all_simple = ec_blending[..num_ec]
            .iter()
            .all(|b| b.mode == PatchBlendMode::Replace || b.mode == PatchBlendMode::None);
        if all_simple {
            for c in 0..3 {
                bg[c].copy_from_slice(fg[c]);
            }
            for i in 0..num_ec {
                match ec_blending[i].mode {
                    PatchBlendMode::Replace => {
                        bg[3 + i].copy_from_slice(fg[3 + i]);
                    }
                    PatchBlendMode::None => {} // keep bg
                    _ => unreachable!(),
                }
            }
            return;
        }
    }

    let has_alpha = extra_channel_info
        .iter()
        .any(|info| info.ec_type == ExtraChannel::Alpha);

    let needed_scratch = num_ec * xsize;

    if tmp.len() < needed_scratch {
        tmp.resize(needed_scratch, 0.0);
    }

    for i in 0..num_ec {
        tmp[i * xsize..][..xsize].copy_from_slice(&bg[3 + i][..xsize]);
    }
    let old_ec: &[f32] = tmp;
    let old_alpha = |a: usize| &old_ec[a * xsize..][..xsize];

    for i in 0..num_ec {
        let alpha = ec_blending[i].alpha_channel;
        let clamp = ec_blending[i].clamp;
        let alpha_associated = extra_channel_info[alpha].alpha_associated();

        let (_bg_color, bg_ec) = bg.split_at_mut(3);
        let ec_out = &mut bg_ec[i][..xsize];

        match ec_blending[i].mode {
            PatchBlendMode::Add => {
                add(ec_out, &fg[3 + i][..xsize], xsize);
            }
            PatchBlendMode::BlendAbove => {
                if i == alpha {
                    blend_alpha(
                        ec_out,
                        &fg[3 + alpha][..xsize],
                        xsize,
                        BlendConfig {
                            clamp,
                            alpha_associated,
                            order: BlendOrder::Above,
                        },
                    );
                } else if alpha_associated {
                    for x in 0..xsize {
                        let fa = maybe_clamp(fg[3 + alpha][x], clamp);
                        ec_out[x] = fg[3 + i][x] + ec_out[x] * (1.0 - fa);
                    }
                } else {
                    for x in 0..xsize {
                        let fa = maybe_clamp(fg[3 + alpha][x], clamp);
                        let oa = old_alpha(alpha)[x];
                        let new_a = 1.0 - (1.0 - fa) * (1.0 - oa);
                        let rnew_a = if new_a > 0.0 { 1.0 / new_a } else { 0.0 };
                        ec_out[x] = (fg[3 + i][x] * fa + ec_out[x] * oa * (1.0 - fa)) * rnew_a;
                    }
                }
            }
            PatchBlendMode::BlendBelow => {
                if i == alpha {
                    blend_alpha(
                        ec_out,
                        &fg[3 + i][..xsize],
                        xsize,
                        BlendConfig {
                            clamp,
                            alpha_associated,
                            order: BlendOrder::Below,
                        },
                    );
                } else if alpha_associated {
                    for x in 0..xsize {
                        let oa = old_alpha(alpha)[x];
                        let ba = maybe_clamp(oa, clamp);
                        ec_out[x] += fg[3 + i][x] * (1.0 - ba);
                    }
                } else {
                    for x in 0..xsize {
                        let oa = old_alpha(alpha)[x];
                        let ba = maybe_clamp(oa, clamp);
                        let new_a = 1.0 - (1.0 - ba) * (1.0 - fg[3 + alpha][x]);
                        let rnew_a = if new_a > 0.0 { 1.0 / new_a } else { 0.0 };
                        ec_out[x] = (ec_out[x] * ba + fg[3 + i][x] * fg[3 + alpha][x] * (1.0 - ba))
                            * rnew_a;
                    }
                }
            }
            PatchBlendMode::AlphaWeightedAddAbove => {
                if i == alpha {
                    // ec_out is already bg[3 + i]
                } else {
                    alpha_weighted_add(
                        ec_out,
                        &fg[3 + i][..xsize],
                        &fg[3 + alpha][..xsize],
                        xsize,
                        BlendConfig {
                            clamp,
                            alpha_associated: false,
                            order: BlendOrder::Above,
                        },
                    );
                }
            }
            PatchBlendMode::AlphaWeightedAddBelow => {
                if i == alpha {
                    ec_out.copy_from_slice(&fg[3 + i][..xsize]);
                } else {
                    alpha_weighted_add(
                        ec_out,
                        &fg[3 + i][..xsize],
                        old_alpha(alpha),
                        xsize,
                        BlendConfig {
                            clamp,
                            alpha_associated: false,
                            order: BlendOrder::Below,
                        },
                    );
                }
            }
            PatchBlendMode::Mul => {
                mul(ec_out, &fg[3 + i][..xsize], xsize, clamp);
            }
            PatchBlendMode::Replace => {
                ec_out.copy_from_slice(&fg[3 + i][..xsize]);
            }
            PatchBlendMode::None => {
                // ec_out is already bg[3 + i]
            }
        }
    }

    let alpha = color_blending.alpha_channel;
    let clamp = color_blending.clamp;

    let (bg_color, bg_ec) = bg.split_at_mut(3);

    match color_blending.mode {
        PatchBlendMode::Add => {
            for c in 0..3 {
                add(bg_color[c], &fg[c][..xsize], xsize);
            }
        }
        PatchBlendMode::AlphaWeightedAddAbove => {
            for c in 0..3 {
                if !has_alpha {
                    add(bg_color[c], &fg[c][..xsize], xsize);
                } else {
                    alpha_weighted_add(
                        bg_color[c],
                        &fg[c][..xsize],
                        &fg[3 + alpha][..xsize],
                        xsize,
                        BlendConfig {
                            clamp,
                            alpha_associated: false,
                            order: BlendOrder::Above,
                        },
                    );
                }
            }
        }
        PatchBlendMode::AlphaWeightedAddBelow => {
            for c in 0..3 {
                if !has_alpha {
                    add(bg_color[c], &fg[c][..xsize], xsize);
                } else {
                    alpha_weighted_add(
                        bg_color[c],
                        &fg[c][..xsize],
                        old_alpha(alpha),
                        xsize,
                        BlendConfig {
                            clamp,
                            alpha_associated: false,
                            order: BlendOrder::Below,
                        },
                    );
                }
            }
        }
        PatchBlendMode::BlendAbove => {
            if !has_alpha {
                for c in 0..3 {
                    bg_color[c][..xsize].copy_from_slice(&fg[c][..xsize]);
                }
            } else {
                blend(
                    bg_color,
                    bg_ec[alpha],
                    old_alpha(alpha),
                    &fg[..3],
                    fg[3 + alpha],
                    xsize,
                    BlendConfig {
                        clamp,
                        alpha_associated: extra_channel_info[alpha].alpha_associated(),
                        order: BlendOrder::Above,
                    },
                );
            }
        }
        PatchBlendMode::BlendBelow => {
            if !has_alpha {
                // already bg[c]
            } else {
                blend(
                    bg_color,
                    bg_ec[alpha],
                    old_alpha(alpha),
                    &fg[..3],
                    fg[3 + alpha],
                    xsize,
                    BlendConfig {
                        clamp,
                        alpha_associated: extra_channel_info[alpha].alpha_associated(),
                        order: BlendOrder::Below,
                    },
                );
            }
        }
        PatchBlendMode::Mul => {
            for c in 0..3 {
                mul(bg_color[c], &fg[c][..xsize], xsize, clamp);
            }
        }
        PatchBlendMode::Replace => {
            for c in 0..3 {
                bg_color[c][..xsize].copy_from_slice(&fg[c][..xsize]);
            }
        }
        PatchBlendMode::None => {
            // already bg_color[c]
        }
    }
}

#[cfg(test)]
mod tests {
    fn clamp(x: f32) -> f32 {
        x.clamp(0.0, 1.0)
    }

    mod perform_blending_tests {
        use super::{super::*, *};
        use crate::{headers::bit_depth::BitDepth, tests::assert_close};
        use test_log::test;

        const ABS_DELTA: f32 = 1e-6;

        /// Test-only wrapper that allocates a tmp scratch buffer per call. Production
        /// callers are expected to supply their own (reusable) tmp.
        fn blend(
            bg: &mut [&mut [f32]],
            fg: &[&[f32]],
            color_blending: &PatchBlending,
            ec_blending: &[PatchBlending],
            extra_channel_info: &[ExtraChannelInfo],
        ) {
            perform_blending(
                bg,
                fg,
                color_blending,
                ec_blending,
                extra_channel_info,
                &mut vec![],
            );
        }

        // Helper for expected value calculations based on C++ logic

        // Alpha compositing formula: Ao = As + Ab * (1 - As)
        // Used for kBlend modes for the alpha channel itself.
        fn expected_alpha_blend(fg_a: f32, bg_a: f32) -> f32 {
            fg_a + bg_a * (1.0 - fg_a)
        }

        // Color compositing for kBlend, premultiplied alpha: Co = Cs_premult + Cb_premult * (1 - As)
        fn expected_color_blend_premultiplied(c_fg: f32, c_bg: f32, fg_a: f32) -> f32 {
            c_fg + c_bg * (1.0 - fg_a)
        }

        // Color compositing for kBlend, non-premultiplied alpha: Co = (Cs * As + Cb * Ab * (1 - As)) / Ao_blend
        fn expected_color_blend_non_premultiplied(
            c_fg: f32,
            fg_a: f32, // Foreground color and its alpha
            c_bg: f32,
            bg_a: f32,            // Background color and its alpha
            alpha_blend_out: f32, // The resulting alpha from expected_alpha_blend(fg_a, bg_a)
        ) -> f32 {
            if alpha_blend_out.abs() < ABS_DELTA {
                // Avoid division by zero
                0.0
            } else {
                (c_fg * fg_a + c_bg * bg_a * (1.0 - fg_a)) / alpha_blend_out
            }
        }

        // For kAlphaWeightedAdd modes: Co = Cb + Cs * As
        fn expected_alpha_weighted_add(c_bg: f32, c_fg: f32, fg_a: f32) -> f32 {
            c_bg + c_fg * fg_a
        }

        // For kMul mode: Co = Cb * Cs
        fn expected_mul_blend(c_bg: f32, c_fg: f32) -> f32 {
            c_bg * c_fg
        }

        #[test]
        fn test_color_replace_fg_over_bg() {
            let mut bg_r = [0.1];
            let mut bg_g = [0.2];
            let mut bg_b = [0.3];
            let fg_r = [0.7];
            let fg_g = [0.8];
            let fg_b = [0.9];

            let mut bg_channels: [&mut [f32]; 3] = [&mut bg_r, &mut bg_g, &mut bg_b];
            let fg_channels: [&[f32]; 3] = [&fg_r, &fg_g, &fg_b];

            let color_blending = PatchBlending {
                mode: PatchBlendMode::Replace,
                alpha_channel: 0, // Not used for Replace
                clamp: false,
            };

            let ec_blending: [PatchBlending; 0] = [];
            let extra_channel_info: [ExtraChannelInfo; 0] = [];

            blend(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            // Expected: output color is fg color
            assert_close!(all, &bg_r, &fg_r, ABS_DELTA);
            assert_close!(all, &bg_g, &fg_g, ABS_DELTA);
            assert_close!(all, &bg_b, &fg_b, ABS_DELTA);
        }

        #[test]
        fn test_color_add() {
            let mut bg_r = [0.1];
            let mut bg_g = [0.2];
            let mut bg_b = [0.3];
            let fg_r = [0.7];
            let fg_g = [0.6];
            let fg_b = [0.5];
            let expected_r = [bg_r[0] + fg_r[0]];
            let expected_g = [bg_g[0] + fg_g[0]];
            let expected_b = [bg_b[0] + fg_b[0]];

            let mut bg_channels: [&mut [f32]; 3] = [&mut bg_r, &mut bg_g, &mut bg_b];
            let fg_channels: [&[f32]; 3] = [&fg_r, &fg_g, &fg_b];

            let color_blending = PatchBlending {
                mode: PatchBlendMode::Add,
                alpha_channel: 0, // Not used
                clamp: false,
            };
            let ec_blending: [PatchBlending; 0] = [];
            let extra_channel_info: [ExtraChannelInfo; 0] = [];

            blend(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            assert_close!(all, &bg_r, &expected_r, ABS_DELTA);
            assert_close!(all, &bg_g, &expected_g, ABS_DELTA);
            assert_close!(all, &bg_b, &expected_b, ABS_DELTA);
        }

        #[test]
        fn test_color_blend_above_premultiplied_alpha() {
            // BG: R=0.1, G=0.2, B=0.3, A=0.8 (premultiplied)
            // FG: R=0.4, G=0.3, B=0.2, A=0.5 (premultiplied)
            let mut bg_r = [0.1];
            let mut bg_g = [0.2];
            let mut bg_b = [0.3];
            let mut bg_a = [0.8];
            let fg_r = [0.4];
            let fg_g = [0.3];
            let fg_b = [0.2];
            let fg_a = [0.5];
            let fga = fg_a[0]; // Not clamped
            let bga = bg_a[0];

            // Expected alpha: Ao = Afg + Abg * (1 - Afg)
            let expected_a_val = expected_alpha_blend(fga, bga);
            // Expected color: Co = Cfg_premult + Cbg_premult * (1 - Afg)
            let expected_r_val = expected_color_blend_premultiplied(fg_r[0], bg_r[0], fga);
            let expected_g_val = expected_color_blend_premultiplied(fg_g[0], bg_g[0], fga);
            let expected_b_val = expected_color_blend_premultiplied(fg_b[0], bg_b[0], fga);

            let mut bg_channels: [&mut [f32]; 4] = [&mut bg_r, &mut bg_g, &mut bg_b, &mut bg_a];
            let fg_channels: [&[f32]; 4] = [&fg_r, &fg_g, &fg_b, &fg_a];

            let color_blending = PatchBlending {
                mode: PatchBlendMode::BlendAbove,
                alpha_channel: 0, // EC0 is the alpha channel
                clamp: false,
            };
            // EC0 (alpha) blending rule.
            // For BlendAbove color mode, the alpha channel itself is also blended using source-over.
            // So this ec_blending rule for alpha will be effectively overridden by color blending's alpha calculation.
            let ec_blending = [PatchBlending {
                mode: PatchBlendMode::Replace, // Arbitrary, will be overwritten by color blend
                alpha_channel: 0,
                clamp: false,
            }];
            let extra_channel_info = [ExtraChannelInfo::new(
                false,
                ExtraChannel::Alpha,
                BitDepth::f32(), // Assuming f32
                0,
                "alpha".to_string(),
                true, // alpha_associated = true (premultiplied)
                None,
                None,
            )];

            blend(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            assert_close!(all, &bg_a, &[expected_a_val], ABS_DELTA);
            assert_close!(all, &bg_r, &[expected_r_val], ABS_DELTA);
            assert_close!(all, &bg_g, &[expected_g_val], ABS_DELTA);
            assert_close!(all, &bg_b, &[expected_b_val], ABS_DELTA);
        }

        #[test]
        fn test_color_blend_above_non_premultiplied_alpha() {
            // BG: R=0.1, G=0.2, B=0.3 (unpremult), A=0.8
            // FG: R=0.7, G=0.6, B=0.5 (unpremult), A=0.5
            let mut bg_r = [0.1];
            let mut bg_g = [0.2];
            let mut bg_b = [0.3];
            let mut bg_a = [0.8];
            let fg_r = [0.7];
            let fg_g = [0.6];
            let fg_b = [0.5];
            let fg_a = [0.5];
            let fga = fg_a[0];
            let bga = bg_a[0];

            // Expected alpha: Ao = Afg + Abg * (1 - Afg)
            let expected_a_val = expected_alpha_blend(fga, bga);
            // Expected color: Co = (Cfg_unpremult * Afg + Cbg_unpremult * Abg * (1 - Afg)) / Ao_blend
            let expected_r_val =
                expected_color_blend_non_premultiplied(fg_r[0], fga, bg_r[0], bga, expected_a_val);
            let expected_g_val =
                expected_color_blend_non_premultiplied(fg_g[0], fga, bg_g[0], bga, expected_a_val);
            let expected_b_val =
                expected_color_blend_non_premultiplied(fg_b[0], fga, bg_b[0], bga, expected_a_val);

            let mut bg_channels: [&mut [f32]; 4] = [&mut bg_r, &mut bg_g, &mut bg_b, &mut bg_a];
            let fg_channels: [&[f32]; 4] = [&fg_r, &fg_g, &fg_b, &fg_a];

            let color_blending = PatchBlending {
                mode: PatchBlendMode::BlendAbove,
                alpha_channel: 0, // EC0
                clamp: false,
            };
            let ec_blending = [PatchBlending {
                // This will be overwritten for the alpha channel by color blending
                mode: PatchBlendMode::Replace,
                alpha_channel: 0,
                clamp: false,
            }];
            let extra_channel_info = [ExtraChannelInfo::new(
                false,
                ExtraChannel::Alpha,
                BitDepth::f32(),
                0,
                "alpha".to_string(),
                false, // alpha_associated = false (non-premultiplied)
                None,
                None,
            )];

            blend(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            assert_close!(all, &bg_a, &[expected_a_val], ABS_DELTA);
            assert_close!(all, &bg_r, &[expected_r_val], ABS_DELTA);
            assert_close!(all, &bg_g, &[expected_g_val], ABS_DELTA);
            assert_close!(all, &bg_b, &[expected_b_val], ABS_DELTA);
        }

        #[test]
        fn test_color_alpha_weighted_add_above() {
            let mut bg_r = [0.1];
            let mut bg_g = [0.2];
            let mut bg_b = [0.3];
            let mut bg_a = [0.8]; // bg alpha used by ec_blending
            let fg_r = [0.7];
            let fg_g = [0.6];
            let fg_b = [0.5];
            let fg_a = [0.5]; // fg alpha used for weighting
            let fga_for_weighting = fg_a[0]; // Not clamped as color_blending.clamp is false

            // Expected color: Co = Cbg + Cfg * Afg_for_weighting
            let expected_r_val = expected_alpha_weighted_add(bg_r[0], fg_r[0], fga_for_weighting);
            let expected_g_val = expected_alpha_weighted_add(bg_g[0], fg_g[0], fga_for_weighting);
            let expected_b_val = expected_alpha_weighted_add(bg_b[0], fg_b[0], fga_for_weighting);

            // Expected alpha (EC0): Blended according to ec_blending[0].
            // Mode is BlendAbove, alpha_channel is 0 (itself).
            // C++: PerformAlphaBlending(bg[3+0], bg[3+0], fg[3+0], fg[3+0], tmp.Row(3+0), ...)
            // This means it's the "alpha channel blends itself" case: Ao = Afg + Abg * (1 - Afg)
            // fg_alpha_for_ec0 = fg_a[0], bg_alpha_for_ec0 = bg_a[0]. ec_blending[0].clamp is false.
            let expected_a_val = expected_alpha_blend(fg_a[0], bg_a[0]);

            let mut bg_channels: [&mut [f32]; 4] = [&mut bg_r, &mut bg_g, &mut bg_b, &mut bg_a];
            let fg_channels: [&[f32]; 4] = [&fg_r, &fg_g, &fg_b, &fg_a];

            let color_blending = PatchBlending {
                mode: PatchBlendMode::AlphaWeightedAddAbove,
                alpha_channel: 0, // EC0 is alpha
                clamp: false,
            };
            // For AlphaWeightedAdd color mode, the alpha channel (EC0) value is determined by its ec_blending rule.
            // Let's make EC0 blend itself using BlendAbove mode.
            let ec_blending = [PatchBlending {
                mode: PatchBlendMode::BlendAbove, // Alpha channel EC0 blends itself
                alpha_channel: 0,                 //  using itself as alpha reference
                clamp: false,
            }];
            let extra_channel_info = [ExtraChannelInfo::new(
                false,
                ExtraChannel::Alpha,
                BitDepth::f32(),
                0,
                "alpha".to_string(),
                true, // alpha_associated (doesn't directly affect AWA color, but affects EC0's own blend)
                None,
                None,
            )];

            blend(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            assert_close!(all, &bg_r, &[expected_r_val], ABS_DELTA);
            assert_close!(all, &bg_g, &[expected_g_val], ABS_DELTA);
            assert_close!(all, &bg_b, &[expected_b_val], ABS_DELTA);
            assert_close!(all, &bg_a, &[expected_a_val], ABS_DELTA);
        }

        #[test]
        fn test_color_mul_with_clamp() {
            let mut bg_r = [0.5];
            let mut bg_g = [0.8];
            let mut bg_b = [1.0];
            let fg_r = [1.5];
            let fg_g = [-0.2];
            let fg_b = [0.5]; // fg values will be clamped
            let expected_r = [expected_mul_blend(bg_r[0], clamp(fg_r[0]))]; // 0.5 * 1.0 = 0.5
            let expected_g = [expected_mul_blend(bg_g[0], clamp(fg_g[0]))]; // 0.8 * 0.0 = 0.0
            let expected_b = [expected_mul_blend(bg_b[0], clamp(fg_b[0]))]; // 1.0 * 0.5 = 0.5

            let mut bg_channels: [&mut [f32]; 3] = [&mut bg_r, &mut bg_g, &mut bg_b];
            let fg_channels: [&[f32]; 3] = [&fg_r, &fg_g, &fg_b];

            let color_blending = PatchBlending {
                mode: PatchBlendMode::Mul,
                alpha_channel: 0, // Not used
                clamp: true,      // Clamp fg values
            };
            let ec_blending: [PatchBlending; 0] = [];
            let extra_channel_info: [ExtraChannelInfo; 0] = [];

            blend(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            assert_close!(all, &bg_r, &expected_r, ABS_DELTA);
            assert_close!(all, &bg_g, &expected_g, ABS_DELTA);
            assert_close!(all, &bg_b, &expected_b, ABS_DELTA);
        }

        #[test]
        fn test_ec_blend_data_with_separate_alpha_premultiplied() {
            // Color: Replace FG over BG (to keep it simple)
            // EC0: Data channel
            // EC1: Alpha channel for EC0
            let mut bg_r = [0.1];
            let mut bg_g = [0.1];
            let mut bg_b = [0.1];
            let mut bg_ec0 = [0.2];
            let mut bg_ec1_alpha = [0.9]; // EC1 is alpha for EC0

            let fg_r = [0.5];
            let fg_g = [0.5];
            let fg_b = [0.5];
            let fg_ec0 = [0.6];
            let fg_ec1_alpha = [0.4];

            // EC1 (Alpha channel for EC0) blending: BlendAbove, uses itself as alpha.
            // fg_alpha = fg_ec1_alpha[0], bg_alpha = bg_ec1_alpha[0]
            let expected_out_ec1_alpha = expected_alpha_blend(fg_ec1_alpha[0], bg_ec1_alpha[0]);

            // EC0 (Data channel) blending: BlendAbove, uses EC1 as alpha.
            // fg_alpha_for_ec0 = fg_ec1_alpha[0] (not clamped as ec_blending[0].clamp is false)
            // is_premultiplied = extra_channel_info[ec_blending[0].alpha_channel (is 1)].alpha_associated = true.
            // Formula: out = fg_data + bg_data * (1.f - fg_alpha_of_data)
            let expected_out_ec0 =
                expected_color_blend_premultiplied(fg_ec0[0], bg_ec0[0], fg_ec1_alpha[0]);

            let mut bg_channels: [&mut [f32]; 5] = [
                &mut bg_r,
                &mut bg_g,
                &mut bg_b,
                &mut bg_ec0,
                &mut bg_ec1_alpha,
            ];
            let fg_channels: [&[f32]; 5] = [&fg_r, &fg_g, &fg_b, &fg_ec0, &fg_ec1_alpha];

            let color_blending = PatchBlending {
                // Simple color replace
                mode: PatchBlendMode::Replace,
                alpha_channel: 0,
                clamp: false,
            };

            let ec_blending = [
                PatchBlending {
                    // EC0 (data) uses EC1 as alpha
                    mode: PatchBlendMode::BlendAbove,
                    alpha_channel: 1, // EC1 is alpha for EC0
                    clamp: false,
                },
                PatchBlending {
                    // EC1 (alpha) blends itself
                    mode: PatchBlendMode::BlendAbove,
                    alpha_channel: 1, // EC1 uses itself as alpha
                    clamp: false,
                },
            ];
            let extra_channel_info = [
                ExtraChannelInfo::new(
                    false,
                    ExtraChannel::Unknown,
                    BitDepth::f32(),
                    0,
                    "ec0".to_string(),
                    false,
                    None,
                    None,
                ), // EC0 data
                ExtraChannelInfo::new(
                    false,
                    ExtraChannel::Alpha,
                    BitDepth::f32(),
                    0,
                    "alpha_for_ec0".to_string(),
                    true, // EC1 is premultiplied alpha
                    None,
                    None,
                ), // EC1 alpha
            ];

            blend(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            // Expected Color (Replace)
            assert_close!(all, &bg_r, &fg_r, ABS_DELTA);
            assert_close!(all, &bg_g, &fg_g, ABS_DELTA);
            assert_close!(all, &bg_b, &fg_b, ABS_DELTA);
            assert_close!(all, &bg_ec1_alpha, &[expected_out_ec1_alpha], ABS_DELTA);
            assert_close!(all, &bg_ec0, &[expected_out_ec0], ABS_DELTA);
        }

        #[test]
        fn test_no_alpha_channel_blend_above_falls_back_to_copy_fg() {
            let mut bg_r = [0.1];
            let mut bg_g = [0.2];
            let mut bg_b = [0.3];
            let fg_r = [0.7];
            let fg_g = [0.8];
            let fg_b = [0.9];

            let mut bg_channels: [&mut [f32]; 3] = [&mut bg_r, &mut bg_g, &mut bg_b];
            let fg_channels: [&[f32]; 3] = [&fg_r, &fg_g, &fg_b];

            let color_blending = PatchBlending {
                mode: PatchBlendMode::BlendAbove,
                alpha_channel: 0, // Irrelevant as no alpha EIs
                clamp: false,
            };

            let ec_blending: [PatchBlending; 0] = [];
            // No ExtraChannelInfo means has_alpha will be false.
            let extra_channel_info: [ExtraChannelInfo; 0] = [];

            blend(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            // Expected: output color is fg color due to fallback
            assert_close!(all, &bg_r, &fg_r, ABS_DELTA);
            assert_close!(all, &bg_g, &fg_g, ABS_DELTA);
            assert_close!(all, &bg_b, &fg_b, ABS_DELTA);
        }

        #[test]
        fn test_empty_pixels() {
            let mut bg_r: [f32; 0] = [];
            let mut bg_g: [f32; 0] = [];
            let mut bg_b: [f32; 0] = [];
            let fg_r: [f32; 0] = [];
            let fg_g: [f32; 0] = [];
            let fg_b: [f32; 0] = [];

            let mut bg_channels: [&mut [f32]; 3] = [&mut bg_r, &mut bg_g, &mut bg_b];
            let fg_channels: [&[f32]; 3] = [&fg_r, &fg_g, &fg_b];

            let color_blending = PatchBlending {
                mode: PatchBlendMode::Replace,
                alpha_channel: 0,
                clamp: false,
            };
            let ec_blending: [PatchBlending; 0] = [];
            let extra_channel_info: [ExtraChannelInfo; 0] = [];

            blend(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            // Expect output slices to also be empty and unchanged.
            assert_eq!(bg_r.len(), 0);
            assert_eq!(bg_g.len(), 0);
            assert_eq!(bg_b.len(), 0);
        }
    }
}
