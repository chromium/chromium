use std::sync::Arc;

use num_complex::Complex;

use crate::algorithm::butterflies::{Butterfly1, Butterfly27, Butterfly3, Butterfly9};
use crate::algorithm::radixn::butterfly_3;
use crate::array_utils::{bitreversed_transpose, compute_logarithm};
use crate::{common::FftNum, twiddles, FftDirection};
use crate::{Direction, Fft, Length};

/// FFT algorithm optimized for power-of-three sizes
///
/// ~~~
/// // Computes a forward FFT of size 2187
/// use rustfft::algorithm::Radix3;
/// use rustfft::{Fft, FftDirection};
/// use rustfft::num_complex::Complex;
///
/// let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 2187];
///
/// let fft = Radix3::new(2187, FftDirection::Forward);
/// fft.process(&mut buffer);
/// ~~~

pub struct Radix3<T> {
    twiddles: Box<[Complex<T>]>,
    butterfly3: Butterfly3<T>,

    base_fft: Arc<dyn Fft<T>>,
    base_len: usize,

    len: usize,
    direction: FftDirection,

    inplace_scratch_len: usize,
    outofplace_scratch_len: usize,
    immut_scratch_len: usize,
}

impl<T: FftNum> Radix3<T> {
    /// Preallocates necessary arrays and precomputes necessary data to efficiently compute the power-of-three FFT
    pub fn new(len: usize, direction: FftDirection) -> Self {
        // Compute the total power of 3 for this length. IE, len = 3^exponent
        let exponent = compute_logarithm::<3>(len).unwrap_or_else(|| {
            panic!(
                "Radix3 algorithm requires a power-of-three input size. Got {}",
                len
            )
        });

        // figure out which base length we're going to use
        let (base_exponent, base_fft) = match exponent {
            0 => (0, Arc::new(Butterfly1::new(direction)) as Arc<dyn Fft<T>>),
            1 => (1, Arc::new(Butterfly3::new(direction)) as Arc<dyn Fft<T>>),
            2 => (2, Arc::new(Butterfly9::new(direction)) as Arc<dyn Fft<T>>),
            _ => (3, Arc::new(Butterfly27::new(direction)) as Arc<dyn Fft<T>>),
        };

        Self::new_with_base(exponent - base_exponent, base_fft)
    }

    /// Constructs a Radix3 instance which computes FFTs of length `3^k * base_fft.len()`
    pub fn new_with_base(k: u32, base_fft: Arc<dyn Fft<T>>) -> Self {
        let base_len = base_fft.len();
        let len = base_len * 3usize.pow(k);

        let direction = base_fft.fft_direction();

        // precompute the twiddle factors this algorithm will use.
        // we're doing the same precomputation of twiddle factors as the mixed radix algorithm where width=3 and height=len/3
        // but mixed radix only does one step and then calls itself recusrively, and this algorithm does every layer all the way down
        // so we're going to pack all the "layers" of twiddle factors into a single array, starting with the bottom layer and going up
        const ROW_COUNT: usize = 3;
        let mut cross_fft_len = base_len;
        let mut twiddle_factors = Vec::with_capacity(len * 2);
        while cross_fft_len < len {
            let num_columns = cross_fft_len;
            cross_fft_len *= ROW_COUNT;

            for i in 0..num_columns {
                for k in 1..ROW_COUNT {
                    let twiddle = twiddles::compute_twiddle(i * k, cross_fft_len, direction);
                    twiddle_factors.push(twiddle);
                }
            }
        }

        let base_inplace_scratch = base_fft.get_inplace_scratch_len();
        let inplace_scratch_len = if base_inplace_scratch > cross_fft_len {
            cross_fft_len + base_inplace_scratch
        } else {
            cross_fft_len
        };
        let outofplace_scratch_len = if base_inplace_scratch > len {
            base_inplace_scratch
        } else {
            0
        };

        Self {
            twiddles: twiddle_factors.into_boxed_slice(),
            butterfly3: Butterfly3::new(direction),

            base_fft,
            base_len,

            len,
            direction,

            inplace_scratch_len,
            outofplace_scratch_len,
            immut_scratch_len: base_inplace_scratch,
        }
    }

    fn inplace_scratch_len(&self) -> usize {
        self.inplace_scratch_len
    }
    fn outofplace_scratch_len(&self) -> usize {
        self.outofplace_scratch_len
    }
    fn immut_scratch_len(&self) -> usize {
        self.immut_scratch_len
    }

    fn perform_fft_immut(
        &self,
        input: &[Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        // copy the data into the output vector
        if self.len() == self.base_len {
            output.copy_from_slice(input);
        } else {
            bitreversed_transpose::<Complex<T>, 3>(self.base_len, input, output);
        }

        // Base-level FFTs
        self.base_fft.process_with_scratch(output, scratch);

        // cross-FFTs
        const ROW_COUNT: usize = 3;
        let mut cross_fft_len = self.base_len;
        let mut layer_twiddles: &[Complex<T>] = &self.twiddles;

        while cross_fft_len < output.len() {
            let num_columns = cross_fft_len;
            cross_fft_len *= ROW_COUNT;

            for data in output.chunks_exact_mut(cross_fft_len) {
                unsafe { butterfly_3(data, layer_twiddles, num_columns, &self.butterfly3) }
            }

            // skip past all the twiddle factors used in this layer
            let twiddle_offset = num_columns * (ROW_COUNT - 1);
            layer_twiddles = &layer_twiddles[twiddle_offset..];
        }
    }

    #[inline]
    fn perform_fft_out_of_place(
        &self,
        input: &mut [Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        // copy the data into the output vector
        if self.len() == self.base_len {
            output.copy_from_slice(input);
        } else {
            bitreversed_transpose::<Complex<T>, 3>(self.base_len, input, output);
        }

        // Base-level FFTs
        let base_scratch = if scratch.len() > 0 { scratch } else { input };
        self.base_fft.process_with_scratch(output, base_scratch);

        // cross-FFTs
        const ROW_COUNT: usize = 3;
        let mut cross_fft_len = self.base_len;
        let mut layer_twiddles: &[Complex<T>] = &self.twiddles;

        while cross_fft_len < output.len() {
            let num_columns = cross_fft_len;
            cross_fft_len *= ROW_COUNT;

            for data in output.chunks_exact_mut(cross_fft_len) {
                unsafe { butterfly_3(data, layer_twiddles, num_columns, &self.butterfly3) }
            }

            // skip past all the twiddle factors used in this layer
            let twiddle_offset = num_columns * (ROW_COUNT - 1);
            layer_twiddles = &layer_twiddles[twiddle_offset..];
        }
    }
}
boilerplate_fft_oop!(Radix3, |this: &Radix3<_>| this.len);

#[cfg(test)]
mod unit_tests {
    use super::*;
    use crate::test_utils::{check_fft_algorithm, construct_base};

    #[test]
    fn test_radix3_with_length() {
        for pow in 0..8 {
            let len = 3usize.pow(pow);

            let forward_fft = Radix3::new(len, FftDirection::Forward);
            check_fft_algorithm::<f32>(&forward_fft, len, FftDirection::Forward);

            let inverse_fft = Radix3::new(len, FftDirection::Inverse);
            check_fft_algorithm::<f32>(&inverse_fft, len, FftDirection::Inverse);
        }
    }

    #[test]
    fn test_radix3_with_base() {
        for base in 1..=9 {
            let base_forward = construct_base(base, FftDirection::Forward);
            let base_inverse = construct_base(base, FftDirection::Inverse);

            for k in 0..5 {
                test_radix3(k, Arc::clone(&base_forward));
                test_radix3(k, Arc::clone(&base_inverse));
            }
        }
    }

    fn test_radix3(k: u32, base_fft: Arc<dyn Fft<f32>>) {
        let len = base_fft.len() * 3usize.pow(k as u32);
        let direction = base_fft.fft_direction();
        let fft = Radix3::new_with_base(k, base_fft);

        check_fft_algorithm::<f32>(&fft, len, direction);
    }
}
