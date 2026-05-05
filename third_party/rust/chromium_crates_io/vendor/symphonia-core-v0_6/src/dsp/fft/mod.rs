// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The `fft` module implements the Fast Fourier Transform (FFT).
//!
//! The complex (I)FFT in this module supports power-of-two sizes up-to 65536.

#[cfg(any(feature = "opt-simd-sse", feature = "opt-simd-avx", feature = "opt-simd-neon"))]
mod simd;

#[cfg(any(feature = "opt-simd-sse", feature = "opt-simd-avx", feature = "opt-simd-neon"))]
pub use simd::*;

#[cfg(not(any(feature = "opt-simd-sse", feature = "opt-simd-avx", feature = "opt-simd-neon")))]
mod no_simd;
#[cfg(not(any(
    feature = "opt-simd-sse",
    feature = "opt-simd-avx",
    feature = "opt-simd-neon"
)))]
pub use no_simd::*;

/// The maximum supported FFT size.
pub const MAX_SIZE: usize = 1 << 16;

#[cfg(test)]
mod tests {
    use std::f64;

    use super::{Fft, Ifft};
    use crate::dsp::complex::Complex;

    /// Compute a naive DFT.
    fn dft_naive(x: &[Complex<f32>], y: &mut [Complex<f32>]) {
        assert_eq!(x.len(), y.len());

        let n = x.len() as u64;

        let theta = 2.0 * f64::consts::PI / (x.len() as f64);

        for (i, y) in y.iter_mut().enumerate() {
            let mut re = 0f64;
            let mut im = 0f64;

            for (j, &x) in x.iter().enumerate() {
                let xre = f64::from(x.re);
                let xim = f64::from(x.im);

                let ij = ((i as u64) * (j as u64)) & (n - 1);

                let wre = (theta * ij as f64).cos();
                let wim = -(theta * ij as f64).sin();

                re += (xre * wre) - (xim * wim);
                im += (xre * wim) + (xim * wre);
            }

            *y = Complex { re: re as f32, im: im as f32 };
        }
    }

    /// Compute a naive IDFT.
    fn idft_naive(x: &[Complex<f32>], y: &mut [Complex<f32>]) {
        let n = x.len() as u64;

        let z = x.iter().map(|x| Complex { re: x.im, im: x.re }).collect::<Vec<Complex<f32>>>();

        dft_naive(&z, y);

        let c = 1.0 / n as f32;

        for y in y.iter_mut() {
            *y = Complex { re: c * y.im, im: c * y.re };
        }
    }

    /// Returns true if both real and imaginary complex number components deviate by less than
    /// `epsilon` between the left-hand side and right-hand side.
    fn check_complex(lhs: Complex<f32>, rhs: Complex<f32>, epsilon: f32) -> bool {
        (lhs.re - rhs.re).abs() < epsilon && (lhs.im - rhs.im).abs() < epsilon
    }

    #[rustfmt::skip]
    const TEST_VECTOR: [Complex<f32>; 64] = [
        Complex { re: -1.82036, im: -0.72591 },
        Complex { re: 1.21002, im: 0.75897 },
        Complex { re: 1.31084, im: -0.51285 },
        Complex { re: 1.26443, im: 1.57430 },
        Complex { re: -1.93680, im: 0.69987 },
        Complex { re: 0.85269, im: -0.20148 },
        Complex { re: 1.10078, im: 0.88904 },
        Complex { re: -1.20634, im: -0.07612 },
        Complex { re: 1.43358, im: -1.91248 },
        Complex { re: 0.10594, im: -0.30743 },
        Complex { re: 1.51258, im: 0.99538 },
        Complex { re: -1.33673, im: 0.23797 },
        Complex { re: 0.43738, im: -1.69900 },
        Complex { re: -0.95355, im: -0.33534 },
        Complex { re: -0.05479, im: -0.32739 },
        Complex { re: -1.85529, im: -1.93157 },
        Complex { re: -1.04220, im: 1.04277 },
        Complex { re: -0.17585, im: 0.40640 },
        Complex { re: 0.09893, im: 1.89538 },
        Complex { re: 1.25018, im: -0.85052 },
        Complex { re: -1.60696, im: -1.41320 },
        Complex { re: -0.25171, im: -0.13830 },
        Complex { re: 1.17782, im: -1.41225 },
        Complex { re: -0.35389, im: -0.30323 },
        Complex { re: -0.16485, im: -0.83675 },
        Complex { re: -1.66729, im: -0.52132 },
        Complex { re: 1.41246, im: 1.58295 },
        Complex { re: -1.84041, im: 0.15331 },
        Complex { re: -1.38897, im: 1.16180 },
        Complex { re: 0.27927, im: -1.84254 },
        Complex { re: -0.46229, im: 0.09699 },
        Complex { re: 1.21027, im: -0.31551 },
        Complex { re: 0.26195, im: -1.19340 },
        Complex { re: 1.60673, im: 1.07094 },
        Complex { re: -0.07456, im: -0.63058 },
        Complex { re: -1.77397, im: 1.39608 },
        Complex { re: -0.80300, im: 0.08858 },
        Complex { re: -0.06289, im: 1.48840 },
        Complex { re: 0.66399, im: 0.49451 },
        Complex { re: -1.49827, im: 1.61856 },
        Complex { re: -1.39006, im: 0.67652 },
        Complex { re: -0.90232, im: 0.18255 },
        Complex { re: 0.00525, im: -1.05797 },
        Complex { re: 0.53688, im: 0.88532 },
        Complex { re: 0.52712, im: -0.58205 },
        Complex { re: -1.77624, im: -0.66799 },
        Complex { re: 1.65335, im: -1.72668 },
        Complex { re: -0.24568, im: 1.50477 },
        Complex { re: -0.15051, im: 0.67824 },
        Complex { re: -1.96744, im: 0.81734 },
        Complex { re: -1.62630, im: -0.73233 },
        Complex { re: -1.98698, im: 0.63824 },
        Complex { re: 0.78115, im: 0.97909 },
        Complex { re: 0.97392, im: 1.82166 },
        Complex { re: 1.30982, im: -1.23975 },
        Complex { re: 0.85813, im: 0.80842 },
        Complex { re: -1.13934, im: 0.81352 },
        Complex { re: -1.22092, im: 0.98348 },
        Complex { re: -1.67949, im: 0.78278 },
        Complex { re: -1.77411, im: 0.00424 },
        Complex { re: 1.93204, im: -0.03273 },
        Complex { re: 1.38529, im: 1.31798 },
        Complex { re: 0.61666, im: -0.09798 },
        Complex { re: 1.02132, im: 1.70293 },
    ];

    #[test]
    fn verify_fft() {
        let mut actual = [Default::default(); TEST_VECTOR.len()];
        let mut expected = [Default::default(); TEST_VECTOR.len()];

        // Expected
        dft_naive(&TEST_VECTOR, &mut expected);

        // Actual
        Fft::new(TEST_VECTOR.len()).fft(&TEST_VECTOR, &mut actual);

        for (&a, &e) in actual.iter().zip(expected.iter()) {
            assert!(check_complex(a, e, 0.00001));
        }
    }

    #[test]
    fn verify_fft_inplace() {
        let mut actual = [Default::default(); TEST_VECTOR.len()];
        let mut expected = [Default::default(); TEST_VECTOR.len()];

        // Expected
        dft_naive(&TEST_VECTOR, &mut expected);

        // Actual
        actual.copy_from_slice(&TEST_VECTOR);
        Fft::new(TEST_VECTOR.len()).fft_inplace(&mut actual);

        for (&a, &e) in actual.iter().zip(expected.iter()) {
            assert!(check_complex(a, e, 0.00001));
        }
    }

    #[test]
    fn verify_ifft() {
        let mut actual = [Default::default(); TEST_VECTOR.len()];
        let mut expected = [Default::default(); TEST_VECTOR.len()];

        // Expected
        idft_naive(&TEST_VECTOR, &mut expected);

        // Actual
        Ifft::new(TEST_VECTOR.len()).ifft(&TEST_VECTOR, &mut actual);

        for (&a, &e) in actual.iter().zip(expected.iter()) {
            assert!(check_complex(a, e, 0.00001));
        }
    }

    #[test]
    fn verify_ifft_inplace() {
        let mut actual = [Default::default(); TEST_VECTOR.len()];
        let mut expected = [Default::default(); TEST_VECTOR.len()];

        // Expected
        idft_naive(&TEST_VECTOR, &mut expected);

        // Actual
        actual.copy_from_slice(&TEST_VECTOR);
        Ifft::new(TEST_VECTOR.len()).ifft_inplace(&mut actual);

        for (&a, &e) in actual.iter().zip(expected.iter()) {
            assert!(check_complex(a, e, 0.00001));
        }
    }

    #[test]
    fn verify_fft_reversible() {
        let mut fft_out = [Default::default(); TEST_VECTOR.len()];
        let mut ifft_out = [Default::default(); TEST_VECTOR.len()];

        let mut fft = Fft::new(TEST_VECTOR.len());
        fft.fft(&TEST_VECTOR, &mut fft_out);
        let mut ifft = Ifft::new(TEST_VECTOR.len());
        ifft.ifft(&fft_out, &mut ifft_out);

        for (&a, &e) in ifft_out.iter().zip(TEST_VECTOR.iter()) {
            assert!(check_complex(a, e, 0.000001));
        }
    }

    #[test]
    fn verify_fft_inplace_reversible() {
        let mut out = [Default::default(); TEST_VECTOR.len()];
        out.copy_from_slice(&TEST_VECTOR);

        let mut fft = Fft::new(TEST_VECTOR.len());
        fft.fft_inplace(&mut out);
        let mut ifft = Ifft::new(TEST_VECTOR.len());
        ifft.ifft_inplace(&mut out);

        for (&a, &e) in out.iter().zip(TEST_VECTOR.iter()) {
            assert!(check_complex(a, e, 0.000001));
        }
    }
}
