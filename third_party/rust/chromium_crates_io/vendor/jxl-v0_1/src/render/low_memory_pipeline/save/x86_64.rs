// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(unsafe_code)]

use std::{
    arch::x86_64::{
        __m256i, __m512i, _mm256_add_epi32, _mm256_blend_epi32, _mm256_loadu_si256,
        _mm256_permutevar8x32_epi32, _mm256_set1_epi32, _mm256_storeu_si256, _mm512_loadu_si512,
        _mm512_mask_permutevar_epi32, _mm512_permutex2var_epi32, _mm512_storeu_si512,
    },
    mem::MaybeUninit,
};

#[target_feature(enable = "avx2")]
fn load_avx2_u32(data: &[u32; 8]) -> __m256i {
    // SAFETY: `data` has the correct size.
    unsafe { _mm256_loadu_si256(data.as_ptr() as *const _) }
}

#[target_feature(enable = "avx2")]
fn load_avx2(data: &[u8; 32]) -> __m256i {
    // SAFETY: `data` has the correct size.
    unsafe { _mm256_loadu_si256(data.as_ptr() as *const _) }
}

#[target_feature(enable = "avx2")]
fn store_avx2(data: __m256i, out: &mut [MaybeUninit<u8>; 32]) {
    // SAFETY: `data` has the correct size.
    unsafe { _mm256_storeu_si256(out.as_mut_ptr() as *mut _, data) }
}

#[target_feature(enable = "avx512f")]
fn load_avx512_u32(data: &[u32; 16]) -> __m512i {
    // SAFETY: `data` has the correct size.
    unsafe { _mm512_loadu_si512(data.as_ptr() as *const _) }
}

#[target_feature(enable = "avx512f")]
fn load_avx512(data: &[u8; 64]) -> __m512i {
    // SAFETY: `data` has the correct size.
    unsafe { _mm512_loadu_si512(data.as_ptr() as *const _) }
}

#[target_feature(enable = "avx512f")]
fn store_avx512(data: __m512i, out: &mut [MaybeUninit<u8>; 64]) {
    // SAFETY: `data` has the correct size.
    unsafe { _mm512_storeu_si512(out.as_mut_ptr() as *mut _, data) }
}

#[target_feature(enable = "avx2")]
fn interleave3_32b_avx2(inp: &[&[u8]; 3], out: &mut [MaybeUninit<u8>]) -> usize {
    let [a, b, c] = inp;

    let idx_a0 = load_avx2_u32(&[0, 0, 0, 1, 0, 0, 2, 0]);
    // c1 = idx_a0 + 2
    // b2 = idx_a0 + 5

    let idx_b0 = load_avx2_u32(&[0, 0, 0, 0, 1, 0, 0, 2]);
    // a1 = idx_b0 + 3
    // c2 = idx_b0 + 5

    let idx_c0 = load_avx2_u32(&[0, 0, 0, 0, 0, 1, 0, 0]);
    // b1 = idx_c0 + 3
    // a2 = idx_c0 + 6

    let two = _mm256_set1_epi32(2);
    let three = _mm256_set1_epi32(3);
    let five = _mm256_set1_epi32(5);
    let six = _mm256_set1_epi32(6);

    const LEN: usize = 32;
    let mut processed = 0;
    for (((a, b), c), out) in a
        .chunks_exact(LEN)
        .zip(b.chunks_exact(LEN))
        .zip(c.chunks_exact(LEN))
        .zip(out.chunks_exact_mut(LEN * 3))
    {
        let a = load_avx2(a.try_into().unwrap());
        let b = load_avx2(b.try_into().unwrap());
        let c = load_avx2(c.try_into().unwrap());

        let a0 = _mm256_permutevar8x32_epi32(a, idx_a0);
        let b0 = _mm256_permutevar8x32_epi32(b, idx_b0);
        let c0 = _mm256_permutevar8x32_epi32(c, idx_c0);
        let out0 = _mm256_blend_epi32::<0b10010010>(a0, b0);
        let out0 = _mm256_blend_epi32::<0b00100100>(out0, c0);

        let a1 = _mm256_permutevar8x32_epi32(a, _mm256_add_epi32(idx_b0, three));
        let b1 = _mm256_permutevar8x32_epi32(b, _mm256_add_epi32(idx_c0, three));
        let c1 = _mm256_permutevar8x32_epi32(c, _mm256_add_epi32(idx_a0, two));
        let out1 = _mm256_blend_epi32::<0b00100100>(a1, b1);
        let out1 = _mm256_blend_epi32::<0b01001001>(out1, c1);

        let a2 = _mm256_permutevar8x32_epi32(a, _mm256_add_epi32(idx_c0, six));
        let b2 = _mm256_permutevar8x32_epi32(b, _mm256_add_epi32(idx_a0, five));
        let c2 = _mm256_permutevar8x32_epi32(c, _mm256_add_epi32(idx_b0, five));
        let out2 = _mm256_blend_epi32::<0b01001001>(a2, b2);
        let out2 = _mm256_blend_epi32::<0b10010010>(out2, c2);

        store_avx2(out0, (&mut out[0..LEN]).try_into().unwrap());
        store_avx2(out1, (&mut out[LEN..2 * LEN]).try_into().unwrap());
        store_avx2(out2, (&mut out[2 * LEN..3 * LEN]).try_into().unwrap());
        processed += LEN / 4;
    }

    processed
}

#[inline(never)]
#[target_feature(enable = "avx512f")]
fn interleave3_32b_avx512(inp: &[&[u8]; 3], out: &mut [MaybeUninit<u8>]) -> usize {
    let [a, b, c] = inp;

    let idx_ab0 = load_avx512_u32(&[0, 16, 0, 1, 17, 0, 2, 18, 0, 3, 19, 0, 4, 20, 0, 5]);
    let idx_c0 = load_avx512_u32(&[0, 0, 0, 0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0]);

    let idx_ab1 = load_avx512_u32(&[21, 0, 6, 22, 0, 7, 23, 0, 8, 24, 0, 9, 25, 0, 10, 26]);
    let idx_c1 = load_avx512_u32(&[0, 5, 0, 0, 6, 0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0]);

    let idx_ab2 = load_avx512_u32(&[0, 11, 27, 0, 12, 28, 0, 13, 29, 0, 14, 30, 0, 15, 31, 0]);
    let idx_c2 = load_avx512_u32(&[10, 0, 0, 11, 0, 0, 12, 0, 0, 13, 0, 0, 14, 0, 0, 15]);

    const LEN: usize = 64;
    let mut processed = 0;
    for (((a, b), c), out) in a
        .chunks_exact(LEN)
        .zip(b.chunks_exact(LEN))
        .zip(c.chunks_exact(LEN))
        .zip(out.chunks_exact_mut(LEN * 3))
    {
        let a = load_avx512(a.try_into().unwrap());
        let b = load_avx512(b.try_into().unwrap());
        let c = load_avx512(c.try_into().unwrap());

        let out0 = _mm512_permutex2var_epi32(a, idx_ab0, b);
        let out0 = _mm512_mask_permutevar_epi32(out0, 0b0100100100100100, idx_c0, c);

        let out1 = _mm512_permutex2var_epi32(a, idx_ab1, b);
        let out1 = _mm512_mask_permutevar_epi32(out1, 0b0010010010010010, idx_c1, c);

        let out2 = _mm512_permutex2var_epi32(a, idx_ab2, b);
        let out2 = _mm512_mask_permutevar_epi32(out2, 0b1001001001001001, idx_c2, c);

        store_avx512(out0, (&mut out[0..LEN]).try_into().unwrap());
        store_avx512(out1, (&mut out[LEN..2 * LEN]).try_into().unwrap());
        store_avx512(out2, (&mut out[2 * LEN..3 * LEN]).try_into().unwrap());
        processed += LEN / 4;
    }

    processed
}

/// Safety note: does not write uninit data in `out`.
pub(super) fn interleave3_32b(inp: &[&[u8]; 3], out: &mut [MaybeUninit<u8>]) -> usize {
    if is_x86_feature_detected!("avx512f") {
        // SAFETY: we just checked for avx512f.
        unsafe { interleave3_32b_avx512(inp, out) }
    } else if is_x86_feature_detected!("avx2") {
        // SAFETY: we just checked for avx2.
        unsafe { interleave3_32b_avx2(inp, out) }
    } else {
        0
    }
}
