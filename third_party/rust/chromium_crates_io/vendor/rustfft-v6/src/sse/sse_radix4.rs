use num_complex::Complex;

use std::any::TypeId;
use std::sync::Arc;

use crate::array_utils::{bitreversed_transpose, workaround_transmute_mut};
use crate::{common::FftNum, FftDirection};
use crate::{Direction, Fft, Length};

use super::SseNum;

use super::sse_vector::{Rotation90, SseArray, SseArrayMut, SseVector};

/// FFT algorithm optimized for power-of-two sizes, SSE accelerated version.
/// This is designed to be used via a Planner, and not created directly.

pub struct SseRadix4<S: SseNum, T> {
    twiddles: Box<[S::VectorType]>,
    rotation: Rotation90<S::VectorType>,

    base_fft: Arc<dyn Fft<T>>,
    base_len: usize,

    len: usize,
    direction: FftDirection,
}

impl<S: SseNum, T: FftNum> SseRadix4<S, T> {
    /// Constructs a new SseRadix4 which computes FFTs of size 4^k * base_fft.len()
    #[inline]
    pub fn new(k: u32, base_fft: Arc<dyn Fft<T>>) -> Result<Self, ()> {
        // Internal sanity check: Make sure that S == T.
        // This struct has two generic parameters S and T, but they must always be the same, and are only kept separate to help work around the lack of specialization.
        // It would be cool if we could do this as a static_assert instead
        let id_a = TypeId::of::<S>();
        let id_t = TypeId::of::<T>();
        assert_eq!(id_a, id_t);

        let has_sse = is_x86_feature_detected!("sse4.1");
        if has_sse {
            // Safety: new_with_sse requires the "sse4.1" feature set. Since we know it's present, we're safe
            Ok(unsafe { Self::new_with_sse(k, base_fft) })
        } else {
            Err(())
        }
    }

    #[target_feature(enable = "sse4.1")]
    unsafe fn new_with_sse(k: u32, base_fft: Arc<dyn Fft<T>>) -> Self {
        let direction = base_fft.fft_direction();
        let base_len = base_fft.len();

        // note that we can eventually release this restriction - we just need to update the rest of the code in here to handle remainders
        assert!(base_len % (2 * S::VectorType::COMPLEX_PER_VECTOR) == 0 && base_len > 0);

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
            let num_vector_columns = num_scalar_columns / S::VectorType::COMPLEX_PER_VECTOR;

            for i in 0..num_vector_columns {
                for k in 1..ROW_COUNT {
                    twiddle_factors.push(SseVector::make_mixedradix_twiddle_chunk(
                        i * S::VectorType::COMPLEX_PER_VECTOR,
                        k,
                        cross_fft_len,
                        direction,
                    ));
                }
            }
            cross_fft_len *= ROW_COUNT;
        }

        Self {
            twiddles: twiddle_factors.into_boxed_slice(),
            rotation: SseVector::make_rotate90(direction),

            base_fft,
            base_len,

            len,
            direction,
        }
    }

    #[target_feature(enable = "sse4.1")]
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
        let mut layer_twiddles: &[S::VectorType] = &self.twiddles;

        while cross_fft_len <= input.len() {
            let num_rows = input.len() / cross_fft_len;
            let num_scalar_columns = cross_fft_len / ROW_COUNT;
            let num_vector_columns = num_scalar_columns / S::VectorType::COMPLEX_PER_VECTOR;

            for i in 0..num_rows {
                butterfly_4::<S, T>(
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

    #[target_feature(enable = "sse4.1")]
    unsafe fn perform_fft_out_of_place(
        &self,
        input: &[Complex<T>],
        output: &mut [Complex<T>],
        _scratch: &mut [Complex<T>],
    ) {
        self.perform_fft_immut(input, output, _scratch);
    }
}
boilerplate_fft_sse_oop!(SseRadix4, |this: &SseRadix4<_, _>| this.len);

#[target_feature(enable = "sse4.1")]
unsafe fn butterfly_4<S: SseNum, T: FftNum>(
    data: &mut [Complex<T>],
    twiddles: &[S::VectorType],
    num_ffts: usize,
    rotation: &Rotation90<S::VectorType>,
) {
    let unroll_offset = S::VectorType::COMPLEX_PER_VECTOR;

    let mut idx = 0usize;
    let mut buffer: &mut [Complex<S>] = workaround_transmute_mut(data);
    for tw in twiddles
        .chunks_exact(6)
        .take(num_ffts / (S::VectorType::COMPLEX_PER_VECTOR * 2))
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

        scratcha[1] = SseVector::mul_complex(scratcha[1], tw[0]);
        scratcha[2] = SseVector::mul_complex(scratcha[2], tw[1]);
        scratcha[3] = SseVector::mul_complex(scratcha[3], tw[2]);
        scratchb[1] = SseVector::mul_complex(scratchb[1], tw[3]);
        scratchb[2] = SseVector::mul_complex(scratchb[2], tw[4]);
        scratchb[3] = SseVector::mul_complex(scratchb[3], tw[5]);

        let scratcha = SseVector::column_butterfly4(scratcha, *rotation);
        let scratchb = SseVector::column_butterfly4(scratchb, *rotation);

        buffer.store_complex(scratcha[0], idx + 0 * num_ffts);
        buffer.store_complex(scratchb[0], idx + 0 * num_ffts + unroll_offset);
        buffer.store_complex(scratcha[1], idx + 1 * num_ffts);
        buffer.store_complex(scratchb[1], idx + 1 * num_ffts + unroll_offset);
        buffer.store_complex(scratcha[2], idx + 2 * num_ffts);
        buffer.store_complex(scratchb[2], idx + 2 * num_ffts + unroll_offset);
        buffer.store_complex(scratcha[3], idx + 3 * num_ffts);
        buffer.store_complex(scratchb[3], idx + 3 * num_ffts + unroll_offset);

        idx += S::VectorType::COMPLEX_PER_VECTOR * 2;
    }
}

#[cfg(test)]
mod unit_tests {
    use super::*;
    use crate::test_utils::{check_fft_algorithm, construct_base};

    #[test]
    fn test_sse_radix4_64() {
        for base in [2, 4, 6, 8, 12, 16] {
            let base_forward = construct_base(base, FftDirection::Forward);
            let base_inverse = construct_base(base, FftDirection::Inverse);
            for k in 0..4 {
                test_sse_radix4_64_with_base(k, Arc::clone(&base_forward));
                test_sse_radix4_64_with_base(k, Arc::clone(&base_inverse));
            }
        }
    }

    fn test_sse_radix4_64_with_base(k: u32, base_fft: Arc<dyn Fft<f64>>) {
        let len = base_fft.len() * 4usize.pow(k);
        let direction = base_fft.fft_direction();
        let fft = SseRadix4::<f64, f64>::new(k, base_fft).unwrap();
        check_fft_algorithm::<f64>(&fft, len, direction);
    }

    #[test]
    fn test_sse_radix4_32() {
        for base in [4, 8, 12, 16] {
            let base_forward = construct_base(base, FftDirection::Forward);
            let base_inverse = construct_base(base, FftDirection::Inverse);
            for k in 0..4 {
                test_sse_radix4_32_with_base(k, Arc::clone(&base_forward));
                test_sse_radix4_32_with_base(k, Arc::clone(&base_inverse));
            }
        }
    }

    fn test_sse_radix4_32_with_base(k: u32, base_fft: Arc<dyn Fft<f32>>) {
        let len = base_fft.len() * 4usize.pow(k);
        let direction = base_fft.fft_direction();
        let fft = SseRadix4::<f32, f32>::new(k, base_fft).unwrap();
        check_fft_algorithm::<f32>(&fft, len, direction);
    }
}
