// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The `mdct` module implements the Modified Discrete Cosine Transform (MDCT).
//!
//! The MDCT in this module is implemented in-terms of a forward FFT.

use crate::dsp::complex::Complex;
use crate::dsp::fft::*;

/// The Inverse Modified Discrete Transform (IMDCT).
pub struct Imdct {
    fft: Fft,
    scratch: Box<[Complex<f32>]>,
    twiddle: Box<[Complex<f32>]>,
}

impl Imdct {
    /// Instantiate a N-point IMDCT with no scaling.
    ///
    /// The value of `n` is the number of spectral samples and must be a power-of-2 and less-than or
    /// equal to `2 * Fft::MAX_SIZE`.
    pub fn new(n: usize) -> Self {
        Imdct::new_scaled(n, 1.0)
    }

    /// Instantiate a N-point IMDCT with scaling.
    ///
    /// The value of `n` is the number of spectral samples and must be a power-of-2 and less-than or
    /// equal to `2 * Fft::MAX_SIZE`.
    pub fn new_scaled(n: usize, scale: f64) -> Self {
        // The FFT requires a power-of-two N.
        assert!(n.is_power_of_two(), "n must be a power of two");
        // A complex FFT of size N/2 is used to compute the IMDCT. Therefore, the maximum value of N
        // is 2 * Fft::MAX_SIZE.
        assert!(n <= 2 * MAX_SIZE, "maximum size exceeded");

        let n2 = n / 2;
        let mut twiddle = Vec::with_capacity(n2);

        let alpha = 1.0 / 8.0 + if scale.is_sign_positive() { 0.0 } else { n2 as f64 };
        let pi_n = std::f64::consts::PI / n as f64;
        let sqrt_scale = scale.abs().sqrt();

        for k in 0..n2 {
            let theta = pi_n * (alpha + k as f64);
            let re = sqrt_scale * theta.cos();
            let im = sqrt_scale * theta.sin();
            twiddle.push(Complex::new(re as f32, im as f32));
        }

        // Allocate scratch for the IMDCT.
        let scratch = vec![Default::default(); n2].into_boxed_slice();

        Imdct { fft: Fft::new(n2), scratch, twiddle: twiddle.into_boxed_slice() }
    }

    /// Performs the the N-point Inverse Modified Discrete Cosine Transform.
    ///
    /// The number of input spectral samples provided by the slice `spec` must equal the value of N
    /// that the IMDCT was instantiated with. The length of the output slice, `out`, must be of
    /// length 2N. Failing to meet these requirements will throw an assertion.
    pub fn imdct(&mut self, spec: &[f32], out: &mut [f32]) {
        // Spectral length: 2x FFT size, 0.5x output length.
        let n = self.fft.size() << 1;
        // 1x FFT size, 0.25x output length.
        let n2 = n >> 1;
        // 0.5x FFT size.
        let n4 = n >> 2;

        // The spectrum length must be the same as N.
        assert_eq!(spec.len(), n);
        // The output length must be 2x the spectrum length.
        assert_eq!(out.len(), 2 * n);

        // Pre-FFT twiddling and packing of the real input signal values into complex signal values.
        for (i, (&w, t)) in self.twiddle.iter().zip(self.scratch.iter_mut()).enumerate() {
            let even = spec[i * 2];
            let odd = -spec[n - 1 - i * 2];

            let re = odd * w.im - even * w.re;
            let im = odd * w.re + even * w.im;
            *t = Complex::new(re, im);
        }

        // Do the FFT.
        self.fft.fft_inplace(&mut self.scratch);

        // Split the output vector (2N samples) into 4 vectors (N/2 samples each).
        let (vec0, vec1) = out.split_at_mut(n2);
        let (vec1, vec2) = vec1.split_at_mut(n2);
        let (vec2, vec3) = vec2.split_at_mut(n2);

        // Post-FFT twiddling and processing to expand the N/2 complex output values into 2N real
        // output samples.
        for (i, (x, &w)) in self.scratch[..n4].iter().zip(self.twiddle[..n4].iter()).enumerate() {
            // The real and imaginary components of the post-twiddled FFT samples are used to
            // generate 4 reak output samples. Using the first half of the complex FFT output,
            // populate each of the 4 output vectors.
            let val = w * x.conj();

            // Forward and reverse order indicies that will be populated.
            let fi = 2 * i;
            let ri = n2 - 1 - 2 * i;

            // The odd indicies in vec0 are populated reverse order.
            vec0[ri] = -val.im;
            // The even indicies in vec1 are populated forward order.
            vec1[fi] = val.im;
            // The odd indicies in vec2 are populated reverse order.
            vec2[ri] = val.re;
            // The even indicies in vec3 are populated forward order.
            vec3[fi] = val.re;
        }

        for (i, (x, &w)) in self.scratch[n4..].iter().zip(self.twiddle[n4..].iter()).enumerate() {
            // Using the second half of the FFT output samples, finish populating each of the 4
            // output vectors.
            let val = w * x.conj();

            // Forward and reverse order indicies that will be populated.
            let fi = 2 * i;
            let ri = n2 - 1 - 2 * i;

            // The even indicies in vec0 are populated in forward order.
            vec0[fi] = -val.re;
            // The odd indicies in vec1 are populated in reverse order.
            vec1[ri] = val.re;
            // The even indicies in vec2 are populated in forward order.
            vec2[fi] = val.im;
            // The odd indicies in vec3 are populated in reverse order.
            vec3[ri] = val.im;
        }

        // Note: As of Rust 1.58, there doesn't appear to be any measurable difference between using
        // iterators or indexing like above. Either the bounds checks are elided above, or they are
        // not elided using iterators. Therefore, for clarity, the indexing method is used.
        //
        // Additionally, note that vectors 0 and 3 are reversed copies (+ negation for vector 0) of
        // vectors 1 and 2, respectively. Pulling these copies out into a separate loop using
        // iterators yielded no measureable difference either.
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::f64;

    fn imdct_analytical(x: &[f32], y: &mut [f32], scale: f64) {
        assert!(y.len() == 2 * x.len());

        // Generates 2N outputs from N inputs.
        let n_in = x.len();
        let n_out = x.len() << 1;

        let pi_2n = f64::consts::PI / (2 * n_out) as f64;

        for (i, item) in y.iter_mut().enumerate().take(n_out) {
            let accum: f64 = x
                .iter()
                .copied()
                .map(f64::from)
                .enumerate()
                .take(n_in)
                .map(|(j, jtem)| jtem * (pi_2n * ((2 * i + 1 + n_in) * (2 * j + 1)) as f64).cos())
                .sum();

            *item = (scale * accum) as f32;
        }
    }

    #[test]
    fn verify_imdct() {
        #[rustfmt::skip]
        const TEST_VECTOR: [f32; 32] = [
             1.0,  2.0,  3.0,  4.0,  5.0,  6.0,  7.0,  8.0,
             9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0,
            17.0, 18.0, 19.0, 20.0, 21.0, 22.0, 23.0, 24.0,
            25.0, 26.0, 27.0, 28.0, 29.0, 30.0, 31.0, 32.0,
        ];

        let mut actual = [0f32; 64];
        let mut expected = [0f32; 64];

        let scale = (2.0f64 / 64.0).sqrt();

        imdct_analytical(&TEST_VECTOR, &mut expected, scale);

        let mut mdct = Imdct::new_scaled(32, scale);
        mdct.imdct(&TEST_VECTOR, &mut actual);

        for i in 0..64 {
            let delta = f64::from(actual[i]) - f64::from(expected[i]);
            assert!(delta.abs() < 0.00001);
        }
    }
}
