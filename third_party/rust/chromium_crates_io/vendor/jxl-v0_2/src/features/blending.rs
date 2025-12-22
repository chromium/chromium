// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::needless_range_loop)]

use crate::headers::extra_channels::{ExtraChannel, ExtraChannelInfo};

use super::patches::{PatchBlendMode, PatchBlending};

#[inline]
fn maybe_clamp(v: f32, clamp: bool) -> f32 {
    if clamp { v.clamp(0.0, 1.0) } else { v }
}

pub fn perform_blending<T: AsRef<[f32]>, V: AsMut<[f32]>>(
    bg: &mut [V],
    fg: &[T],
    color_blending: &PatchBlending,
    ec_blending: &[PatchBlending],
    extra_channel_info: &[ExtraChannelInfo],
) {
    let has_alpha = extra_channel_info
        .iter()
        .any(|info| info.ec_type == ExtraChannel::Alpha);
    let num_ec = extra_channel_info.len();
    let xsize = bg[0].as_mut().len();

    let mut tmp = vec![vec![0.0f32; xsize]; 3 + num_ec];

    for i in 0..num_ec {
        let alpha = ec_blending[i].alpha_channel;
        let clamp = ec_blending[i].clamp;
        let alpha_associated = extra_channel_info[alpha].alpha_associated();

        match ec_blending[i].mode {
            PatchBlendMode::Add => {
                for x in 0..xsize {
                    tmp[3 + i][x] = bg[3 + i].as_mut()[x] + fg[3 + i].as_ref()[x];
                }
            }
            PatchBlendMode::BlendAbove => {
                if i == alpha {
                    for x in 0..xsize {
                        let fa = maybe_clamp(fg[3 + alpha].as_ref()[x], clamp);
                        tmp[3 + i][x] = 1.0 - (1.0 - fa) * (1.0 - bg[3 + i].as_mut()[x]);
                    }
                } else if alpha_associated {
                    for x in 0..xsize {
                        let fa = maybe_clamp(fg[3 + alpha].as_ref()[x], clamp);
                        tmp[3 + i][x] = fg[3 + i].as_ref()[x] + bg[3 + i].as_mut()[x] * (1.0 - fa);
                    }
                } else {
                    for x in 0..xsize {
                        let fa = maybe_clamp(fg[3 + alpha].as_ref()[x], clamp);
                        let new_a = 1.0 - (1.0 - fa) * (1.0 - bg[3 + alpha].as_mut()[x]);
                        let rnew_a = if new_a > 0.0 { 1.0 / new_a } else { 0.0 };
                        tmp[3 + i][x] = (fg[3 + i].as_ref()[x] * fa
                            + bg[3 + i].as_mut()[x] * bg[3 + alpha].as_mut()[x] * (1.0 - fa))
                            * rnew_a;
                    }
                }
            }
            PatchBlendMode::BlendBelow => {
                if i == alpha {
                    for x in 0..xsize {
                        let ba = maybe_clamp(bg[3 + alpha].as_mut()[x], clamp);
                        tmp[3 + i][x] = 1.0 - (1.0 - ba) * (1.0 - fg[3 + i].as_ref()[x]);
                    }
                } else if alpha_associated {
                    for x in 0..xsize {
                        let ba = maybe_clamp(bg[3 + alpha].as_mut()[x], clamp);
                        tmp[3 + i][x] = bg[3 + i].as_mut()[x] + fg[3 + i].as_ref()[x] * (1.0 - ba);
                    }
                } else {
                    for x in 0..xsize {
                        let ba = maybe_clamp(bg[3 + alpha].as_mut()[x], clamp);
                        let new_a = 1.0 - (1.0 - ba) * (1.0 - fg[3 + alpha].as_ref()[x]);
                        let rnew_a = if new_a > 0.0 { 1.0 / new_a } else { 0.0 };
                        tmp[3 + i][x] = (bg[3 + i].as_mut()[x] * ba
                            + fg[3 + i].as_ref()[x] * fg[3 + alpha].as_ref()[x] * (1.0 - ba))
                            * rnew_a;
                    }
                }
            }
            PatchBlendMode::AlphaWeightedAddAbove => {
                if i == alpha {
                    tmp[3 + i].copy_from_slice(bg[3 + i].as_mut());
                } else if clamp {
                    for x in 0..xsize {
                        tmp[3 + i][x] = bg[3 + i].as_mut()[x]
                            + fg[3 + i].as_ref()[x] * fg[3 + alpha].as_ref()[x].clamp(0.0, 1.0);
                    }
                } else {
                    for x in 0..xsize {
                        tmp[3 + i][x] = bg[3 + i].as_mut()[x]
                            + fg[3 + i].as_ref()[x] * fg[3 + alpha].as_ref()[x];
                    }
                }
            }
            PatchBlendMode::AlphaWeightedAddBelow => {
                if i == alpha {
                    tmp[3 + i].copy_from_slice(fg[3 + i].as_ref());
                } else if clamp {
                    for x in 0..xsize {
                        tmp[3 + i][x] = fg[3 + i].as_ref()[x]
                            + bg[3 + i].as_mut()[x] * bg[3 + alpha].as_mut()[x].clamp(0.0, 1.0);
                    }
                } else {
                    for x in 0..xsize {
                        tmp[3 + i][x] = fg[3 + i].as_ref()[x]
                            + bg[3 + i].as_mut()[x] * bg[3 + alpha].as_mut()[x];
                    }
                }
            }
            PatchBlendMode::Mul => {
                if clamp {
                    for x in 0..xsize {
                        tmp[3 + i][x] =
                            bg[3 + i].as_mut()[x] * fg[3 + i].as_ref()[x].clamp(0.0, 1.0);
                    }
                } else {
                    for x in 0..xsize {
                        tmp[3 + i][x] = bg[3 + i].as_mut()[x] * fg[3 + i].as_ref()[x];
                    }
                }
            }
            PatchBlendMode::Replace => {
                tmp[3 + i].copy_from_slice(fg[3 + i].as_ref());
            }
            PatchBlendMode::None => {
                tmp[3 + i].copy_from_slice(bg[3 + i].as_mut());
            }
        }
    }

    let alpha = color_blending.alpha_channel;
    let clamp = color_blending.clamp;

    match color_blending.mode {
        PatchBlendMode::Add => {
            for c in 0..3 {
                for x in 0..xsize {
                    tmp[c][x] = bg[c].as_mut()[x] + fg[c].as_ref()[x];
                }
            }
        }
        PatchBlendMode::AlphaWeightedAddAbove => {
            for c in 0..3 {
                if !has_alpha {
                    for x in 0..xsize {
                        tmp[c][x] = bg[c].as_mut()[x] + fg[c].as_ref()[x];
                    }
                } else if clamp {
                    for x in 0..xsize {
                        tmp[c][x] = bg[c].as_mut()[x]
                            + fg[c].as_ref()[x] * fg[3 + alpha].as_ref()[x].clamp(0.0, 1.0);
                    }
                } else {
                    for x in 0..xsize {
                        tmp[c][x] =
                            bg[c].as_mut()[x] + fg[c].as_ref()[x] * fg[3 + alpha].as_ref()[x];
                    }
                }
            }
        }
        PatchBlendMode::AlphaWeightedAddBelow => {
            for c in 0..3 {
                if !has_alpha {
                    for x in 0..xsize {
                        tmp[c][x] = bg[c].as_mut()[x] + fg[c].as_ref()[x];
                    }
                } else if clamp {
                    for x in 0..xsize {
                        tmp[c][x] = fg[c].as_ref()[x]
                            + bg[c].as_mut()[x] * bg[3 + alpha].as_mut()[x].clamp(0.0, 1.0);
                    }
                } else {
                    for x in 0..xsize {
                        tmp[c][x] =
                            fg[c].as_ref()[x] + bg[c].as_mut()[x] * bg[3 + alpha].as_mut()[x];
                    }
                }
            }
        }
        PatchBlendMode::BlendAbove => {
            if !has_alpha {
                for c in 0..3 {
                    tmp[c].copy_from_slice(fg[c].as_ref());
                }
            } else if extra_channel_info[alpha].alpha_associated() {
                for x in 0..xsize {
                    let fa = maybe_clamp(fg[3 + alpha].as_ref()[x], clamp);
                    for c in 0..3 {
                        tmp[c][x] = fg[c].as_ref()[x] + bg[c].as_mut()[x] * (1.0 - fa);
                    }
                    tmp[3 + alpha][x] = 1.0 - (1.0 - fa) * (1.0 - bg[3 + alpha].as_mut()[x]);
                }
            } else {
                for x in 0..xsize {
                    let fa = maybe_clamp(fg[3 + alpha].as_ref()[x], clamp);
                    let new_a = 1.0 - (1.0 - fa) * (1.0 - bg[3 + alpha].as_mut()[x]);
                    let rnew_a = if new_a > 0.0 { 1.0 / new_a } else { 0.0 };
                    for c in 0..3 {
                        tmp[c][x] = (fg[c].as_ref()[x] * fa
                            + bg[c].as_mut()[x] * bg[3 + alpha].as_mut()[x] * (1.0 - fa))
                            * rnew_a;
                    }
                    tmp[3 + alpha][x] = new_a;
                }
            }
        }
        PatchBlendMode::BlendBelow => {
            if !has_alpha {
                for c in 0..3 {
                    tmp[c].copy_from_slice(bg[c].as_mut());
                }
            } else if extra_channel_info[alpha].alpha_associated() {
                for x in 0..xsize {
                    let ba = maybe_clamp(bg[3 + alpha].as_mut()[x], clamp);
                    for c in 0..3 {
                        tmp[c][x] = bg[c].as_mut()[x] + fg[c].as_ref()[x] * (1.0 - ba);
                    }
                    tmp[3 + alpha][x] = 1.0 - (1.0 - ba) * (1.0 - fg[3 + alpha].as_ref()[x]);
                }
            } else {
                for x in 0..xsize {
                    let ba = maybe_clamp(bg[3 + alpha].as_mut()[x], clamp);
                    let new_a = 1.0 - (1.0 - ba) * (1.0 - fg[3 + alpha].as_ref()[x]);
                    let rnew_a = if new_a > 0.0 { 1.0 / new_a } else { 0.0 };
                    for c in 0..3 {
                        tmp[c][x] = (bg[c].as_mut()[x] * ba
                            + fg[c].as_ref()[x] * fg[3 + alpha].as_ref()[x] * (1.0 - ba))
                            * rnew_a;
                    }
                    tmp[3 + alpha][x] = new_a;
                }
            }
        }
        PatchBlendMode::Mul => {
            for c in 0..3 {
                for x in 0..xsize {
                    tmp[c][x] = bg[c].as_mut()[x] * maybe_clamp(fg[c].as_ref()[x], clamp);
                }
            }
        }
        PatchBlendMode::Replace => {
            for c in 0..3 {
                tmp[c].copy_from_slice(fg[c].as_ref());
            }
        }
        PatchBlendMode::None => {
            for c in 0..3 {
                tmp[c].copy_from_slice(bg[c].as_mut());
            }
        }
    }
    for i in 0..(3 + num_ec) {
        bg[i].as_mut().copy_from_slice(&tmp[i]);
    }
}

#[cfg(test)]
mod tests {
    fn clamp(x: f32) -> f32 {
        x.clamp(0.0, 1.0)
    }

    mod perform_blending_tests {
        use super::{super::*, *};
        use crate::{headers::bit_depth::BitDepth, util::test::assert_all_almost_abs_eq};
        use test_log::test;

        const ABS_DELTA: f32 = 1e-6;

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

            perform_blending(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            // Expected: output color is fg color
            assert_all_almost_abs_eq(&bg_r, &fg_r, ABS_DELTA);
            assert_all_almost_abs_eq(&bg_g, &fg_g, ABS_DELTA);
            assert_all_almost_abs_eq(&bg_b, &fg_b, ABS_DELTA);
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

            perform_blending(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            assert_all_almost_abs_eq(&bg_r, &expected_r, ABS_DELTA);
            assert_all_almost_abs_eq(&bg_g, &expected_g, ABS_DELTA);
            assert_all_almost_abs_eq(&bg_b, &expected_b, ABS_DELTA);
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

            perform_blending(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            assert_all_almost_abs_eq(&bg_a, &[expected_a_val], ABS_DELTA);
            assert_all_almost_abs_eq(&bg_r, &[expected_r_val], ABS_DELTA);
            assert_all_almost_abs_eq(&bg_g, &[expected_g_val], ABS_DELTA);
            assert_all_almost_abs_eq(&bg_b, &[expected_b_val], ABS_DELTA);
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

            perform_blending(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            assert_all_almost_abs_eq(&bg_a, &[expected_a_val], ABS_DELTA);
            assert_all_almost_abs_eq(&bg_r, &[expected_r_val], ABS_DELTA);
            assert_all_almost_abs_eq(&bg_g, &[expected_g_val], ABS_DELTA);
            assert_all_almost_abs_eq(&bg_b, &[expected_b_val], ABS_DELTA);
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

            perform_blending(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            assert_all_almost_abs_eq(&bg_r, &[expected_r_val], ABS_DELTA);
            assert_all_almost_abs_eq(&bg_g, &[expected_g_val], ABS_DELTA);
            assert_all_almost_abs_eq(&bg_b, &[expected_b_val], ABS_DELTA);
            assert_all_almost_abs_eq(&bg_a, &[expected_a_val], ABS_DELTA);
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

            perform_blending(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            assert_all_almost_abs_eq(&bg_r, &expected_r, ABS_DELTA);
            assert_all_almost_abs_eq(&bg_g, &expected_g, ABS_DELTA);
            assert_all_almost_abs_eq(&bg_b, &expected_b, ABS_DELTA);
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

            perform_blending(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            // Expected Color (Replace)
            assert_all_almost_abs_eq(&bg_r, &fg_r, ABS_DELTA);
            assert_all_almost_abs_eq(&bg_g, &fg_g, ABS_DELTA);
            assert_all_almost_abs_eq(&bg_b, &fg_b, ABS_DELTA);

            assert_all_almost_abs_eq(&bg_ec1_alpha, &[expected_out_ec1_alpha], ABS_DELTA);

            assert_all_almost_abs_eq(&bg_ec0, &[expected_out_ec0], ABS_DELTA);
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

            perform_blending(
                &mut bg_channels,
                &fg_channels,
                &color_blending,
                &ec_blending,
                &extra_channel_info,
            );

            // Expected: output color is fg color due to fallback
            assert_all_almost_abs_eq(&bg_r, &fg_r, ABS_DELTA);
            assert_all_almost_abs_eq(&bg_g, &fg_g, ABS_DELTA);
            assert_all_almost_abs_eq(&bg_b, &fg_b, ABS_DELTA);
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

            perform_blending(
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
