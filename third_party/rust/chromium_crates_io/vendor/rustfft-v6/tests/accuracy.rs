//! To test the accuracy of our FFT algorithm, we first test that our
//! naive Dft function is correct by comparing its output against several
//! known signal/spectrum relationships. Then, we generate random signals
//! for a variety of lengths, and test that our FFT algorithm matches our
//! Dft calculation for those signals.

use std::sync::Arc;

use num_traits::Float;
use rustfft::{
    algorithm::{BluesteinsAlgorithm, Radix4},
    num_complex::Complex,
    Fft, FftNum, FftPlanner,
};
use rustfft::{num_traits::Zero, FftDirection};

use rand::distributions::{uniform::SampleUniform, Distribution, Uniform};
use rand::{rngs::StdRng, SeedableRng};
use wasm_bindgen_test::wasm_bindgen_test;

/// The seed for the random number generator used to generate
/// random signals. It's defined here so that we have deterministic
/// tests
const RNG_SEED: [u8; 32] = [
    1, 9, 1, 0, 1, 1, 4, 3, 1, 4, 9, 8, 4, 1, 4, 8, 2, 8, 1, 2, 2, 2, 6, 1, 2, 3, 4, 5, 6, 7, 8, 9,
];

/// Returns true if the mean difference in the elements of the two vectors
/// is small
fn compare_vectors<T: rustfft::FftNum + Float>(vec1: &[Complex<T>], vec2: &[Complex<T>]) -> bool {
    assert_eq!(vec1.len(), vec2.len());
    let mut error = T::zero();
    for (&a, &b) in vec1.iter().zip(vec2.iter()) {
        error = error + (a - b).norm();
    }
    return (error / T::from_usize(vec1.len()).unwrap()) < T::from_f32(0.1).unwrap();
}

fn fft_matches_control<T: FftNum + Float>(control: Arc<dyn Fft<T>>, input: &[Complex<T>]) -> bool {
    let mut control_input = input.to_vec();

    let mut planner = FftPlanner::new();
    let fft = planner.plan_fft(control.len(), control.fft_direction());
    assert_eq!(
        fft.len(),
        control.len(),
        "FFTplanner created FFT of wrong length"
    );
    assert_eq!(
        fft.fft_direction(),
        control.fft_direction(),
        "FFTplanner created FFT of wrong direction"
    );

    let scratch_max = std::cmp::max(
        control.get_inplace_scratch_len(),
        std::cmp::max(
            fft.get_inplace_scratch_len(),
            std::cmp::max(
                fft.get_outofplace_scratch_len(),
                fft.get_immutable_scratch_len(),
            ),
        ),
    );
    let mut scratch = vec![Zero::zero(); scratch_max];

    control.process_with_scratch(&mut control_input, &mut scratch);

    let mut test_output_inplace = input.to_vec();
    fft.process_with_scratch(&mut test_output_inplace, &mut scratch);

    let mut input_oop = input.to_vec();
    let mut test_output_oop = input.to_vec();
    fft.process_outofplace_with_scratch(&mut input_oop, &mut test_output_oop, &mut scratch);

    let mut test_output_immut = input.to_vec();
    fft.process_immutable_with_scratch(input, &mut test_output_immut, &mut scratch);

    return compare_vectors(&control_input, &test_output_inplace)
        && compare_vectors(&control_input, &test_output_oop)
        && compare_vectors(&control_input, &test_output_immut);
}

fn random_signal<T: FftNum + SampleUniform>(length: usize) -> Vec<Complex<T>> {
    let mut sig = Vec::with_capacity(length);
    let dist: Uniform<T> = Uniform::new(T::zero(), T::from_f64(10.0).unwrap());
    let mut rng: StdRng = SeedableRng::from_seed(RNG_SEED);
    for _ in 0..length {
        sig.push(Complex {
            re: (dist.sample(&mut rng)),
            im: (dist.sample(&mut rng)),
        });
    }
    return sig;
}

// A cache that makes setup for integration tests faster
struct ControlCache<T: FftNum> {
    fft_cache: Vec<Arc<dyn Fft<T>>>,
}
impl<T: FftNum> ControlCache<T> {
    pub fn new(max_outer_len: usize, direction: FftDirection) -> Self {
        let max_inner_len = (max_outer_len * 2 - 1).checked_next_power_of_two().unwrap();
        let max_power = max_inner_len.trailing_zeros() as usize;

        Self {
            fft_cache: (0..=max_power)
                .map(|i| {
                    let len = 1 << i;
                    Arc::new(Radix4::new(len, direction)) as Arc<dyn Fft<_>>
                })
                .collect(),
        }
    }

    pub fn plan_fft(&self, len: usize) -> Arc<dyn Fft<T>> {
        let inner_fft_len = (len * 2 - 1).checked_next_power_of_two().unwrap();
        let inner_fft_index = inner_fft_len.trailing_zeros() as usize;
        let inner_fft = Arc::clone(&self.fft_cache[inner_fft_index]);
        Arc::new(BluesteinsAlgorithm::new(len, inner_fft))
    }
}

const TEST_MAX: usize = 1001;

/// Integration tests that verify our FFT output matches the direct Dft calculation
/// for random signals.
#[test]
fn test_planned_fft_forward_f32() {
    let direction = FftDirection::Forward;
    let cache: ControlCache<f32> = ControlCache::new(TEST_MAX, direction);

    for len in 1..TEST_MAX {
        println!("len: {len}");
        let control = cache.plan_fft(len);
        assert_eq!(control.len(), len);
        assert_eq!(control.fft_direction(), direction);

        let signal = random_signal(len);
        assert!(fft_matches_control(control, &signal), "length = {}", len);
    }
}

#[test]
fn test_planned_fft_inverse_f32() {
    let direction = FftDirection::Inverse;
    let cache: ControlCache<f32> = ControlCache::new(TEST_MAX, direction);

    for len in 1..TEST_MAX {
        let control = cache.plan_fft(len);
        assert_eq!(control.len(), len);
        assert_eq!(control.fft_direction(), direction);

        let signal = random_signal(len);
        assert!(fft_matches_control(control, &signal), "length = {}", len);
    }
}

#[test]
fn test_planned_fft_forward_f64() {
    let direction = FftDirection::Forward;
    let cache: ControlCache<f64> = ControlCache::new(TEST_MAX, direction);

    for len in 1..TEST_MAX {
        let control = cache.plan_fft(len);
        assert_eq!(control.len(), len);
        assert_eq!(control.fft_direction(), direction);

        let signal = random_signal(len);
        assert!(fft_matches_control(control, &signal), "length = {}", len);
    }
}

#[test]
fn test_planned_fft_inverse_f64() {
    let direction = FftDirection::Inverse;
    let cache: ControlCache<f64> = ControlCache::new(TEST_MAX, direction);

    for len in 1..TEST_MAX {
        let control = cache.plan_fft(len);
        assert_eq!(control.len(), len);
        assert_eq!(control.fft_direction(), direction);

        let signal = random_signal(len);
        assert!(fft_matches_control(control, &signal), "length = {}", len);
    }
}

#[wasm_bindgen_test]
fn wasm_test_planned_fft_forward_f32() {
    let direction = FftDirection::Forward;
    let cache: ControlCache<f32> = ControlCache::new(TEST_MAX, direction);

    for len in 1..TEST_MAX {
        println!("len: {len}");
        let control = cache.plan_fft(len);
        assert_eq!(control.len(), len);
        assert_eq!(control.fft_direction(), direction);

        let signal = random_signal(len);
        assert!(fft_matches_control(control, &signal), "length = {}", len);
    }
}

#[wasm_bindgen_test]
fn wasm_test_planned_fft_inverse_f32() {
    let direction = FftDirection::Inverse;
    let cache: ControlCache<f32> = ControlCache::new(TEST_MAX, direction);

    for len in 1..TEST_MAX {
        let control = cache.plan_fft(len);
        assert_eq!(control.len(), len);
        assert_eq!(control.fft_direction(), direction);

        let signal = random_signal(len);
        assert!(fft_matches_control(control, &signal), "length = {}", len);
    }
}

#[wasm_bindgen_test]
fn wasm_test_planned_fft_forward_f64() {
    let direction = FftDirection::Forward;
    let cache: ControlCache<f64> = ControlCache::new(TEST_MAX, direction);

    for len in 1..TEST_MAX {
        let control = cache.plan_fft(len);
        assert_eq!(control.len(), len);
        assert_eq!(control.fft_direction(), direction);

        let signal = random_signal(len);
        assert!(fft_matches_control(control, &signal), "length = {}", len);
    }
}

#[wasm_bindgen_test]
fn wasm_test_planned_fft_inverse_f64() {
    let direction = FftDirection::Inverse;
    let cache: ControlCache<f64> = ControlCache::new(TEST_MAX, direction);

    for len in 1..TEST_MAX {
        let control = cache.plan_fft(len);
        assert_eq!(control.len(), len);
        assert_eq!(control.fft_direction(), direction);

        let signal = random_signal(len);
        assert!(fft_matches_control(control, &signal), "length = {}", len);
    }
}
