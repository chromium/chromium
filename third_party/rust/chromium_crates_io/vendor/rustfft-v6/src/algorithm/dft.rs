use num_complex::Complex;
use num_traits::Zero;

use crate::{twiddles, FftDirection};
use crate::{Direction, Fft, FftNum, Length};

/// Naive O(n^2 ) Discrete Fourier Transform implementation
///
/// This implementation is primarily used to test other FFT algorithms.
///
/// ~~~
/// // Computes a naive DFT of size 123
/// use rustfft::algorithm::Dft;
/// use rustfft::{Fft, FftDirection};
/// use rustfft::num_complex::Complex;
///
/// let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 123];
///
/// let dft = Dft::new(123, FftDirection::Forward);
/// dft.process(&mut buffer);
/// ~~~
pub struct Dft<T> {
    twiddles: Vec<Complex<T>>,
    direction: FftDirection,
}

impl<T: FftNum> Dft<T> {
    /// Preallocates necessary arrays and precomputes necessary data to efficiently compute Dft
    pub fn new(len: usize, direction: FftDirection) -> Self {
        let twiddles = (0..len)
            .map(|i| twiddles::compute_twiddle(i, len, direction))
            .collect();
        Self {
            twiddles,
            direction,
        }
    }

    fn inplace_scratch_len(&self) -> usize {
        self.len()
    }
    fn outofplace_scratch_len(&self) -> usize {
        0
    }
    fn immut_scratch_len(&self) -> usize {
        0
    }

    fn perform_fft_immut(
        &self,
        signal: &[Complex<T>],
        spectrum: &mut [Complex<T>],
        _scratch: &mut [Complex<T>],
    ) {
        for k in 0..spectrum.len() {
            let output_cell = spectrum.get_mut(k).unwrap();

            *output_cell = Zero::zero();
            let mut twiddle_index = 0;

            for input_cell in signal {
                let twiddle = self.twiddles[twiddle_index];
                *output_cell = *output_cell + twiddle * input_cell;

                twiddle_index += k;
                if twiddle_index >= self.twiddles.len() {
                    twiddle_index -= self.twiddles.len();
                }
            }
        }
    }

    fn perform_fft_out_of_place(
        &self,
        signal: &[Complex<T>],
        spectrum: &mut [Complex<T>],
        _scratch: &mut [Complex<T>],
    ) {
        self.perform_fft_immut(signal, spectrum, _scratch);
    }
}
boilerplate_fft_oop!(Dft, |this: &Dft<_>| this.twiddles.len());

#[cfg(test)]
mod unit_tests {
    use super::*;
    use crate::test_utils::{compare_vectors, random_signal};
    use num_complex::Complex;
    use num_traits::Zero;
    use std::f32;

    fn dft(signal: &[Complex<f32>], spectrum: &mut [Complex<f32>]) {
        for (k, spec_bin) in spectrum.iter_mut().enumerate() {
            let mut sum = Zero::zero();
            for (i, &x) in signal.iter().enumerate() {
                let angle = -1f32 * (i * k) as f32 * 2f32 * f32::consts::PI / signal.len() as f32;
                let twiddle = Complex::from_polar(1f32, angle);

                sum = sum + twiddle * x;
            }
            *spec_bin = sum;
        }
    }

    #[test]
    fn test_matches_dft() {
        let n = 4;

        for len in 1..20 {
            let dft_instance = Dft::new(len, FftDirection::Forward);
            assert_eq!(
                dft_instance.len(),
                len,
                "Dft instance reported incorrect length"
            );

            let input = random_signal(len * n);
            let mut expected_output = input.clone();

            // Compute the control data using our simplified Dft definition
            for (input_chunk, output_chunk) in
                input.chunks(len).zip(expected_output.chunks_mut(len))
            {
                dft(input_chunk, output_chunk);
            }

            // test process()
            {
                let mut inplace_buffer = input.clone();

                dft_instance.process(&mut inplace_buffer);

                assert!(
                    compare_vectors(&expected_output, &inplace_buffer),
                    "process() failed, length = {}",
                    len
                );
            }

            // test process_with_scratch()
            {
                let mut inplace_with_scratch_buffer = input.clone();
                let mut inplace_scratch =
                    vec![Zero::zero(); dft_instance.get_inplace_scratch_len()];

                dft_instance
                    .process_with_scratch(&mut inplace_with_scratch_buffer, &mut inplace_scratch);

                assert!(
                    compare_vectors(&expected_output, &inplace_with_scratch_buffer),
                    "process_inplace() failed, length = {}",
                    len
                );

                // one more thing: make sure that the Dft algorithm even works with dirty scratch space
                for item in inplace_scratch.iter_mut() {
                    *item = Complex::new(100.0, 100.0);
                }
                inplace_with_scratch_buffer.copy_from_slice(&input);

                dft_instance
                    .process_with_scratch(&mut inplace_with_scratch_buffer, &mut inplace_scratch);

                assert!(
                    compare_vectors(&expected_output, &inplace_with_scratch_buffer),
                    "process_with_scratch() failed the 'dirty scratch' test for len = {}",
                    len
                );
            }

            // test process_outofplace_with_scratch
            {
                let mut outofplace_input = input.clone();
                let mut outofplace_output = expected_output.clone();

                dft_instance.process_outofplace_with_scratch(
                    &mut outofplace_input,
                    &mut outofplace_output,
                    &mut [],
                );

                assert!(
                    compare_vectors(&expected_output, &outofplace_output),
                    "process_outofplace_with_scratch() failed, length = {}",
                    len
                );
            }
        }

        //verify that it doesn't crash or infinite loop if we have a length of 0
        let zero_dft = Dft::new(0, FftDirection::Forward);
        let mut zero_input: Vec<Complex<f32>> = Vec::new();
        let mut zero_output: Vec<Complex<f32>> = Vec::new();
        let mut zero_scratch: Vec<Complex<f32>> = Vec::new();

        zero_dft.process(&mut zero_input);
        zero_dft.process_with_scratch(&mut zero_input, &mut zero_scratch);
        zero_dft.process_outofplace_with_scratch(
            &mut zero_input,
            &mut zero_output,
            &mut zero_scratch,
        );
    }

    /// Returns true if our `dft` function calculates the given output from the
    /// given input, and if rustfft's Dft struct does the same
    fn test_dft_correct(input: &[Complex<f32>], expected_output: &[Complex<f32>]) {
        assert_eq!(input.len(), expected_output.len());
        let len = input.len();

        let mut reference_output = vec![Zero::zero(); len];
        dft(&input, &mut reference_output);
        assert!(
            compare_vectors(expected_output, &reference_output),
            "Reference implementation failed for len={}",
            len
        );

        let dft_instance = Dft::new(len, FftDirection::Forward);

        // test process()
        {
            let mut inplace_buffer = input.to_vec();

            dft_instance.process(&mut inplace_buffer);

            assert!(
                compare_vectors(&expected_output, &inplace_buffer),
                "process() failed, length = {}",
                len
            );
        }

        // test process_with_scratch()
        {
            let mut inplace_with_scratch_buffer = input.to_vec();
            let mut inplace_scratch = vec![Zero::zero(); dft_instance.get_inplace_scratch_len()];

            dft_instance
                .process_with_scratch(&mut inplace_with_scratch_buffer, &mut inplace_scratch);

            assert!(
                compare_vectors(&expected_output, &inplace_with_scratch_buffer),
                "process_inplace() failed, length = {}",
                len
            );

            // one more thing: make sure that the Dft algorithm even works with dirty scratch space
            for item in inplace_scratch.iter_mut() {
                *item = Complex::new(100.0, 100.0);
            }
            inplace_with_scratch_buffer.copy_from_slice(&input);

            dft_instance
                .process_with_scratch(&mut inplace_with_scratch_buffer, &mut inplace_scratch);

            assert!(
                compare_vectors(&expected_output, &inplace_with_scratch_buffer),
                "process_with_scratch() failed the 'dirty scratch' test for len = {}",
                len
            );
        }

        // test process_outofplace_with_scratch
        {
            let mut outofplace_input = input.to_vec();
            let mut outofplace_output = expected_output.to_vec();

            dft_instance.process_outofplace_with_scratch(
                &mut outofplace_input,
                &mut outofplace_output,
                &mut [],
            );

            assert!(
                compare_vectors(&expected_output, &outofplace_output),
                "process_outofplace_with_scratch() failed, length = {}",
                len
            );
        }
    }

    #[test]
    fn test_dft_known_len_2() {
        let signal = [
            Complex { re: 1f32, im: 0f32 },
            Complex {
                re: -1f32,
                im: 0f32,
            },
        ];
        let spectrum = [
            Complex { re: 0f32, im: 0f32 },
            Complex { re: 2f32, im: 0f32 },
        ];
        test_dft_correct(&signal[..], &spectrum[..]);
    }

    #[test]
    fn test_dft_known_len_3() {
        let signal = [
            Complex { re: 1f32, im: 1f32 },
            Complex {
                re: 2f32,
                im: -3f32,
            },
            Complex {
                re: -1f32,
                im: 4f32,
            },
        ];
        let spectrum = [
            Complex { re: 2f32, im: 2f32 },
            Complex {
                re: -5.562177f32,
                im: -2.098076f32,
            },
            Complex {
                re: 6.562178f32,
                im: 3.09807f32,
            },
        ];
        test_dft_correct(&signal[..], &spectrum[..]);
    }

    #[test]
    fn test_dft_known_len_4() {
        let signal = [
            Complex { re: 0f32, im: 1f32 },
            Complex {
                re: 2.5f32,
                im: -3f32,
            },
            Complex {
                re: -1f32,
                im: -1f32,
            },
            Complex { re: 4f32, im: 0f32 },
        ];
        let spectrum = [
            Complex {
                re: 5.5f32,
                im: -3f32,
            },
            Complex {
                re: -2f32,
                im: 3.5f32,
            },
            Complex {
                re: -7.5f32,
                im: 3f32,
            },
            Complex {
                re: 4f32,
                im: 0.5f32,
            },
        ];
        test_dft_correct(&signal[..], &spectrum[..]);
    }

    #[test]
    fn test_dft_known_len_6() {
        let signal = [
            Complex { re: 1f32, im: 1f32 },
            Complex { re: 2f32, im: 2f32 },
            Complex { re: 3f32, im: 3f32 },
            Complex { re: 4f32, im: 4f32 },
            Complex { re: 5f32, im: 5f32 },
            Complex { re: 6f32, im: 6f32 },
        ];
        let spectrum = [
            Complex {
                re: 21f32,
                im: 21f32,
            },
            Complex {
                re: -8.16f32,
                im: 2.16f32,
            },
            Complex {
                re: -4.76f32,
                im: -1.24f32,
            },
            Complex {
                re: -3f32,
                im: -3f32,
            },
            Complex {
                re: -1.24f32,
                im: -4.76f32,
            },
            Complex {
                re: 2.16f32,
                im: -8.16f32,
            },
        ];
        test_dft_correct(&signal[..], &spectrum[..]);
    }
}
