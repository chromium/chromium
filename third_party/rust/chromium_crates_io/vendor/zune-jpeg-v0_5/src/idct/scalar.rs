/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

//! Platform independent IDCT algorithm
//!
//! Not as fast as AVX one.

use super::{CONST_BITS, OUTPUT_SHIFT, PASS1_ROUND, PASS1_SHIFT, SCALE_BITS};

#[inline(always)]
fn wa(a: i32, b: i32) -> i32 {
    a.wrapping_add(b)
}

#[inline(always)]
fn ws(a: i32, b: i32) -> i32 {
    a.wrapping_sub(b)
}

#[inline(always)]
fn wm(a: i32, b: i32) -> i32 {
    a.wrapping_mul(b)
}

#[inline]
pub fn idct_int_1x1(in_vector: &mut [i32; 64], mut out_vector: &mut [i16], stride: usize) {
    let coeff = ((wa(wa(in_vector[0], 4), 1024) >> 3).clamp(0, 255)) as i16;

    out_vector[..8].fill(coeff);
    for _ in 0..7 {
        out_vector = &mut out_vector[stride..];
        out_vector[..8].fill(coeff);

    }
}

#[allow(unused_assignments)]
#[allow(
    clippy::too_many_lines,
    clippy::op_ref,
    clippy::cast_possible_truncation
)]
pub fn idct_int(in_vector: &mut [i32; 64], out_vector: &mut [i16], stride: usize) {
    let mut pos = 0;
    let mut i = 0;

    if &in_vector[1..] == &[0_i32; 63] {
        return idct_int_1x1(in_vector, out_vector, stride);
    }

    // vertical pass
    for ptr in 0..8 {
        let p2 = in_vector[ptr + 16];
        let p3 = in_vector[ptr + 48];

        let p1 = wm(wa(p2, p3), 4433);

        let t2 = wa(p1, wm(p3, -15137));
        let t3 = wa(p1, wm(p2, 6270));

        let p2 = in_vector[ptr];
        let p3 = in_vector[32 + ptr];

        let t0 = fsh(wa(p2, p3));
        let t1 = fsh(ws(p2, p3));

        let x0 = wa(wa(t0, t3), PASS1_ROUND);
        let x3 = wa(ws(t0, t3), PASS1_ROUND);
        let x1 = wa(wa(t1, t2), PASS1_ROUND);
        let x2 = wa(ws(t1, t2), PASS1_ROUND);

        let mut t0 = in_vector[ptr + 56];
        let mut t1 = in_vector[ptr + 40];
        let mut t2 = in_vector[ptr + 24];
        let mut t3 = in_vector[ptr + 8];

        let p3 = wa(t0, t2);
        let p4 = wa(t1, t3);
        let p1 = wa(t0, t3);
        let p2 = wa(t1, t2);
        let p5 = wm(wa(p3, p4), 9633);

        t0 = wm(t0, 2446);
        t1 = wm(t1, 16819);
        t2 = wm(t2, 25172);
        t3 = wm(t3, 12299);

        let p1 = wa(p5, wm(p1, -7373));
        let p2 = wa(p5, wm(p2, -20995));
        let p3 = wm(p3, -16069);
        let p4 = wm(p4, -3196);

        t3 = wa(t3, wa(p1, p4));
        t2 = wa(t2, wa(p2, p3));
        t1 = wa(t1, wa(p2, p4));
        t0 = wa(t0, wa(p1, p3));

        in_vector[ptr] = wa(x0, t3) >> PASS1_SHIFT;
        in_vector[ptr + 8] = wa(x1, t2) >> PASS1_SHIFT;
        in_vector[ptr + 16] = wa(x2, t1) >> PASS1_SHIFT;
        in_vector[ptr + 24] = wa(x3, t0) >> PASS1_SHIFT;
        in_vector[ptr + 32] = ws(x3, t0) >> PASS1_SHIFT;
        in_vector[ptr + 40] = ws(x2, t1) >> PASS1_SHIFT;
        in_vector[ptr + 48] = ws(x1, t2) >> PASS1_SHIFT;
        in_vector[ptr + 56] = ws(x0, t3) >> PASS1_SHIFT;
    }

    // horizontal pass
    while i < 64 {
        let p2 = in_vector[i + 2];
        let p3 = in_vector[i + 6];

        let p1 = wm(wa(p2, p3), 4433);
        let t2 = wa(p1, wm(p3, -15137));
        let t3 = wa(p1, wm(p2, 6270));

        let p2 = in_vector[i];
        let p3 = in_vector[i + 4];

        let t0 = fsh(wa(p2, p3));
        let t1 = fsh(ws(p2, p3));

        let x0 = wa(wa(t0, t3), SCALE_BITS);
        let x3 = wa(ws(t0, t3), SCALE_BITS);
        let x1 = wa(wa(t1, t2), SCALE_BITS);
        let x2 = wa(ws(t1, t2), SCALE_BITS);

        let mut t0 = in_vector[i + 7];
        let mut t1 = in_vector[i + 5];
        let mut t2 = in_vector[i + 3];
        let mut t3 = in_vector[i + 1];

        let p3 = wa(t0, t2);
        let p4 = wa(t1, t3);
        let p1 = wa(t0, t3);
        let p2 = wa(t1, t2);
        let p5 = wm(wa(p3, p4), 9633);

        t0 = wm(t0, 2446);
        t1 = wm(t1, 16819);
        t2 = wm(t2, 25172);
        t3 = wm(t3, 12299);

        let p1 = wa(p5, wm(p1, -7373));
        let p2 = wa(p5, wm(p2, -20995));
        let p3 = wm(p3, -16069);
        let p4 = wm(p4, -3196);

        t3 = wa(t3, wa(p1, p4));
        t2 = wa(t2, wa(p2, p3));
        t1 = wa(t1, wa(p2, p4));
        t0 = wa(t0, wa(p1, p3));

        // to prevent some bad images from crashing
        let mut tmp = [0; 8];

        let out: &mut [i16; 8] = out_vector
            .get_mut(pos..pos + 8)
            .unwrap_or(&mut tmp)
            .try_into()
            .unwrap();

        out[0] = clamp(wa(x0, t3) >> OUTPUT_SHIFT);
        out[1] = clamp(wa(x1, t2) >> OUTPUT_SHIFT);
        out[2] = clamp(wa(x2, t1) >> OUTPUT_SHIFT);
        out[3] = clamp(wa(x3, t0) >> OUTPUT_SHIFT);
        out[4] = clamp(ws(x3, t0) >> OUTPUT_SHIFT);
        out[5] = clamp(ws(x2, t1) >> OUTPUT_SHIFT);
        out[6] = clamp(ws(x1, t2) >> OUTPUT_SHIFT);
        out[7] = clamp(ws(x0, t3) >> OUTPUT_SHIFT);

        i += 8;
        pos += stride;
    }
}

#[inline]
/// Scale a number to the IDCT constant precision.
fn fsh(x: i32) -> i32 {
    x << CONST_BITS
}

/// Clamp values between 0 and 255
#[inline]
#[allow(clippy::cast_possible_truncation)]
fn clamp(a: i32) -> i16 {
    a.clamp(0, 255) as i16
}

/// IDCT assuming only the upper 4x4 is filled.
pub fn idct4x4(in_vector: &mut [i32; 64], out_vector: &mut [i16], stride: usize) {
    let mut pos = 0;

    // vertical pass
    for ptr in 0..4 {
        let i0 = wa(fsh(in_vector[ptr]), PASS1_ROUND);
        let i2 = in_vector[ptr + 16];

        let p1 = wm(i2, 4433);
        let p3 = wm(i2, 10703);

        let x0 = wa(i0, p3);
        let x1 = wa(i0, p1);
        let x2 = ws(i0, p1);
        let x3 = ws(i0, p3);

        // odd part
        let i4 = in_vector[ptr + 24];
        let i3 = in_vector[ptr + 8];

        let p5 = wm(wa(i4, i3), 9633);

        let p1 = wa(p5, wm(i3, -7373));
        let p2 = wa(p5, wm(i4, -20995));

        let t3 = wa(p5, wm(i3, 1730));
        let t2 = wa(p5, wm(i4, -11892));

        let t1 = wa(p2, wm(i3, -3196));
        let t0 = wa(p1, wm(i4, -16069));

        in_vector[ptr] = wa(x0, t3) >> PASS1_SHIFT;
        in_vector[ptr + 8] = wa(x1, t2) >> PASS1_SHIFT;
        in_vector[ptr + 16] = wa(x2, t1) >> PASS1_SHIFT;
        in_vector[ptr + 24] = wa(x3, t0) >> PASS1_SHIFT;
        in_vector[ptr + 32] = ws(x3, t0) >> PASS1_SHIFT;
        in_vector[ptr + 40] = ws(x2, t1) >> PASS1_SHIFT;
        in_vector[ptr + 48] = ws(x1, t2) >> PASS1_SHIFT;
        in_vector[ptr + 56] = ws(x0, t3) >> PASS1_SHIFT;
    }

    // horizontal pass
    for i in (0..8).map(|i| 8 * i) {
        let i2 = in_vector[i + 2];
        let i0 = in_vector[i];

        let t0 = wa(fsh(i0), SCALE_BITS);
        let t2 = wm(i2, 4433);
        let t3 = wm(i2, 10703);

        let x0 = wa(t0, t3);
        let x3 = ws(t0, t3);
        let x1 = wa(t0, t2);
        let x2 = ws(t0, t2);

        // odd part
        let i3 = in_vector[i + 3];
        let i1 = in_vector[i + 1];

        let p5 = wm(wa(i3, i1), 9633);

        let p1 = wa(p5, wm(i1, -7373));
        let p2 = wa(p5, wm(i3, -20995));

        let t3 = wa(p5, wm(i1, 1730));
        let t2 = wa(p5, wm(i3, -11892));

        let t1 = wa(p2, wm(i1, -3196));
        let t0 = wa(p1, wm(i3, -16069));

        // to prevent some bad images from crashing
        let mut tmp = [0; 8];

        let out: &mut [i16; 8] = out_vector
            .get_mut(pos..pos + 8)
            .unwrap_or(&mut tmp)
            .try_into()
            .unwrap();

        out.copy_from_slice(&[
            clamp(wa(x0, t3) >> OUTPUT_SHIFT),
            clamp(wa(x1, t2) >> OUTPUT_SHIFT),
            clamp(wa(x2, t1) >> OUTPUT_SHIFT),
            clamp(wa(x3, t0) >> OUTPUT_SHIFT),
            clamp(ws(x3, t0) >> OUTPUT_SHIFT),
            clamp(ws(x2, t1) >> OUTPUT_SHIFT),
            clamp(ws(x1, t2) >> OUTPUT_SHIFT),
            clamp(ws(x0, t3) >> OUTPUT_SHIFT)
        ]);

        pos += stride;
    }

    in_vector[32..36].fill(0);
    in_vector[40..44].fill(0);
    in_vector[48..52].fill(0);
    in_vector[56..60].fill(0);
}
