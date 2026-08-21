// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use super::*;
use jxl_simd::{test_all_instruction_sets, ScalarDescriptor, SimdDescriptor};
use rand::Rng;
use rand::SeedableRng;
use rand_chacha::ChaCha12Rng;
use test_log::test;

use std::f64::consts::FRAC_1_SQRT_2;
use std::f64::consts::PI;
use std::f64::consts::SQRT_2;

#[inline(always)]
fn alpha(u: usize) -> f64 {
    if u == 0 {
        FRAC_1_SQRT_2
    } else {
        1.0
    }
}

pub fn dct1d(input_matrix: &[Vec<f64>]) -> Vec<Vec<f64>> {
    let num_rows = input_matrix.len();

    if num_rows == 0 {
        return Vec::new();
    }

    let num_cols = input_matrix[0].len();

    let mut output_matrix = vec![vec![0.0f64; num_cols]; num_rows];

    let scale: f64 = SQRT_2;

    // Precompute the DCT matrix (size: n_rows x n_rows)
    let mut dct_coeff_matrix = vec![vec![0.0f64; num_rows]; num_rows];
    for (u_freq, row) in dct_coeff_matrix.iter_mut().enumerate() {
        let alpha_u_val = alpha(u_freq);
        for (y_spatial, coeff) in row.iter_mut().enumerate() {
            *coeff = alpha_u_val
                * ((y_spatial as f64 + 0.5) * u_freq as f64 * PI / num_rows as f64).cos()
                * scale;
        }
    }

    // Perform the DCT calculation column by column
    for x_col_idx in 0..num_cols {
        for u_freq_idx in 0..num_rows {
            let mut sum = 0.0;
            for (y_spatial_idx, col) in input_matrix.iter().enumerate() {
                // This access `input_matrix[y_spatial_idx][x_col_idx]` assumes the input_matrix
                // is rectangular. If not, it might panic here.
                sum += dct_coeff_matrix[u_freq_idx][y_spatial_idx] * col[x_col_idx];
            }
            output_matrix[u_freq_idx][x_col_idx] = sum;
        }
    }

    output_matrix
}

pub fn idct1d(input_matrix: &[Vec<f64>]) -> Vec<Vec<f64>> {
    let num_rows = input_matrix.len();

    if num_rows == 0 {
        return Vec::new();
    }

    let num_cols = input_matrix[0].len();

    let mut output_matrix = vec![vec![0.0f64; num_cols]; num_rows];

    let scale: f64 = SQRT_2;

    // Precompute the DCT matrix (size: num_rows x num_rows)
    let mut dct_coeff_matrix = vec![vec![0.0f64; num_rows]; num_rows];
    for (u_freq, row) in dct_coeff_matrix.iter_mut().enumerate() {
        let alpha_u_val = alpha(u_freq);
        for (y_def_idx, coeff) in row.iter_mut().enumerate() {
            *coeff = alpha_u_val
                * ((y_def_idx as f64 + 0.5) * u_freq as f64 * PI / num_rows as f64).cos()
                * scale;
        }
    }

    // Perform the IDCT calculation column by column
    for x_col_idx in 0..num_cols {
        for (y_row_idx, row) in output_matrix.iter_mut().enumerate() {
            let mut sum = 0.0;
            for (u_freq_idx, col) in input_matrix.iter().enumerate() {
                // This access input_coeffs_matrix[u_freq_idx][x_col_idx] assumes input_coeffs_matrix
                // is rectangular. If not, it might panic here.
                sum += dct_coeff_matrix[u_freq_idx][y_row_idx] * col[x_col_idx];
            }
            row[x_col_idx] = sum;
        }
    }

    output_matrix
}

fn transpose_f64(matrix: &[Vec<f64>]) -> Vec<Vec<f64>> {
    if matrix.is_empty() {
        return Vec::new();
    }
    let num_rows = matrix.len();
    let num_cols = matrix[0].len();
    let mut transposed = vec![vec![0.0; num_rows]; num_cols];
    for i in 0..num_rows {
        for j in 0..num_cols {
            transposed[j][i] = matrix[i][j];
        }
    }
    transposed
}

pub fn slow_idct2d(input: &[Vec<f64>]) -> Vec<Vec<f64>> {
    let rows = input.len();
    let cols = input[0].len();
    let idct1 = if rows < cols {
        let transposed = transpose_f64(input);
        idct1d(&transposed)
    } else {
        let input: Vec<_> = input.iter().flat_map(|x| x.iter()).copied().collect();
        let input: Vec<_> = input.chunks_exact(rows).map(|x| x.to_vec()).collect();
        idct1d(&input)
    };
    let transposed1 = transpose_f64(&idct1);
    idct1d(&transposed1)
}

pub fn scales(n: usize) -> Vec<f64> {
    (0..n)
        .map(|i| {
            (i as f64 / (16 * n) as f64 * PI).cos()
                * (i as f64 / (8 * n) as f64 * PI).cos()
                * (i as f64 / (4 * n) as f64 * PI).cos()
                * n as f64
        })
        .collect()
}

pub fn slow_reinterpreting_dct2d(input: &[Vec<f64>]) -> Vec<Vec<f64>> {
    let rows = input.len();
    let cols = input[0].len();
    let dct1 = dct1d(input);
    let tdct1 = transpose_f64(&dct1);
    let dct2 = dct1d(&tdct1);
    let mut res = if rows < cols {
        transpose_f64(&dct2)
    } else {
        dct2
    };

    let row_scales = scales(rows);
    let col_scales = scales(cols);
    if rows < cols {
        for y in 0..rows {
            for x in 0..cols {
                res[y][x] /= row_scales[y] * col_scales[x];
            }
        }
    } else {
        for y in 0..cols {
            for x in 0..rows {
                res[y][x] /= row_scales[x] * col_scales[y];
            }
        }
    }
    res
}

#[track_caller]
fn check_close(a: f64, b: f64, max_err: f64) {
    let abs = (a - b).abs();
    let rel = abs / a.abs().max(b.abs());
    assert!(
        abs < max_err || rel < max_err,
        "a: {a} b: {b} abs diff: {abs:?} rel diff: {rel:?}"
    );
}

macro_rules! test_reinterpreting_dct1d_eq_slow_n {
    ($test_name:ident, $n_val:expr, $do_idct_fun:path, $tolerance:expr) => {
        #[test]
        fn $test_name() {
            const N: usize = $n_val;

            let input_matrix_for_ref = random_matrix(N, 1);

            let output_matrix_slow: Vec<Vec<f64>> = dct1d(&input_matrix_for_ref);

            let mut output: Vec<_> = input_matrix_for_ref.iter().map(|x| x[0] as f32).collect();
            let d = ScalarDescriptor {};

            let (output_chunks, remainder) = output.as_chunks_mut::<1>();
            assert!(remainder.is_empty());
            $do_idct_fun(d, output_chunks, 1);

            let scales = scales(N);

            for i in 0..N {
                check_close(
                    output[i] as f64,
                    output_matrix_slow[i][0] / scales[i],
                    $tolerance,
                );
            }
        }
    };
}

test_reinterpreting_dct1d_eq_slow_n!(
    test_reinterpreting_dct1d_2_eq_slow,
    2,
    do_reinterpreting_dct_2,
    1e-6
);
test_reinterpreting_dct1d_eq_slow_n!(
    test_reinterpreting_dct1d_4_eq_slow,
    4,
    do_reinterpreting_dct_4,
    1e-6
);
test_reinterpreting_dct1d_eq_slow_n!(
    test_reinterpreting_dct1d_8_eq_slow,
    8,
    do_reinterpreting_dct_8,
    1e-6
);
test_reinterpreting_dct1d_eq_slow_n!(
    test_reinterpreting_dct1d_16_eq_slow,
    16,
    do_reinterpreting_dct_16,
    5e-6
);
test_reinterpreting_dct1d_eq_slow_n!(
    test_reinterpreting_dct1d_32_eq_slow,
    32,
    do_reinterpreting_dct_32,
    5e-6
);

fn random_matrix(n: usize, m: usize) -> Vec<Vec<f64>> {
    let mut rng = ChaCha12Rng::seed_from_u64(0);
    let mut data = vec![vec![0.0; m]; n];

    data.iter_mut()
        .flat_map(|x| x.iter_mut())
        .for_each(|x| *x = rng.random_range(-1.0..1.0));

    data
}

macro_rules! test_idct1d_eq_slow_n {
    ($test_name:ident, $n_val:expr, $do_idct_fun:path, $tolerance:expr) => {
        #[test]
        fn $test_name() {
            const N: usize = $n_val;

            let input_matrix_for_ref = random_matrix(N, 1);

            let output_matrix_slow: Vec<Vec<f64>> = idct1d(&input_matrix_for_ref);

            let mut output: Vec<_> = input_matrix_for_ref.iter().map(|x| x[0] as f32).collect();
            let d = ScalarDescriptor {};

            let (output_chunks, remainder) = output.as_chunks_mut::<1>();
            assert!(remainder.is_empty());
            $do_idct_fun(d, output_chunks, 1);

            for i in 0..N {
                check_close(output[i] as f64, output_matrix_slow[i][0], $tolerance);
            }
        }
    };
}

test_idct1d_eq_slow_n!(test_idct1d_2_eq_slow, 2, do_idct_2, 1e-6);
test_idct1d_eq_slow_n!(test_idct1d_4_eq_slow, 4, do_idct_4, 1e-6);
test_idct1d_eq_slow_n!(test_idct1d_8_eq_slow, 8, do_idct_8, 1e-6);
test_idct1d_eq_slow_n!(test_idct1d_16_eq_slow, 16, do_idct_16, 1e-6);
test_idct1d_eq_slow_n!(test_idct1d_32_eq_slow, 32, do_idct_32, 5e-6);
test_idct1d_eq_slow_n!(test_idct1d_64_eq_slow, 64, do_idct_64, 5e-6);
test_idct1d_eq_slow_n!(test_idct1d_128_eq_slow, 128, do_idct_128, 5e-5);
test_idct1d_eq_slow_n!(test_idct1d_256_eq_slow, 256, do_idct_256, 5e-5);

macro_rules! test_idct2d_eq_slow {
    ($test_name:ident, $rows:expr, $cols:expr, $fast_idct:ident, $tol:expr) => {
        fn $test_name<D: SimdDescriptor>(d: D) {
            const N: usize = $rows;
            const M: usize = $cols;

            let slow_input = random_matrix(N, M);

            let slow_output = slow_idct2d(&slow_input);

            let mut fast_input: Vec<_> = slow_input
                .iter()
                .flat_map(|x| x.iter())
                .map(|x| *x as f32)
                .collect();

            $fast_idct(d, &mut fast_input);

            for r in 0..N {
                for c in 0..M {
                    check_close(fast_input[r * M + c] as f64, slow_output[r][c], $tol);
                }
            }
        }
        test_all_instruction_sets!($test_name);
    };
}

test_idct2d_eq_slow!(test_idct2d_2_2_eq_slow, 2, 2, idct2d_2_2, 1e-6);
test_idct2d_eq_slow!(test_idct2d_4_4_eq_slow, 4, 4, idct2d_4_4, 1e-6);
test_idct2d_eq_slow!(test_idct2d_4_8_eq_slow, 4, 8, idct2d_4_8, 1e-6);
test_idct2d_eq_slow!(test_idct2d_8_4_eq_slow, 8, 4, idct2d_8_4, 1e-6);
test_idct2d_eq_slow!(test_idct2d_8_8_eq_slow, 8, 8, idct2d_8_8, 5e-6);
test_idct2d_eq_slow!(test_idct2d_16_8_eq_slow, 16, 8, idct2d_16_8, 5e-6);
test_idct2d_eq_slow!(test_idct2d_8_16_eq_slow, 8, 16, idct2d_8_16, 5e-6);
test_idct2d_eq_slow!(test_idct2d_16_16_eq_slow, 16, 16, idct2d_16_16, 1e-5);
test_idct2d_eq_slow!(test_idct2d_32_8_eq_slow, 32, 8, idct2d_32_8, 5e-6);
test_idct2d_eq_slow!(test_idct2d_8_32_eq_slow, 8, 32, idct2d_8_32, 5e-6);
test_idct2d_eq_slow!(test_idct2d_32_16_eq_slow, 32, 16, idct2d_32_16, 1e-5);
test_idct2d_eq_slow!(test_idct2d_16_32_eq_slow, 16, 32, idct2d_16_32, 1e-5);
test_idct2d_eq_slow!(test_idct2d_32_32_eq_slow, 32, 32, idct2d_32_32, 5e-5);
test_idct2d_eq_slow!(test_idct2d_64_32_eq_slow, 64, 32, idct2d_64_32, 1e-4);
test_idct2d_eq_slow!(test_idct2d_32_64_eq_slow, 32, 64, idct2d_32_64, 1e-4);
test_idct2d_eq_slow!(test_idct2d_64_64_eq_slow, 64, 64, idct2d_64_64, 1e-4);
test_idct2d_eq_slow!(test_idct2d_128_64_eq_slow, 128, 64, idct2d_128_64, 5e-4);
test_idct2d_eq_slow!(test_idct2d_64_128_eq_slow, 64, 128, idct2d_64_128, 5e-4);
test_idct2d_eq_slow!(test_idct2d_128_128_eq_slow, 128, 128, idct2d_128_128, 5e-4);
test_idct2d_eq_slow!(test_idct2d_256_128_eq_slow, 256, 128, idct2d_256_128, 1e-3);
test_idct2d_eq_slow!(test_idct2d_128_256_eq_slow, 128, 256, idct2d_128_256, 1e-3);
test_idct2d_eq_slow!(test_idct2d_256_256_eq_slow, 256, 256, idct2d_256_256, 5e-3);

macro_rules! test_reinterpreting_dct_eq_slow {
    ($test_name:ident, $fun: ident, $rows:expr, $cols:expr, $tol:expr) => {
        fn $test_name<D: SimdDescriptor>(d: D) {
            const N: usize = $rows;
            const M: usize = $cols;

            let slow_input = random_matrix(N, M);

            let slow_output = slow_reinterpreting_dct2d(&slow_input);

            let mut fast_input: Vec<_> = slow_input
                .iter()
                .flat_map(|x| x.iter())
                .map(|x| *x as f32)
                .collect();

            let mut output = [0.0; $rows * $cols * 64];

            $fun(d, &mut fast_input, &mut output);

            let on = slow_output.len();
            let om = slow_output[0].len();

            for r in 0..on {
                for c in 0..om {
                    check_close(output[r * om * 8 + c] as f64, slow_output[r][c], $tol);
                }
            }
        }
        test_all_instruction_sets!($test_name);
    };
}

test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_1x2_eq_slow,
    reinterpreting_dct2d_1_2,
    1,
    2,
    1e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_2x1_eq_slow,
    reinterpreting_dct2d_2_1,
    2,
    1,
    1e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_2x2_eq_slow,
    reinterpreting_dct2d_2_2,
    2,
    2,
    1e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_1x4_eq_slow,
    reinterpreting_dct2d_1_4,
    1,
    4,
    1e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_4x1_eq_slow,
    reinterpreting_dct2d_4_1,
    4,
    1,
    1e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_2x4_eq_slow,
    reinterpreting_dct2d_2_4,
    2,
    4,
    1e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_4x2_eq_slow,
    reinterpreting_dct2d_4_2,
    4,
    2,
    1e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_4x4_eq_slow,
    reinterpreting_dct2d_4_4,
    4,
    4,
    1e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_8x4_eq_slow,
    reinterpreting_dct2d_8_4,
    8,
    4,
    1e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_4x8_eq_slow,
    reinterpreting_dct2d_4_8,
    4,
    8,
    1e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_8x8_eq_slow,
    reinterpreting_dct2d_8_8,
    8,
    8,
    1e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_8x16_eq_slow,
    reinterpreting_dct2d_8_16,
    8,
    16,
    5e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_16x8_eq_slow,
    reinterpreting_dct2d_16_8,
    16,
    8,
    5e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_16x16_eq_slow,
    reinterpreting_dct2d_16_16,
    16,
    16,
    5e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_32x16_eq_slow,
    reinterpreting_dct2d_32_16,
    32,
    16,
    5e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_16x32_eq_slow,
    reinterpreting_dct2d_16_32,
    16,
    32,
    5e-6
);
test_reinterpreting_dct_eq_slow!(
    test_reinterpreting_dct_32x32_eq_slow,
    reinterpreting_dct2d_32_32,
    32,
    32,
    5e-6
);
