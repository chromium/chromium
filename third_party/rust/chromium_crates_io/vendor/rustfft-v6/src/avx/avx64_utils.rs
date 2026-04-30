use std::arch::x86_64::*;

// Treat the input like the rows of a 2x2 array, and transpose said rows to the columns
#[inline(always)]
pub unsafe fn transpose_2x2_f64(rows: [__m256d; 2]) -> [__m256d; 2] {
    let col0 = _mm256_permute2f128_pd(rows[0], rows[1], 0x20);
    let col1 = _mm256_permute2f128_pd(rows[0], rows[1], 0x31);

    [col0, col1]
}

// Treat the input like the rows of a 4x2 array, and transpose it to a 2x4 array
#[inline(always)]
pub unsafe fn transpose_4x2_to_2x4_f64(rows0: [__m256d; 2], rows1: [__m256d; 2]) -> [__m256d; 4] {
    let output00 = transpose_2x2_f64(rows0);
    let output01 = transpose_2x2_f64(rows1);

    [output00[0], output00[1], output01[0], output01[1]]
}

// Treat the input like the rows of a 3x3 array, and transpose it
// The assumption here is that it's very likely that the caller wants to do some more AVX operations on the columns of the transposed array, so the output is arranged to make that more convenient
#[inline(always)]
pub unsafe fn transpose_3x3_f64(
    rows0: [__m128d; 3],
    rows1: [__m256d; 3],
) -> ([__m128d; 3], [__m256d; 3]) {
    // the first column of output will be made up of the first row of input
    let output0 = [
        rows0[0],
        _mm256_castpd256_pd128(rows1[0]),
        _mm256_extractf128_pd(rows1[0], 1),
    ];

    // the second column of output will be made of the second 2 rows of input
    let output10 = _mm256_permute2f128_pd(
        _mm256_castpd128_pd256(rows0[1]),
        _mm256_castpd128_pd256(rows0[2]),
        0x20,
    );
    let lower_chunk = [rows1[1], rows1[2]];
    let lower_transposed = transpose_2x2_f64(lower_chunk);
    let output1 = [output10, lower_transposed[0], lower_transposed[1]];

    (output0, output1)
}

// Treat the input like the rows of a 3x4 array, and transpose it to a 4x3 array
// The assumption here is that the caller wants to do some more AVX operations on the columns of the transposed array, so the output is arranged to make that more convenient
#[inline(always)]
pub unsafe fn transpose_3x4_to_4x3_f64(
    rows0: [__m128d; 4],
    rows1: [__m256d; 4],
) -> ([__m256d; 3], [__m256d; 3]) {
    // the top row of each output array will come from the first column, and the second 2 rows will come from 2x2 transposing the rows1 array
    let merged0 = _mm256_permute2f128_pd(
        _mm256_castpd128_pd256(rows0[0]),
        _mm256_castpd128_pd256(rows0[1]),
        0x20,
    );
    let merged1 = _mm256_permute2f128_pd(
        _mm256_castpd128_pd256(rows0[2]),
        _mm256_castpd128_pd256(rows0[3]),
        0x20,
    );

    let chunk0 = [rows1[0], rows1[1]];
    let chunk1 = [rows1[2], rows1[3]];

    let lower0 = transpose_2x2_f64(chunk0);
    let lower1 = transpose_2x2_f64(chunk1);

    (
        [merged0, lower0[0], lower0[1]],
        [merged1, lower1[0], lower1[1]],
    )
}

// Treat the input like the rows of a 3x6 array, and transpose it to a 6x3 array
// The assumption here is that caller wants to do some more AVX operations on the columns of the transposed array, so the output is arranged to make that more convenient
#[inline(always)]
pub unsafe fn transpose_3x6_to_6x3_f64(
    rows0: [__m128d; 6],
    rows1: [__m256d; 6],
) -> ([__m256d; 3], [__m256d; 3], [__m256d; 3]) {
    let chunk0 = [rows1[0], rows1[1]];
    let chunk1 = [rows1[2], rows1[3]];
    let chunk2 = [rows1[4], rows1[5]];

    let transposed0 = transpose_2x2_f64(chunk0);
    let transposed1 = transpose_2x2_f64(chunk1);
    let transposed2 = transpose_2x2_f64(chunk2);

    let output0 = [
        _mm256_insertf128_pd(_mm256_castpd128_pd256(rows0[0]), rows0[1], 1),
        transposed0[0],
        transposed0[1],
    ];
    let output1 = [
        _mm256_insertf128_pd(_mm256_castpd128_pd256(rows0[2]), rows0[3], 1),
        transposed1[0],
        transposed1[1],
    ];
    let output2 = [
        _mm256_insertf128_pd(_mm256_castpd128_pd256(rows0[4]), rows0[5], 1),
        transposed2[0],
        transposed2[1],
    ];

    (output0, output1, output2)
}

// Treat the input like the rows of a 9x3 array, and transpose it to a 3x9 array
// The assumption here is that caller wants to do some more AVX operations on the columns of the transposed array, so the output is arranged to make that more convenient
#[inline(always)]
pub unsafe fn transpose_9x3_to_3x9_f64(
    rows0: [__m128d; 3],
    rows1: [__m256d; 3],
    rows2: [__m256d; 3],
    rows3: [__m256d; 3],
    rows4: [__m256d; 3],
) -> ([__m128d; 9], [__m256d; 9]) {
    let chunk1 = [rows1[1], rows1[2]];
    let chunk2 = [rows2[1], rows2[2]];
    let chunk3 = [rows3[1], rows3[2]];
    let chunk4 = [rows4[1], rows4[2]];

    let transposed1 = transpose_2x2_f64(chunk1);
    let transposed2 = transpose_2x2_f64(chunk2);
    let transposed3 = transpose_2x2_f64(chunk3);
    let transposed4 = transpose_2x2_f64(chunk4);

    let output0 = [
        rows0[0],
        _mm256_castpd256_pd128(rows1[0]),
        _mm256_extractf128_pd(rows1[0], 1),
        _mm256_castpd256_pd128(rows2[0]),
        _mm256_extractf128_pd(rows2[0], 1),
        _mm256_castpd256_pd128(rows3[0]),
        _mm256_extractf128_pd(rows3[0], 1),
        _mm256_castpd256_pd128(rows4[0]),
        _mm256_extractf128_pd(rows4[0], 1),
    ];
    let output1 = [
        _mm256_insertf128_pd(_mm256_castpd128_pd256(rows0[1]), rows0[2], 1),
        transposed1[0],
        transposed1[1],
        transposed2[0],
        transposed2[1],
        transposed3[0],
        transposed3[1],
        transposed4[0],
        transposed4[1],
    ];

    (output0, output1)
}

// Treat the input like the rows of a 4x4 array, and transpose said rows to the columns
// The assumption here is that the caller wants to do some more AVX operations on the columns of the transposed array, so the output is arranged to make that more convenient
#[inline(always)]
pub unsafe fn transpose_4x4_f64(
    rows0: [__m256d; 4],
    rows1: [__m256d; 4],
) -> ([__m256d; 4], [__m256d; 4]) {
    let chunk00 = [rows0[0], rows0[1]];
    let chunk01 = [rows0[2], rows0[3]];
    let chunk10 = [rows1[0], rows1[1]];
    let chunk11 = [rows1[2], rows1[3]];

    let output00 = transpose_2x2_f64(chunk00);
    let output01 = transpose_2x2_f64(chunk10);
    let output10 = transpose_2x2_f64(chunk01);
    let output11 = transpose_2x2_f64(chunk11);

    (
        [output00[0], output00[1], output01[0], output01[1]],
        [output10[0], output10[1], output11[0], output11[1]],
    )
}

// Treat the input like the rows of a 6x6 array, and transpose said rows to the columns
// The assumption here is that the caller wants to do some more AVX operations on the columns of the transposed array, so the output is arranged to make that more convenient
#[inline(always)]
pub unsafe fn transpose_6x6_f64(
    rows0: [__m256d; 6],
    rows1: [__m256d; 6],
    rows2: [__m256d; 6],
) -> ([__m256d; 6], [__m256d; 6], [__m256d; 6]) {
    let chunk00 = [rows0[0], rows0[1]];
    let chunk01 = [rows0[2], rows0[3]];
    let chunk02 = [rows0[4], rows0[5]];
    let chunk10 = [rows1[0], rows1[1]];
    let chunk11 = [rows1[2], rows1[3]];
    let chunk12 = [rows1[4], rows1[5]];
    let chunk20 = [rows2[0], rows2[1]];
    let chunk21 = [rows2[2], rows2[3]];
    let chunk22 = [rows2[4], rows2[5]];

    let output00 = transpose_2x2_f64(chunk00);
    let output01 = transpose_2x2_f64(chunk10);
    let output02 = transpose_2x2_f64(chunk20);
    let output10 = transpose_2x2_f64(chunk01);
    let output11 = transpose_2x2_f64(chunk11);
    let output12 = transpose_2x2_f64(chunk21);
    let output20 = transpose_2x2_f64(chunk02);
    let output21 = transpose_2x2_f64(chunk12);
    let output22 = transpose_2x2_f64(chunk22);

    (
        [
            output00[0],
            output00[1],
            output01[0],
            output01[1],
            output02[0],
            output02[1],
        ],
        [
            output10[0],
            output10[1],
            output11[0],
            output11[1],
            output12[0],
            output12[1],
        ],
        [
            output20[0],
            output20[1],
            output21[0],
            output21[1],
            output22[0],
            output22[1],
        ],
    )
}

// Treat the input like the rows of a 6x4 array, and transpose said rows to the columns
// The assumption here is that the caller wants to do some more AVX operations on the columns of the transposed array, so the output is arranged to make that more convenient
#[inline(always)]
pub unsafe fn transpose_6x4_to_4x6_f64(
    rows0: [__m256d; 4],
    rows1: [__m256d; 4],
    rows2: [__m256d; 4],
) -> ([__m256d; 6], [__m256d; 6]) {
    let chunk00 = [rows0[0], rows0[1]];
    let chunk01 = [rows0[2], rows0[3]];
    let chunk10 = [rows1[0], rows1[1]];
    let chunk11 = [rows1[2], rows1[3]];
    let chunk20 = [rows2[0], rows2[1]];
    let chunk21 = [rows2[2], rows2[3]];

    let output00 = transpose_2x2_f64(chunk00);
    let output01 = transpose_2x2_f64(chunk10);
    let output02 = transpose_2x2_f64(chunk20);
    let output10 = transpose_2x2_f64(chunk01);
    let output11 = transpose_2x2_f64(chunk11);
    let output12 = transpose_2x2_f64(chunk21);

    (
        [
            output00[0],
            output00[1],
            output01[0],
            output01[1],
            output02[0],
            output02[1],
        ],
        [
            output10[0],
            output10[1],
            output11[0],
            output11[1],
            output12[0],
            output12[1],
        ],
    )
}

// Treat the input like the rows of a 8x4 array, and transpose it to a 4x8 array
// The assumption here is that it's very likely that the caller wants to do some more AVX operations on the columns of the transposed array, so the output is arranged to make that more convenient
#[inline(always)]
pub unsafe fn transpose_8x4_to_4x8_f64(
    rows0: [__m256d; 4],
    rows1: [__m256d; 4],
    rows2: [__m256d; 4],
    rows3: [__m256d; 4],
) -> ([__m256d; 8], [__m256d; 8]) {
    let chunk00 = [rows0[0], rows0[1]];
    let chunk01 = [rows0[2], rows0[3]];
    let chunk10 = [rows1[0], rows1[1]];
    let chunk11 = [rows1[2], rows1[3]];
    let chunk20 = [rows2[0], rows2[1]];
    let chunk21 = [rows2[2], rows2[3]];
    let chunk30 = [rows3[0], rows3[1]];
    let chunk31 = [rows3[2], rows3[3]];

    let output00 = transpose_2x2_f64(chunk00);
    let output01 = transpose_2x2_f64(chunk10);
    let output02 = transpose_2x2_f64(chunk20);
    let output03 = transpose_2x2_f64(chunk30);
    let output10 = transpose_2x2_f64(chunk01);
    let output11 = transpose_2x2_f64(chunk11);
    let output12 = transpose_2x2_f64(chunk21);
    let output13 = transpose_2x2_f64(chunk31);

    (
        [
            output00[0],
            output00[1],
            output01[0],
            output01[1],
            output02[0],
            output02[1],
            output03[0],
            output03[1],
        ],
        [
            output10[0],
            output10[1],
            output11[0],
            output11[1],
            output12[0],
            output12[1],
            output13[0],
            output13[1],
        ],
    )
}
