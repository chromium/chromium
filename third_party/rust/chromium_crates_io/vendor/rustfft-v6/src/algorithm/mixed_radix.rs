use std::cmp::max;
use std::sync::Arc;

use num_complex::Complex;
use num_traits::Zero;
use transpose;

use crate::array_utils;
use crate::{common::FftNum, twiddles, FftDirection};
use crate::{Direction, Fft, Length};

/// Implementation of the Mixed-Radix FFT algorithm
///
/// This algorithm factors a size n FFT into n1 * n2, computes several inner FFTs of size n1 and n2, then combines the
/// results to get the final answer
///
/// ~~~
/// // Computes a forward FFT of size 1200, using the Mixed-Radix Algorithm
/// use rustfft::algorithm::MixedRadix;
/// use rustfft::{Fft, FftPlanner};
/// use rustfft::num_complex::Complex;
///
/// let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 1200];
///
/// // we need to find an n1 and n2 such that n1 * n2 == 1200
/// // n1 = 30 and n2 = 40 satisfies this
/// let mut planner = FftPlanner::new();
/// let inner_fft_n1 = planner.plan_fft_forward(30);
/// let inner_fft_n2 = planner.plan_fft_forward(40);
///
/// // the mixed radix FFT length will be inner_fft_n1.len() * inner_fft_n2.len() = 1200
/// let fft = MixedRadix::new(inner_fft_n1, inner_fft_n2);
/// fft.process(&mut buffer);
/// ~~~
pub struct MixedRadix<T> {
    twiddles: Box<[Complex<T>]>,

    width_size_fft: Arc<dyn Fft<T>>,
    width: usize,

    height_size_fft: Arc<dyn Fft<T>>,
    height: usize,

    inplace_scratch_len: usize,
    outofplace_scratch_len: usize,
    immut_scratch_len: usize,

    direction: FftDirection,
}

impl<T: FftNum> MixedRadix<T> {
    /// Creates a FFT instance which will process inputs/outputs of size `width_fft.len() * height_fft.len()`
    pub fn new(width_fft: Arc<dyn Fft<T>>, height_fft: Arc<dyn Fft<T>>) -> Self {
        assert_eq!(
            width_fft.fft_direction(), height_fft.fft_direction(),
            "width_fft and height_fft must have the same direction. got width direction={}, height direction={}",
            width_fft.fft_direction(), height_fft.fft_direction());

        let direction = width_fft.fft_direction();

        let width = width_fft.len();
        let height = height_fft.len();

        let len = width * height;

        let mut twiddles = vec![Complex::zero(); len];
        for (x, twiddle_chunk) in twiddles.chunks_exact_mut(height).enumerate() {
            for (y, twiddle_element) in twiddle_chunk.iter_mut().enumerate() {
                *twiddle_element = twiddles::compute_twiddle(x * y, len, direction);
            }
        }

        // Collect some data about what kind of scratch space our inner FFTs need
        let height_inplace_scratch = height_fft.get_inplace_scratch_len();
        let width_inplace_scratch = width_fft.get_inplace_scratch_len();
        let width_outofplace_scratch = width_fft.get_outofplace_scratch_len();

        // Computing the scratch we'll require is a somewhat confusing process.
        // When we compute an out-of-place FFT, both of our inner FFTs are in-place
        // When we compute an inplace FFT, our inner width FFT will be inplace, and our height FFT will be out-of-place
        // For the out-of-place FFT, one of 2 things can happen regarding scratch:
        //      - If the required scratch of both FFTs is <= self.len(), then we can use the input or output buffer as scratch, and so we need 0 extra scratch
        //      - If either of the inner FFTs require more, then we'll have to request an entire scratch buffer for the inner FFTs,
        //          whose size is the max of the two inner FFTs' required scratch
        let max_inner_inplace_scratch = max(height_inplace_scratch, width_inplace_scratch);
        let outofplace_scratch_len = if max_inner_inplace_scratch > len {
            max_inner_inplace_scratch
        } else {
            0
        };

        // For the in-place FFT, again the best case is that we can just bounce data around between internal buffers, and the only inplace scratch we need is self.len()
        // If our width fft's OOP FFT requires any scratch, then we can tack that on the end of our own scratch, and use split_at_mut to separate our own from our internal FFT's
        // Likewise, if our height inplace FFT requires more inplace scracth than self.len(), we can tack that on to the end of our own inplace scratch.
        // Thus, the total inplace scratch is our own length plus the max of what the two inner FFTs will need
        let inplace_scratch_len = len
            + max(
                if height_inplace_scratch > len {
                    height_inplace_scratch
                } else {
                    0
                },
                width_outofplace_scratch,
            );

        let immut_scratch_len = max(
            len + width_fft.get_inplace_scratch_len(),
            height_fft.get_inplace_scratch_len(),
        );

        Self {
            twiddles: twiddles.into_boxed_slice(),

            width_size_fft: width_fft,
            width: width,

            height_size_fft: height_fft,
            height: height,

            inplace_scratch_len,
            outofplace_scratch_len,
            immut_scratch_len,

            direction,
        }
    }

    fn perform_fft_inplace(&self, buffer: &mut [Complex<T>], scratch: &mut [Complex<T>]) {
        // SIX STEP FFT:
        let (scratch, inner_scratch) = scratch.split_at_mut(self.len());

        // STEP 1: transpose
        transpose::transpose(buffer, scratch, self.width, self.height);

        // STEP 2: perform FFTs of size `height`
        let height_scratch = if inner_scratch.len() > buffer.len() {
            &mut inner_scratch[..]
        } else {
            &mut buffer[..]
        };
        self.height_size_fft
            .process_with_scratch(scratch, height_scratch);

        // STEP 3: Apply twiddle factors
        for (element, twiddle) in scratch.iter_mut().zip(self.twiddles.iter()) {
            *element = *element * twiddle;
        }

        // STEP 4: transpose again
        transpose::transpose(scratch, buffer, self.height, self.width);

        // STEP 5: perform FFTs of size `width`
        self.width_size_fft
            .process_outofplace_with_scratch(buffer, scratch, inner_scratch);

        // STEP 6: transpose again
        transpose::transpose(scratch, buffer, self.width, self.height);
    }

    fn perform_fft_immut(
        &self,
        input: &[Complex<T>],
        output: &mut [Complex<T>],
        scratch_raw: &mut [Complex<T>],
    ) {
        // STEP 1: transpose
        transpose::transpose(input, output, self.width, self.height);

        // STEP 2: perform FFTs of size `height`
        self.height_size_fft
            .process_with_scratch(output, scratch_raw);

        // STEP 3: Apply twiddle factors
        for (element, twiddle) in output.iter_mut().zip(self.twiddles.iter()) {
            *element = *element * twiddle;
        }

        let (scratch, inner_scratch) = scratch_raw.split_at_mut(self.len());

        // STEP 4: transpose again
        transpose::transpose(output, scratch, self.height, self.width);

        // STEP 5: perform FFTs of size `width`
        self.width_size_fft
            .process_with_scratch(scratch, inner_scratch);

        // STEP 6: transpose again
        transpose::transpose(scratch, output, self.width, self.height);
    }

    fn perform_fft_out_of_place(
        &self,
        input: &mut [Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        // SIX STEP FFT:

        // STEP 1: transpose
        transpose::transpose(input, output, self.width, self.height);

        // STEP 2: perform FFTs of size `height`
        let height_scratch = if scratch.len() > input.len() {
            &mut scratch[..]
        } else {
            &mut input[..]
        };
        self.height_size_fft
            .process_with_scratch(output, height_scratch);

        // STEP 3: Apply twiddle factors
        for (element, twiddle) in output.iter_mut().zip(self.twiddles.iter()) {
            *element = *element * twiddle;
        }

        // STEP 4: transpose again
        transpose::transpose(output, input, self.height, self.width);

        // STEP 5: perform FFTs of size `width`
        let width_scratch = if scratch.len() > output.len() {
            &mut scratch[..]
        } else {
            &mut output[..]
        };
        self.width_size_fft
            .process_with_scratch(input, width_scratch);

        // STEP 6: transpose again
        transpose::transpose(input, output, self.width, self.height);
    }
}
boilerplate_fft!(
    MixedRadix,
    |this: &MixedRadix<_>| this.twiddles.len(),
    |this: &MixedRadix<_>| this.inplace_scratch_len,
    |this: &MixedRadix<_>| this.outofplace_scratch_len,
    |this: &MixedRadix<_>| this.immut_scratch_len
);

/// Implementation of the Mixed-Radix FFT algorithm, specialized for smaller input sizes
///
/// This algorithm factors a size n FFT into n1 * n2, computes several inner FFTs of size n1 and n2, then combines the
/// results to get the final answer
///
/// ~~~
/// // Computes a forward FFT of size 40 using MixedRadixSmall
/// use std::sync::Arc;
/// use rustfft::algorithm::MixedRadixSmall;
/// use rustfft::algorithm::butterflies::{Butterfly5, Butterfly8};
/// use rustfft::{Fft, FftDirection};
/// use rustfft::num_complex::Complex;
///
/// let len = 40;
///
/// let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; len];
///
/// // we need to find an n1 and n2 such that n1 * n2 == 40
/// // n1 = 5 and n2 = 8 satisfies this
/// let inner_fft_n1 = Arc::new(Butterfly5::new(FftDirection::Forward));
/// let inner_fft_n2 = Arc::new(Butterfly8::new(FftDirection::Forward));
///
/// // the mixed radix FFT length will be inner_fft_n1.len() * inner_fft_n2.len() = 40
/// let fft = MixedRadixSmall::new(inner_fft_n1, inner_fft_n2);
/// fft.process(&mut buffer);
/// ~~~
pub struct MixedRadixSmall<T> {
    twiddles: Box<[Complex<T>]>,

    width_size_fft: Arc<dyn Fft<T>>,
    width: usize,

    height_size_fft: Arc<dyn Fft<T>>,
    height: usize,

    direction: FftDirection,
}

impl<T: FftNum> MixedRadixSmall<T> {
    /// Creates a FFT instance which will process inputs/outputs of size `width_fft.len() * height_fft.len()`
    pub fn new(width_fft: Arc<dyn Fft<T>>, height_fft: Arc<dyn Fft<T>>) -> Self {
        assert_eq!(
            width_fft.fft_direction(), height_fft.fft_direction(),
            "width_fft and height_fft must have the same direction. got width direction={}, height direction={}",
            width_fft.fft_direction(), height_fft.fft_direction());

        // Verify that the inner FFTs don't require out-of-place scratch, and only arequire a small amount of inplace scratch
        let width = width_fft.len();
        let height = height_fft.len();
        let len = width * height;

        assert_eq!(width_fft.get_outofplace_scratch_len(), 0, "MixedRadixSmall should only be used with algorithms that require 0 out-of-place scratch. Width FFT (len={}) requires {}, should require 0", width, width_fft.get_outofplace_scratch_len());
        assert_eq!(height_fft.get_outofplace_scratch_len(), 0, "MixedRadixSmall should only be used with algorithms that require 0 out-of-place scratch. Height FFT (len={}) requires {}, should require 0", height, height_fft.get_outofplace_scratch_len());

        assert!(width_fft.get_inplace_scratch_len() <= width, "MixedRadixSmall should only be used with algorithms that require little inplace scratch. Width FFT (len={}) requires {}, should require {} or less", width, width_fft.get_inplace_scratch_len(), width);
        assert!(height_fft.get_inplace_scratch_len() <= height, "MixedRadixSmall should only be used with algorithms that require little inplace scratch. Height FFT (len={}) requires {}, should require {} or less", height, height_fft.get_inplace_scratch_len(), height);

        let direction = width_fft.fft_direction();

        let mut twiddles = vec![Complex::zero(); len];
        for (x, twiddle_chunk) in twiddles.chunks_exact_mut(height).enumerate() {
            for (y, twiddle_element) in twiddle_chunk.iter_mut().enumerate() {
                *twiddle_element = twiddles::compute_twiddle(x * y, len, direction);
            }
        }

        Self {
            twiddles: twiddles.into_boxed_slice(),

            width_size_fft: width_fft,
            width: width,

            height_size_fft: height_fft,
            height: height,

            direction,
        }
    }

    fn perform_fft_inplace(&self, buffer: &mut [Complex<T>], scratch: &mut [Complex<T>]) {
        // SIX STEP FFT:
        // STEP 1: transpose
        unsafe { array_utils::transpose_small(self.width, self.height, buffer, scratch) };

        // STEP 2: perform FFTs of size `height`
        self.height_size_fft.process_with_scratch(scratch, buffer);

        // STEP 3: Apply twiddle factors
        for (element, twiddle) in scratch.iter_mut().zip(self.twiddles.iter()) {
            *element = *element * twiddle;
        }

        // STEP 4: transpose again
        unsafe { array_utils::transpose_small(self.height, self.width, scratch, buffer) };

        // STEP 5: perform FFTs of size `width`
        self.width_size_fft
            .process_outofplace_with_scratch(buffer, scratch, &mut []);

        // STEP 6: transpose again
        unsafe { array_utils::transpose_small(self.width, self.height, scratch, buffer) };
    }

    fn perform_fft_immut(
        &self,
        input: &[Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        // SIX STEP FFT:
        // STEP 1: transpose
        unsafe { array_utils::transpose_small(self.width, self.height, input, output) };

        // STEP 2: perform FFTs of size `height`
        self.height_size_fft.process_with_scratch(output, scratch);

        // STEP 3: Apply twiddle factors
        for (element, twiddle) in output.iter_mut().zip(self.twiddles.iter()) {
            *element = *element * twiddle;
        }

        // STEP 4: transpose again
        unsafe { array_utils::transpose_small(self.height, self.width, output, scratch) };

        // STEP 5: perform FFTs of size `width`
        self.width_size_fft.process_with_scratch(scratch, output);

        // STEP 6: transpose again
        unsafe { array_utils::transpose_small(self.width, self.height, scratch, output) };
    }

    fn perform_fft_out_of_place(
        &self,
        input: &mut [Complex<T>],
        output: &mut [Complex<T>],
        _scratch: &mut [Complex<T>],
    ) {
        // SIX STEP FFT:
        // STEP 1: transpose
        unsafe { array_utils::transpose_small(self.width, self.height, input, output) };

        // STEP 2: perform FFTs of size `height`
        self.height_size_fft.process_with_scratch(output, input);

        // STEP 3: Apply twiddle factors
        for (element, twiddle) in output.iter_mut().zip(self.twiddles.iter()) {
            *element = *element * twiddle;
        }

        // STEP 4: transpose again
        unsafe { array_utils::transpose_small(self.height, self.width, output, input) };

        // STEP 5: perform FFTs of size `width`
        self.width_size_fft.process_with_scratch(input, output);

        // STEP 6: transpose again
        unsafe { array_utils::transpose_small(self.width, self.height, input, output) };
    }
}
boilerplate_fft!(
    MixedRadixSmall,
    |this: &MixedRadixSmall<_>| this.twiddles.len(),
    |this: &MixedRadixSmall<_>| this.len(),
    |_| 0,
    |this: &MixedRadixSmall<_>| this.len()
);

#[cfg(test)]
mod unit_tests {
    use super::*;
    use crate::test_utils::check_fft_algorithm;
    use crate::{algorithm::Dft, test_utils::BigScratchAlgorithm};
    use num_traits::Zero;
    use std::sync::Arc;

    #[test]
    fn test_mixed_radix() {
        for width in 1..7 {
            for height in 1..7 {
                test_mixed_radix_with_lengths(width, height, FftDirection::Forward);
                test_mixed_radix_with_lengths(width, height, FftDirection::Inverse);
            }
        }
    }

    #[test]
    fn test_mixed_radix_small() {
        for width in 2..7 {
            for height in 2..7 {
                test_mixed_radix_small_with_lengths(width, height, FftDirection::Forward);
                test_mixed_radix_small_with_lengths(width, height, FftDirection::Inverse);
            }
        }
    }

    fn test_mixed_radix_with_lengths(width: usize, height: usize, direction: FftDirection) {
        let width_fft = Arc::new(Dft::new(width, direction)) as Arc<dyn Fft<f32>>;
        let height_fft = Arc::new(Dft::new(height, direction)) as Arc<dyn Fft<f32>>;

        let fft = MixedRadix::new(width_fft, height_fft);

        check_fft_algorithm(&fft, width * height, direction);
    }

    fn test_mixed_radix_small_with_lengths(width: usize, height: usize, direction: FftDirection) {
        let width_fft = Arc::new(Dft::new(width, direction)) as Arc<dyn Fft<f32>>;
        let height_fft = Arc::new(Dft::new(height, direction)) as Arc<dyn Fft<f32>>;

        let fft = MixedRadixSmall::new(width_fft, height_fft);

        check_fft_algorithm(&fft, width * height, direction);
    }

    // Verify that the mixed radix algorithm correctly provides scratch space to inner FFTs
    #[test]
    fn test_mixed_radix_inner_scratch() {
        let scratch_lengths = [1, 5, 25];

        let mut inner_ffts = Vec::new();

        for &len in &scratch_lengths {
            for &inplace_scratch in &scratch_lengths {
                for &outofplace_scratch in &scratch_lengths {
                    for &immut_scratch in &scratch_lengths {
                        inner_ffts.push(Arc::new(BigScratchAlgorithm {
                            len,
                            inplace_scratch,
                            outofplace_scratch,
                            immut_scratch,
                            direction: FftDirection::Forward,
                        }) as Arc<dyn Fft<f32>>);
                    }
                }
            }
        }

        for width_fft in inner_ffts.iter() {
            for height_fft in inner_ffts.iter() {
                let fft = MixedRadix::new(Arc::clone(width_fft), Arc::clone(height_fft));

                let mut inplace_buffer = vec![Complex::zero(); fft.len()];
                let mut inplace_scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];

                fft.process_with_scratch(&mut inplace_buffer, &mut inplace_scratch);

                let mut outofplace_input = vec![Complex::zero(); fft.len()];
                let mut outofplace_output = vec![Complex::zero(); fft.len()];
                let mut outofplace_scratch =
                    vec![Complex::zero(); fft.get_outofplace_scratch_len()];
                fft.process_outofplace_with_scratch(
                    &mut outofplace_input,
                    &mut outofplace_output,
                    &mut outofplace_scratch,
                );

                let immut_input = vec![Complex::zero(); fft.len()];
                let mut immut_output = vec![Complex::zero(); fft.len()];
                let mut immut_scratch = vec![Complex::zero(); fft.get_immutable_scratch_len()];

                fft.process_immutable_with_scratch(
                    &immut_input,
                    &mut immut_output,
                    &mut immut_scratch,
                );
            }
        }
    }
}
