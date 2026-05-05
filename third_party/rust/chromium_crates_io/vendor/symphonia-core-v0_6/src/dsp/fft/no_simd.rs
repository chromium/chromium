// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::convert::TryInto;
use std::f32;

use lazy_static::lazy_static;

use crate::dsp::complex::Complex;
use crate::dsp::fft::MAX_SIZE;

macro_rules! fft_twiddle_table {
    ($bi:expr, $name:ident) => {
        lazy_static! {
            static ref $name: Box<[Complex<f32>; (1 << $bi) >> 1]> = {
                const N: usize = 1 << $bi;
                const TABLE_SIZE: usize = N >> 1;
                let theta = std::f64::consts::PI / TABLE_SIZE as f64;

                let table: Vec<Complex<f32>> = (0..TABLE_SIZE)
                    .map(|k| {
                        let angle = theta * k as f64;
                        Complex::new(angle.cos() as f32, -angle.sin() as f32)
                    })
                    .collect();

                // UNWRAP: The vector was initialized to be the correct size.
                table.into_boxed_slice().try_into().unwrap()
            };
        }
    };
}

fft_twiddle_table!(6, FFT_TWIDDLE_TABLE_64);
fft_twiddle_table!(7, FFT_TWIDDLE_TABLE_128);
fft_twiddle_table!(8, FFT_TWIDDLE_TABLE_256);
fft_twiddle_table!(9, FFT_TWIDDLE_TABLE_512);
fft_twiddle_table!(10, FFT_TWIDDLE_TABLE_1024);
fft_twiddle_table!(11, FFT_TWIDDLE_TABLE_2048);
fft_twiddle_table!(12, FFT_TWIDDLE_TABLE_4096);
fft_twiddle_table!(13, FFT_TWIDDLE_TABLE_8192);
fft_twiddle_table!(14, FFT_TWIDDLE_TABLE_16384);
fft_twiddle_table!(15, FFT_TWIDDLE_TABLE_32768);
fft_twiddle_table!(16, FFT_TWIDDLE_TABLE_65536);

/// Get the twiddle factors for a FFT of size `n`.
fn fft_twiddle_factors(n: usize) -> &'static [Complex<f32>] {
    // FFT sizes <= 32 use unrolled FFT implementations with hard-coded twiddle factors.
    match n {
        64 => FFT_TWIDDLE_TABLE_64.as_ref(),
        128 => FFT_TWIDDLE_TABLE_128.as_ref(),
        256 => FFT_TWIDDLE_TABLE_256.as_ref(),
        512 => FFT_TWIDDLE_TABLE_512.as_ref(),
        1024 => FFT_TWIDDLE_TABLE_1024.as_ref(),
        2048 => FFT_TWIDDLE_TABLE_2048.as_ref(),
        4096 => FFT_TWIDDLE_TABLE_4096.as_ref(),
        8192 => FFT_TWIDDLE_TABLE_8192.as_ref(),
        16384 => FFT_TWIDDLE_TABLE_16384.as_ref(),
        32768 => FFT_TWIDDLE_TABLE_32768.as_ref(),
        65536 => FFT_TWIDDLE_TABLE_65536.as_ref(),
        _ => panic!("fft size is invalid"),
    }
}

/// The complex Fast Fourier Transform (FFT).
pub struct Fft {
    perm: Box<[u16]>,
}

impl Fft {
    pub fn new(n: usize) -> Self {
        // The FFT size must be a power of two.
        assert!(n.is_power_of_two());
        // The permutation table uses 16-bit indicies. Therefore, the absolute maximum FFT size is
        // limited to 2^16.
        assert!(n <= MAX_SIZE);

        // Calculate the bit reversal table.
        let n = n as u16;
        let shift = n.leading_zeros() + 1;
        let perm = (0..n).map(|i| i.reverse_bits() >> shift).collect();

        Self { perm }
    }

    /// Get the size of the FFT.
    pub fn size(&self) -> usize {
        self.perm.len()
    }

    /// Calculate the FFT in-place.
    pub fn fft_inplace(&mut self, x: &mut [Complex<f32>]) {
        let n = x.len();
        assert_eq!(n, x.len());
        assert_eq!(n, self.perm.len());

        for (i, &j) in self.perm.iter().enumerate() {
            let j = usize::from(j);

            if i < j {
                x.swap(i, j);
            }
        }

        // Start FFT recursion.
        match n {
            1 => (),
            2 => fft2(x.try_into().unwrap()),
            4 => fft4(x.try_into().unwrap()),
            8 => fft8(x.try_into().unwrap()),
            16 => fft16(x.try_into().unwrap()),
            _ => transform(x, n),
        }
    }

    /// Calculate the FFT.
    pub fn fft(&mut self, x: &[Complex<f32>], y: &mut [Complex<f32>]) {
        let n = x.len();
        assert_eq!(n, y.len());
        assert_eq!(n, self.perm.len());

        // Bit reversal using pre-computed permutation table.
        for (x, y) in self.perm.iter().map(|&i| x[usize::from(i)]).zip(y.iter_mut()) {
            *y = x;
        }

        // Start FFT recursion.
        match n {
            1 => (),
            2 => fft2(y.try_into().unwrap()),
            4 => fft4(y.try_into().unwrap()),
            8 => fft8(y.try_into().unwrap()),
            16 => fft16(y.try_into().unwrap()),
            _ => transform(y, n),
        }
    }
}

/// The inverse complex Fast Fourier Transform (FFT).
pub struct Ifft {
    perm: Box<[u16]>,
}

impl Ifft {
    pub fn new(n: usize) -> Self {
        // The FFT size must be a power of two.
        assert!(n.is_power_of_two());
        // The permutation table uses 16-bit indicies. Therefore, the absolute maximum FFT size is
        // limited to 2^16.
        assert!(n <= MAX_SIZE);

        // Calculate the bit reversal table.
        let n = n as u16;
        let shift = n.leading_zeros() + 1;
        let perm = (0..n).map(|i| i.reverse_bits() >> shift).collect();

        Self { perm }
    }

    /// Get the size of the FFT.
    pub fn size(&self) -> usize {
        self.perm.len()
    }

    /// Calculate the inverse FFT.
    pub fn ifft(&mut self, x: &[Complex<f32>], y: &mut [Complex<f32>]) {
        let n = x.len();
        assert_eq!(n, y.len());
        assert_eq!(n, self.perm.len());

        // Bit reversal using pre-computed permutation table.
        for (x, y) in self.perm.iter().map(|&i| x[usize::from(i)]).zip(y.iter_mut()) {
            *y = Complex { re: x.im, im: x.re };
        }

        // Do the forward FFT.
        transform(y, n);

        // Output scaling.
        let c = 1.0 / n as f32;

        for y in y.iter_mut() {
            *y = Complex { re: c * y.im, im: c * y.re };
        }
    }

    /// Calculate the inverse FFT in-place.
    pub fn ifft_inplace(&mut self, x: &mut [Complex<f32>]) {
        let n = x.len();
        assert_eq!(n, self.perm.len());

        // Bit reversal using pre-computed permutation table.
        for (i, &j) in self.perm.iter().enumerate() {
            let j = usize::from(j);

            if i <= j {
                // Swap real and imaginary components while swapping for bit-reversal.
                let xi = x[i];
                let xj = x[j];
                x[i] = Complex::new(xj.im, xj.re);
                x[j] = Complex::new(xi.im, xi.re);
            }
        }

        // Do the forward FFT.
        transform(x, n);

        // Output scaling.
        let c = 1.0 / n as f32;

        for x in x.iter_mut() {
            *x = Complex { re: c * x.im, im: c * x.re };
        }
    }
}

fn transform(x: &mut [Complex<f32>], n: usize) {
    fn merge(even: &mut [Complex<f32>], odd: &mut [Complex<f32>], twiddles: &[Complex<f32>]) {
        for ((e, o), w) in
            even.chunks_exact_mut(2).zip(odd.chunks_exact_mut(2)).zip(twiddles.chunks_exact(2))
        {
            let p0 = e[0];
            let q0 = o[0] * w[0];

            e[0] = p0 + q0;
            o[0] = p0 - q0;

            let p1 = e[1];
            let q1 = o[1] * w[1];

            e[1] = p1 + q1;
            o[1] = p1 - q1;
        }
    }

    if let Ok(x) = x.try_into() {
        // N is exactly 32.
        fft32(x);
    }
    else {
        // N is > 32. Therefore, N must be >= 64. Begin a breadth-first FFT over 64-point chunks
        // using 32-point halves.
        let mut step = 32;

        // The first iteration must dispatch to a specialized 32-point FFT function. This can be
        // considered the base case of the breadth-first FFT.
        {
            // Twiddle factors used when merging the two halves of each chunk.
            let twiddles = fft_twiddle_factors(step << 1);

            // Iterate over adjacent chunks containing the two halves that will be merged.
            for pair in x.chunks_exact_mut(step << 1) {
                // Split the chunk into two adjacent halves and compute the FFT of each.
                let (even, odd) = pair.split_at_mut(step);
                fft32(even.try_into().unwrap());
                fft32(odd.try_into().unwrap());

                // Merge the two adjacent halves back into one chunk.
                merge(even, odd, twiddles);
            }

            // The next iteration will be over the adjacent chunks computed in this iteration.
            step <<= 1;
        }

        // Each subsequent iteration is large enough that there is no need for a specialized FFT
        // function.
        while step < n {
            let twiddles = fft_twiddle_factors(step << 1);
            for pair in x.chunks_exact_mut(step << 1) {
                let (even, odd) = pair.split_at_mut(step);
                merge(even, odd, twiddles);
            }
            step <<= 1;
        }
    }
}

macro_rules! complex {
    ($re:expr, $im:expr) => {
        Complex { re: $re, im: $im }
    };
}

fn fft32(x: &mut [Complex<f32>; 32]) {
    let mut x0 = [
        x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], x[8], x[9], x[10], x[11], x[12], x[13],
        x[14], x[15],
    ];
    let mut x1 = [
        x[16], x[17], x[18], x[19], x[20], x[21], x[22], x[23], x[24], x[25], x[26], x[27], x[28],
        x[29], x[30], x[31],
    ];

    fft16(&mut x0);
    fft16(&mut x1);

    let a4 = f32::consts::FRAC_1_SQRT_2 * x1[4].re;
    let b4 = f32::consts::FRAC_1_SQRT_2 * x1[4].im;
    let a12 = -f32::consts::FRAC_1_SQRT_2 * x1[12].re;
    let b12 = -f32::consts::FRAC_1_SQRT_2 * x1[12].im;

    let x1p = [
        x1[0],
        complex!(0.98078528040323044913, -0.19509032201612826785) * x1[1],
        complex!(0.92387953251128675613, -0.38268343236508977173) * x1[2],
        complex!(0.83146961230254523708, -0.55557023301960222474) * x1[3],
        complex!(a4 + b4, b4 - a4),
        complex!(0.55557023301960222474, -0.83146961230254523708) * x1[5],
        complex!(0.38268343236508977173, -0.92387953251128675613) * x1[6],
        complex!(0.19509032201612826785, -0.98078528040323044913) * x1[7],
        complex!(x1[8].im, -x1[8].re),
        complex!(-0.19509032201612826785, -0.98078528040323044913) * x1[9],
        complex!(-0.38268343236508977173, -0.92387953251128675613) * x1[10],
        complex!(-0.55557023301960222474, -0.83146961230254523708) * x1[11],
        complex!(a12 - b12, a12 + b12),
        complex!(-0.83146961230254523708, -0.55557023301960222474) * x1[13],
        complex!(-0.92387953251128675613, -0.38268343236508977173) * x1[14],
        complex!(-0.98078528040323044913, -0.19509032201612826785) * x1[15],
    ];

    x[0] = x0[0] + x1p[0];
    x[1] = x0[1] + x1p[1];
    x[2] = x0[2] + x1p[2];
    x[3] = x0[3] + x1p[3];
    x[4] = x0[4] + x1p[4];
    x[5] = x0[5] + x1p[5];
    x[6] = x0[6] + x1p[6];
    x[7] = x0[7] + x1p[7];
    x[8] = x0[8] + x1p[8];
    x[9] = x0[9] + x1p[9];
    x[10] = x0[10] + x1p[10];
    x[11] = x0[11] + x1p[11];
    x[12] = x0[12] + x1p[12];
    x[13] = x0[13] + x1p[13];
    x[14] = x0[14] + x1p[14];
    x[15] = x0[15] + x1p[15];

    x[16] = x0[0] - x1p[0];
    x[17] = x0[1] - x1p[1];
    x[18] = x0[2] - x1p[2];
    x[19] = x0[3] - x1p[3];
    x[20] = x0[4] - x1p[4];
    x[21] = x0[5] - x1p[5];
    x[22] = x0[6] - x1p[6];
    x[23] = x0[7] - x1p[7];
    x[24] = x0[8] - x1p[8];
    x[25] = x0[9] - x1p[9];
    x[26] = x0[10] - x1p[10];
    x[27] = x0[11] - x1p[11];
    x[28] = x0[12] - x1p[12];
    x[29] = x0[13] - x1p[13];
    x[30] = x0[14] - x1p[14];
    x[31] = x0[15] - x1p[15];
}

#[inline(always)]
fn fft16(x: &mut [Complex<f32>; 16]) {
    let mut x0 = [x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7]];
    let mut x1 = [x[8], x[9], x[10], x[11], x[12], x[13], x[14], x[15]];

    fft8(&mut x0);
    fft8(&mut x1);

    let a2 = f32::consts::FRAC_1_SQRT_2 * x1[2].re;
    let b2 = f32::consts::FRAC_1_SQRT_2 * x1[2].im;
    let a6 = -f32::consts::FRAC_1_SQRT_2 * x1[6].re;
    let b6 = -f32::consts::FRAC_1_SQRT_2 * x1[6].im;

    let x1p = [
        x1[0],
        complex!(0.92387953251128675613, -0.38268343236508977173) * x1[1],
        complex!(a2 + b2, b2 - a2),
        complex!(0.38268343236508977173, -0.92387953251128675613) * x1[3],
        complex!(x1[4].im, -x1[4].re),
        complex!(-0.38268343236508977173, -0.92387953251128675613) * x1[5],
        complex!(a6 - b6, a6 + b6),
        complex!(-0.92387953251128675613, -0.38268343236508977173) * x1[7],
    ];

    x[0] = x0[0] + x1p[0];
    x[1] = x0[1] + x1p[1];
    x[2] = x0[2] + x1p[2];
    x[3] = x0[3] + x1p[3];
    x[4] = x0[4] + x1p[4];
    x[5] = x0[5] + x1p[5];
    x[6] = x0[6] + x1p[6];
    x[7] = x0[7] + x1p[7];

    x[8] = x0[0] - x1p[0];
    x[9] = x0[1] - x1p[1];
    x[10] = x0[2] - x1p[2];
    x[11] = x0[3] - x1p[3];
    x[12] = x0[4] - x1p[4];
    x[13] = x0[5] - x1p[5];
    x[14] = x0[6] - x1p[6];
    x[15] = x0[7] - x1p[7];
}

#[inline(always)]
fn fft8(x: &mut [Complex<f32>; 8]) {
    let mut x0 = [x[0], x[1], x[2], x[3]];
    let mut x1 = [x[4], x[5], x[6], x[7]];

    fft4(&mut x0);
    fft4(&mut x1);

    let a1 = f32::consts::FRAC_1_SQRT_2 * x1[1].re;
    let b1 = f32::consts::FRAC_1_SQRT_2 * x1[1].im;
    let a3 = -f32::consts::FRAC_1_SQRT_2 * x1[3].re;
    let b3 = -f32::consts::FRAC_1_SQRT_2 * x1[3].im;

    let x1p = [
        x1[0],
        complex!(a1 + b1, b1 - a1),
        complex!(x1[2].im, -x1[2].re),
        complex!(a3 - b3, a3 + b3),
    ];

    x[0] = x0[0] + x1p[0];
    x[1] = x0[1] + x1p[1];
    x[2] = x0[2] + x1p[2];
    x[3] = x0[3] + x1p[3];

    x[4] = x0[0] - x1p[0];
    x[5] = x0[1] - x1p[1];
    x[6] = x0[2] - x1p[2];
    x[7] = x0[3] - x1p[3];
}

#[inline(always)]
fn fft4(x: &mut [Complex<f32>; 4]) {
    let x0 = [x[0] + x[1], x[0] - x[1]];
    let x1 = [x[2] + x[3], x[2] - x[3]];

    let x1p1 = complex!(x1[1].im, -x1[1].re);

    x[0] = x0[0] + x1[0];
    x[1] = x0[1] + x1p1;

    x[2] = x0[0] - x1[0];
    x[3] = x0[1] - x1p1;
}

#[inline(always)]
fn fft2(x: &mut [Complex<f32>; 2]) {
    let x0 = x[0];
    x[0] = x0 + x[1];
    x[1] = x0 - x[1];
}
