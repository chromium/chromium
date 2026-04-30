use std::sync::Arc;

use num_complex::Complex;
use num_traits::{Float, One, Zero};

use rand::distributions::{uniform::SampleUniform, Distribution, Uniform};
use rand::{rngs::StdRng, SeedableRng};

use crate::algorithm::butterflies::{
    Butterfly1, Butterfly12, Butterfly16, Butterfly2, Butterfly3, Butterfly4, Butterfly5,
    Butterfly6, Butterfly7, Butterfly8, Butterfly9,
};
use crate::{algorithm::Dft, Direction, FftNum, Length};
use crate::{Fft, FftDirection};

/// The seed for the random number generator used to generate
/// random signals. It's defined here so that we have deterministic
/// tests
const RNG_SEED: [u8; 32] = [
    1, 9, 1, 0, 1, 1, 4, 3, 1, 4, 9, 8, 4, 1, 4, 8, 2, 8, 1, 2, 2, 2, 6, 1, 2, 3, 4, 5, 6, 7, 8, 9,
];

pub fn random_signal<T: FftNum + SampleUniform>(length: usize) -> Vec<Complex<T>> {
    let mut sig = Vec::with_capacity(length);
    let normal_dist: Uniform<T> = Uniform::new(T::zero(), T::from_f32(10.0).unwrap());
    let mut rng: StdRng = SeedableRng::from_seed(RNG_SEED);
    for _ in 0..length {
        sig.push(Complex {
            re: normal_dist.sample(&mut rng),
            im: normal_dist.sample(&mut rng),
        });
    }
    return sig;
}

pub fn compare_vectors<T: FftNum + Float>(vec1: &[Complex<T>], vec2: &[Complex<T>]) -> bool {
    assert_eq!(vec1.len(), vec2.len());
    let mut error = T::zero();
    for (&a, &b) in vec1.iter().zip(vec2.iter()) {
        error = error + (a - b).norm();
    }
    return (error.to_f64().unwrap() / vec1.len() as f64) < 0.1f64;
}
pub fn first_diff<T: FftNum + Float>(vec1: &[Complex<T>], vec2: &[Complex<T>]) -> Option<usize> {
    assert_eq!(vec1.len(), vec2.len());
    for (i, (&a, &b)) in vec1.iter().zip(vec2.iter()).enumerate() {
        if (a - b).norm().to_f64().unwrap() > 0.1 {
            return Some(i);
        }
    }
    None
}

#[allow(unused)]
fn transppose_diagnostic<T: FftNum + Float>(expected: &[Complex<T>], actual: &[Complex<T>]) {
    for (i, (&e, &a)) in expected.iter().zip(actual.iter()).enumerate() {
        if (e - a).norm().to_f32().unwrap() > 0.01 {
            if let Some(found_index) = expected
                .iter()
                .position(|&ev| (ev - a).norm().to_f32().unwrap() < 0.01)
            {
                println!("{} incorrectly contained {}", i, found_index);
            } else {
                println!("{} X", i);
            }
        }
    }
}

pub fn check_fft_algorithm<T: FftNum + Float + SampleUniform>(
    fft: &dyn Fft<T>,
    len: usize,
    direction: FftDirection,
) {
    assert_eq!(
        fft.len(),
        len,
        "Algorithm reported incorrect size. Expected {}, got {}",
        len,
        fft.len()
    );
    assert_eq!(
        fft.fft_direction(),
        direction,
        "Algorithm reported incorrect FFT direction"
    );

    let n = 3;

    //test the forward direction
    let dft = Dft::new(len, direction);

    let dirty_scratch_value = Complex::one() * T::from_i32(100).unwrap();

    // set up buffers
    let reference_input = random_signal(len * n);
    let mut expected_output = reference_input.clone();
    let mut dft_scratch = vec![Zero::zero(); dft.get_inplace_scratch_len()];
    dft.process_with_scratch(&mut expected_output, &mut dft_scratch);

    // test process()
    {
        let mut buffer = reference_input.clone();

        fft.process(&mut buffer);

        if !compare_vectors(&expected_output, &buffer) {
            panic!(
                "process() failed, length = {}, direction = {}, first diff = {:?}",
                len,
                direction,
                first_diff(&expected_output, &buffer)
            );
        }
    }

    // test process_with_scratch()
    {
        let mut buffer = reference_input.clone();
        let mut scratch = vec![Zero::zero(); fft.get_inplace_scratch_len()];

        fft.process_with_scratch(&mut buffer, &mut scratch);

        assert!(
            compare_vectors(&expected_output, &buffer),
            "process_with_scratch() failed, length = {}, direction = {}",
            len,
            direction
        );

        // make sure this algorithm works correctly with dirty scratch
        if scratch.len() > 0 {
            for item in scratch.iter_mut() {
                *item = dirty_scratch_value;
            }
            buffer.copy_from_slice(&reference_input);

            fft.process_with_scratch(&mut buffer, &mut scratch);

            assert!(compare_vectors(&expected_output, &buffer), "process_with_scratch() failed the 'dirty scratch' test, length = {}, direction = {}", len, direction);
        }
    }

    // test process_outofplace_with_scratch()
    {
        let mut input = reference_input.clone();
        let mut scratch = vec![Zero::zero(); fft.get_outofplace_scratch_len()];
        let mut output = vec![Zero::zero(); n * len];

        fft.process_outofplace_with_scratch(&mut input, &mut output, &mut scratch);

        assert!(
            compare_vectors(&expected_output, &output),
            "process_outofplace_with_scratch() failed, length = {}, direction = {}",
            len,
            direction
        );

        // make sure this algorithm works correctly with dirty scratch
        if scratch.len() > 0 {
            for item in scratch.iter_mut() {
                *item = dirty_scratch_value;
            }
            input.copy_from_slice(&reference_input);

            fft.process_outofplace_with_scratch(&mut input, &mut output, &mut scratch);

            assert!(
                compare_vectors(&expected_output, &output),
                "process_outofplace_with_scratch() failed the 'dirty scratch' test, length = {}, direction = {}",
                len,
                direction
            );
        }
    }

    // test process_immutable_with_scratch()
    {
        let mut input = reference_input.clone();
        let mut scratch = vec![Zero::zero(); fft.get_immutable_scratch_len()];
        let mut output = vec![Zero::zero(); n * len];

        fft.process_immutable_with_scratch(&input, &mut output, &mut scratch);

        assert!(
            compare_vectors(&expected_output, &output),
            "process_immutable_with_scratch() failed, length = {}, direction = {}",
            len,
            direction
        );

        // make sure this algorithm works correctly with dirty scratch
        if scratch.len() > 0 {
            for item in scratch.iter_mut() {
                *item = dirty_scratch_value;
            }
            input.copy_from_slice(&reference_input);

            fft.process_immutable_with_scratch(&input, &mut output, &mut scratch);

            assert!(
                compare_vectors(&expected_output, &output),
                "process_immutable_with_scratch() failed the 'dirty scratch' test, length = {}, direction = {}",
                len,
                direction
            );
        }
    }
}

// A fake FFT algorithm that requests much more scratch than it needs. You can use this as an inner FFT to other algorithms to test their scratch-supplying logic
#[derive(Debug)]
pub struct BigScratchAlgorithm {
    pub len: usize,

    pub inplace_scratch: usize,
    pub outofplace_scratch: usize,
    pub immut_scratch: usize,

    pub direction: FftDirection,
}
impl<T: FftNum> Fft<T> for BigScratchAlgorithm {
    fn process_immutable_with_scratch(
        &self,
        _input: &[Complex<T>],
        _output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        assert!(
            scratch.len() >= self.immut_scratch,
            "Not enough immut scratch provided, self={:?}, provided scratch={}",
            &self,
            scratch.len()
        );
    }
    fn process_with_scratch(&self, _buffer: &mut [Complex<T>], scratch: &mut [Complex<T>]) {
        assert!(
            scratch.len() >= self.inplace_scratch,
            "Not enough inplace scratch provided, self={:?}, provided scratch={}",
            &self,
            scratch.len()
        );
    }
    fn process_outofplace_with_scratch(
        &self,
        _input: &mut [Complex<T>],
        _output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        assert!(
            scratch.len() >= self.outofplace_scratch,
            "Not enough OOP scratch provided, self={:?}, provided scratch={}",
            &self,
            scratch.len()
        );
    }
    fn get_inplace_scratch_len(&self) -> usize {
        self.inplace_scratch
    }
    fn get_outofplace_scratch_len(&self) -> usize {
        self.outofplace_scratch
    }
    fn get_immutable_scratch_len(&self) -> usize {
        self.immut_scratch
    }
}
impl Length for BigScratchAlgorithm {
    fn len(&self) -> usize {
        self.len
    }
}
impl Direction for BigScratchAlgorithm {
    fn fft_direction(&self) -> FftDirection {
        self.direction
    }
}

pub fn construct_base<T: FftNum>(n: usize, direction: FftDirection) -> Arc<dyn Fft<T>> {
    match n {
        1 => Arc::new(Butterfly1::new(direction)) as Arc<dyn Fft<T>>,
        2 => Arc::new(Butterfly2::new(direction)) as Arc<dyn Fft<T>>,
        3 => Arc::new(Butterfly3::new(direction)) as Arc<dyn Fft<T>>,
        4 => Arc::new(Butterfly4::new(direction)) as Arc<dyn Fft<T>>,
        5 => Arc::new(Butterfly5::new(direction)) as Arc<dyn Fft<T>>,
        6 => Arc::new(Butterfly6::new(direction)) as Arc<dyn Fft<T>>,
        7 => Arc::new(Butterfly7::new(direction)) as Arc<dyn Fft<T>>,
        8 => Arc::new(Butterfly8::new(direction)) as Arc<dyn Fft<T>>,
        9 => Arc::new(Butterfly9::new(direction)) as Arc<dyn Fft<T>>,
        12 => Arc::new(Butterfly12::new(direction)) as Arc<dyn Fft<T>>,
        16 => Arc::new(Butterfly16::new(direction)) as Arc<dyn Fft<T>>,
        _ => unimplemented!(),
    }
}
