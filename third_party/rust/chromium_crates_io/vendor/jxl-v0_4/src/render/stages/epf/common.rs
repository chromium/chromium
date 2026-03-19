// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::BLOCK_DIM;
use crate::features::epf::SigmaSource;

use jxl_simd::{F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask};

/// Sigma row source for EPF processing.
/// Either a slice from the variable sigma image, or a constant value.
#[derive(Clone, Copy)]
pub(super) enum SigmaRow<'a> {
    Variable(&'a [f32]),
    Constant(f32),
}

impl SigmaSource {
    /// Get the sigma row for a given y position.
    #[inline(always)]
    pub(super) fn row(&self, y: usize) -> SigmaRow<'_> {
        match self {
            SigmaSource::Variable(image) => SigmaRow::Variable(image.row(y)),
            SigmaSource::Constant(sigma) => SigmaRow::Constant(*sigma),
        }
    }
}

#[inline(always)]
pub(super) fn prepare_sad_mul_storage(x: usize, y: usize, sm: f32, bsm: f32) -> [f32; 24] {
    let mut sad_mul_storage = [bsm; 24];
    if ![0, BLOCK_DIM - 1].contains(&(y % BLOCK_DIM)) {
        for (i, s) in sad_mul_storage.iter_mut().enumerate().take(16) {
            if ![0, BLOCK_DIM - 1].contains(&((x + i) % BLOCK_DIM)) {
                *s = sm;
            }
        }
    }
    sad_mul_storage
}

#[inline(always)]
pub(super) fn get_sigma<D: SimdDescriptor>(d: D, x: usize, row_sigma: SigmaRow<'_>) -> D::F32Vec {
    match row_sigma {
        SigmaRow::Constant(sigma) => D::F32Vec::splat(d, sigma),
        SigmaRow::Variable(row_sigma) => get_sigma_from_row(d, x, row_sigma),
    }
}

#[inline(always)]
fn get_sigma_from_row<D: SimdDescriptor>(d: D, x: usize, row_sigma: &[f32]) -> D::F32Vec {
    const { assert!(BLOCK_DIM == 8) }
    const { assert!(D::F32Vec::LEN <= 16) }
    let iota = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
    let iota = D::I32Vec::load(d, &iota);
    let sigma_start = x / BLOCK_DIM;
    let offset = D::I32Vec::splat(d, (x - sigma_start * BLOCK_DIM) as i32) + iota;
    if D::F32Vec::LEN > 8 {
        let [sigma0, sigma1, sigma2, ..] = row_sigma[sigma_start..] else {
            unreachable!();
        };
        let sigma0 = D::F32Vec::splat(d, sigma0);
        let sigma1 = D::F32Vec::splat(d, sigma1);
        let sigma2 = D::F32Vec::splat(d, sigma2);
        let above_8 = offset.gt(D::I32Vec::splat(d, 7));
        let above_16 = offset.gt(D::I32Vec::splat(d, 15));
        above_16.if_then_else_f32(sigma2, above_8.if_then_else_f32(sigma1, sigma0))
    } else {
        let [sigma0, sigma1, ..] = row_sigma[sigma_start..] else {
            unreachable!();
        };
        let sigma0 = D::F32Vec::splat(d, sigma0);
        let sigma1 = D::F32Vec::splat(d, sigma1);
        let above_8 = offset.gt(D::I32Vec::splat(d, 7));
        above_8.if_then_else_f32(sigma1, sigma0)
    }
}
