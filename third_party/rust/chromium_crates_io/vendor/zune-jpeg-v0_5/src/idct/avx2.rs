/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

#![cfg(any(target_arch = "x86", target_arch = "x86_64"))]
//! AVX optimised IDCT.
//!
//! Okay not thaat optimised.
//!
//!
//! # The implementation
//! The implementation is neatly broken down into two operations.
//!
//! 1. Test for zeroes
//! > There is a shortcut method for idct  where when all AC values are zero, we can get the answer really quickly.
//!  by scaling the 1/8th of the DCT coefficient of the block to the whole block and level shifting.
//!
//! 2. If above fails, we proceed to carry out IDCT as a two pass one dimensional algorithm.
//! IT does two whole scans where it carries out IDCT on all items
//! After each successive scan, data is transposed in register(thank you x86 SIMD powers). and the second
//! pass is carried out.
//!
//! The code is not super optimized, it produces bit identical results with scalar code hence it's
//! `mm256_add_epi16`
//! and it also has the advantage of making this implementation easy to maintain.

#![cfg(feature = "x86")]
#![allow(dead_code)]

#[cfg(target_arch = "x86")]
use core::arch::x86::*;
#[cfg(target_arch = "x86_64")]
use core::arch::x86_64::*;

use super::{CONST_BITS, OUTPUT_SHIFT, PASS1_ROUND, PASS1_SHIFT, SCALE_BITS};
use crate::unsafe_utils::{transpose, YmmRegister};

const fn pair_i16(lo: i16, hi: i16) -> i32 {
    let lo = lo.to_le_bytes();
    let hi = hi.to_le_bytes();
    i32::from_le_bytes([lo[0], lo[1], hi[0], hi[1]])
}

const C_EVEN_T2: i32 = pair_i16(4433, -10704);
const C_EVEN_T3: i32 = pair_i16(10703, 4433);
const C_T0_13: i32 = pair_i16(2260, -6436);
const C_T0_57: i32 = pair_i16(9633, -11363);
const C_T1_13: i32 = pair_i16(6437, -11362);
const C_T1_57: i32 = pair_i16(2261, 9633);
const C_T2_13: i32 = pair_i16(9633, -2259);
const C_T2_57: i32 = pair_i16(-11362, -6436);
const C_T3_13: i32 = pair_i16(11363, 9633);
const C_T3_57: i32 = pair_i16(6437, 2260);

macro_rules! rows_fit_i16 {
    ($($row:expr),+ $(,)?) => {{
        let bias = _mm256_set1_epi32(0x8000);
        let mut outside = _mm256_setzero_si256();
        $(
            outside = _mm256_or_si256(outside, _mm256_add_epi32($row, bias));
        )+
        _mm256_testz_si256(outside, _mm256_set1_epi32(-65536)) == 1
    }};
}

#[inline]
#[target_feature(enable = "avx2")]
#[allow(clippy::too_many_arguments)]
unsafe fn transpose_and_store(
    v0: __m256i, v1: __m256i, v2: __m256i, v3: __m256i, v4: __m256i, v5: __m256i, v6: __m256i,
    v7: __m256i, out_ptr: *mut i16, stride: usize
) {
    let t0 = _mm256_unpacklo_epi32(v0, v1);
    let t1 = _mm256_unpackhi_epi32(v0, v1);
    let t2 = _mm256_unpacklo_epi32(v2, v3);
    let t3 = _mm256_unpackhi_epi32(v2, v3);
    let t4 = _mm256_unpacklo_epi32(v4, v5);
    let t5 = _mm256_unpackhi_epi32(v4, v5);
    let t6 = _mm256_unpacklo_epi32(v6, v7);
    let t7 = _mm256_unpackhi_epi32(v6, v7);

    let x0 = _mm256_unpacklo_epi64(t0, t2);
    let x1 = _mm256_unpackhi_epi64(t0, t2);
    let x2 = _mm256_unpacklo_epi64(t1, t3);
    let x3 = _mm256_unpackhi_epi64(t1, t3);
    let x4 = _mm256_unpacklo_epi64(t4, t6);
    let x5 = _mm256_unpackhi_epi64(t4, t6);
    let x6 = _mm256_unpacklo_epi64(t5, t7);
    let x7 = _mm256_unpackhi_epi64(t5, t7);

    let p04 = clamp_avx(_mm256_packs_epi32(x0, x4));
    let p15 = clamp_avx(_mm256_packs_epi32(x1, x5));
    let p26 = clamp_avx(_mm256_packs_epi32(x2, x6));
    let p37 = clamp_avx(_mm256_packs_epi32(x3, x7));

    _mm_storeu_si128(out_ptr.cast(), _mm256_castsi256_si128(p04));
    _mm_storeu_si128(out_ptr.add(stride).cast(), _mm256_castsi256_si128(p15));
    _mm_storeu_si128(out_ptr.add(2 * stride).cast(), _mm256_castsi256_si128(p26));
    _mm_storeu_si128(out_ptr.add(3 * stride).cast(), _mm256_castsi256_si128(p37));
    _mm_storeu_si128(
        out_ptr.add(4 * stride).cast(),
        _mm256_extracti128_si256::<1>(p04)
    );
    _mm_storeu_si128(
        out_ptr.add(5 * stride).cast(),
        _mm256_extracti128_si256::<1>(p15)
    );
    _mm_storeu_si128(
        out_ptr.add(6 * stride).cast(),
        _mm256_extracti128_si256::<1>(p26)
    );
    _mm_storeu_si128(
        out_ptr.add(7 * stride).cast(),
        _mm256_extracti128_si256::<1>(p37)
    );
}

#[target_feature(enable = "avx2")]
pub unsafe fn idct_avx2_1x1(in_vector: &mut [i32; 64], out_vector: &mut [i16], stride: usize) {
    assert!(stride >= 8 && stride <= out_vector.len().saturating_sub(8) / 7);
    let coeff = ((in_vector[0].wrapping_add(1028)) >> 3).clamp(0, 255) as i16;
    let idct_value = _mm_set1_epi16(coeff);
    let out_ptr = out_vector.as_mut_ptr();
    let mut pos = 0;
    for _ in 0..8 {
        _mm_storeu_si128(out_ptr.add(pos).cast(), idct_value);
        pos += stride;
    }
}

#[target_feature(enable = "avx2")]
#[allow(
    clippy::too_many_lines,
    clippy::cast_possible_truncation,
    clippy::similar_names,
    clippy::op_ref,
    unused_assignments,
    clippy::zero_prefixed_literal
)]
pub unsafe fn idct_avx2(in_vector: &mut [i32; 64], out_vector: &mut [i16], stride: usize) {
    assert!(stride >= 8 && stride <= out_vector.len().saturating_sub(8) / 7);
    let out_ptr = out_vector.as_mut_ptr();
    let mut pos = 0;

    let rw0 = _mm256_loadu_si256(in_vector[00..].as_ptr().cast());
    let rw1 = _mm256_loadu_si256(in_vector[08..].as_ptr().cast());
    let rw2 = _mm256_loadu_si256(in_vector[16..].as_ptr().cast());
    let rw3 = _mm256_loadu_si256(in_vector[24..].as_ptr().cast());
    let rw4 = _mm256_loadu_si256(in_vector[32..].as_ptr().cast());
    let rw5 = _mm256_loadu_si256(in_vector[40..].as_ptr().cast());
    let rw6 = _mm256_loadu_si256(in_vector[48..].as_ptr().cast());
    let rw7 = _mm256_loadu_si256(in_vector[56..].as_ptr().cast());

    let rw8 = _mm256_blend_epi32::<0xFE>(_mm256_setzero_si256(), rw0);

    let mut bitmap = _mm256_or_si256(rw1, rw2);
    bitmap = _mm256_or_si256(bitmap, rw3);
    bitmap = _mm256_or_si256(bitmap, rw4);
    bitmap = _mm256_or_si256(bitmap, rw5);
    bitmap = _mm256_or_si256(bitmap, rw6);
    bitmap = _mm256_or_si256(bitmap, rw7);
    bitmap = _mm256_or_si256(bitmap, rw8);

    if _mm256_testz_si256(bitmap, bitmap) == 1 {
        let coeff = ((in_vector[0].wrapping_add(1028)) >> 3).clamp(0, 255) as i16;
        let idct_value = _mm_set1_epi16(coeff);
        for _ in 0..8 {
            _mm_storeu_si128(out_ptr.add(pos).cast(), idct_value);
            pos += stride;
        }
        return;
    }

    idct_avx2_core(rw0, rw1, rw2, rw3, rw4, rw5, rw6, rw7, out_ptr, stride);
}

#[target_feature(enable = "avx2")]
pub unsafe fn idct_avx2_nonzero(in_vector: &mut [i32; 64], out_vector: &mut [i16], stride: usize) {
    assert!(stride >= 8 && stride <= out_vector.len().saturating_sub(8) / 7);
    let out_ptr = out_vector.as_mut_ptr();
    let rw0 = _mm256_loadu_si256(in_vector.as_ptr().cast());
    let rw1 = _mm256_loadu_si256(in_vector[8..].as_ptr().cast());
    let rw2 = _mm256_loadu_si256(in_vector[16..].as_ptr().cast());
    let rw3 = _mm256_loadu_si256(in_vector[24..].as_ptr().cast());
    let rw4 = _mm256_loadu_si256(in_vector[32..].as_ptr().cast());
    let rw5 = _mm256_loadu_si256(in_vector[40..].as_ptr().cast());
    let rw6 = _mm256_loadu_si256(in_vector[48..].as_ptr().cast());
    let rw7 = _mm256_loadu_si256(in_vector[56..].as_ptr().cast());
    idct_avx2_core(rw0, rw1, rw2, rw3, rw4, rw5, rw6, rw7, out_ptr, stride);
}

#[inline]
#[target_feature(enable = "avx2")]
#[allow(clippy::too_many_arguments, clippy::too_many_lines)]
unsafe fn idct_avx2_core(
    rw0: __m256i, rw1: __m256i, rw2: __m256i, rw3: __m256i, rw4: __m256i, rw5: __m256i,
    rw6: __m256i, rw7: __m256i, out_ptr: *mut i16, stride: usize
) {
    let mut row0 = YmmRegister { mm256: rw0 };
    let mut row1 = YmmRegister { mm256: rw1 };
    let mut row2 = YmmRegister { mm256: rw2 };
    let mut row3 = YmmRegister { mm256: rw3 };

    let mut row4 = YmmRegister { mm256: rw4 };
    let mut row5 = YmmRegister { mm256: rw5 };
    let mut row6 = YmmRegister { mm256: rw6 };
    let mut row7 = YmmRegister { mm256: rw7 };

    macro_rules! dct_pass_madd {
        ($SCALE_BITS:tt,$scale:tt) => {
            let r26 = _mm256_blend_epi16::<0xAA>(row2.mm256, _mm256_slli_epi32::<16>(row6.mm256));
            let even_t2 = _mm256_madd_epi16(r26, _mm256_set1_epi32(C_EVEN_T2));
            let even_t3 = _mm256_madd_epi16(r26, _mm256_set1_epi32(C_EVEN_T3));

            let sum04 = _mm256_slli_epi32(_mm256_add_epi32(row0.mm256, row4.mm256), CONST_BITS);
            let dif04 = _mm256_slli_epi32(_mm256_sub_epi32(row0.mm256, row4.mm256), CONST_BITS);
            let round = _mm256_set1_epi32($SCALE_BITS);
            let t0_r = _mm256_add_epi32(sum04, round);
            let t1_r = _mm256_add_epi32(dif04, round);

            let x0 = _mm256_add_epi32(t0_r, even_t3);
            let x3 = _mm256_sub_epi32(t0_r, even_t3);
            let x1 = _mm256_add_epi32(t1_r, even_t2);
            let x2 = _mm256_sub_epi32(t1_r, even_t2);

            let r13 = _mm256_blend_epi16::<0xAA>(row1.mm256, _mm256_slli_epi32::<16>(row3.mm256));
            let r57 = _mm256_blend_epi16::<0xAA>(row5.mm256, _mm256_slli_epi32::<16>(row7.mm256));

            let t0 = _mm256_add_epi32(
                _mm256_madd_epi16(r13, _mm256_set1_epi32(C_T0_13)),
                _mm256_madd_epi16(r57, _mm256_set1_epi32(C_T0_57)),
            );
            let t1 = _mm256_add_epi32(
                _mm256_madd_epi16(r13, _mm256_set1_epi32(C_T1_13)),
                _mm256_madd_epi16(r57, _mm256_set1_epi32(C_T1_57)),
            );
            let t2 = _mm256_add_epi32(
                _mm256_madd_epi16(r13, _mm256_set1_epi32(C_T2_13)),
                _mm256_madd_epi16(r57, _mm256_set1_epi32(C_T2_57)),
            );
            let t3 = _mm256_add_epi32(
                _mm256_madd_epi16(r13, _mm256_set1_epi32(C_T3_13)),
                _mm256_madd_epi16(r57, _mm256_set1_epi32(C_T3_57)),
            );

            row0.mm256 = _mm256_srai_epi32(_mm256_add_epi32(x0, t3), $scale);
            row1.mm256 = _mm256_srai_epi32(_mm256_add_epi32(x1, t2), $scale);
            row2.mm256 = _mm256_srai_epi32(_mm256_add_epi32(x2, t1), $scale);
            row3.mm256 = _mm256_srai_epi32(_mm256_add_epi32(x3, t0), $scale);

            row4.mm256 = _mm256_srai_epi32(_mm256_sub_epi32(x3, t0), $scale);
            row5.mm256 = _mm256_srai_epi32(_mm256_sub_epi32(x2, t1), $scale);
            row6.mm256 = _mm256_srai_epi32(_mm256_sub_epi32(x1, t2), $scale);
            row7.mm256 = _mm256_srai_epi32(_mm256_sub_epi32(x0, t3), $scale);
        };
    }

    macro_rules! dct_pass_i32 {
        ($SCALE_BITS:tt,$scale:tt) => {
            let p1 = (row2 + row6) * 4433;
            let tmp2 = p1 + row6 * -15137;
            let tmp3 = p1 + row2 * 6270;

            let tmp0 = YmmRegister { mm256: _mm256_slli_epi32((row0 + row4).mm256, CONST_BITS) } + $SCALE_BITS;
            let tmp1 = YmmRegister { mm256: _mm256_slli_epi32((row0 - row4).mm256, CONST_BITS) } + $SCALE_BITS;

            let x0 = tmp0 + tmp3;
            let x3 = tmp0 - tmp3;
            let x1 = tmp1 + tmp2;
            let x2 = tmp1 - tmp2;

            let p1 = row7 + row1;
            let p2 = row5 + row3;
            let p3 = row7 + row3;
            let p4 = row5 + row1;
            let p5 = (p3 + p4) * 9633;

            let tmp0 = row7 * 2446 + p1 * -7373 + (p3 * -16069 + p5);
            let tmp1 = row5 * 16819 + p2 * -20995 + (p4 * -3196 + p5);
            let tmp2 = row3 * 25172 + p2 * -20995 + (p3 * -16069 + p5);
            let tmp3 = row1 * 12299 + p1 * -7373 + (p4 * -3196 + p5);

            row0.mm256 = _mm256_srai_epi32((x0 + tmp3).mm256, $scale);
            row7.mm256 = _mm256_srai_epi32((x0 - tmp3).mm256, $scale);
            row1.mm256 = _mm256_srai_epi32((x1 + tmp2).mm256, $scale);
            row6.mm256 = _mm256_srai_epi32((x1 - tmp2).mm256, $scale);
            row2.mm256 = _mm256_srai_epi32((x2 + tmp1).mm256, $scale);
            row5.mm256 = _mm256_srai_epi32((x2 - tmp1).mm256, $scale);
            row3.mm256 = _mm256_srai_epi32((x3 + tmp0).mm256, $scale);
            row4.mm256 = _mm256_srai_epi32((x3 - tmp0).mm256, $scale);
        };
    }

    // Dequantized coefficients are i32, including for baseline JPEGs with large tables.
    // Narrow only when every lane fits; otherwise retain wrapping i32 arithmetic.
    if rows_fit_i16!(rw0, rw1, rw2, rw3, rw4, rw5, rw6, rw7,) {
        dct_pass_madd!(PASS1_ROUND, PASS1_SHIFT);
    } else {
        dct_pass_i32!(PASS1_ROUND, PASS1_SHIFT);
    }
    transpose(
        &mut row0, &mut row1, &mut row2, &mut row3, &mut row4, &mut row5, &mut row6, &mut row7,
    );

    if rows_fit_i16!(
        row0.mm256, row1.mm256, row2.mm256, row3.mm256, row4.mm256, row5.mm256, row6.mm256,
        row7.mm256,
    ) {
        dct_pass_madd!(SCALE_BITS, OUTPUT_SHIFT);
    } else {
        dct_pass_i32!(SCALE_BITS, OUTPUT_SHIFT);
    }
    transpose_and_store(
        row0.mm256, row1.mm256, row2.mm256, row3.mm256, row4.mm256, row5.mm256, row6.mm256,
        row7.mm256, out_ptr, stride
    );
}


#[target_feature(enable = "avx2")]
#[allow(
    clippy::too_many_lines,
    clippy::cast_possible_truncation,
    clippy::similar_names,
    clippy::op_ref,
    unused_assignments,
    clippy::zero_prefixed_literal
)]
pub unsafe fn idct_avx2_4x4(in_vector: &mut [i32; 64], out_vector: &mut [i16], stride: usize) {
    assert!(stride >= 8 && stride <= out_vector.len().saturating_sub(8) / 7);
    let out_ptr = out_vector.as_mut_ptr();
    let rw0 = _mm256_loadu_si256(in_vector[00..].as_ptr().cast());
    let rw1 = _mm256_loadu_si256(in_vector[08..].as_ptr().cast());
    let rw2 = _mm256_loadu_si256(in_vector[16..].as_ptr().cast());
    let rw3 = _mm256_loadu_si256(in_vector[24..].as_ptr().cast());

    if !rows_fit_i16!(rw0, rw1, rw2, rw3) {
        return idct_avx2_nonzero(in_vector, out_vector, stride);
    }

    let mut row0 = YmmRegister { mm256: rw0 };
    let mut row1 = YmmRegister { mm256: rw1 };
    let mut row2 = YmmRegister { mm256: rw2 };
    let mut row3 = YmmRegister { mm256: rw3 };

    let mut row4 = YmmRegister { mm256: rw0 };
    let mut row5 = YmmRegister { mm256: rw0 };
    let mut row6 = YmmRegister { mm256: rw0 };
    let mut row7 = YmmRegister { mm256: rw0 };

    macro_rules! dct_pass_4x4 {
        ($SCALE_BITS:tt, $scale:tt) => {
            let t0_r = _mm256_add_epi32(
                _mm256_slli_epi32(row0.mm256, CONST_BITS),
                _mm256_set1_epi32($SCALE_BITS),
            );
            let r2_low = _mm256_and_si256(row2.mm256, _mm256_set1_epi32(0xFFFF));
            let p1 = _mm256_madd_epi16(r2_low, _mm256_set1_epi32(pair_i16(4433, 0)));
            let p3 = _mm256_madd_epi16(r2_low, _mm256_set1_epi32(pair_i16(10703, 0)));

            let x0 = _mm256_add_epi32(t0_r, p3);
            let x1 = _mm256_add_epi32(t0_r, p1);
            let x2 = _mm256_sub_epi32(t0_r, p1);
            let x3 = _mm256_sub_epi32(t0_r, p3);

            let r13 = _mm256_blend_epi16::<0xAA>(row1.mm256, _mm256_slli_epi32::<16>(row3.mm256));
            let t0 = _mm256_madd_epi16(r13, _mm256_set1_epi32(C_T0_13));
            let t1 = _mm256_madd_epi16(r13, _mm256_set1_epi32(C_T1_13));
            let t2 = _mm256_madd_epi16(r13, _mm256_set1_epi32(C_T2_13));
            let t3 = _mm256_madd_epi16(r13, _mm256_set1_epi32(C_T3_13));

            row0.mm256 = _mm256_srai_epi32(_mm256_add_epi32(x0, t3), $scale);
            row1.mm256 = _mm256_srai_epi32(_mm256_add_epi32(x1, t2), $scale);
            row2.mm256 = _mm256_srai_epi32(_mm256_add_epi32(x2, t1), $scale);
            row3.mm256 = _mm256_srai_epi32(_mm256_add_epi32(x3, t0), $scale);
            row4.mm256 = _mm256_srai_epi32(_mm256_sub_epi32(x3, t0), $scale);
            row5.mm256 = _mm256_srai_epi32(_mm256_sub_epi32(x2, t1), $scale);
            row6.mm256 = _mm256_srai_epi32(_mm256_sub_epi32(x1, t2), $scale);
            row7.mm256 = _mm256_srai_epi32(_mm256_sub_epi32(x0, t3), $scale);
        };
    }

    dct_pass_4x4!(PASS1_ROUND, PASS1_SHIFT);

    {
        let t0 = _mm256_unpacklo_epi32(row0.mm256, row1.mm256);
        let t1 = _mm256_unpackhi_epi32(row0.mm256, row1.mm256);
        let t2 = _mm256_unpacklo_epi32(row2.mm256, row3.mm256);
        let t3 = _mm256_unpackhi_epi32(row2.mm256, row3.mm256);
        let t4 = _mm256_unpacklo_epi32(row4.mm256, row5.mm256);
        let t5 = _mm256_unpackhi_epi32(row4.mm256, row5.mm256);
        let t6 = _mm256_unpacklo_epi32(row6.mm256, row7.mm256);
        let t7 = _mm256_unpackhi_epi32(row6.mm256, row7.mm256);

        let x0 = _mm256_unpacklo_epi64(t0, t2);
        let x1 = _mm256_unpackhi_epi64(t0, t2);
        let x2 = _mm256_unpacklo_epi64(t1, t3);
        let x3 = _mm256_unpackhi_epi64(t1, t3);
        let x4 = _mm256_unpacklo_epi64(t4, t6);
        let x5 = _mm256_unpackhi_epi64(t4, t6);
        let x6 = _mm256_unpacklo_epi64(t5, t7);
        let x7 = _mm256_unpackhi_epi64(t5, t7);

        row0.mm256 = _mm256_permute2x128_si256::<0x20>(x0, x4);
        row1.mm256 = _mm256_permute2x128_si256::<0x20>(x1, x5);
        row2.mm256 = _mm256_permute2x128_si256::<0x20>(x2, x6);
        row3.mm256 = _mm256_permute2x128_si256::<0x20>(x3, x7);
    }

    if rows_fit_i16!(row0.mm256, row1.mm256, row2.mm256, row3.mm256) {
        dct_pass_4x4!(SCALE_BITS, OUTPUT_SHIFT);
    } else {
        let i2 = row2;
        let i0 = row0;
        row0.mm256 = _mm256_slli_epi32(i0.mm256, CONST_BITS);
        let t0 = row0 + SCALE_BITS;
        let t2 = i2 * 4433;
        let t3 = i2 * 10703;
        let x0 = t0 + t3;
        let x3 = t0 - t3;
        let x1 = t0 + t2;
        let x2 = t0 - t2;
        let i4 = row3;
        let i3 = row1;
        let p5 = (i4 + i3) * 9633;
        let p1 = p5 + i3 * -7373;
        let p2 = p5 + i4 * -20995;
        let t3 = p5 + i3 * 1730;
        let t2 = p5 + i4 * -11892;
        let t1 = p2 + i3 * -3196;
        let t0 = p1 + i4 * -16069;
        row0.mm256 = _mm256_srai_epi32((x0 + t3).mm256, OUTPUT_SHIFT);
        row1.mm256 = _mm256_srai_epi32((x1 + t2).mm256, OUTPUT_SHIFT);
        row2.mm256 = _mm256_srai_epi32((x2 + t1).mm256, OUTPUT_SHIFT);
        row3.mm256 = _mm256_srai_epi32((x3 + t0).mm256, OUTPUT_SHIFT);
        row4.mm256 = _mm256_srai_epi32((x3 - t0).mm256, OUTPUT_SHIFT);
        row5.mm256 = _mm256_srai_epi32((x2 - t1).mm256, OUTPUT_SHIFT);
        row6.mm256 = _mm256_srai_epi32((x1 - t2).mm256, OUTPUT_SHIFT);
        row7.mm256 = _mm256_srai_epi32((x0 - t3).mm256, OUTPUT_SHIFT);
    }
    transpose_and_store(
        row0.mm256, row1.mm256, row2.mm256, row3.mm256, row4.mm256, row5.mm256, row6.mm256,
        row7.mm256, out_ptr, stride
    );
}

#[inline]
#[target_feature(enable = "avx2")]
unsafe fn clamp_avx(reg: __m256i) -> __m256i {
    let min_s = _mm256_set1_epi16(0);
    let max_s = _mm256_set1_epi16(255);

    let max_v = _mm256_max_epi16(reg, min_s); //max(a,0)
    let min_v = _mm256_min_epi16(max_v, max_s); //min(max(a,0),255)
    return min_v;
}

/// A copy of `_MM_SHUFFLE()` that doesn't require
/// a nightly compiler
#[inline]
const fn shuffle(z: i32, y: i32, x: i32, w: i32) -> i32 {
    ((z << 6) | (y << 4) | (x << 2) | w)
}
