// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

//! SIMD utilities for interleaving and deinterleaving channel data.
//!
//! These functions assume that input buffers are padded to at least the SIMD
//! vector length (up to 16 elements), as is standard in the render pipeline.

use jxl_simd::{F32SimdVec, simd_function};

simd_function!(
    interleave_2_dispatch,
    d: D,
    /// Interleave 2 planar channels into packed format.
    /// Buffers must be padded to SIMD vector length.
    pub fn interleave_2(a: &[f32], b: &[f32], out: &mut [f32]) {
        let len = D::F32Vec::LEN;
        for ((chunk_a, chunk_b), chunk_out) in a
            .chunks_exact(len)
            .zip(b.chunks_exact(len))
            .zip(out.chunks_exact_mut(len * 2))
        {
            let va = D::F32Vec::load(d, chunk_a);
            let vb = D::F32Vec::load(d, chunk_b);
            D::F32Vec::store_interleaved_2(va, vb, chunk_out);
        }
    }
);

simd_function!(
    deinterleave_2_dispatch,
    d: D,
    /// Deinterleave packed format into 2 planar channels.
    /// Buffers must be padded to SIMD vector length.
    pub fn deinterleave_2(input: &[f32], a: &mut [f32], b: &mut [f32]) {
        let len = D::F32Vec::LEN;
        for ((chunk_a, chunk_b), chunk_in) in a
            .chunks_exact_mut(len)
            .zip(b.chunks_exact_mut(len))
            .zip(input.chunks_exact(len * 2))
        {
            let (va, vb) = D::F32Vec::load_deinterleaved_2(d, chunk_in);
            va.store(chunk_a);
            vb.store(chunk_b);
        }
    }
);

simd_function!(
    interleave_3_dispatch,
    d: D,
    /// Interleave 3 planar channels into packed RGB format.
    /// Buffers must be padded to SIMD vector length.
    pub fn interleave_3(a: &[f32], b: &[f32], c: &[f32], out: &mut [f32]) {
        let len = D::F32Vec::LEN;
        for (((chunk_a, chunk_b), chunk_c), chunk_out) in a
            .chunks_exact(len)
            .zip(b.chunks_exact(len))
            .zip(c.chunks_exact(len))
            .zip(out.chunks_exact_mut(len * 3))
        {
            let va = D::F32Vec::load(d, chunk_a);
            let vb = D::F32Vec::load(d, chunk_b);
            let vc = D::F32Vec::load(d, chunk_c);
            D::F32Vec::store_interleaved_3(va, vb, vc, chunk_out);
        }
    }
);

simd_function!(
    deinterleave_3_dispatch,
    d: D,
    /// Deinterleave packed RGB format into 3 planar channels.
    /// Buffers must be padded to SIMD vector length.
    pub fn deinterleave_3(input: &[f32], a: &mut [f32], b: &mut [f32], c: &mut [f32]) {
        let len = D::F32Vec::LEN;
        for (((chunk_a, chunk_b), chunk_c), chunk_in) in a
            .chunks_exact_mut(len)
            .zip(b.chunks_exact_mut(len))
            .zip(c.chunks_exact_mut(len))
            .zip(input.chunks_exact(len * 3))
        {
            let (va, vb, vc) = D::F32Vec::load_deinterleaved_3(d, chunk_in);
            va.store(chunk_a);
            vb.store(chunk_b);
            vc.store(chunk_c);
        }
    }
);

simd_function!(
    interleave_4_dispatch,
    d: D,
    /// Interleave 4 planar channels into packed RGBA format.
    /// Buffers must be padded to SIMD vector length.
    pub fn interleave_4(a: &[f32], b: &[f32], c: &[f32], e: &[f32], out: &mut [f32]) {
        let len = D::F32Vec::LEN;
        for ((((chunk_a, chunk_b), chunk_c), chunk_d), chunk_out) in a
            .chunks_exact(len)
            .zip(b.chunks_exact(len))
            .zip(c.chunks_exact(len))
            .zip(e.chunks_exact(len))
            .zip(out.chunks_exact_mut(len * 4))
        {
            let va = D::F32Vec::load(d, chunk_a);
            let vb = D::F32Vec::load(d, chunk_b);
            let vc = D::F32Vec::load(d, chunk_c);
            let vd = D::F32Vec::load(d, chunk_d);
            D::F32Vec::store_interleaved_4(va, vb, vc, vd, chunk_out);
        }
    }
);

simd_function!(
    deinterleave_4_dispatch,
    d: D,
    /// Deinterleave packed RGBA format into 4 planar channels.
    /// Buffers must be padded to SIMD vector length.
    pub fn deinterleave_4(
        input: &[f32],
        a: &mut [f32],
        b: &mut [f32],
        c: &mut [f32],
        e: &mut [f32],
    ) {
        let len = D::F32Vec::LEN;
        for ((((chunk_a, chunk_b), chunk_c), chunk_d), chunk_in) in a
            .chunks_exact_mut(len)
            .zip(b.chunks_exact_mut(len))
            .zip(c.chunks_exact_mut(len))
            .zip(e.chunks_exact_mut(len))
            .zip(input.chunks_exact(len * 4))
        {
            let (va, vb, vc, vd) = D::F32Vec::load_deinterleaved_4(d, chunk_in);
            va.store(chunk_a);
            vb.store(chunk_b);
            vc.store(chunk_c);
            vd.store(chunk_d);
        }
    }
);

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_interleave_deinterleave_2_roundtrip() {
        // Use 16 elements to ensure SIMD alignment for all backends
        let a = vec![
            1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0,
        ];
        let b = vec![
            10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0, 110.0, 120.0, 130.0,
            140.0, 150.0, 160.0,
        ];

        let mut packed = vec![0.0; 32];
        interleave_2_dispatch(&a, &b, &mut packed);

        // Check interleaved format
        assert_eq!(packed[0], 1.0);
        assert_eq!(packed[1], 10.0);
        assert_eq!(packed[2], 2.0);
        assert_eq!(packed[3], 20.0);

        // Deinterleave back
        let mut a_out = vec![0.0; 16];
        let mut b_out = vec![0.0; 16];
        deinterleave_2_dispatch(&packed, &mut a_out, &mut b_out);

        assert_eq!(a_out, a);
        assert_eq!(b_out, b);
    }

    #[test]
    fn test_interleave_deinterleave_3_roundtrip() {
        // Use 16 elements to ensure SIMD alignment for all backends
        let a: Vec<f32> = (1..=16).map(|x| x as f32).collect();
        let b: Vec<f32> = (1..=16).map(|x| x as f32 * 10.0).collect();
        let c: Vec<f32> = (1..=16).map(|x| x as f32 * 100.0).collect();

        let mut packed = vec![0.0; 48];
        interleave_3_dispatch(&a, &b, &c, &mut packed);

        // Check interleaved format
        assert_eq!(packed[0], 1.0);
        assert_eq!(packed[1], 10.0);
        assert_eq!(packed[2], 100.0);
        assert_eq!(packed[3], 2.0);
        assert_eq!(packed[4], 20.0);
        assert_eq!(packed[5], 200.0);

        // Deinterleave back
        let mut a_out = vec![0.0; 16];
        let mut b_out = vec![0.0; 16];
        let mut c_out = vec![0.0; 16];
        deinterleave_3_dispatch(&packed, &mut a_out, &mut b_out, &mut c_out);

        assert_eq!(a_out, a);
        assert_eq!(b_out, b);
        assert_eq!(c_out, c);
    }

    #[test]
    fn test_interleave_deinterleave_4_roundtrip() {
        // Use 16 elements to ensure SIMD alignment for all backends
        let a: Vec<f32> = (1..=16).map(|x| x as f32).collect();
        let b: Vec<f32> = (1..=16).map(|x| x as f32 * 10.0).collect();
        let c: Vec<f32> = (1..=16).map(|x| x as f32 * 100.0).collect();
        let d: Vec<f32> = (1..=16).map(|x| x as f32 * 1000.0).collect();

        let mut packed = vec![0.0; 64];
        interleave_4_dispatch(&a, &b, &c, &d, &mut packed);

        // Check interleaved format
        assert_eq!(packed[0], 1.0);
        assert_eq!(packed[1], 10.0);
        assert_eq!(packed[2], 100.0);
        assert_eq!(packed[3], 1000.0);
        assert_eq!(packed[4], 2.0);
        assert_eq!(packed[5], 20.0);

        // Deinterleave back
        let mut a_out = vec![0.0; 16];
        let mut b_out = vec![0.0; 16];
        let mut c_out = vec![0.0; 16];
        let mut d_out = vec![0.0; 16];
        deinterleave_4_dispatch(&packed, &mut a_out, &mut b_out, &mut c_out, &mut d_out);

        assert_eq!(a_out, a);
        assert_eq!(b_out, b);
        assert_eq!(c_out, c);
        assert_eq!(d_out, d);
    }
}
