/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

use core::simd::prelude::*;

const LANES: usize = 16;
type V = Simd<i16, LANES>;

pub fn upsample_horizontal_simd(
    input: &[i16], _in_near: &[i16], _in_far: &[i16], _scratch: &mut [i16], output: &mut [i16]
) {
    upsample_horizontal_filtered::<2, 1, 2>(input, output);
}

fn upsample_horizontal_filtered<const SHIFT: i32, const EVEN: i16, const ODD: i16>(
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

    let v_three = V::splat(3);
    let v_even_bias = V::splat(EVEN);
    let v_odd_bias = V::splat(ODD);
    let shift = i16::try_from(SHIFT).expect("upsampling shift fits i16");

    let upsample16 = |input: &[i16; 18], output: &mut [i16; 32]| {
        let v_prev = V::from_slice(&input[0..LANES]);
        let v_curr = V::from_slice(&input[1..LANES + 1]);
        let v_next = V::from_slice(&input[2..LANES + 2]);

        let v_common = v_curr * v_three;
        let v_even = (v_common + v_prev + v_even_bias) >> shift;
        let v_odd = (v_common + v_next + v_odd_bias) >> shift;

        let (v_res_1, v_res_2) = v_even.interleave(v_odd);

        v_res_1.copy_to_slice(&mut output[0..LANES]);
        v_res_2.copy_to_slice(&mut output[LANES..2 * LANES]);
    };

    for (input, output) in input
        .windows(18)
        .step_by(16)
        .zip(output[2..].chunks_exact_mut(32))
    {
        upsample16(input.try_into().unwrap(), output.try_into().unwrap());
    }

    // Upsample the remainder. This may have some overlap, but that's fine.
    if let Some(rest_input) = input.last_chunk::<18>() {
        let end = output.len() - 2;
        if let Some(rest_output) = output[..end].last_chunk_mut::<32>() {
            upsample16(rest_input, rest_output);
        }
    }

    // Last two pixels.
    output[output.len() - 2] = (3 * input[len - 1] + input[len - 2] + EVEN) >> SHIFT;
    output[output.len() - 1] = (4 * input[len - 1] + ODD) >> SHIFT;
}

pub fn upsample_vertical_simd(
    input: &[i16], in_near: &[i16], in_far: &[i16], _scratch_space: &mut [i16], output: &mut [i16]
) {
    upsample_vertical_filtered::<true>(input, in_near, in_far, output);
}

fn upsample_vertical_filtered<const ROUND: bool>(
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

    let v_three = V::splat(3);
    let v_one = V::splat(1);
    let v_two = V::splat(2);

    let upsample16 = |input: &[i16; 16],
                      in_near: &[i16; 16],
                      in_far: &[i16; 16],
                      out_top: &mut [i16; 16],
                      out_bottom: &mut [i16; 16]| {
        let v_in = V::from(*input);
        let v_near = V::from(*in_near);
        let v_far = V::from(*in_far);

        let v_common = v_in * v_three;
        let top = v_common + v_near;
        let bottom = v_common + v_far;
        let v_out_top = if ROUND { (top + v_one) >> 2 } else { top };
        let v_out_bottom = if ROUND { (bottom + v_two) >> 2 } else { bottom };

        v_out_top.copy_to_slice(out_top.as_mut_slice());
        v_out_bottom.copy_to_slice(out_bottom.as_mut_slice());
    };

    let chunks = input
        .chunks_exact(16)
        .zip(in_near.chunks_exact(16))
        .zip(in_far.chunks_exact(16))
        .zip(out_top.chunks_exact_mut(16))
        .zip(out_bottom.chunks_exact_mut(16));

    for ((((input, in_near), in_far), out_top), out_bottom) in chunks {
        upsample16(
            input.try_into().unwrap(),
            in_near.try_into().unwrap(),
            in_far.try_into().unwrap(),
            out_top.try_into().unwrap(),
            out_bottom.try_into().unwrap(),
        );
    }

    // Upsample the remainder. This may have some overlap, but that's fine.
    // Edition upgrade will fix this nested awfulness.
    if let Some(rest) = input.last_chunk::<16>() {
        if let Some( rest_near) = in_near.last_chunk::<16>() {
            if let Some( rest_far) = in_far.last_chunk::<16>() {
                if let Some( rest_top) = out_top.last_chunk_mut::<16>() {
                    if let Some(  rest_bottom) = out_bottom.last_chunk_mut::<16>() {
                        upsample16(rest, rest_near, rest_far, rest_top, rest_bottom);
                    }
                }
            }
        }
    }
}

pub fn upsample_hv_simd(
    input: &[i16], in_near: &[i16], in_far: &[i16], scratch_space: &mut [i16], output: &mut [i16]
) {
    assert_eq!(input.len() * 4, output.len());
    
    assert!(input.len() * 2 <= scratch_space.len());
    let scratch_space = &mut scratch_space[..input.len() * 2];

    upsample_vertical_filtered::<false>(input, in_near, in_far, scratch_space);

    let scratch_half = scratch_space.len() / 2;
    let output_half = output.len() / 2;

    let (scratch_top, scratch_bottom) = scratch_space.split_at_mut(scratch_half);
    let (out_top, out_bottom) = output.split_at_mut(output_half);

    upsample_horizontal_filtered::<4, 8, 7>(scratch_top, out_top);
    upsample_horizontal_filtered::<4, 8, 7>(scratch_bottom, out_bottom);
}
