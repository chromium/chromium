/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

pub fn upsample_horizontal(
    input: &[i16], _ref: &[i16], _in_near: &[i16], _scratch: &mut [i16], output: &mut [i16]
) {
    upsample_horizontal_filtered::<2, 1, 2>(input, output);
}

pub(super) fn upsample_horizontal_filtered<const SHIFT: i32, const EVEN: i16, const ODD: i16>(
    input: &[i16], output: &mut [i16]
) {
    assert_eq!(input.len() * 2, output.len());
    assert!(input.len() > 2);
    output[0] = (4 * input[0] + EVEN) >> SHIFT;
    output[1] = (3 * input[0] + input[1] + ODD) >> SHIFT;
    for (out, samples) in output[2..].chunks_exact_mut(2).zip(input.windows(3)) {
        let current = 3 * samples[1];
        out[0] = (current + samples[0] + EVEN) >> SHIFT;
        out[1] = (current + samples[2] + ODD) >> SHIFT;
    }
    let last = input.len() - 1;
    let end = output.len() - 2;
    output[end] = (3 * input[last] + input[last - 1] + EVEN) >> SHIFT;
    output[end + 1] = (4 * input[last] + ODD) >> SHIFT;
}

pub fn upsample_vertical(
    input: &[i16], in_near: &[i16], in_far: &[i16], _scratch_space: &mut [i16], output: &mut [i16]
) {
    upsample_vertical_filtered::<true>(input, in_near, in_far, output);
}

pub(super) fn upsample_vertical_filtered<const ROUND: bool>(
    input: &[i16], in_near: &[i16], in_far: &[i16], output: &mut [i16]
) {
    assert_eq!(input.len() * 2, output.len());
    assert_eq!(in_near.len(), input.len());
    assert_eq!(in_far.len(), input.len());

    let middle = output.len() / 2;

    let (out_top, out_bottom) = output.split_at_mut(middle);

    for ((current, near), pixel) in input.iter().zip(in_near).zip(out_top) {
        let sum = 3 * current + near;
        *pixel = if ROUND { (sum + 1) >> 2 } else { sum };
    }
    for ((current, far), pixel) in input.iter().zip(in_far).zip(out_bottom) {
        let sum = 3 * current + far;
        *pixel = if ROUND { (sum + 2) >> 2 } else { sum };
    }
}

pub fn upsample_hv(
    input: &[i16], in_near: &[i16], in_far: &[i16], scratch_space: &mut [i16], output: &mut [i16]
) {
    assert_eq!(input.len() * 4, output.len());

    assert!(input.len() * 2 <= scratch_space.len());
    let scratch_space = &mut scratch_space[..input.len() * 2];


    // Keep vertical sums unrounded until the combined 2-D filter is complete.
    upsample_vertical_filtered::<false>(input, in_near, in_far, scratch_space);
    let scratch_half = scratch_space.len() / 2;

    let output_half = output.len() / 2;

    upsample_horizontal_filtered::<4, 8, 7>(
        &scratch_space[..scratch_half],
        &mut output[..output_half]
    );
    upsample_horizontal_filtered::<4, 8, 7>(
        &scratch_space[scratch_half..],
        &mut output[output_half..]
    );
}

pub fn upsample_generic(
    input: &[i16], _in_near: &[i16], _in_far: &[i16], _scratch_space: &mut [i16],
    output: &mut [i16]
) {
    // use nearest sample
    let difference = output.len() / input.len();
    if difference > 0 {
        // nearest neighbour
        for (input, chunk_output) in input.iter().zip(output.chunks_exact_mut(difference)) {
            for x in chunk_output.iter_mut() { *x = *input; }
        }
    }
}
