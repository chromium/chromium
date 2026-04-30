use std::arch::x86_64::*;

use super::avx_vector::{AvxVector, AvxVector256};

// Treat the input like the rows of a 4x4 array, and transpose said rows to the columns
#[inline(always)]
pub unsafe fn transpose_4x4_f32(rows: [__m256; 4]) -> [__m256; 4] {
    let permute0 = _mm256_permute2f128_ps(rows[0], rows[2], 0x20);
    let permute1 = _mm256_permute2f128_ps(rows[1], rows[3], 0x20);
    let permute2 = _mm256_permute2f128_ps(rows[0], rows[2], 0x31);
    let permute3 = _mm256_permute2f128_ps(rows[1], rows[3], 0x31);

    let [unpacked0, unpacked1] = AvxVector::unpack_complex([permute0, permute1]);
    let [unpacked2, unpacked3] = AvxVector::unpack_complex([permute2, permute3]);

    [unpacked0, unpacked1, unpacked2, unpacked3]
}

// Treat the input like the rows of a 4x8 array, and transpose it to a 8x4 array, where each array of 4 is one set of 4 columns
// The assumption here is that it's very likely that the caller wants to do some more AVX operations on the columns of the transposed array, so the output is arranged to make that more convenient
// The second array only has two columns of valid data. TODO: make them __m128 instead
#[inline(always)]
pub unsafe fn transpose_4x6_to_6x4_f32(rows: [__m256; 6]) -> ([__m256; 4], [__m256; 4]) {
    let chunk0 = [rows[0], rows[1], rows[2], rows[3]];
    let chunk1 = [rows[4], rows[5], _mm256_setzero_ps(), _mm256_setzero_ps()];

    let output0 = transpose_4x4_f32(chunk0);
    let output1 = transpose_4x4_f32(chunk1);

    (output0, output1)
}

// Treat the input like the rows of a 8x4 array, and transpose it to a 4x8 array
#[inline(always)]
pub unsafe fn transpose_8x4_to_4x8_f32(rows0: [__m256; 4], rows1: [__m256; 4]) -> [__m256; 8] {
    let transposed0 = transpose_4x4_f32(rows0);
    let transposed1 = transpose_4x4_f32(rows1);

    [
        transposed0[0],
        transposed0[1],
        transposed0[2],
        transposed0[3],
        transposed1[0],
        transposed1[1],
        transposed1[2],
        transposed1[3],
    ]
}

// Treat the input like the rows of a 9x3 array, and transpose it to a 3x9 array.
// our parameters are technically 10 columns, not 9 -- we're going to discard the second element of row0
#[inline(always)]
pub unsafe fn transpose_9x3_to_3x9_emptycolumn1_f32(
    rows0: [__m128; 3],
    rows1: [__m256; 3],
    rows2: [__m256; 3],
) -> [__m256; 9] {
    // the first row of the output will be the first column of the input
    let unpacked0 = AvxVector::unpacklo_complex([rows0[0], rows0[1]]);
    let unpacked1 = AvxVector::unpacklo_complex([rows0[2], _mm_setzero_ps()]);
    let output0 = AvxVector256::merge(unpacked0, unpacked1);

    let transposed0 = transpose_4x4_f32([rows1[0], rows1[1], rows1[2], _mm256_setzero_ps()]);
    let transposed1 = transpose_4x4_f32([rows2[0], rows2[1], rows2[2], _mm256_setzero_ps()]);

    [
        output0,
        transposed0[0],
        transposed0[1],
        transposed0[2],
        transposed0[3],
        transposed1[0],
        transposed1[1],
        transposed1[2],
        transposed1[3],
    ]
}

// Treat the input like the rows of a 9x4 array, and transpose it to a 4x9 array.
// our parameters are technically 10 columns, not 9 -- we're going to discard the second element of row0
#[inline(always)]
pub unsafe fn transpose_9x4_to_4x9_emptycolumn1_f32(
    rows0: [__m128; 4],
    rows1: [__m256; 4],
    rows2: [__m256; 4],
) -> [__m256; 9] {
    // the first row of the output will be the first column of the input
    let unpacked0 = AvxVector::unpacklo_complex([rows0[0], rows0[1]]);
    let unpacked1 = AvxVector::unpacklo_complex([rows0[2], rows0[3]]);
    let output0 = AvxVector256::merge(unpacked0, unpacked1);

    let transposed0 = transpose_4x4_f32([rows1[0], rows1[1], rows1[2], rows1[3]]);
    let transposed1 = transpose_4x4_f32([rows2[0], rows2[1], rows2[2], rows2[3]]);

    [
        output0,
        transposed0[0],
        transposed0[1],
        transposed0[2],
        transposed0[3],
        transposed1[0],
        transposed1[1],
        transposed1[2],
        transposed1[3],
    ]
}

// Treat the input like the rows of a 9x4 array, and transpose it to a 4x9 array.
// our parameters are technically 10 columns, not 9 -- we're going to discard the second element of row0
#[inline(always)]
pub unsafe fn transpose_9x6_to_6x9_emptycolumn1_f32(
    rows0: [__m128; 6],
    rows1: [__m256; 6],
    rows2: [__m256; 6],
) -> ([__m256; 9], [__m128; 9]) {
    // the first row of the output will be the first column of the input
    let unpacked0 = AvxVector::unpacklo_complex([rows0[0], rows0[1]]);
    let unpacked1 = AvxVector::unpacklo_complex([rows0[2], rows0[3]]);
    let unpacked2 = AvxVector::unpacklo_complex([rows0[4], rows0[5]]);
    let output0 = AvxVector256::merge(unpacked0, unpacked1);

    let transposed_hi0 = transpose_4x4_f32([rows1[0], rows1[1], rows1[2], rows1[3]]);
    let transposed_hi1 = transpose_4x4_f32([rows2[0], rows2[1], rows2[2], rows2[3]]);

    let [unpacked_bottom0, unpacked_bottom1] = AvxVector::unpack_complex([rows1[4], rows1[5]]);
    let [unpacked_bottom2, unpacked_bottom3] = AvxVector::unpack_complex([rows2[4], rows2[5]]);

    let transposed_lo = [
        unpacked2,
        unpacked_bottom0.lo(),
        unpacked_bottom1.lo(),
        unpacked_bottom0.hi(),
        unpacked_bottom1.hi(),
        unpacked_bottom2.lo(),
        unpacked_bottom3.lo(),
        unpacked_bottom2.hi(),
        unpacked_bottom3.hi(),
    ];

    (
        [
            output0,
            transposed_hi0[0],
            transposed_hi0[1],
            transposed_hi0[2],
            transposed_hi0[3],
            transposed_hi1[0],
            transposed_hi1[1],
            transposed_hi1[2],
            transposed_hi1[3],
        ],
        transposed_lo,
    )
}

// Treat the input like the rows of a 12x4 array, and transpose it to a 4x12 array
// The assumption here is that the caller wants to do some more AVX operations on the columns of the transposed array, so the output is arranged to make that more convenient
#[inline(always)]
pub unsafe fn transpose_12x4_to_4x12_f32(
    rows0: [__m256; 4],
    rows1: [__m256; 4],
    rows2: [__m256; 4],
) -> [__m256; 12] {
    let transposed0 = transpose_4x4_f32(rows0);
    let transposed1 = transpose_4x4_f32(rows1);
    let transposed2 = transpose_4x4_f32(rows2);

    [
        transposed0[0],
        transposed0[1],
        transposed0[2],
        transposed0[3],
        transposed1[0],
        transposed1[1],
        transposed1[2],
        transposed1[3],
        transposed2[0],
        transposed2[1],
        transposed2[2],
        transposed2[3],
    ]
}

// Treat the input like the rows of a 12x6 array, and transpose it to a 6x12 array
// The assumption here is that the caller wants to do some more AVX operations on the columns of the transposed array, so the output is arranged to make that more convenient
#[inline(always)]
pub unsafe fn transpose_12x6_to_6x12_f32(
    rows0: [__m256; 6],
    rows1: [__m256; 6],
    rows2: [__m256; 6],
) -> ([__m128; 12], [__m256; 12]) {
    let [unpacked0, unpacked1] = AvxVector::unpack_complex([rows0[0], rows0[1]]);
    let [unpacked2, unpacked3] = AvxVector::unpack_complex([rows1[0], rows1[1]]);
    let [unpacked4, unpacked5] = AvxVector::unpack_complex([rows2[0], rows2[1]]);

    let output0 = [
        unpacked0.lo(),
        unpacked1.lo(),
        unpacked0.hi(),
        unpacked1.hi(),
        unpacked2.lo(),
        unpacked3.lo(),
        unpacked2.hi(),
        unpacked3.hi(),
        unpacked4.lo(),
        unpacked5.lo(),
        unpacked4.hi(),
        unpacked5.hi(),
    ];
    let transposed0 = transpose_4x4_f32([rows0[2], rows0[3], rows0[4], rows0[5]]);
    let transposed1 = transpose_4x4_f32([rows1[2], rows1[3], rows1[4], rows1[5]]);
    let transposed2 = transpose_4x4_f32([rows2[2], rows2[3], rows2[4], rows2[5]]);

    let output1 = [
        transposed0[0],
        transposed0[1],
        transposed0[2],
        transposed0[3],
        transposed1[0],
        transposed1[1],
        transposed1[2],
        transposed1[3],
        transposed2[0],
        transposed2[1],
        transposed2[2],
        transposed2[3],
    ];

    (output0, output1)
}

// Treat the input like the rows of a 8x8 array, and transpose said rows to the columns
// The assumption here is that the caller wants to do some more AVX operations on the columns of the transposed array, so the output is arranged to make that more convenient
#[inline(always)]
pub unsafe fn transpose_8x8_f32(
    rows0: [__m256; 8],
    rows1: [__m256; 8],
) -> ([__m256; 8], [__m256; 8]) {
    let chunk00 = [rows0[0], rows0[1], rows0[2], rows0[3]];
    let chunk01 = [rows0[4], rows0[5], rows0[6], rows0[7]];
    let chunk10 = [rows1[0], rows1[1], rows1[2], rows1[3]];
    let chunk11 = [rows1[4], rows1[5], rows1[6], rows1[7]];

    let transposed00 = transpose_4x4_f32(chunk00);
    let transposed01 = transpose_4x4_f32(chunk10);
    let transposed10 = transpose_4x4_f32(chunk01);
    let transposed11 = transpose_4x4_f32(chunk11);

    let output0 = [
        transposed00[0],
        transposed00[1],
        transposed00[2],
        transposed00[3],
        transposed01[0],
        transposed01[1],
        transposed01[2],
        transposed01[3],
    ];
    let output1 = [
        transposed10[0],
        transposed10[1],
        transposed10[2],
        transposed10[3],
        transposed11[0],
        transposed11[1],
        transposed11[2],
        transposed11[3],
    ];

    (output0, output1)
}
