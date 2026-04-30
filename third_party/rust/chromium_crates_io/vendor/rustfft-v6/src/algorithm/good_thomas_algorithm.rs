use std::cmp::max;
use std::sync::Arc;

use num_complex::Complex;
use num_integer::Integer;
use strength_reduce::StrengthReducedUsize;
use transpose;

use crate::array_utils;
use crate::{common::FftNum, FftDirection};
use crate::{Direction, Fft, Length};

/// Implementation of the [Good-Thomas Algorithm (AKA Prime Factor Algorithm)](https://en.wikipedia.org/wiki/Prime-factor_FFT_algorithm)
///
/// This algorithm factors a size n FFT into n1 * n2, where GCD(n1, n2) == 1
///
/// Conceptually, this algorithm is very similar to the Mixed-Radix, except because GCD(n1, n2) == 1 we can do some
/// number theory trickery to reduce the number of floating-point multiplications and additions. Additionally, It can
/// be faster than Mixed-Radix at sizes below 10,000 or so.
///
/// ~~~
/// // Computes a forward FFT of size 1200, using the Good-Thomas Algorithm
/// use rustfft::algorithm::GoodThomasAlgorithm;
/// use rustfft::{Fft, FftPlanner};
/// use rustfft::num_complex::Complex;
/// use rustfft::num_traits::Zero;
///
/// let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 1200];
///
/// // we need to find an n1 and n2 such that n1 * n2 == 1200 and GCD(n1, n2) == 1
/// // n1 = 48 and n2 = 25 satisfies this
/// let mut planner = FftPlanner::new();
/// let inner_fft_n1 = planner.plan_fft_forward(48);
/// let inner_fft_n2 = planner.plan_fft_forward(25);
///
/// // the good-thomas FFT length will be inner_fft_n1.len() * inner_fft_n2.len() = 1200
/// let fft = GoodThomasAlgorithm::new(inner_fft_n1, inner_fft_n2);
/// fft.process(&mut buffer);
/// ~~~
pub struct GoodThomasAlgorithm<T> {
    width: usize,
    width_size_fft: Arc<dyn Fft<T>>,

    height: usize,
    height_size_fft: Arc<dyn Fft<T>>,

    reduced_width: StrengthReducedUsize,
    reduced_width_plus_one: StrengthReducedUsize,

    inplace_scratch_len: usize,
    outofplace_scratch_len: usize,
    immut_scratch_len: usize,

    len: usize,
    direction: FftDirection,
}

impl<T: FftNum> GoodThomasAlgorithm<T> {
    /// Creates a FFT instance which will process inputs/outputs of size `width_fft.len() * height_fft.len()`
    ///
    /// `GCD(width_fft.len(), height_fft.len())` must be equal to 1
    pub fn new(mut width_fft: Arc<dyn Fft<T>>, mut height_fft: Arc<dyn Fft<T>>) -> Self {
        assert_eq!(
            width_fft.fft_direction(), height_fft.fft_direction(),
            "width_fft and height_fft must have the same direction. got width direction={}, height direction={}",
            width_fft.fft_direction(), height_fft.fft_direction());

        let mut width = width_fft.len();
        let mut height = height_fft.len();
        let direction = width_fft.fft_direction();

        // This algorithm doesn't work if width and height aren't coprime
        let gcd = num_integer::gcd(width as i64, height as i64);
        assert!(gcd == 1,
                "Invalid width and height for Good-Thomas Algorithm (width={}, height={}): Inputs must be coprime",
                width,
                height);

        // The trick we're using for our index remapping will only work if width < height, so just swap them if it isn't
        if width > height {
            std::mem::swap(&mut width, &mut height);
            std::mem::swap(&mut width_fft, &mut height_fft);
        }

        let len = width * height;

        // Collect some data about what kind of scratch space our inner FFTs need
        let width_inplace_scratch = width_fft.get_inplace_scratch_len();
        let height_inplace_scratch = height_fft.get_inplace_scratch_len();
        let height_outofplace_scratch = height_fft.get_outofplace_scratch_len();

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
        // If our height fft's OOP FFT requires any scratch, then we can tack that on the end of our own scratch, and use split_at_mut to separate our own from our internal FFT's
        // Likewise, if our width inplace FFT requires more inplace scracth than self.len(), we can tack that on to the end of our own inplace scratch.
        // Thus, the total inplace scratch is our own length plus the max of what the two inner FFTs will need
        let inplace_scratch_len = len
            + max(
                if width_inplace_scratch > len {
                    width_inplace_scratch
                } else {
                    0
                },
                height_outofplace_scratch,
            );

        let immut_scratch_len = max(
            width_fft.get_inplace_scratch_len(),
            len + height_fft.get_inplace_scratch_len(),
        );

        Self {
            width,
            width_size_fft: width_fft,

            height,
            height_size_fft: height_fft,

            reduced_width: StrengthReducedUsize::new(width),
            reduced_width_plus_one: StrengthReducedUsize::new(width + 1),

            inplace_scratch_len,
            outofplace_scratch_len,
            immut_scratch_len,

            len,
            direction,
        }
    }

    fn reindex_input(&self, source: &[Complex<T>], destination: &mut [Complex<T>]) {
        // A critical part of the good-thomas algorithm is re-indexing the inputs and outputs.
        // To remap the inputs, we will use the CRT mapping, paired with the normal transpose we'd do for mixed radix.
        //
        // The algorithm for the CRT mapping will work like this:
        // 1: Keep an output index, initialized to 0
        // 2: The output index will be incremented by width + 1
        // 3: At the start of the row, compute if we will increment output_index past self.len()
        //      3a: If we will, then compute exactly how many increments it will take,
        //      3b: Increment however many times as we scan over the input row, copying each element to the output index
        //      3c: Subtract self.len() from output_index
        // 4: Scan over the rest of the row, incrementing output_index, and copying each element to output_index, thne incrementing output_index
        // 5: The first index of each row will be the final index of the previous row plus one, but because of our incrementing (width+1) inside the loop, we overshot, so at the end of the row, subtract width from output_index
        //
        // This ends up producing the same result as computing the multiplicative inverse of width mod height and etc by the CRT mapping, but with only one integer division per row, instead of one per element.
        let mut destination_index = 0;
        for mut source_row in source.chunks_exact(self.width) {
            let increments_until_cycle =
                1 + (self.len() - destination_index) / self.reduced_width_plus_one;

            // If we will have to rollover output_index on this row, do it in a separate loop
            if increments_until_cycle < self.width {
                let (pre_cycle_row, post_cycle_row) = source_row.split_at(increments_until_cycle);

                for input_element in pre_cycle_row {
                    destination[destination_index] = *input_element;
                    destination_index += self.reduced_width_plus_one.get();
                }

                // Store the split slice back to input_row, os that outside the loop, we can finish the job of iterating the row
                source_row = post_cycle_row;
                destination_index -= self.len();
            }

            // Loop over the entire row (if we did not roll over) or what's left of the row (if we did) and keep incrementing output_row
            for input_element in source_row {
                destination[destination_index] = *input_element;
                destination_index += self.reduced_width_plus_one.get();
            }

            // The first index of the next will be the final index this row, plus one.
            // But because of our incrementing (width+1) inside the loop above, we overshot, so subtract width, and we'll get (width + 1) - width = 1
            destination_index -= self.width;
        }
    }

    fn reindex_output(&self, source: &[Complex<T>], destination: &mut [Complex<T>]) {
        // A critical part of the good-thomas algorithm is re-indexing the inputs and outputs.
        // To remap the outputs, we will use the ruritanian mapping, paired with the normal transpose we'd do for mixed radix.
        //
        // The algorithm for the ruritanian mapping will work like this:
        // 1: At the start of every row, compute the output index = (y * self.height) % self.width
        // 2: We will increment this output index by self.width for every element
        // 3: Compute where in the row the output index will wrap around
        // 4: Instead of starting copying from the beginning of the row, start copying from after the rollover point
        // 5: When we hit the end of the row, continue from the beginning of the row, continuing to increment the output index by self.width
        //
        // This achieves the same result as the modular arithmetic ofthe ruritanian mapping, but with only one integer divison per row, instead of one per element
        for (y, source_chunk) in source.chunks_exact(self.height).enumerate() {
            let (quotient, remainder) =
                StrengthReducedUsize::div_rem(y * self.height, self.reduced_width);

            // Compute our base index and starting point in the row
            let mut destination_index = remainder;
            let start_x = self.height - quotient;

            // Process the first part of the row
            for x in start_x..self.height {
                destination[destination_index] = source_chunk[x];
                destination_index += self.width;
            }

            // Wrap back around to the beginning of the row and keep incrementing
            for x in 0..start_x {
                destination[destination_index] = source_chunk[x];
                destination_index += self.width;
            }
        }
    }

    fn perform_fft_inplace(&self, buffer: &mut [Complex<T>], scratch: &mut [Complex<T>]) {
        let (scratch, inner_scratch) = scratch.split_at_mut(self.len());

        // Re-index the input, copying from the buffer to the scratch in the process
        self.reindex_input(buffer, scratch);

        // run FFTs of size `width`
        let width_scratch = if inner_scratch.len() > buffer.len() {
            &mut inner_scratch[..]
        } else {
            &mut buffer[..]
        };
        self.width_size_fft
            .process_with_scratch(scratch, width_scratch);

        // transpose
        transpose::transpose(scratch, buffer, self.width, self.height);

        // run FFTs of size 'height'
        self.height_size_fft
            .process_outofplace_with_scratch(buffer, scratch, inner_scratch);

        // Re-index the output, copying from the scratch to the buffer in the process
        self.reindex_output(scratch, buffer);
    }

    fn perform_fft_immut(
        &self,
        input: &[Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        // Re-index the input, copying from the input to the output in the process
        self.reindex_input(input, output);

        // run FFTs of size `width`
        self.width_size_fft.process_with_scratch(output, scratch);

        let (scratch, inner_scratch) = scratch.split_at_mut(self.len());

        // transpose
        transpose::transpose(output, scratch, self.width, self.height);

        // run FFTs of size 'height'
        self.height_size_fft
            .process_with_scratch(scratch, inner_scratch);

        // Re-index the output, copying from the input to the output in the process
        self.reindex_output(scratch, output);
    }

    fn perform_fft_out_of_place(
        &self,
        input: &mut [Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        // Re-index the input, copying from the input to the output in the process
        self.reindex_input(input, output);

        // run FFTs of size `width`
        let width_scratch = if scratch.len() > input.len() {
            &mut scratch[..]
        } else {
            &mut input[..]
        };
        self.width_size_fft
            .process_with_scratch(output, width_scratch);

        // transpose
        transpose::transpose(output, input, self.width, self.height);

        // run FFTs of size 'height'
        let height_scratch = if scratch.len() > output.len() {
            &mut scratch[..]
        } else {
            &mut output[..]
        };
        self.height_size_fft
            .process_with_scratch(input, height_scratch);

        // Re-index the output, copying from the input to the output in the process
        self.reindex_output(input, output);
    }
}
boilerplate_fft!(
    GoodThomasAlgorithm,
    |this: &GoodThomasAlgorithm<_>| this.len,
    |this: &GoodThomasAlgorithm<_>| this.inplace_scratch_len,
    |this: &GoodThomasAlgorithm<_>| this.outofplace_scratch_len,
    |this: &GoodThomasAlgorithm<_>| this.immut_scratch_len
);

/// Implementation of the Good-Thomas Algorithm, specialized for smaller input sizes
///
/// This algorithm factors a size n FFT into n1 * n2, where GCD(n1, n2) == 1
///
/// Conceptually, this algorithm is very similar to MixedRadix, except because GCD(n1, n2) == 1 we can do some
/// number theory trickery to reduce the number of floating point operations. It typically performs
/// better than MixedRadixSmall, especially at the smallest sizes.
///
/// ~~~
/// // Computes a forward FFT of size 56 using GoodThomasAlgorithmSmall
/// use std::sync::Arc;
/// use rustfft::algorithm::GoodThomasAlgorithmSmall;
/// use rustfft::algorithm::butterflies::{Butterfly7, Butterfly8};
/// use rustfft::{Fft, FftDirection};
/// use rustfft::num_complex::Complex;
///
/// let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 56];
///
/// // we need to find an n1 and n2 such that n1 * n2 == 56 and GCD(n1, n2) == 1
/// // n1 = 7 and n2 = 8 satisfies this
/// let inner_fft_n1 = Arc::new(Butterfly7::new(FftDirection::Forward));
/// let inner_fft_n2 = Arc::new(Butterfly8::new(FftDirection::Forward));
///
/// // the good-thomas FFT length will be inner_fft_n1.len() * inner_fft_n2.len() = 56
/// let fft = GoodThomasAlgorithmSmall::new(inner_fft_n1, inner_fft_n2);
/// fft.process(&mut buffer);
/// ~~~
pub struct GoodThomasAlgorithmSmall<T> {
    width: usize,
    width_size_fft: Arc<dyn Fft<T>>,

    height: usize,
    height_size_fft: Arc<dyn Fft<T>>,

    input_output_map: Box<[usize]>,

    direction: FftDirection,
}

impl<T: FftNum> GoodThomasAlgorithmSmall<T> {
    /// Creates a FFT instance which will process inputs/outputs of size `width_fft.len() * height_fft.len()`
    ///
    /// `GCD(width_fft.len(), height_fft.len())` must be equal to 1
    pub fn new(width_fft: Arc<dyn Fft<T>>, height_fft: Arc<dyn Fft<T>>) -> Self {
        assert_eq!(
            width_fft.fft_direction(), height_fft.fft_direction(),
            "n1_fft and height_fft must have the same direction. got width direction={}, height direction={}",
            width_fft.fft_direction(), height_fft.fft_direction());

        let width = width_fft.len();
        let height = height_fft.len();
        let len = width * height;

        assert_eq!(width_fft.get_outofplace_scratch_len(), 0, "GoodThomasAlgorithmSmall should only be used with algorithms that require 0 out-of-place scratch. Width FFT (len={}) requires {}, should require 0", width, width_fft.get_outofplace_scratch_len());
        assert_eq!(height_fft.get_outofplace_scratch_len(), 0, "GoodThomasAlgorithmSmall should only be used with algorithms that require 0 out-of-place scratch. Height FFT (len={}) requires {}, should require 0", height, height_fft.get_outofplace_scratch_len());

        assert!(width_fft.get_inplace_scratch_len() <= width, "GoodThomasAlgorithmSmall should only be used with algorithms that require little inplace scratch. Width FFT (len={}) requires {}, should require {} or less", width, width_fft.get_inplace_scratch_len(), width);
        assert!(height_fft.get_inplace_scratch_len() <= height, "GoodThomasAlgorithmSmall should only be used with algorithms that require little inplace scratch. Height FFT (len={}) requires {}, should require {} or less", height, height_fft.get_inplace_scratch_len(), height);

        // compute the multiplicative inverse of width mod height and vice versa. x will be width mod height, and y will be height mod width
        let gcd_data = i64::extended_gcd(&(width as i64), &(height as i64));
        assert!(gcd_data.gcd == 1,
                "Invalid input width and height to Good-Thomas Algorithm: ({},{}): Inputs must be coprime",
                width,
                height);

        // width_inverse or height_inverse might be negative, make it positive by wrapping
        let width_inverse = if gcd_data.x >= 0 {
            gcd_data.x
        } else {
            gcd_data.x + height as i64
        } as usize;
        let height_inverse = if gcd_data.y >= 0 {
            gcd_data.y
        } else {
            gcd_data.y + width as i64
        } as usize;

        // NOTE: we are precomputing the input and output reordering indexes, because benchmarking shows that it's 10-20% faster
        // If we wanted to optimize for memory use or setup time instead of multiple-FFT speed, we could compute these on the fly in the perform_fft() method
        let input_iter = (0..len)
            .map(|i| (i % width, i / width))
            .map(|(x, y)| (x * height + y * width) % len);
        let output_iter = (0..len).map(|i| (i % height, i / height)).map(|(y, x)| {
            (x * height * height_inverse as usize + y * width * width_inverse as usize) % len
        });

        let input_output_map: Vec<usize> = input_iter.chain(output_iter).collect();

        Self {
            direction: width_fft.fft_direction(),

            width,
            width_size_fft: width_fft,

            height,
            height_size_fft: height_fft,

            input_output_map: input_output_map.into_boxed_slice(),
        }
    }

    fn perform_fft_immut(
        &self,
        input: &[Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        // These asserts are for the unsafe blocks down below. we're relying on the optimizer to get rid of this assert
        assert_eq!(self.len(), input.len());
        assert_eq!(self.len(), output.len());

        let (input_map, output_map) = self.input_output_map.split_at(self.len());

        // copy the input using our reordering mapping
        for (output_element, &input_index) in output.iter_mut().zip(input_map.iter()) {
            *output_element = input[input_index];
        }

        // run FFTs of size `width`
        self.width_size_fft.process_with_scratch(output, scratch);

        // transpose
        unsafe { array_utils::transpose_small(self.width, self.height, output, scratch) };

        // run FFTs of size 'height'
        self.height_size_fft.process_with_scratch(scratch, output);

        // copy to the output, using our output redordeing mapping
        for (input_element, &output_index) in scratch.iter().zip(output_map.iter()) {
            output[output_index] = *input_element;
        }
    }

    fn perform_fft_out_of_place(
        &self,
        input: &mut [Complex<T>],
        output: &mut [Complex<T>],
        _scratch: &mut [Complex<T>],
    ) {
        // These asserts are for the unsafe blocks down below. we're relying on the optimizer to get rid of this assert
        assert_eq!(self.len(), input.len());
        assert_eq!(self.len(), output.len());

        let (input_map, output_map) = self.input_output_map.split_at(self.len());

        // copy the input using our reordering mapping
        for (output_element, &input_index) in output.iter_mut().zip(input_map.iter()) {
            *output_element = input[input_index];
        }

        // run FFTs of size `width`
        self.width_size_fft.process_with_scratch(output, input);

        // transpose
        unsafe { array_utils::transpose_small(self.width, self.height, output, input) };

        // run FFTs of size 'height'
        self.height_size_fft.process_with_scratch(input, output);

        // copy to the output, using our output redordeing mapping
        for (input_element, &output_index) in input.iter().zip(output_map.iter()) {
            output[output_index] = *input_element;
        }
    }

    fn perform_fft_inplace(&self, buffer: &mut [Complex<T>], scratch: &mut [Complex<T>]) {
        // These asserts are for the unsafe blocks down below. we're relying on the optimizer to get rid of this assert
        assert_eq!(self.len(), buffer.len());
        assert_eq!(self.len(), scratch.len());

        let (input_map, output_map) = self.input_output_map.split_at(self.len());

        // copy the input using our reordering mapping
        for (output_element, &input_index) in scratch.iter_mut().zip(input_map.iter()) {
            *output_element = buffer[input_index];
        }

        // run FFTs of size `width`
        self.width_size_fft.process_with_scratch(scratch, buffer);

        // transpose
        unsafe { array_utils::transpose_small(self.width, self.height, scratch, buffer) };

        // run FFTs of size 'height'
        self.height_size_fft
            .process_outofplace_with_scratch(buffer, scratch, &mut []);

        // copy to the output, using our output redordeing mapping
        for (input_element, &output_index) in scratch.iter().zip(output_map.iter()) {
            buffer[output_index] = *input_element;
        }
    }
}
boilerplate_fft!(
    GoodThomasAlgorithmSmall,
    |this: &GoodThomasAlgorithmSmall<_>| this.width * this.height,
    |this: &GoodThomasAlgorithmSmall<_>| this.len(),
    |_| 0,
    |this: &GoodThomasAlgorithmSmall<_>| this.len()
);

#[cfg(test)]
mod unit_tests {
    use super::*;
    use crate::test_utils::check_fft_algorithm;
    use crate::{algorithm::Dft, test_utils::BigScratchAlgorithm};
    use num_integer::gcd;
    use num_traits::Zero;
    use std::sync::Arc;

    #[test]
    fn test_good_thomas() {
        for width in 1..12 {
            for height in 1..12 {
                if gcd(width, height) == 1 {
                    test_good_thomas_with_lengths(width, height, FftDirection::Forward);
                    test_good_thomas_with_lengths(width, height, FftDirection::Inverse);
                }
            }
        }
    }

    #[test]
    fn test_good_thomas_small() {
        let butterfly_sizes = [2, 3, 4, 5, 6, 7, 8, 16];
        for width in &butterfly_sizes {
            for height in &butterfly_sizes {
                if gcd(*width, *height) == 1 {
                    test_good_thomas_small_with_lengths(*width, *height, FftDirection::Forward);
                    test_good_thomas_small_with_lengths(*width, *height, FftDirection::Inverse);
                }
            }
        }
    }

    fn test_good_thomas_with_lengths(width: usize, height: usize, direction: FftDirection) {
        let width_fft = Arc::new(Dft::new(width, direction)) as Arc<dyn Fft<f32>>;
        let height_fft = Arc::new(Dft::new(height, direction)) as Arc<dyn Fft<f32>>;

        let fft = GoodThomasAlgorithm::new(width_fft, height_fft);

        check_fft_algorithm(&fft, width * height, direction);
    }

    fn test_good_thomas_small_with_lengths(width: usize, height: usize, direction: FftDirection) {
        let width_fft = Arc::new(Dft::new(width, direction)) as Arc<dyn Fft<f32>>;
        let height_fft = Arc::new(Dft::new(height, direction)) as Arc<dyn Fft<f32>>;

        let fft = GoodThomasAlgorithmSmall::new(width_fft, height_fft);

        check_fft_algorithm(&fft, width * height, direction);
    }

    #[test]
    fn test_output_mapping() {
        let width = 15;
        for height in 3..width {
            if gcd(width, height) == 1 {
                let width_fft =
                    Arc::new(Dft::new(width, FftDirection::Forward)) as Arc<dyn Fft<f32>>;
                let height_fft =
                    Arc::new(Dft::new(height, FftDirection::Forward)) as Arc<dyn Fft<f32>>;

                let fft = GoodThomasAlgorithm::new(width_fft, height_fft);

                let mut buffer = vec![Complex { re: 0.0, im: 0.0 }; fft.len()];

                fft.process(&mut buffer);
            }
        }
    }

    // Verify that the Good-Thomas algorithm correctly provides scratch space to inner FFTs
    #[test]
    fn test_good_thomas_inner_scratch() {
        let scratch_lengths = [1, 5, 24];

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
                if width_fft.len() == height_fft.len() {
                    continue;
                }

                let fft = GoodThomasAlgorithm::new(Arc::clone(width_fft), Arc::clone(height_fft));

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
