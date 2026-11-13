/*
 * Copyright (c) 2025.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

#[cfg(target_arch = "x86")]
use core::arch::x86::*;
#[cfg(target_arch = "x86_64")]
use core::arch::x86_64::*;

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[target_feature(enable = "avx2")]
pub unsafe fn upsample_horizontal_avx2(
    input: &[i16], _in_near: &[i16], _in_far: &[i16], _scratch: &mut [i16], output: &mut [i16]
) {
    unsafe { upsample_horizontal_filtered::<2, 1, 2>(input, output) }
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[target_feature(enable = "avx2")]
unsafe fn upsample_horizontal_filtered<const SHIFT: i32, const EVEN: i16, const ODD: i16>(
    input: &[i16], output: &mut [i16]
) {
    assert_eq!(input.len() * 2, output.len());
    assert!(input.len() > 2);

    let len = input.len();

    if len < 18 {
        return super::scalar::upsample_horizontal_filtered::<SHIFT, EVEN, ODD>(input, output);
    }

    // First two pixels
    output[0] = (4 * input[0] + EVEN) >> SHIFT;
    output[1] = (input[0] * 3 + input[1] + ODD) >> SHIFT;

    let v_three = _mm256_set1_epi16(3);
    let v_even_bias = _mm256_set1_epi16(EVEN);
    let v_odd_bias = _mm256_set1_epi16(ODD);

    let in_ptr = input.as_ptr();
    let out_ptr = unsafe { output.as_mut_ptr().add(2) };
    let mut v_prev_vec = unsafe { _mm256_loadu_si256(in_ptr.cast()) };
    let mut pos = 0usize;

    while pos + 32 <= len {
        let v_next_vec = unsafe { _mm256_loadu_si256(in_ptr.add(pos + 16).cast()) };
        let v_mid = _mm256_permute2x128_si256::<0x21>(v_prev_vec, v_next_vec);
        let v_prev = v_prev_vec;
        let v_curr = _mm256_alignr_epi8::<2>(v_mid, v_prev_vec);
        let v_next = _mm256_alignr_epi8::<4>(v_mid, v_prev_vec);
        v_prev_vec = v_next_vec;

        let v_common = _mm256_mullo_epi16(v_curr, v_three);
        let v_even = _mm256_srai_epi16::<SHIFT>(
            _mm256_add_epi16(_mm256_add_epi16(v_common, v_prev), v_even_bias));
        let v_odd = _mm256_srai_epi16::<SHIFT>(
            _mm256_add_epi16(_mm256_add_epi16(v_common, v_next), v_odd_bias));

        let v_res_1 = _mm256_unpacklo_epi16(v_even, v_odd);
        let v_res_2 = _mm256_unpackhi_epi16(v_even, v_odd);

        let v_final_1 = _mm256_permute2x128_si256(v_res_1, v_res_2, 0x20);
        let v_final_2 = _mm256_permute2x128_si256(v_res_1, v_res_2, 0x31);

        unsafe {
            _mm256_storeu_si256(out_ptr.add(pos * 2).cast(), v_final_1);
            _mm256_storeu_si256(out_ptr.add(pos * 2 + 16).cast(), v_final_2);
        }
        pos += 16;
    }

    let upsample_tail = |last_pos: usize| {
        let v_prev = unsafe { _mm256_loadu_si256(in_ptr.add(last_pos).cast()) };
        let v_curr = unsafe { _mm256_loadu_si256(in_ptr.add(last_pos + 1).cast()) };
        let v_next = unsafe { _mm256_loadu_si256(in_ptr.add(last_pos + 2).cast()) };

        let v_common = _mm256_mullo_epi16(v_curr, v_three);
        let v_even = _mm256_srai_epi16::<SHIFT>(
            _mm256_add_epi16(_mm256_add_epi16(v_common, v_prev), v_even_bias));
        let v_odd = _mm256_srai_epi16::<SHIFT>(
            _mm256_add_epi16(_mm256_add_epi16(v_common, v_next), v_odd_bias));

        let v_res_1 = _mm256_unpacklo_epi16(v_even, v_odd);
        let v_res_2 = _mm256_unpackhi_epi16(v_even, v_odd);

        let v_final_1 = _mm256_permute2x128_si256(v_res_1, v_res_2, 0x20);
        let v_final_2 = _mm256_permute2x128_si256(v_res_1, v_res_2, 0x31);

        unsafe {
            _mm256_storeu_si256(out_ptr.add(last_pos * 2).cast(), v_final_1);
            _mm256_storeu_si256(out_ptr.add(last_pos * 2 + 16).cast(), v_final_2);
        }
    };
    let last_pos = len - 18;
    if pos < last_pos {
        upsample_tail(pos);
    }
    upsample_tail(last_pos);

    // Last two pixels.
    output[output.len() - 2] = (3 * input[len - 1] + input[len - 2] + EVEN) >> SHIFT;
    output[output.len() - 1] = (4 * input[len - 1] + ODD) >> SHIFT;
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[target_feature(enable = "avx2")]
pub unsafe fn upsample_vertical_avx2(
    input: &[i16], in_near: &[i16], in_far: &[i16], _scratch: &mut [i16], output: &mut [i16]
) {
    unsafe { upsample_vertical_filtered::<true>(input, in_near, in_far, output) }
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[target_feature(enable = "avx2")]
unsafe fn upsample_vertical_filtered<const ROUND: bool>(
    input: &[i16], in_near: &[i16], in_far: &[i16], output: &mut [i16]
) {
    assert_eq!(input.len() * 2, output.len());
    assert_eq!(in_near.len(), input.len());
    assert_eq!(in_far.len(), input.len());

    let len = input.len();

    if len < 16 {
        return super::scalar::upsample_vertical_filtered::<ROUND>(input, in_near, in_far, output);
    }

    let middle = output.len() / 2;
    let (out_top, out_bottom) = output.split_at_mut(middle);

    let v_three = _mm256_set1_epi16(3);
    let v_one = _mm256_set1_epi16(1);
    let v_two = _mm256_set1_epi16(2);

    let in_ptr = input.as_ptr();
    let near_ptr = in_near.as_ptr();
    let far_ptr = in_far.as_ptr();
    let top_ptr = out_top.as_mut_ptr();
    let bot_ptr = out_bottom.as_mut_ptr();

    let mut pos = 0usize;
    while pos + 16 <= len {
        let v_in = unsafe { _mm256_loadu_si256(in_ptr.add(pos).cast()) };
        let v_near = unsafe { _mm256_loadu_si256(near_ptr.add(pos).cast()) };
        let v_far = unsafe { _mm256_loadu_si256(far_ptr.add(pos).cast()) };

        let v_common = _mm256_mullo_epi16(v_in, v_three);
        let top = _mm256_add_epi16(v_common, v_near);
        let bottom = _mm256_add_epi16(v_common, v_far);
        let v_out_top = if ROUND {
            _mm256_srai_epi16::<2>(_mm256_add_epi16(top, v_one))
        } else { top };
        let v_out_bottom = if ROUND {
            _mm256_srai_epi16::<2>(_mm256_add_epi16(bottom, v_two))
        } else { bottom };

        unsafe {
            _mm256_storeu_si256(top_ptr.add(pos).cast(), v_out_top);
            _mm256_storeu_si256(bot_ptr.add(pos).cast(), v_out_bottom);
        }
        pos += 16;
    }

    if pos < len {
        let last = len - 16;
        let v_in = unsafe { _mm256_loadu_si256(in_ptr.add(last).cast()) };
        let v_near = unsafe { _mm256_loadu_si256(near_ptr.add(last).cast()) };
        let v_far = unsafe { _mm256_loadu_si256(far_ptr.add(last).cast()) };

        let v_common = _mm256_mullo_epi16(v_in, v_three);
        let top = _mm256_add_epi16(v_common, v_near);
        let bottom = _mm256_add_epi16(v_common, v_far);
        let v_out_top = if ROUND {
            _mm256_srai_epi16::<2>(_mm256_add_epi16(top, v_one))
        } else { top };
        let v_out_bottom = if ROUND {
            _mm256_srai_epi16::<2>(_mm256_add_epi16(bottom, v_two))
        } else { bottom };

        unsafe {
            _mm256_storeu_si256(top_ptr.add(last).cast(), v_out_top);
            _mm256_storeu_si256(bot_ptr.add(last).cast(), v_out_bottom);
        }
    }
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[target_feature(enable = "avx2")]
pub unsafe fn upsample_hv_avx2(
    input: &[i16], in_near: &[i16], in_far: &[i16], scratch_space: &mut [i16], output: &mut [i16]
) {
    assert_eq!(input.len() * 4, output.len());
    assert_eq!(in_near.len(), input.len());
    assert_eq!(in_far.len(), input.len());

    let len = input.len();
    if len < 32 {
        assert!(len * 2 <= scratch_space.len());
        let scratch_space = &mut scratch_space[..len * 2];
        upsample_vertical_filtered::<false>(input, in_near, in_far, scratch_space);
        let (scratch_top, scratch_bottom) = scratch_space.split_at_mut(len);
        let (out_top, out_bottom) = output.split_at_mut(len * 2);
        upsample_horizontal_filtered::<4, 8, 7>(scratch_top, out_top);
        upsample_horizontal_filtered::<4, 8, 7>(scratch_bottom, out_bottom);
        return;
    }

    let (out_top, out_bottom) = output.split_at_mut(len * 2);
    let top0 = input[0] * 3 + in_near[0];
    let top1 = input[1] * 3 + in_near[1];
    out_top[0] = (top0 * 4 + 8) >> 4;
    out_top[1] = (top0 * 3 + top1 + 7) >> 4;

    let bot0 = input[0] * 3 + in_far[0];
    let bot1 = input[1] * 3 + in_far[1];
    out_bottom[0] = (bot0 * 4 + 8) >> 4;
    out_bottom[1] = (bot0 * 3 + bot1 + 7) >> 4;

    let v_three = _mm256_set1_epi16(3);
    // Preserve the existing combined filter's single final rounding step.
    let v_even_bias = _mm256_set1_epi16(8);
    let v_odd_bias = _mm256_set1_epi16(7);

    let in_ptr = input.as_ptr();
    let near_ptr = in_near.as_ptr();
    let far_ptr = in_far.as_ptr();
    let top_out_ptr = unsafe { out_top.as_mut_ptr().add(2) };
    let bot_out_ptr = unsafe { out_bottom.as_mut_ptr().add(2) };

    let v_in0 = unsafe { _mm256_loadu_si256(in_ptr.cast()) };
    let v_near0 = unsafe { _mm256_loadu_si256(near_ptr.cast()) };
    let v_far0 = unsafe { _mm256_loadu_si256(far_ptr.cast()) };
    let v_com0 = _mm256_mullo_epi16(v_in0, v_three);
    let mut prev_top = _mm256_add_epi16(v_com0, v_near0);
    let mut prev_bot = _mm256_add_epi16(v_com0, v_far0);

    let mut pos = 0usize;
    while pos + 32 <= len {
        let v_in1 = unsafe { _mm256_loadu_si256(in_ptr.add(pos + 16).cast()) };
        let v_near1 = unsafe { _mm256_loadu_si256(near_ptr.add(pos + 16).cast()) };
        let v_far1 = unsafe { _mm256_loadu_si256(far_ptr.add(pos + 16).cast()) };
        let v_com1 = _mm256_mullo_epi16(v_in1, v_three);
        let next_top = _mm256_add_epi16(v_com1, v_near1);
        let next_bot = _mm256_add_epi16(v_com1, v_far1);

        let mid_top = _mm256_permute2x128_si256::<0x21>(prev_top, next_top);
        let curr_top = _mm256_alignr_epi8::<2>(mid_top, prev_top);
        let nxt_top = _mm256_alignr_epi8::<4>(mid_top, prev_top);
        let com_top = _mm256_mullo_epi16(curr_top, v_three);
        let even_top = _mm256_srai_epi16::<4>(
            _mm256_add_epi16(_mm256_add_epi16(com_top, prev_top), v_even_bias));
        let odd_top = _mm256_srai_epi16::<4>(
            _mm256_add_epi16(_mm256_add_epi16(com_top, nxt_top), v_odd_bias));
        let res1_top = _mm256_unpacklo_epi16(even_top, odd_top);
        let res2_top = _mm256_unpackhi_epi16(even_top, odd_top);
        unsafe {
            _mm256_storeu_si256(
                top_out_ptr.add(pos * 2).cast(),
                _mm256_permute2x128_si256(res1_top, res2_top, 0x20)
            );
            _mm256_storeu_si256(
                top_out_ptr.add(pos * 2 + 16).cast(),
                _mm256_permute2x128_si256(res1_top, res2_top, 0x31)
            );
        }

        let mid_bot = _mm256_permute2x128_si256::<0x21>(prev_bot, next_bot);
        let curr_bot = _mm256_alignr_epi8::<2>(mid_bot, prev_bot);
        let nxt_bot = _mm256_alignr_epi8::<4>(mid_bot, prev_bot);
        let com_bot = _mm256_mullo_epi16(curr_bot, v_three);
        let even_bot = _mm256_srai_epi16::<4>(
            _mm256_add_epi16(_mm256_add_epi16(com_bot, prev_bot), v_even_bias));
        let odd_bot = _mm256_srai_epi16::<4>(
            _mm256_add_epi16(_mm256_add_epi16(com_bot, nxt_bot), v_odd_bias));
        let res1_bot = _mm256_unpacklo_epi16(even_bot, odd_bot);
        let res2_bot = _mm256_unpackhi_epi16(even_bot, odd_bot);
        unsafe {
            _mm256_storeu_si256(
                bot_out_ptr.add(pos * 2).cast(),
                _mm256_permute2x128_si256(res1_bot, res2_bot, 0x20)
            );
            _mm256_storeu_si256(
                bot_out_ptr.add(pos * 2 + 16).cast(),
                _mm256_permute2x128_si256(res1_bot, res2_bot, 0x31)
            );
        }

        prev_top = next_top;
        prev_bot = next_bot;
        pos += 16;
    }

    let upsample_tail = |last_pos: usize| {
        let vp_in = unsafe { _mm256_loadu_si256(in_ptr.add(last_pos).cast()) };
        let vp_near = unsafe { _mm256_loadu_si256(near_ptr.add(last_pos).cast()) };
        let vp_far = unsafe { _mm256_loadu_si256(far_ptr.add(last_pos).cast()) };
        let vp_com = _mm256_mullo_epi16(vp_in, v_three);
        let vp_top = _mm256_add_epi16(vp_com, vp_near);
        let vp_bot = _mm256_add_epi16(vp_com, vp_far);

        let vc_in = unsafe { _mm256_loadu_si256(in_ptr.add(last_pos + 1).cast()) };
        let vc_near = unsafe { _mm256_loadu_si256(near_ptr.add(last_pos + 1).cast()) };
        let vc_far = unsafe { _mm256_loadu_si256(far_ptr.add(last_pos + 1).cast()) };
        let vc_com = _mm256_mullo_epi16(vc_in, v_three);
        let vc_top = _mm256_add_epi16(vc_com, vc_near);
        let vc_bot = _mm256_add_epi16(vc_com, vc_far);

        let vn_in = unsafe { _mm256_loadu_si256(in_ptr.add(last_pos + 2).cast()) };
        let vn_near = unsafe { _mm256_loadu_si256(near_ptr.add(last_pos + 2).cast()) };
        let vn_far = unsafe { _mm256_loadu_si256(far_ptr.add(last_pos + 2).cast()) };
        let vn_com = _mm256_mullo_epi16(vn_in, v_three);
        let vn_top = _mm256_add_epi16(vn_com, vn_near);
        let vn_bot = _mm256_add_epi16(vn_com, vn_far);

        let com_top = _mm256_mullo_epi16(vc_top, v_three);
        let even_top = _mm256_srai_epi16::<4>(
            _mm256_add_epi16(_mm256_add_epi16(com_top, vp_top), v_even_bias));
        let odd_top = _mm256_srai_epi16::<4>(
            _mm256_add_epi16(_mm256_add_epi16(com_top, vn_top), v_odd_bias));
        let res1_top = _mm256_unpacklo_epi16(even_top, odd_top);
        let res2_top = _mm256_unpackhi_epi16(even_top, odd_top);
        unsafe {
            _mm256_storeu_si256(
                top_out_ptr.add(last_pos * 2).cast(),
                _mm256_permute2x128_si256(res1_top, res2_top, 0x20)
            );
            _mm256_storeu_si256(
                top_out_ptr.add(last_pos * 2 + 16).cast(),
                _mm256_permute2x128_si256(res1_top, res2_top, 0x31)
            );
        }

        let com_bot = _mm256_mullo_epi16(vc_bot, v_three);
        let even_bot = _mm256_srai_epi16::<4>(
            _mm256_add_epi16(_mm256_add_epi16(com_bot, vp_bot), v_even_bias));
        let odd_bot = _mm256_srai_epi16::<4>(
            _mm256_add_epi16(_mm256_add_epi16(com_bot, vn_bot), v_odd_bias));
        let res1_bot = _mm256_unpacklo_epi16(even_bot, odd_bot);
        let res2_bot = _mm256_unpackhi_epi16(even_bot, odd_bot);
        unsafe {
            _mm256_storeu_si256(
                bot_out_ptr.add(last_pos * 2).cast(),
                _mm256_permute2x128_si256(res1_bot, res2_bot, 0x20)
            );
            _mm256_storeu_si256(
                bot_out_ptr.add(last_pos * 2 + 16).cast(),
                _mm256_permute2x128_si256(res1_bot, res2_bot, 0x31)
            );
        }
    };
    let last_pos = len - 18;
    // A non-aligned width can leave one full window between the sliding loop and the tail.
    if pos < last_pos {
        upsample_tail(pos);
    }
    upsample_tail(last_pos);

    let top_last = input[len - 1] * 3 + in_near[len - 1];
    let top_prev = input[len - 2] * 3 + in_near[len - 2];
    out_top[len * 2 - 2] = (3 * top_last + top_prev + 8) >> 4;
    out_top[len * 2 - 1] = (4 * top_last + 7) >> 4;

    let bot_last = input[len - 1] * 3 + in_far[len - 1];
    let bot_prev = input[len - 2] * 3 + in_far[len - 2];
    out_bottom[len * 2 - 2] = (3 * bot_last + bot_prev + 8) >> 4;
    out_bottom[len * 2 - 1] = (4 * bot_last + 7) >> 4;
}

#[cfg(all(test, feature = "std"))]
mod tests {
    use super::*;
    use crate::upsampler::scalar;

    #[test]
    fn sliding_windows_and_fused_hv_match_scalar() {
        if !is_x86_feature_detected!("avx2") {
            return;
        }
        for len in (3..=160).chain([256, 1280]) {
            for pattern in 0..4 {
                let row = |seed| -> Vec<i16> {
                    (0..len)
                        .map(|x| match pattern {
                            0 => 0,
                            1 => 255,
                            2 => {
                                if (x + seed) % 2 == 0 {
                                    0
                                } else {
                                    255
                                }
                            }
                            _ => ((x * 79 + seed * 53) % 256) as i16
                        })
                        .collect()
                };
                let input = row(0);
                let near = row(1);
                let far = row(2);
                for kind in 0..3 {
                    let output_len = len * if kind == 2 { 4 } else { 2 };
                    let mut actual = vec![-1234; output_len + 17];
                    let mut expected = actual.clone();
                    let mut scratch = vec![0; len * 2];
                    let out = &mut actual[5..5 + output_len];
                    let reference = &mut expected[5..5 + output_len];
                    unsafe {
                        match kind {
                            0 => {
                                scalar::upsample_horizontal(
                                    &input,
                                    &near,
                                    &far,
                                    &mut scratch,
                                    reference
                                );
                                upsample_horizontal_avx2(&input, &near, &far, &mut [], out);
                            }
                            1 => {
                                scalar::upsample_vertical(
                                    &input,
                                    &near,
                                    &far,
                                    &mut scratch,
                                    reference
                                );
                                upsample_vertical_avx2(&input, &near, &far, &mut [], out);
                            }
                            _ => {
                                scalar::upsample_hv(&input, &near, &far, &mut scratch, reference);
                                if len >= 32 {
                                    upsample_hv_avx2(&input, &near, &far, &mut [], out);
                                } else {
                                    upsample_hv_avx2(&input, &near, &far, &mut scratch, out);
                                }
                            }
                        }
                    }
                    assert_eq!(
                        actual, expected,
                        "len {len}, pattern {pattern}, kind {kind}"
                    );
                }
            }
        }
    }
}
