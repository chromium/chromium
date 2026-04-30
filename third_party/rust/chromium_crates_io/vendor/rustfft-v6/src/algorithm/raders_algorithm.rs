use std::sync::Arc;

use num_complex::Complex;
use num_integer::Integer;
use num_traits::Zero;
use primal_check::miller_rabin;
use strength_reduce::StrengthReducedU64;

use crate::math_utils;
use crate::{common::FftNum, twiddles, FftDirection};
use crate::{Direction, Fft, Length};

/// Implementation of Rader's Algorithm
///
/// This algorithm computes a prime-sized FFT in O(nlogn) time. It does this by converting this size-N FFT into a
/// size-(N - 1) FFT, which is guaranteed to be composite.
///
/// The worst case for this algorithm is when (N - 1) is 2 * prime, resulting in a
/// [Cunningham Chain](https://en.wikipedia.org/wiki/Cunningham_chain)
///
/// ~~~
/// // Computes a forward FFT of size 1201 (prime number), using Rader's Algorithm
/// use rustfft::algorithm::RadersAlgorithm;
/// use rustfft::{Fft, FftPlanner};
/// use rustfft::num_complex::Complex;
///
/// let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 1201];
///
/// // plan a FFT of size n - 1 = 1200
/// let mut planner = FftPlanner::new();
/// let inner_fft = planner.plan_fft_forward(1200);
///
/// let fft = RadersAlgorithm::new(inner_fft);
/// fft.process(&mut buffer);
/// ~~~
///
/// Rader's Algorithm is relatively expensive compared to other FFT algorithms. Benchmarking shows that it is up to
/// an order of magnitude slower than similar composite sizes. In the example size above of 1201, benchmarking shows
/// that it takes 2.5x more time to compute than a FFT of size 1200.

pub struct RadersAlgorithm<T> {
    inner_fft: Arc<dyn Fft<T>>,
    inner_fft_data: Box<[Complex<T>]>,

    primitive_root: u64,
    primitive_root_inverse: u64,

    len: StrengthReducedU64,
    inplace_scratch_len: usize,
    outofplace_scratch_len: usize,
    immut_scratch_len: usize,

    direction: FftDirection,
}

impl<T: FftNum> RadersAlgorithm<T> {
    /// Creates a FFT instance which will process inputs/outputs of size `inner_fft.len() + 1`.
    ///
    /// Note that this constructor is quite expensive to run; This algorithm must compute a FFT using `inner_fft` within the
    /// constructor. This further underlines the fact that Rader's Algorithm is more expensive to run than other
    /// FFT algorithms
    ///
    /// # Panics
    /// Panics if `inner_fft.len() + 1` is not a prime number.
    pub fn new(inner_fft: Arc<dyn Fft<T>>) -> Self {
        let inner_fft_len = inner_fft.len();
        let len = inner_fft_len + 1;
        assert!(miller_rabin(len as u64), "For raders algorithm, inner_fft.len() + 1 must be prime. Expected prime number, got {} + 1 = {}", inner_fft_len, len);

        let direction = inner_fft.fft_direction();
        let reduced_len = StrengthReducedU64::new(len as u64);

        // compute the primitive root and its inverse for this size
        let primitive_root = math_utils::primitive_root(len as u64).unwrap();

        // compute the multiplicative inverse of primative_root mod len and vice versa.
        // i64::extended_gcd will compute both the inverse of left mod right, and the inverse of right mod left, but we're only goingto use one of them
        // the primtive root inverse might be negative, if o make it positive by wrapping
        let gcd_data = i64::extended_gcd(&(primitive_root as i64), &(len as i64));
        let primitive_root_inverse = if gcd_data.x >= 0 {
            gcd_data.x
        } else {
            gcd_data.x + len as i64
        } as u64;

        // precompute the coefficients to use inside the process method
        let inner_fft_scale = T::one() / T::from_usize(inner_fft_len).unwrap();
        let mut inner_fft_input = vec![Complex::zero(); inner_fft_len];
        let mut twiddle_input = 1;
        for input_cell in &mut inner_fft_input {
            let twiddle = twiddles::compute_twiddle(twiddle_input, len, direction);
            *input_cell = twiddle * inner_fft_scale;

            twiddle_input =
                ((twiddle_input as u64 * primitive_root_inverse) % reduced_len) as usize;
        }

        let required_inner_scratch = inner_fft.get_inplace_scratch_len();
        let extra_inner_scratch = if required_inner_scratch <= inner_fft_len {
            0
        } else {
            required_inner_scratch
        };
        let inplace_scratch_len = inner_fft_len + extra_inner_scratch;
        let immut_scratch_len = inner_fft_len + required_inner_scratch;

        //precompute a FFT of our reordered twiddle factors
        let mut inner_fft_scratch = vec![Zero::zero(); required_inner_scratch];
        inner_fft.process_with_scratch(&mut inner_fft_input, &mut inner_fft_scratch);

        Self {
            inner_fft,
            inner_fft_data: inner_fft_input.into_boxed_slice(),

            primitive_root,
            primitive_root_inverse,

            len: reduced_len,
            inplace_scratch_len,
            outofplace_scratch_len: extra_inner_scratch,
            immut_scratch_len,
            direction,
        }
    }

    fn perform_fft_immut(
        &self,
        input: &[Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        // The first output element is just the sum of all the input elements, and we need to store off the first input value
        let (output_first, output) = output.split_first_mut().unwrap();
        let (input_first, input) = input.split_first().unwrap();
        let (scratch, extra_scratch) = scratch.split_at_mut(self.len() - 1);

        // copy the input into the scratch space, reordering as we go
        let mut input_index = 1;
        for output_element in scratch.iter_mut() {
            input_index = ((input_index as u64 * self.primitive_root) % self.len) as usize;

            let input_element = input[input_index - 1];
            *output_element = input_element;
        }

        self.inner_fft.process_with_scratch(scratch, extra_scratch);

        // output[0] now contains the sum of elements 1..len. We need the sum of all elements, so all we have to do is add the first input
        *output_first = *input_first + scratch[0];

        // multiply the inner result with our cached setup data
        // also conjugate every entry. this sets us up to do an inverse FFT
        // (because an inverse FFT is equivalent to a normal FFT where you conjugate both the inputs and outputs)
        for (scratch_cell, &twiddle) in scratch.iter_mut().zip(self.inner_fft_data.iter()) {
            *scratch_cell = (*scratch_cell * twiddle).conj();
        }

        // We need to add the first input value to all output values. We can accomplish this by adding it to the DC input of our inner ifft.
        // Of course, we have to conjugate it, just like we conjugated the complex multiplied above
        scratch[0] = scratch[0] + input_first.conj();

        // execute the second FFT
        self.inner_fft.process_with_scratch(scratch, extra_scratch);

        // copy the final values into the output, reordering as we go
        let mut output_index = 1;
        for scratch_element in scratch {
            output_index =
                ((output_index as u64 * self.primitive_root_inverse) % self.len) as usize;
            output[output_index - 1] = scratch_element.conj();
        }
    }

    fn perform_fft_out_of_place(
        &self,
        input: &mut [Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        // The first output element is just the sum of all the input elements, and we need to store off the first input value
        let (output_first, output) = output.split_first_mut().unwrap();
        let (input_first, input) = input.split_first_mut().unwrap();

        // copy the input into the output, reordering as we go. also compute a sum of all elements
        let mut input_index = 1;
        for output_element in output.iter_mut() {
            input_index = ((input_index as u64 * self.primitive_root) % self.len) as usize;

            let input_element = input[input_index - 1];
            *output_element = input_element;
        }

        // perform the first of two inner FFTs
        let inner_scratch = if scratch.len() > 0 {
            &mut scratch[..]
        } else {
            &mut input[..]
        };
        self.inner_fft.process_with_scratch(output, inner_scratch);

        // output[0] now contains the sum of elements 1..len. We need the sum of all elements, so all we have to do is add the first input
        *output_first = *input_first + output[0];

        // multiply the inner result with our cached setup data
        // also conjugate every entry. this sets us up to do an inverse FFT
        // (because an inverse FFT is equivalent to a normal FFT where you conjugate both the inputs and outputs)
        for ((output_cell, input_cell), &multiple) in output
            .iter()
            .zip(input.iter_mut())
            .zip(self.inner_fft_data.iter())
        {
            *input_cell = (*output_cell * multiple).conj();
        }

        // We need to add the first input value to all output values. We can accomplish this by adding it to the DC input of our inner ifft.
        // Of course, we have to conjugate it, just like we conjugated the complex multiplied above
        input[0] = input[0] + input_first.conj();

        // execute the second FFT
        let inner_scratch = if scratch.len() > 0 {
            scratch
        } else {
            &mut output[..]
        };
        self.inner_fft.process_with_scratch(input, inner_scratch);

        // copy the final values into the output, reordering as we go
        let mut output_index = 1;
        for input_element in input {
            output_index =
                ((output_index as u64 * self.primitive_root_inverse) % self.len) as usize;
            output[output_index - 1] = input_element.conj();
        }
    }
    fn perform_fft_inplace(&self, buffer: &mut [Complex<T>], scratch: &mut [Complex<T>]) {
        // The first output element is just the sum of all the input elements, and we need to store off the first input value
        let (buffer_first, buffer) = buffer.split_first_mut().unwrap();
        let buffer_first_val = *buffer_first;

        let (scratch, extra_scratch) = scratch.split_at_mut(self.len() - 1);

        // copy the buffer into the scratch, reordering as we go. also compute a sum of all elements
        let mut input_index = 1;
        for scratch_element in scratch.iter_mut() {
            input_index = ((input_index as u64 * self.primitive_root) % self.len) as usize;

            let buffer_element = buffer[input_index - 1];
            *scratch_element = buffer_element;
        }

        // perform the first of two inner FFTs
        let inner_scratch = if extra_scratch.len() > 0 {
            extra_scratch
        } else {
            &mut buffer[..]
        };
        self.inner_fft.process_with_scratch(scratch, inner_scratch);

        // scratch[0] now contains the sum of elements 1..len. We need the sum of all elements, so all we have to do is add the first input
        *buffer_first = *buffer_first + scratch[0];

        // multiply the inner result with our cached setup data
        // also conjugate every entry. this sets us up to do an inverse FFT
        // (because an inverse FFT is equivalent to a normal FFT where you conjugate both the inputs and outputs)
        for (scratch_cell, &twiddle) in scratch.iter_mut().zip(self.inner_fft_data.iter()) {
            *scratch_cell = (*scratch_cell * twiddle).conj();
        }

        // We need to add the first input value to all output values. We can accomplish this by adding it to the DC input of our inner ifft.
        // Of course, we have to conjugate it, just like we conjugated the complex multiplied above
        scratch[0] = scratch[0] + buffer_first_val.conj();

        // execute the second FFT
        self.inner_fft.process_with_scratch(scratch, inner_scratch);

        // copy the final values into the output, reordering as we go
        let mut output_index = 1;
        for scratch_element in scratch {
            output_index =
                ((output_index as u64 * self.primitive_root_inverse) % self.len) as usize;
            buffer[output_index - 1] = scratch_element.conj();
        }
    }
}
boilerplate_fft!(
    RadersAlgorithm,
    |this: &RadersAlgorithm<_>| this.len.get() as usize,
    |this: &RadersAlgorithm<_>| this.inplace_scratch_len,
    |this: &RadersAlgorithm<_>| this.outofplace_scratch_len,
    |this: &RadersAlgorithm<_>| this.immut_scratch_len
);

#[cfg(test)]
mod unit_tests {
    use super::*;
    use crate::algorithm::Dft;
    use crate::test_utils::check_fft_algorithm;
    use crate::FftPlanner;
    use std::sync::Arc;

    #[test]
    fn test_raders() {
        for len in 3..100 {
            if miller_rabin(len as u64) {
                test_raders_with_length(len, FftDirection::Forward);
                test_raders_with_length(len, FftDirection::Inverse);
            }
        }
    }

    #[test]
    fn test_raders_32bit_overflow() {
        // Construct and use Raders instances for a few large primes
        // that could panic due to overflow errors on 32-bit builds.
        let mut planner = FftPlanner::<f32>::new();
        for len in [112501, 216569, 417623] {
            let inner_fft = planner.plan_fft_forward(len - 1);
            let fft: RadersAlgorithm<f32> = RadersAlgorithm::new(inner_fft);
            let mut data = vec![Complex::new(0.0, 0.0); len];
            fft.process(&mut data);
        }
    }

    fn test_raders_with_length(len: usize, direction: FftDirection) {
        let inner_fft = Arc::new(Dft::new(len - 1, direction));
        let fft = RadersAlgorithm::new(inner_fft);

        check_fft_algorithm::<f32>(&fft, len, direction);
    }
}
