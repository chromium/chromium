// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The `fft` module implements the Fast Fourier Transform (FFT).
//!
//! The complex (I)FFT in this module supports a size up-to 65536. The FFT is implemented using the
//! radix-2 Cooley-Tukey algorithm.

use std::{f32, sync::Arc};

use crate::dsp::complex::Complex;
use crate::dsp::fft::MAX_SIZE;

/// The complex Fast Fourier Transform (FFT).
pub struct Fft {
    scratch: Box<[Complex<f32>]>,
    fft: Arc<dyn rustfft::Fft<f32>>,
}

impl Fft {
    pub fn new(n: usize) -> Self {
        // Assert the limits of the FFT interface (shared by SIMD and no-SIMD implementations).
        assert!(n.is_power_of_two());
        assert!(n <= MAX_SIZE);

        let mut planner = rustfft::FftPlanner::<f32>::new();

        let fft = planner.plan_fft_forward(n);

        // Create a scratch buffer to
        let scratch = vec![Default::default(); fft.get_inplace_scratch_len()].into_boxed_slice();

        Self { scratch, fft }
    }

    /// Get the size of the FFT.
    pub fn size(&self) -> usize {
        self.fft.len()
    }

    /// Calculate the FFT.
    pub fn fft(&mut self, x: &[Complex<f32>], y: &mut [Complex<f32>]) {
        assert_eq!(x.len(), y.len());
        assert_eq!(x.len(), self.fft.len());

        // Copy the input buffer, `x`, to the output buffer, `y`, to perform an in-place transform
        // on the output buffer. This keeps the input immutable.
        y.copy_from_slice(x);
        self.fft.process_with_scratch(y, &mut self.scratch);
    }

    /// Calculate the FFT in-place.
    pub fn fft_inplace(&mut self, x: &mut [Complex<f32>]) {
        assert_eq!(x.len(), x.len());
        assert_eq!(x.len(), self.fft.len());
        self.fft.process_with_scratch(x, &mut self.scratch);
    }
}

/// The complex inverse Fast Fourier Transform (IFFT).
pub struct Ifft {
    scratch: Box<[Complex<f32>]>,
    ifft: Arc<dyn rustfft::Fft<f32>>,
}

impl Ifft {
    pub fn new(n: usize) -> Self {
        // Assert the limits of the FFT interface (shared by SIMD and no-SIMD implementations).
        assert!(n.is_power_of_two());
        assert!(n <= MAX_SIZE);

        let mut planner = rustfft::FftPlanner::<f32>::new();

        let ifft = planner.plan_fft_inverse(n);

        let scratch = vec![Default::default(); ifft.get_inplace_scratch_len()].into_boxed_slice();

        Self { scratch, ifft }
    }

    /// Get the size of the FFT.
    pub fn size(&self) -> usize {
        self.ifft.len()
    }

    /// Calculate the inverse FFT.
    pub fn ifft(&mut self, x: &[Complex<f32>], y: &mut [Complex<f32>]) {
        assert_eq!(x.len(), y.len());
        assert_eq!(x.len(), self.ifft.len());

        // Copy the input buffer, `x`, to the output buffer, `y`, to perform an in-place transform
        // on the output buffer. This keeps the input immutable.
        y.copy_from_slice(x);
        self.ifft.process_with_scratch(y, &mut self.scratch);

        // Output scaling.
        let c = 1.0 / y.len() as f32;

        for y in y.iter_mut() {
            *y *= c;
        }
    }

    /// Calculate the inverse FFT in-place.
    pub fn ifft_inplace(&mut self, x: &mut [Complex<f32>]) {
        assert_eq!(x.len(), self.ifft.len());
        self.ifft.process_with_scratch(x, &mut self.scratch);

        // Output scaling.
        let c = 1.0 / x.len() as f32;

        for x in x.iter_mut() {
            *x *= c;
        }
    }
}
