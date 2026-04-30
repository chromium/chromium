use std::sync::Arc;

use num_complex::Complex;

use crate::algorithm::butterflies::{
    Butterfly1, Butterfly16, Butterfly2, Butterfly32, Butterfly4, Butterfly8,
};
use crate::algorithm::radixn::butterfly_4;
use crate::array_utils::bitreversed_transpose;
use crate::{common::FftNum, twiddles, FftDirection};
use crate::{Direction, Fft, Length};

/// FFT algorithm optimized for power-of-two sizes
///
/// ~~~
/// // Computes a forward FFT of size 4096
/// use rustfft::algorithm::Radix4;
/// use rustfft::{Fft, FftDirection};
/// use rustfft::num_complex::Complex;
///
/// let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 4096];
///
/// let fft = Radix4::new(4096, FftDirection::Forward);
/// fft.process(&mut buffer);
/// ~~~

pub struct Radix4<T> {
    twiddles: Box<[Complex<T>]>,

    base_fft: Arc<dyn Fft<T>>,
    base_len: usize,

    len: usize,
    direction: FftDirection,
    inplace_scratch_len: usize,
    outofplace_scratch_len: usize,
    immut_scratch_len: usize,
}

impl<T: FftNum> Radix4<T> {
    /// Preallocates necessary arrays and precomputes necessary data to efficiently compute the power-of-two FFT
    pub fn new(len: usize, direction: FftDirection) -> Self {
        assert!(
            len.is_power_of_two(),
            "Radix4 algorithm requires a power-of-two input size. Got {}",
            len
        );

        // figure out which base length we're going to use
        let exponent = len.trailing_zeros();
        let (base_exponent, base_fft) = match exponent {
            0 => (0, Arc::new(Butterfly1::new(direction)) as Arc<dyn Fft<T>>),
            1 => (1, Arc::new(Butterfly2::new(direction)) as Arc<dyn Fft<T>>),
            2 => (2, Arc::new(Butterfly4::new(direction)) as Arc<dyn Fft<T>>),
            3 => (3, Arc::new(Butterfly8::new(direction)) as Arc<dyn Fft<T>>),
            _ => {
                if exponent % 2 == 1 {
                    (5, Arc::new(Butterfly32::new(direction)) as Arc<dyn Fft<T>>)
                } else {
                    (4, Arc::new(Butterfly16::new(direction)) as Arc<dyn Fft<T>>)
                }
            }
        };

        Self::new_with_base((exponent - base_exponent) / 2, base_fft)
    }

    /// Constructs a Radix4 instance which computes FFTs of length `4^k * base_fft.len()`
    pub fn new_with_base(k: u32, base_fft: Arc<dyn Fft<T>>) -> Self {
        let base_len = base_fft.len();
        let len = base_len * (1 << (k * 2));

        let direction = base_fft.fft_direction();

        // precompute the twiddle factors this algorithm will use.
        // we're doing the same precomputation of twiddle factors as the mixed radix algorithm where width=4 and height=len/4
        // but mixed radix only does one step and then calls itself recusrively, and this algorithm does every layer all the way down
        // so we're going to pack all the "layers" of twiddle factors into a single array, starting with the bottom layer and going up
        const ROW_COUNT: usize = 4;
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
            bitreversed_transpose::<Complex<T>, 4>(self.base_len, input, output);
        }

        self.base_fft.process_with_scratch(output, scratch);

        // cross-FFTs
        const ROW_COUNT: usize = 4;
        let mut cross_fft_len = self.base_len;
        let mut layer_twiddles: &[Complex<T>] = &self.twiddles;

        let butterfly4 = Butterfly4::new(self.direction);

        while cross_fft_len < output.len() {
            let num_columns = cross_fft_len;
            cross_fft_len *= ROW_COUNT;

            for data in output.chunks_exact_mut(cross_fft_len) {
                unsafe { butterfly_4(data, layer_twiddles, num_columns, &butterfly4) }
            }

            // skip past all the twiddle factors used in this layer
            let twiddle_offset = num_columns * (ROW_COUNT - 1);
            layer_twiddles = &layer_twiddles[twiddle_offset..];
        }
    }

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
            bitreversed_transpose::<Complex<T>, 4>(self.base_len, input, output);
        }

        // Base-level FFTs
        let base_scratch = if scratch.len() > 0 { scratch } else { input };
        self.base_fft.process_with_scratch(output, base_scratch);

        // cross-FFTs
        const ROW_COUNT: usize = 4;
        let mut cross_fft_len = self.base_len;
        let mut layer_twiddles: &[Complex<T>] = &self.twiddles;

        let butterfly4 = Butterfly4::new(self.direction);

        while cross_fft_len < output.len() {
            let num_columns = cross_fft_len;
            cross_fft_len *= ROW_COUNT;

            for data in output.chunks_exact_mut(cross_fft_len) {
                unsafe { butterfly_4(data, layer_twiddles, num_columns, &butterfly4) }
            }

            // skip past all the twiddle factors used in this layer
            let twiddle_offset = num_columns * (ROW_COUNT - 1);
            layer_twiddles = &layer_twiddles[twiddle_offset..];
        }
    }
}
boilerplate_fft_oop!(Radix4, |this: &Radix4<_>| this.len);

#[cfg(test)]
mod unit_tests {
    use super::*;
    use crate::test_utils::{check_fft_algorithm, construct_base};

    #[test]
    fn test_radix4_with_length() {
        for pow in 0..8 {
            let len = 1 << pow;

            let forward_fft = Radix4::new(len, FftDirection::Forward);
            check_fft_algorithm::<f32>(&forward_fft, len, FftDirection::Forward);

            let inverse_fft = Radix4::new(len, FftDirection::Inverse);
            check_fft_algorithm::<f32>(&inverse_fft, len, FftDirection::Inverse);
        }
    }

    #[test]
    fn test_radix4_with_base() {
        for base in 1..=9 {
            let base_forward = construct_base(base, FftDirection::Forward);
            let base_inverse = construct_base(base, FftDirection::Inverse);

            for k in 0..4 {
                test_radix4(k, Arc::clone(&base_forward));
                test_radix4(k, Arc::clone(&base_inverse));
            }
        }
    }

    fn test_radix4(k: u32, base_fft: Arc<dyn Fft<f64>>) {
        let len = base_fft.len() * 4usize.pow(k as u32);
        let direction = base_fft.fft_direction();
        let fft = Radix4::new_with_base(k, base_fft);

        check_fft_algorithm::<f64>(&fft, len, direction);
    }
}
