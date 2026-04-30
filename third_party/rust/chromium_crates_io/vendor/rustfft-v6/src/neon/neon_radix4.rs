use num_complex::Complex;

use std::any::TypeId;
use std::sync::Arc;

use crate::array_utils::{bitreversed_transpose, workaround_transmute_mut};
use crate::{common::FftNum, FftDirection};
use crate::{Direction, Fft, Length};

use super::NeonNum;

use super::neon_vector::{NeonArray, NeonArrayMut, NeonVector, Rotation90};

/// FFT algorithm optimized for power-of-two sizes, Neon accelerated version.
/// This is designed to be used via a Planner, and not created directly.

pub struct NeonRadix4<N: NeonNum, T> {
    twiddles: Box<[N::VectorType]>,
    rotation: Rotation90<N::VectorType>,

    base_fft: Arc<dyn Fft<T>>,
    base_len: usize,

    len: usize,
    direction: FftDirection,
}

impl<N: NeonNum, T: FftNum> NeonRadix4<N, T> {
    /// Constructs a new NeonRadix4 which computes FFTs of size 4^k * base_fft.len()
    #[inline]
    pub fn new(k: u32, base_fft: Arc<dyn Fft<T>>) -> Self {
        // Internal sanity check: Make sure that S == T.
        // This struct has two generic parameters S and T, but they must always be the same, and are only kept separate to help work around the lack of specialization.
        // It would be cool if we could do this as a static_assert instead
        let id_a = TypeId::of::<N>();
        let id_t = TypeId::of::<T>();
        assert_eq!(id_a, id_t);

        let direction = base_fft.fft_direction();
        let base_len = base_fft.len();

        // note that we can eventually release this restriction - we just need to update the rest of the code in here to handle remainders
        assert!(base_len % (2 * N::VectorType::COMPLEX_PER_VECTOR) == 0 && base_len > 0);

        let len = base_len * (1 << (k * 2));

        // precompute the twiddle factors this algorithm will use.
        // we're doing the same precomputation of twiddle factors as the mixed radix algorithm where width=4 and height=len/4
        // but mixed radix only does one step and then calls itself recusrively, and this algorithm does every layer all the way down
        // so we're going to pack all the "layers" of twiddle factors into a single array, starting with the bottom layer and going up
        const ROW_COUNT: usize = 4;
        let mut cross_fft_len = base_len * ROW_COUNT;
        let mut twiddle_factors = Vec::with_capacity(len * 2);
        while cross_fft_len <= len {
            let num_scalar_columns = cross_fft_len / ROW_COUNT;
            let num_vector_columns = num_scalar_columns / N::VectorType::COMPLEX_PER_VECTOR;

            for i in 0..num_vector_columns {
                for k in 1..ROW_COUNT {
                    unsafe {
                        twiddle_factors.push(NeonVector::make_mixedradix_twiddle_chunk(
                            i * N::VectorType::COMPLEX_PER_VECTOR,
                            k,
                            cross_fft_len,
                            direction,
                        ));
                    }
                }
            }
            cross_fft_len *= ROW_COUNT;
        }

        Self {
            twiddles: twiddle_factors.into_boxed_slice(),
            rotation: unsafe { NeonVector::make_rotate90(direction) },

            base_fft,
            base_len,

            len,
            direction,
        }
    }

    unsafe fn perform_fft_immut(
        &self,
        input: &[Complex<T>],
        output: &mut [Complex<T>],
        _scratch: &mut [Complex<T>],
    ) {
        // copy the data into the output vector
        if self.len() == self.base_len {
            output.copy_from_slice(input);
        } else {
            bitreversed_transpose::<Complex<T>, 4>(self.base_len, input, output);
        }

        // Base-level FFTs
        self.base_fft.process_with_scratch(output, &mut []);

        // cross-FFTs
        const ROW_COUNT: usize = 4;
        let mut cross_fft_len = self.base_len * ROW_COUNT;
        let mut layer_twiddles: &[N::VectorType] = &self.twiddles;

        while cross_fft_len <= input.len() {
            let num_rows = input.len() / cross_fft_len;
            let num_scalar_columns = cross_fft_len / ROW_COUNT;
            let num_vector_columns = num_scalar_columns / N::VectorType::COMPLEX_PER_VECTOR;

            for i in 0..num_rows {
                butterfly_4::<N, T>(
                    &mut output[i * cross_fft_len..],
                    layer_twiddles,
                    num_scalar_columns,
                    &self.rotation,
                )
            }

            // skip past all the twiddle factors used in this layer
            let twiddle_offset = num_vector_columns * (ROW_COUNT - 1);
            layer_twiddles = &layer_twiddles[twiddle_offset..];

            cross_fft_len *= ROW_COUNT;
        }
    }

    //#[target_feature(enable = "neon")]
    unsafe fn perform_fft_out_of_place(
        &self,
        input: &[Complex<T>],
        output: &mut [Complex<T>],
        _scratch: &mut [Complex<T>],
    ) {
        self.perform_fft_immut(input, output, _scratch);
    }
}
boilerplate_fft_neon_oop!(NeonRadix4, |this: &NeonRadix4<_, _>| this.len);

//#[target_feature(enable = "neon")]
unsafe fn butterfly_4<N: NeonNum, T: FftNum>(
    data: &mut [Complex<T>],
    twiddles: &[N::VectorType],
    num_ffts: usize,
    rotation: &Rotation90<N::VectorType>,
) {
    let unroll_offset = N::VectorType::COMPLEX_PER_VECTOR;

    let mut idx = 0usize;
    let mut buffer: &mut [Complex<N>] = workaround_transmute_mut(data);
    for tw in twiddles
        .chunks_exact(6)
        .take(num_ffts / (N::VectorType::COMPLEX_PER_VECTOR * 2))
    {
        let mut scratcha = [
            buffer.load_complex(idx + 0 * num_ffts),
            buffer.load_complex(idx + 1 * num_ffts),
            buffer.load_complex(idx + 2 * num_ffts),
            buffer.load_complex(idx + 3 * num_ffts),
        ];
        let mut scratchb = [
            buffer.load_complex(idx + 0 * num_ffts + unroll_offset),
            buffer.load_complex(idx + 1 * num_ffts + unroll_offset),
            buffer.load_complex(idx + 2 * num_ffts + unroll_offset),
            buffer.load_complex(idx + 3 * num_ffts + unroll_offset),
        ];

        scratcha[1] = NeonVector::mul_complex(scratcha[1], tw[0]);
        scratcha[2] = NeonVector::mul_complex(scratcha[2], tw[1]);
        scratcha[3] = NeonVector::mul_complex(scratcha[3], tw[2]);
        scratchb[1] = NeonVector::mul_complex(scratchb[1], tw[3]);
        scratchb[2] = NeonVector::mul_complex(scratchb[2], tw[4]);
        scratchb[3] = NeonVector::mul_complex(scratchb[3], tw[5]);

        let scratcha = NeonVector::column_butterfly4(scratcha, *rotation);
        let scratchb = NeonVector::column_butterfly4(scratchb, *rotation);

        buffer.store_complex(scratcha[0], idx + 0 * num_ffts);
        buffer.store_complex(scratchb[0], idx + 0 * num_ffts + unroll_offset);
        buffer.store_complex(scratcha[1], idx + 1 * num_ffts);
        buffer.store_complex(scratchb[1], idx + 1 * num_ffts + unroll_offset);
        buffer.store_complex(scratcha[2], idx + 2 * num_ffts);
        buffer.store_complex(scratchb[2], idx + 2 * num_ffts + unroll_offset);
        buffer.store_complex(scratcha[3], idx + 3 * num_ffts);
        buffer.store_complex(scratchb[3], idx + 3 * num_ffts + unroll_offset);

        idx += N::VectorType::COMPLEX_PER_VECTOR * 2;
    }
}

#[cfg(test)]
mod unit_tests {
    use super::*;
    use crate::test_utils::{check_fft_algorithm, construct_base};

    #[test]
    fn test_neon_radix4_64() {
        for base in [2, 4, 6, 8, 12, 16] {
            let base_forward = construct_base(base, FftDirection::Forward);
            let base_inverse = construct_base(base, FftDirection::Inverse);
            for k in 0..4 {
                test_neon_radix4_64_with_base(k, Arc::clone(&base_forward));
                test_neon_radix4_64_with_base(k, Arc::clone(&base_inverse));
            }
        }
    }

    fn test_neon_radix4_64_with_base(k: u32, base_fft: Arc<dyn Fft<f64>>) {
        let len = base_fft.len() * 4usize.pow(k);
        let direction = base_fft.fft_direction();
        let fft = NeonRadix4::<f64, f64>::new(k, base_fft);
        check_fft_algorithm::<f64>(&fft, len, direction);
    }

    #[test]
    fn test_neon_radix4_32() {
        for base in [4, 8, 12, 16] {
            let base_forward = construct_base(base, FftDirection::Forward);
            let base_inverse = construct_base(base, FftDirection::Inverse);
            for k in 0..4 {
                test_neon_radix4_32_with_base(k, Arc::clone(&base_forward));
                test_neon_radix4_32_with_base(k, Arc::clone(&base_inverse));
            }
        }
    }

    fn test_neon_radix4_32_with_base(k: u32, base_fft: Arc<dyn Fft<f32>>) {
        let len = base_fft.len() * 4usize.pow(k);
        let direction = base_fft.fft_direction();
        let fft = NeonRadix4::<f32, f32>::new(k, base_fft);
        check_fft_algorithm::<f32>(&fft, len, direction);
    }
}
