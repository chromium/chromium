// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::error::Error;

pub type Matrix3x3<T> = [[T; 3]; 3];
pub type Vector3<T> = [T; 3];

pub fn matmul3_vec(m: [f32; 9], v: [f32; 3]) -> [f32; 3] {
    [
        v[0] * m[0] + v[1] * m[1] + v[2] * m[2],
        v[0] * m[3] + v[1] * m[4] + v[2] * m[5],
        v[0] * m[6] + v[1] * m[7] + v[2] * m[8],
    ]
}

pub fn mul_3x3_vector(matrix: &Matrix3x3<f64>, vector: &Vector3<f64>) -> Vector3<f64> {
    std::array::from_fn(|i| {
        matrix[i]
            .iter()
            .zip(vector.iter())
            .map(|(&matrix_element, &vector_element)| matrix_element * vector_element)
            .sum()
    })
}

pub fn mul_3x3_matrix(mat1: &Matrix3x3<f64>, mat2: &Matrix3x3<f64>) -> Matrix3x3<f64> {
    std::array::from_fn(|i| std::array::from_fn(|j| (0..3).map(|k| mat1[i][k] * mat2[k][j]).sum()))
}

fn det2x2(a: f64, b: f64, c: f64, d: f64) -> f64 {
    a * d - b * c
}

fn calculate_cofactor(m: &Matrix3x3<f64>, r: usize, c: usize) -> f64 {
    // Determine the actual row and column indices for the 2x2 submatrix
    // by excluding the current row 'r' and column 'c'.
    // Ensure they are taken in ascending order to form the submatrix consistently.
    let mut sub_rows = [0; 2];
    let mut sub_cols = [0; 2];

    let mut current_idx = 0;
    for i in 0..3 {
        if i != r {
            sub_rows[current_idx] = i;
            current_idx += 1;
        }
    }

    current_idx = 0;
    for i in 0..3 {
        if i != c {
            sub_cols[current_idx] = i;
            current_idx += 1;
        }
    }

    let minor_val = det2x2(
        m[sub_rows[0]][sub_cols[0]],
        m[sub_rows[0]][sub_cols[1]],
        m[sub_rows[1]][sub_cols[0]],
        m[sub_rows[1]][sub_cols[1]],
    );

    // Apply the checkerboard pattern sign for the cofactor
    if (r + c).is_multiple_of(2) {
        minor_val
    } else {
        -minor_val
    }
}

/// Calculates the inverse of a 3x3 matrix.
pub fn inv_3x3_matrix(m: &Matrix3x3<f64>) -> Result<Matrix3x3<f64>, Error> {
    let cofactor_matrix: [[f64; 3]; 3] = std::array::from_fn(|r_idx| {
        std::array::from_fn(|c_idx| calculate_cofactor(m, r_idx, c_idx))
    });

    let det = m[0]
        .iter()
        .zip(cofactor_matrix[0].iter())
        .map(|(&m_element, &cof_element)| m_element * cof_element)
        .sum::<f64>();

    // Check for numerical singularity.
    const EPSILON: f64 = 1e-12;
    if det.abs() < EPSILON {
        return Err(Error::MatrixInversionFailed(det.abs()));
    }

    let inv_det = 1.0 / det;

    let adjugate_matrix: [[f64; 3]; 3] =
        std::array::from_fn(|r_idx| std::array::from_fn(|c_idx| cofactor_matrix[c_idx][r_idx]));

    // Inverse matrix = (1/det) * Adjugate matrix.
    Ok(std::array::from_fn(|r_idx| {
        std::array::from_fn(|c_idx| adjugate_matrix[r_idx][c_idx] * inv_det)
    }))
}

#[cfg(test)]
mod test {
    use super::*;

    fn assert_matrix_eq(a: &Matrix3x3<f64>, b: &Matrix3x3<f64>, epsilon: f64) {
        for r in 0..3 {
            for c in 0..3 {
                assert!(
                    (a[r][c] - b[r][c]).abs() < epsilon,
                    "Matrices differ at [{}][{}]: expected {}, got {}. Diff: {}",
                    r,
                    c,
                    b[r][c],
                    a[r][c],
                    (a[r][c] - b[r][c]).abs()
                );
            }
        }
    }

    #[test]
    fn test_3x3_inverse() {
        // Random matrix (https://xkcd.com/221/)
        let m: Matrix3x3<f64> = [[1.0f64, -3.0, -2.0], [2.0, 2.0, 1.0], [2.0, 1.0, 1.0]];

        let expected_inv: Matrix3x3<f64> = [[0.2, 0.2, 0.2], [0., 1., -1.], [-0.4, -1.4, 1.6]];

        match inv_3x3_matrix(&m) {
            Ok(inv_m) => {
                assert_matrix_eq(&inv_m, &expected_inv, 1e-12);
            }
            Err(e) => {
                panic!("Matrix inversion failed unexpectedly: {e:?}");
            }
        }
    }
}
