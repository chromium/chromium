use std::sync::Arc;

use num_complex::Complex;
use num_traits::Zero;

use crate::{common::FftNum, twiddles, FftDirection};
use crate::{Direction, Fft, Length};

/// Implementation of Bluestein's Algorithm
///
/// This algorithm computes an arbitrary-sized FFT in O(nlogn) time. It does this by converting this size-N FFT into a
/// size-M FFT where M >= 2N - 1.
///
/// The choice of M is very important for the performance of Bluestein's Algorithm. The most obvious choice is the next-largest
/// power of two -- but if there's a smaller/faster FFT size that satisfies the `>= 2N - 1` requirement, that will significantly
/// improve this algorithm's overall performance.
///
/// ~~~
/// // Computes a forward FFT of size 1201, using Bluestein's Algorithm
/// use rustfft::algorithm::BluesteinsAlgorithm;
/// use rustfft::{Fft, FftPlanner};
/// use rustfft::num_complex::Complex;
///
/// let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 1201];
///
/// // We need to find an inner FFT whose size is greater than 1201*2 - 1.
/// // The size 2401 (7^4) satisfies this requirement, while also being relatively fast.
/// let mut planner = FftPlanner::new();
/// let inner_fft = planner.plan_fft_forward(2401);
///
/// let fft = BluesteinsAlgorithm::new(1201, inner_fft);
/// fft.process(&mut buffer);
/// ~~~
///
/// Bluesteins's Algorithm is relatively expensive compared to other FFT algorithms. Benchmarking shows that it is up to
/// an order of magnitude slower than similar composite sizes. In the example size above of 1201, benchmarking shows
/// that it takes 5x more time to compute than computing a FFT of size 1200 via a step of MixedRadix.

pub struct BluesteinsAlgorithm<T> {
    inner_fft: Arc<dyn Fft<T>>,

    inner_fft_multiplier: Box<[Complex<T>]>,
    twiddles: Box<[Complex<T>]>,

    len: usize,
    direction: FftDirection,
}

impl<T: FftNum> BluesteinsAlgorithm<T> {
    /// Creates a FFT instance which will process inputs/outputs of size `len`. `inner_fft.len()` must be >= `len * 2 - 1`
    ///
    /// Note that this constructor is quite expensive to run; This algorithm must compute a FFT using `inner_fft` within the
    /// constructor. This further underlines the fact that Bluesteins Algorithm is more expensive to run than other
    /// FFT algorithms
    ///
    /// # Panics
    /// Panics if `inner_fft.len() < len * 2 - 1`.
    pub fn new(len: usize, inner_fft: Arc<dyn Fft<T>>) -> Self {
        let inner_fft_len = inner_fft.len();
        assert!(len * 2 - 1 <= inner_fft_len, "Bluestein's algorithm requires inner_fft.len() >= self.len() * 2 - 1. Expected >= {}, got {}", len * 2 - 1, inner_fft_len);

        // when computing FFTs, we're going to run our inner multiply pairise by some precomputed data, then run an inverse inner FFT. We need to precompute that inner data here
        let inner_fft_scale = T::one() / T::from_usize(inner_fft_len).unwrap();
        let direction = inner_fft.fft_direction();

        // Compute twiddle factors that we'll run our inner FFT on
        let mut inner_fft_input = vec![Complex::zero(); inner_fft_len];
        twiddles::fill_bluesteins_twiddles(
            &mut inner_fft_input[..len],
            direction.opposite_direction(),
        );

        // Scale the computed twiddles and copy them to the end of the array
        inner_fft_input[0] = inner_fft_input[0] * inner_fft_scale;
        for i in 1..len {
            let twiddle = inner_fft_input[i] * inner_fft_scale;
            inner_fft_input[i] = twiddle;
            inner_fft_input[inner_fft_len - i] = twiddle;
        }

        //Compute the inner fft
        let mut inner_fft_scratch = vec![Complex::zero(); inner_fft.get_inplace_scratch_len()];
        inner_fft.process_with_scratch(&mut inner_fft_input, &mut inner_fft_scratch);

        // also compute some more mundane twiddle factors to start and end with
        let mut twiddles = vec![Complex::zero(); len];
        twiddles::fill_bluesteins_twiddles(&mut twiddles, direction);

        Self {
            inner_fft: inner_fft,

            inner_fft_multiplier: inner_fft_input.into_boxed_slice(),
            twiddles: twiddles.into_boxed_slice(),

            len,
            direction,
        }
    }

    fn perform_fft_inplace(&self, input: &mut [Complex<T>], scratch: &mut [Complex<T>]) {
        let (inner_input, inner_scratch) = scratch.split_at_mut(self.inner_fft_multiplier.len());

        // Copy the buffer into our inner FFT input. the buffer will only fill part of the FFT input, so zero fill the rest
        for ((buffer_entry, inner_entry), twiddle) in input
            .iter()
            .zip(inner_input.iter_mut())
            .zip(self.twiddles.iter())
        {
            *inner_entry = *buffer_entry * *twiddle;
        }
        for inner in (&mut inner_input[input.len()..]).iter_mut() {
            *inner = Complex::zero();
        }

        // run our inner forward FFT
        self.inner_fft
            .process_with_scratch(inner_input, inner_scratch);

        // Multiply our inner FFT output by our precomputed data. Then, conjugate the result to set up for an inverse FFT
        for (inner, multiplier) in inner_input.iter_mut().zip(self.inner_fft_multiplier.iter()) {
            *inner = (*inner * *multiplier).conj();
        }

        // inverse FFT. we're computing a forward but we're massaging it into an inverse by conjugating the inputs and outputs
        self.inner_fft
            .process_with_scratch(inner_input, inner_scratch);

        // copy our data back to the buffer, applying twiddle factors again as we go. Also conjugate inner_input to complete the inverse FFT
        for ((buffer_entry, inner_entry), twiddle) in input
            .iter_mut()
            .zip(inner_input.iter())
            .zip(self.twiddles.iter())
        {
            *buffer_entry = inner_entry.conj() * twiddle;
        }
    }

    #[inline]
    fn perform_fft_immut(
        &self,
        input: &[Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        let (inner_input, inner_scratch) = scratch.split_at_mut(self.inner_fft_multiplier.len());

        // Copy the buffer into our inner FFT input. the buffer will only fill part of the FFT input, so zero fill the rest
        for ((buffer_entry, inner_entry), twiddle) in input
            .iter()
            .zip(inner_input.iter_mut())
            .zip(self.twiddles.iter())
        {
            *inner_entry = *buffer_entry * *twiddle;
        }
        for inner in inner_input.iter_mut().skip(input.len()) {
            *inner = Complex::zero();
        }

        // run our inner forward FFT
        self.inner_fft
            .process_with_scratch(inner_input, inner_scratch);

        // Multiply our inner FFT output by our precomputed data. Then, conjugate the result to set up for an inverse FFT
        for (inner, multiplier) in inner_input.iter_mut().zip(self.inner_fft_multiplier.iter()) {
            *inner = (*inner * *multiplier).conj();
        }

        // inverse FFT. we're computing a forward but we're massaging it into an inverse by conjugating the inputs and outputs
        self.inner_fft
            .process_with_scratch(inner_input, inner_scratch);

        // copy our data back to the buffer, applying twiddle factors again as we go. Also conjugate inner_input to complete the inverse FFT
        for ((buffer_entry, inner_entry), twiddle) in output
            .iter_mut()
            .zip(inner_input.iter())
            .zip(self.twiddles.iter())
        {
            *buffer_entry = inner_entry.conj() * twiddle;
        }
    }

    fn perform_fft_out_of_place(
        &self,
        input: &mut [Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        self.perform_fft_immut(input, output, scratch);
    }
}
boilerplate_fft!(
    BluesteinsAlgorithm,
    |this: &BluesteinsAlgorithm<_>| this.len, // FFT len
    |this: &BluesteinsAlgorithm<_>| this.inner_fft_multiplier.len()
        + this.inner_fft.get_inplace_scratch_len(), // in-place scratch len
    |this: &BluesteinsAlgorithm<_>| this.inner_fft_multiplier.len()
        + this.inner_fft.get_inplace_scratch_len(), // out of place scratch len
    |this: &BluesteinsAlgorithm<_>| this.inner_fft_multiplier.len()
        + this.inner_fft.get_inplace_scratch_len()  // immut scratch len
);

#[cfg(test)]
mod unit_tests {
    use super::*;
    use crate::algorithm::Dft;
    use crate::test_utils::check_fft_algorithm;
    use std::sync::Arc;

    #[test]
    fn test_bluesteins_scalar() {
        for &len in &[3, 5, 7, 11, 13] {
            test_bluesteins_with_length(len, FftDirection::Forward);
            test_bluesteins_with_length(len, FftDirection::Inverse);
        }
    }

    fn test_bluesteins_with_length(len: usize, direction: FftDirection) {
        let inner_fft = Arc::new(Dft::new(
            (len * 2 - 1).checked_next_power_of_two().unwrap(),
            direction,
        ));
        let fft = BluesteinsAlgorithm::new(len, inner_fft);

        check_fft_algorithm::<f32>(&fft, len, direction);
    }
}
