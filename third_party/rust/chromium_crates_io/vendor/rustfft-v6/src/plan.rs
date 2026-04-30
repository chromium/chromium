use num_integer::gcd;
use std::collections::HashMap;
use std::sync::Arc;

use crate::common::RadixFactor;
use crate::wasm_simd::wasm_simd_planner::FftPlannerWasmSimd;
use crate::{common::FftNum, fft_cache::FftCache, FftDirection};

use crate::algorithm::butterflies::*;
use crate::algorithm::*;
use crate::Fft;

use crate::FftPlannerAvx;
use crate::FftPlannerNeon;
use crate::FftPlannerSse;

use crate::math_utils::PrimeFactors;

enum ChosenFftPlanner<T: FftNum> {
    Scalar(FftPlannerScalar<T>),
    Avx(FftPlannerAvx<T>),
    Sse(FftPlannerSse<T>),
    Neon(FftPlannerNeon<T>),
    WasmSimd(FftPlannerWasmSimd<T>),
    // todo: If we add NEON, avx-512 etc support, add more enum variants for them here
}

/// The FFT planner creates new FFT algorithm instances.
///
/// RustFFT has several FFT algorithms available. For a given FFT size, the `FftPlanner` decides which of the
/// available FFT algorithms to use and then initializes them.
///
/// ~~~
/// // Perform a forward Fft of size 1234
/// use std::sync::Arc;
/// use rustfft::{FftPlanner, num_complex::Complex};
///
/// let mut planner = FftPlanner::new();
/// let fft = planner.plan_fft_forward(1234);
///
/// let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 1234];
/// fft.process(&mut buffer);
///
/// // The FFT instance returned by the planner has the type `Arc<dyn Fft<T>>`,
/// // where T is the numeric type, ie f32 or f64, so it's cheap to clone
/// let fft_clone = Arc::clone(&fft);
/// ~~~
///
/// If you plan on creating multiple FFT instances, it is recommended to reuse the same planner for all of them. This
/// is because the planner re-uses internal data across FFT instances wherever possible, saving memory and reducing
/// setup time. (FFT instances created with one planner will never re-use data and buffers with FFT instances created
/// by a different planner)
///
/// Each FFT instance owns [`Arc`s](std::sync::Arc) to its internal data, rather than borrowing it from the planner, so it's perfectly
/// safe to drop the planner after creating Fft instances.
///
/// In the constructor, the FftPlanner will detect available CPU features. If AVX, SSE, Neon, or WASM SIMD are available, it will set itself up to plan FFTs with the fastest available instruction set.
/// If no SIMD instruction sets are available, the planner will seamlessly fall back to planning non-SIMD FFTs.
///
/// If you'd prefer not to compute a FFT at all if a certain SIMD instruction set isn't available, or otherwise specify your own custom fallback, RustFFT exposes dedicated planners for each instruction set:
///  - [`FftPlannerAvx`](crate::FftPlannerAvx)
///  - [`FftPlannerSse`](crate::FftPlannerSse)
///  - [`FftPlannerNeon`](crate::FftPlannerNeon)
///  - [`FftPlannerWasmSimd`](crate::FftPlannerWasmSimd)
///
/// If you'd prefer to opt out of SIMD algorithms, consider creating a [`FftPlannerScalar`](crate::FftPlannerScalar) instead.
pub struct FftPlanner<T: FftNum> {
    chosen_planner: ChosenFftPlanner<T>,
}
impl<T: FftNum> FftPlanner<T> {
    /// Creates a new `FftPlanner` instance.
    pub fn new() -> Self {
        if let Ok(avx_planner) = FftPlannerAvx::new() {
            Self {
                chosen_planner: ChosenFftPlanner::Avx(avx_planner),
            }
        } else if let Ok(sse_planner) = FftPlannerSse::new() {
            Self {
                chosen_planner: ChosenFftPlanner::Sse(sse_planner),
            }
        } else if let Ok(neon_planner) = FftPlannerNeon::new() {
            Self {
                chosen_planner: ChosenFftPlanner::Neon(neon_planner),
            }
        } else if let Ok(wasm_simd_planner) = FftPlannerWasmSimd::new() {
            Self {
                chosen_planner: ChosenFftPlanner::WasmSimd(wasm_simd_planner),
            }
        } else {
            Self {
                chosen_planner: ChosenFftPlanner::Scalar(FftPlannerScalar::new()),
            }
        }
    }

    /// Returns a `Fft` instance which computes FFTs of size `len`.
    ///
    /// If the provided `direction` is `FftDirection::Forward`, the returned instance will compute forward FFTs. If it's `FftDirection::Inverse`, it will compute inverse FFTs.
    ///
    /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
    pub fn plan_fft(&mut self, len: usize, direction: FftDirection) -> Arc<dyn Fft<T>> {
        match &mut self.chosen_planner {
            ChosenFftPlanner::Scalar(scalar_planner) => scalar_planner.plan_fft(len, direction),
            ChosenFftPlanner::Avx(avx_planner) => avx_planner.plan_fft(len, direction),
            ChosenFftPlanner::Sse(sse_planner) => sse_planner.plan_fft(len, direction),
            ChosenFftPlanner::Neon(neon_planner) => neon_planner.plan_fft(len, direction),
            ChosenFftPlanner::WasmSimd(wasm_simd_planner) => {
                wasm_simd_planner.plan_fft(len, direction)
            }
        }
    }

    /// Returns a `Fft` instance which computes forward FFTs of size `len`
    ///
    /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
    pub fn plan_fft_forward(&mut self, len: usize) -> Arc<dyn Fft<T>> {
        self.plan_fft(len, FftDirection::Forward)
    }

    /// Returns a `Fft` instance which computes inverse FFTs of size `len`
    ///
    /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
    pub fn plan_fft_inverse(&mut self, len: usize) -> Arc<dyn Fft<T>> {
        self.plan_fft(len, FftDirection::Inverse)
    }
}

const MAX_RADIXN_FACTOR: usize = 7; // The largest blutterfly factor that the RadixN algorithm can handle
const MAX_RADER_PRIME_FACTOR: usize = 23; // don't use Raders if the inner fft length has prime factor larger than this

/// A Recipe is a structure that describes the design of a FFT, without actually creating it.
/// It is used as a middle step in the planning process.
#[derive(Debug, PartialEq, Clone)]
pub(crate) enum Recipe {
    Dft(usize),
    MixedRadix {
        left_fft: Arc<Recipe>,
        right_fft: Arc<Recipe>,
    },
    #[allow(dead_code)]
    GoodThomasAlgorithm {
        left_fft: Arc<Recipe>,
        right_fft: Arc<Recipe>,
    },
    MixedRadixSmall {
        left_fft: Arc<Recipe>,
        right_fft: Arc<Recipe>,
    },
    GoodThomasAlgorithmSmall {
        left_fft: Arc<Recipe>,
        right_fft: Arc<Recipe>,
    },
    RadersAlgorithm {
        inner_fft: Arc<Recipe>,
    },
    BluesteinsAlgorithm {
        len: usize,
        inner_fft: Arc<Recipe>,
    },
    RadixN {
        factors: Box<[RadixFactor]>,
        base_fft: Arc<Recipe>,
    },
    Radix4 {
        k: u32,
        base_fft: Arc<Recipe>,
    },
    Butterfly2,
    Butterfly3,
    Butterfly4,
    Butterfly5,
    Butterfly6,
    Butterfly7,
    Butterfly8,
    Butterfly9,
    Butterfly11,
    Butterfly12,
    Butterfly13,
    Butterfly16,
    Butterfly17,
    Butterfly19,
    Butterfly23,
    Butterfly24,
    Butterfly27,
    Butterfly29,
    Butterfly31,
    Butterfly32,
}

impl Recipe {
    pub fn len(&self) -> usize {
        match self {
            Recipe::Dft(length) => *length,
            Recipe::RadixN { factors, base_fft } => {
                base_fft.len() * factors.iter().map(|f| f.radix()).product::<usize>()
            }
            Recipe::Radix4 { k, base_fft } => base_fft.len() * (1 << (k * 2)),
            Recipe::Butterfly2 => 2,
            Recipe::Butterfly3 => 3,
            Recipe::Butterfly4 => 4,
            Recipe::Butterfly5 => 5,
            Recipe::Butterfly6 => 6,
            Recipe::Butterfly7 => 7,
            Recipe::Butterfly8 => 8,
            Recipe::Butterfly9 => 9,
            Recipe::Butterfly11 => 11,
            Recipe::Butterfly12 => 12,
            Recipe::Butterfly13 => 13,
            Recipe::Butterfly16 => 16,
            Recipe::Butterfly17 => 17,
            Recipe::Butterfly19 => 19,
            Recipe::Butterfly23 => 23,
            Recipe::Butterfly24 => 24,
            Recipe::Butterfly27 => 27,
            Recipe::Butterfly29 => 29,
            Recipe::Butterfly31 => 31,
            Recipe::Butterfly32 => 32,
            Recipe::MixedRadix {
                left_fft,
                right_fft,
            } => left_fft.len() * right_fft.len(),
            Recipe::GoodThomasAlgorithm {
                left_fft,
                right_fft,
            } => left_fft.len() * right_fft.len(),
            Recipe::MixedRadixSmall {
                left_fft,
                right_fft,
            } => left_fft.len() * right_fft.len(),
            Recipe::GoodThomasAlgorithmSmall {
                left_fft,
                right_fft,
            } => left_fft.len() * right_fft.len(),
            Recipe::RadersAlgorithm { inner_fft } => inner_fft.len() + 1,
            Recipe::BluesteinsAlgorithm { len, .. } => *len,
        }
    }
}

/// The Scalar FFT planner creates new FFT algorithm instances using non-SIMD algorithms.
///
/// RustFFT has several FFT algorithms available. For a given FFT size, the `FftPlannerScalar` decides which of the
/// available FFT algorithms to use and then initializes them.
///
/// Use `FftPlannerScalar` instead of [`FftPlanner`](crate::FftPlanner) or [`FftPlannerAvx`](crate::FftPlannerAvx) when you want to explicitly opt out of using any SIMD-accelerated algorithms.
///
/// ~~~
/// // Perform a forward Fft of size 1234
/// use std::sync::Arc;
/// use rustfft::{FftPlannerScalar, num_complex::Complex};
///
/// let mut planner = FftPlannerScalar::new();
/// let fft = planner.plan_fft_forward(1234);
///
/// let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 1234];
/// fft.process(&mut buffer);
///
/// // The FFT instance returned by the planner has the type `Arc<dyn Fft<T>>`,
/// // where T is the numeric type, ie f32 or f64, so it's cheap to clone
/// let fft_clone = Arc::clone(&fft);
/// ~~~
///
/// If you plan on creating multiple FFT instances, it is recommended to reuse the same planner for all of them. This
/// is because the planner re-uses internal data across FFT instances wherever possible, saving memory and reducing
/// setup time. (FFT instances created with one planner will never re-use data and buffers with FFT instances created
/// by a different planner)
///
/// Each FFT instance owns [`Arc`s](std::sync::Arc) to its internal data, rather than borrowing it from the planner, so it's perfectly
/// safe to drop the planner after creating Fft instances.
pub struct FftPlannerScalar<T: FftNum> {
    algorithm_cache: FftCache<T>,
    recipe_cache: HashMap<usize, Arc<Recipe>>,
}

impl<T: FftNum> FftPlannerScalar<T> {
    /// Creates a new `FftPlannerScalar` instance.
    pub fn new() -> Self {
        Self {
            algorithm_cache: FftCache::new(),
            recipe_cache: HashMap::new(),
        }
    }

    /// Returns a `Fft` instance which computes FFTs of size `len`.
    ///
    /// If the provided `direction` is `FftDirection::Forward`, the returned instance will compute forward FFTs. If it's `FftDirection::Inverse`, it will compute inverse FFTs.
    ///
    /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
    pub fn plan_fft(&mut self, len: usize, direction: FftDirection) -> Arc<dyn Fft<T>> {
        // Step 1: Create a "recipe" for this FFT, which will tell us exactly which combination of algorithms to use
        let recipe = self.design_fft_for_len(len);

        // Step 2: Use our recipe to construct a Fft trait object
        self.build_fft(&recipe, direction)
    }

    /// Returns a `Fft` instance which computes forward FFTs of size `len`
    ///
    /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
    pub fn plan_fft_forward(&mut self, len: usize) -> Arc<dyn Fft<T>> {
        self.plan_fft(len, FftDirection::Forward)
    }

    /// Returns a `Fft` instance which computes inverse FFTs of size `len`
    ///
    /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
    pub fn plan_fft_inverse(&mut self, len: usize) -> Arc<dyn Fft<T>> {
        self.plan_fft(len, FftDirection::Inverse)
    }

    // Make a recipe for a length
    fn design_fft_for_len(&mut self, len: usize) -> Arc<Recipe> {
        if len < 2 {
            Arc::new(Recipe::Dft(len))
        } else if let Some(recipe) = self.recipe_cache.get(&len) {
            Arc::clone(&recipe)
        } else {
            let factors = PrimeFactors::compute(len);
            let recipe = self.design_fft_with_factors(len, factors);
            self.recipe_cache.insert(len, Arc::clone(&recipe));
            recipe
        }
    }

    // Create the fft from a recipe, take from cache if possible
    fn build_fft(&mut self, recipe: &Recipe, direction: FftDirection) -> Arc<dyn Fft<T>> {
        let len = recipe.len();
        if let Some(instance) = self.algorithm_cache.get(len, direction) {
            instance
        } else {
            let fft = self.build_new_fft(recipe, direction);
            self.algorithm_cache.insert(&fft);
            fft
        }
    }

    // Create a new fft from a recipe
    fn build_new_fft(&mut self, recipe: &Recipe, direction: FftDirection) -> Arc<dyn Fft<T>> {
        match recipe {
            Recipe::Dft(len) => Arc::new(Dft::new(*len, direction)) as Arc<dyn Fft<T>>,
            Recipe::RadixN { factors, base_fft } => {
                let base_fft = self.build_fft(base_fft, direction);
                Arc::new(RadixN::new(factors, base_fft)) as Arc<dyn Fft<T>>
            }
            Recipe::Radix4 { k, base_fft } => {
                let base_fft = self.build_fft(base_fft, direction);
                Arc::new(Radix4::new_with_base(*k, base_fft)) as Arc<dyn Fft<T>>
            }
            Recipe::Butterfly2 => Arc::new(Butterfly2::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly3 => Arc::new(Butterfly3::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly4 => Arc::new(Butterfly4::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly5 => Arc::new(Butterfly5::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly6 => Arc::new(Butterfly6::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly7 => Arc::new(Butterfly7::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly8 => Arc::new(Butterfly8::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly9 => Arc::new(Butterfly9::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly11 => Arc::new(Butterfly11::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly12 => Arc::new(Butterfly12::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly13 => Arc::new(Butterfly13::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly16 => Arc::new(Butterfly16::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly17 => Arc::new(Butterfly17::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly19 => Arc::new(Butterfly19::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly23 => Arc::new(Butterfly23::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly24 => Arc::new(Butterfly24::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly27 => Arc::new(Butterfly27::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly29 => Arc::new(Butterfly29::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly31 => Arc::new(Butterfly31::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::Butterfly32 => Arc::new(Butterfly32::new(direction)) as Arc<dyn Fft<T>>,
            Recipe::MixedRadix {
                left_fft,
                right_fft,
            } => {
                let left_fft = self.build_fft(&left_fft, direction);
                let right_fft = self.build_fft(&right_fft, direction);
                Arc::new(MixedRadix::new(left_fft, right_fft)) as Arc<dyn Fft<T>>
            }
            Recipe::GoodThomasAlgorithm {
                left_fft,
                right_fft,
            } => {
                let left_fft = self.build_fft(&left_fft, direction);
                let right_fft = self.build_fft(&right_fft, direction);
                Arc::new(GoodThomasAlgorithm::new(left_fft, right_fft)) as Arc<dyn Fft<T>>
            }
            Recipe::MixedRadixSmall {
                left_fft,
                right_fft,
            } => {
                let left_fft = self.build_fft(&left_fft, direction);
                let right_fft = self.build_fft(&right_fft, direction);
                Arc::new(MixedRadixSmall::new(left_fft, right_fft)) as Arc<dyn Fft<T>>
            }
            Recipe::GoodThomasAlgorithmSmall {
                left_fft,
                right_fft,
            } => {
                let left_fft = self.build_fft(&left_fft, direction);
                let right_fft = self.build_fft(&right_fft, direction);
                Arc::new(GoodThomasAlgorithmSmall::new(left_fft, right_fft)) as Arc<dyn Fft<T>>
            }
            Recipe::RadersAlgorithm { inner_fft } => {
                let inner_fft = self.build_fft(&inner_fft, direction);
                Arc::new(RadersAlgorithm::new(inner_fft)) as Arc<dyn Fft<T>>
            }
            Recipe::BluesteinsAlgorithm { len, inner_fft } => {
                let inner_fft = self.build_fft(&inner_fft, direction);
                Arc::new(BluesteinsAlgorithm::new(*len, inner_fft)) as Arc<dyn Fft<T>>
            }
        }
    }

    fn design_fft_with_factors(&mut self, len: usize, factors: PrimeFactors) -> Arc<Recipe> {
        if let Some(fft_instance) = self.design_butterfly_algorithm(len) {
            fft_instance
        } else if factors.is_prime() {
            self.design_prime(len)
        } else if let Some(butterfly_product) = self.design_butterfly_product(len) {
            butterfly_product
        } else if factors.has_factors_leq(MAX_RADIXN_FACTOR) {
            self.design_radixn(factors)
        } else {
            let (left_factors, right_factors) = factors.partition_factors();
            self.design_mixed_radix(left_factors, right_factors)
        }
    }

    fn design_butterfly_product(&mut self, len: usize) -> Option<Arc<Recipe>> {
        if len > 992 || len.is_power_of_two() {
            return None;
        } // 31*32 = 992. if we're above this size, don't bother. anddon't bother for powers of 2 because radix4 is fast

        let limit = (len as f64).sqrt().ceil() as usize + 1;
        let butterflies = [
            2, 3, 4, 5, 6, 7, 8, 9, 11, 13, 16, 17, 19, 23, 24, 27, 29, 31, 32,
        ];

        // search through our butterflies. if we find one that divides the length, see of the quotient is also a butterfly
        // if it is, we have a butterfly product
        // if there are multiple valid pairs, take the one with the smallest sum - we want the values to be as close together as possible
        // ie 32 x 2, sum = 34, 16 x 4, sum = 20, 8 x 8, sum = 16, so even though 32,2 and 16x4 are valid, we want 8x8

        let mut min_sum = usize::MAX;
        let mut found_butterflies = None;
        for left in butterflies.iter().take_while(|n| **n < limit) {
            let right = len / left;
            if left * right == len && butterflies.contains(&right) {
                let sum = left + right;
                if sum < min_sum {
                    min_sum = sum;
                    found_butterflies = Some((*left, right))
                }
            }
        }

        // if we found a valid pair of butterflies, construct a recipe for them
        found_butterflies.map(|(left_len, right_len)| {
            let left_fft = self.design_fft_for_len(left_len);
            let right_fft = self.design_fft_for_len(right_len);

            if gcd(left_len, right_len) == 1 {
                Arc::new(Recipe::GoodThomasAlgorithmSmall {
                    left_fft,
                    right_fft,
                })
            } else {
                Arc::new(Recipe::MixedRadixSmall {
                    left_fft,
                    right_fft,
                })
            }
        })
    }

    fn design_mixed_radix(
        &mut self,
        left_factors: PrimeFactors,
        right_factors: PrimeFactors,
    ) -> Arc<Recipe> {
        let left_len = left_factors.get_product();
        let right_len = right_factors.get_product();

        //neither size is a butterfly, so go with the normal algorithm
        let left_fft = self.design_fft_with_factors(left_len, left_factors);
        let right_fft = self.design_fft_with_factors(right_len, right_factors);

        //if both left_len and right_len are small, use algorithms optimized for small FFTs
        if left_len < 31 && right_len < 31 {
            // for small FFTs, if gcd is 1, good-thomas is faster
            if gcd(left_len, right_len) == 1 {
                Arc::new(Recipe::GoodThomasAlgorithmSmall {
                    left_fft,
                    right_fft,
                })
            } else {
                Arc::new(Recipe::MixedRadixSmall {
                    left_fft,
                    right_fft,
                })
            }
        } else {
            Arc::new(Recipe::MixedRadix {
                left_fft,
                right_fft,
            })
        }
    }

    fn design_radixn(&mut self, factors: PrimeFactors) -> Arc<Recipe> {
        let p2 = factors.get_power_of_two();
        let p3 = factors.get_power_of_three();
        let p5 = factors
            .get_other_factors()
            .iter()
            .find_map(|f| if f.value == 5 { Some(f.count) } else { None }) // if we had rustc 1.62, we could use (f.value == 5).then_some(f.count)
            .unwrap_or(0);
        let p7 = factors
            .get_other_factors()
            .iter()
            .find_map(|f| if f.value == 7 { Some(f.count) } else { None })
            .unwrap_or(0);

        let base_len: usize = if factors.has_factors_gt(MAX_RADIXN_FACTOR) {
            // If we have factors larger than RadixN can handle, we *must* use the product of those factors as our base
            factors.product_above(MAX_RADIXN_FACTOR)
        } else if p7 == 0 && p5 == 0 && p3 < 2 {
            // here we handle pure powers of 2 and 3 times a power of 2 - we want to hand these to radix4, so we need to consume the correct number of factors to leave us with 4^k
            if p3 == 0 {
                // pure power of 2
                assert!(p2 > 5); // butterflies should have caught this
                if p2 % 2 == 1 {
                    8
                } else {
                    16
                }
            } else {
                // 3 times a power of 2
                assert!(p2 > 3); // butterflies should have caught this
                if p2 % 2 == 1 {
                    24
                } else {
                    12
                }
            }
        } else if p2 > 0 && p3 > 0 {
            // we have a mixed bag of 2s and 3s
            // todo: if we have way more 3s than 2s, benchmark using butterfly27 as the base
            let excess_p2 = p2.saturating_sub(p3);
            match excess_p2 {
                0 => 6,
                1 => 12,
                _ => 24,
            }
        } else if p3 > 2 {
            27
        } else if p3 > 1 {
            9
        } else if p7 > 0 {
            7
        } else {
            assert!(p5 > 0);
            5
        };

        // now that we know the base length, divide it out get what radix4 needs to compute
        let base_fft = self.design_fft_for_len(base_len);
        let mut cross_len = factors.get_product() / base_len;

        // see if we can use radix4
        let cross_bits = cross_len.trailing_zeros();
        if cross_len.is_power_of_two() && cross_bits % 2 == 0 {
            let k = cross_bits / 2;
            return Arc::new(Recipe::Radix4 { k, base_fft });
        }

        // we weren't able to use radix4, so fall back to RadixN
        // theoretically we could do this with the p2, p3, p5 etc values above, but our choice of base knocked them out of sync
        let mut factors = Vec::new();
        while cross_len % 7 == 0 {
            cross_len /= 7;
            factors.push(RadixFactor::Factor7);
        }
        while cross_len % 6 == 0 {
            cross_len /= 6;
            factors.push(RadixFactor::Factor6);
        }
        while cross_len % 5 == 0 {
            cross_len /= 5;
            factors.push(RadixFactor::Factor5);
        }
        while cross_len % 3 == 0 {
            cross_len /= 3;
            factors.push(RadixFactor::Factor3);
        }
        assert!(cross_len.is_power_of_two());

        // benchmarking suggests that we want to add the 4s *last*, i suspect because 4 is a better-than-usual value for the transpose
        let cross_bits = cross_len.trailing_zeros();
        if cross_bits % 2 == 1 {
            factors.push(RadixFactor::Factor2);
        }
        factors.extend(std::iter::repeat(RadixFactor::Factor4).take(cross_bits as usize / 2));

        Arc::new(Recipe::RadixN {
            factors: factors.into_boxed_slice(),
            base_fft,
        })
    }

    // Returns Some(instance) if we have a butterfly available for this size. Returns None if there is no butterfly available for this size
    fn design_butterfly_algorithm(&mut self, len: usize) -> Option<Arc<Recipe>> {
        match len {
            2 => Some(Arc::new(Recipe::Butterfly2)),
            3 => Some(Arc::new(Recipe::Butterfly3)),
            4 => Some(Arc::new(Recipe::Butterfly4)),
            5 => Some(Arc::new(Recipe::Butterfly5)),
            6 => Some(Arc::new(Recipe::Butterfly6)),
            7 => Some(Arc::new(Recipe::Butterfly7)),
            8 => Some(Arc::new(Recipe::Butterfly8)),
            9 => Some(Arc::new(Recipe::Butterfly9)),
            11 => Some(Arc::new(Recipe::Butterfly11)),
            12 => Some(Arc::new(Recipe::Butterfly12)),
            13 => Some(Arc::new(Recipe::Butterfly13)),
            16 => Some(Arc::new(Recipe::Butterfly16)),
            17 => Some(Arc::new(Recipe::Butterfly17)),
            19 => Some(Arc::new(Recipe::Butterfly19)),
            23 => Some(Arc::new(Recipe::Butterfly23)),
            24 => Some(Arc::new(Recipe::Butterfly24)),
            27 => Some(Arc::new(Recipe::Butterfly27)),
            29 => Some(Arc::new(Recipe::Butterfly29)),
            31 => Some(Arc::new(Recipe::Butterfly31)),
            32 => Some(Arc::new(Recipe::Butterfly32)),
            _ => None,
        }
    }

    fn design_prime(&mut self, len: usize) -> Arc<Recipe> {
        let inner_fft_len_rader = len - 1;
        let raders_factors = PrimeFactors::compute(inner_fft_len_rader);
        // If any of the prime factors is too large, Rader's gets slow and Bluestein's is the better choice
        if raders_factors
            .get_other_factors()
            .iter()
            .any(|val| val.value > MAX_RADER_PRIME_FACTOR)
        {
            // we want to use bluestein's algorithm. we have a free choice of which inner FFT length to use
            // the only restriction is that it has to be (2 * len - 1) or larger. So we want the fastest FFT we can compute at or above that size.

            // the most obvious choice is the next-highest power of two, but there's one trick we can pull to get a smaller fft that we can be 100% certain will be faster
            let min_inner_len = 2 * len - 1;
            let inner_len_pow2 = min_inner_len.checked_next_power_of_two().unwrap();
            let inner_len_factor3 = inner_len_pow2 / 4 * 3;

            let inner_len = if inner_len_factor3 >= min_inner_len {
                inner_len_factor3
            } else {
                inner_len_pow2
            };
            let inner_fft = self.design_fft_for_len(inner_len);

            Arc::new(Recipe::BluesteinsAlgorithm { len, inner_fft })
        } else {
            let inner_fft = self.design_fft_with_factors(inner_fft_len_rader, raders_factors);
            Arc::new(Recipe::RadersAlgorithm { inner_fft })
        }
    }
}

#[cfg(test)]
mod unit_tests {
    use super::*;

    fn is_mixedradixsmall(plan: &Recipe) -> bool {
        match plan {
            &Recipe::MixedRadixSmall { .. } => true,
            _ => false,
        }
    }

    fn is_goodthomassmall(plan: &Recipe) -> bool {
        match plan {
            &Recipe::GoodThomasAlgorithmSmall { .. } => true,
            _ => false,
        }
    }

    fn is_raders(plan: &Recipe) -> bool {
        match plan {
            &Recipe::RadersAlgorithm { .. } => true,
            _ => false,
        }
    }

    fn is_bluesteins(plan: &Recipe) -> bool {
        match plan {
            &Recipe::BluesteinsAlgorithm { .. } => true,
            _ => false,
        }
    }

    #[test]
    fn test_plan_scalar_trivial() {
        // Length 0 and 1 should use Dft
        let mut planner = FftPlannerScalar::<f64>::new();
        for len in 0..2 {
            let plan = planner.design_fft_for_len(len);
            assert_eq!(*plan, Recipe::Dft(len));
            assert_eq!(plan.len(), len, "Recipe reports wrong length");
        }
    }

    #[test]
    fn test_plan_scalar_largepoweroftwo() {
        // Powers of 2 above 64 should use Radix4
        let mut planner = FftPlannerScalar::<f64>::new();
        for pow in 6..32 {
            let len = 1 << pow;
            let plan = planner.design_fft_for_len(len);
            assert!(matches!(*plan, Recipe::Radix4 { k: _, base_fft: _ }));
            assert_eq!(plan.len(), len, "Recipe reports wrong length");
        }
    }

    #[test]
    fn test_plan_scalar_butterflies() {
        // Check that all butterflies are used
        let mut planner = FftPlannerScalar::<f64>::new();
        assert_eq!(*planner.design_fft_for_len(2), Recipe::Butterfly2);
        assert_eq!(*planner.design_fft_for_len(3), Recipe::Butterfly3);
        assert_eq!(*planner.design_fft_for_len(4), Recipe::Butterfly4);
        assert_eq!(*planner.design_fft_for_len(5), Recipe::Butterfly5);
        assert_eq!(*planner.design_fft_for_len(6), Recipe::Butterfly6);
        assert_eq!(*planner.design_fft_for_len(7), Recipe::Butterfly7);
        assert_eq!(*planner.design_fft_for_len(8), Recipe::Butterfly8);
        assert_eq!(*planner.design_fft_for_len(11), Recipe::Butterfly11);
        assert_eq!(*planner.design_fft_for_len(12), Recipe::Butterfly12);
        assert_eq!(*planner.design_fft_for_len(13), Recipe::Butterfly13);
        assert_eq!(*planner.design_fft_for_len(16), Recipe::Butterfly16);
        assert_eq!(*planner.design_fft_for_len(17), Recipe::Butterfly17);
        assert_eq!(*planner.design_fft_for_len(19), Recipe::Butterfly19);
        assert_eq!(*planner.design_fft_for_len(23), Recipe::Butterfly23);
        assert_eq!(*planner.design_fft_for_len(24), Recipe::Butterfly24);
        assert_eq!(*planner.design_fft_for_len(29), Recipe::Butterfly29);
        assert_eq!(*planner.design_fft_for_len(31), Recipe::Butterfly31);
        assert_eq!(*planner.design_fft_for_len(32), Recipe::Butterfly32);
    }

    #[test]
    fn test_plan_scalar_radixn() {
        // Products of several different small primes should become RadixN
        let mut planner = FftPlannerScalar::<f64>::new();
        for pow2 in 2..5 {
            for pow3 in 2..5 {
                for pow5 in 2..5 {
                    for pow7 in 2..5 {
                        let len = 2usize.pow(pow2)
                            * 3usize.pow(pow3)
                            * 5usize.pow(pow5)
                            * 7usize.pow(pow7);
                        let plan = planner.design_fft_for_len(len);
                        assert!(
                            matches!(
                                *plan,
                                Recipe::RadixN {
                                    factors: _,
                                    base_fft: _
                                }
                            ),
                            "Expected MixedRadix, got {:?}",
                            plan
                        );
                        assert_eq!(plan.len(), len, "Recipe reports wrong length");
                    }
                }
            }
        }
    }

    #[test]
    fn test_plan_scalar_mixedradixsmall() {
        // Products of two "small" butterflies < 31 that have a common divisor >1, and isn't a power of 2 should be MixedRadixSmall
        let mut planner = FftPlannerScalar::<f64>::new();
        for len in [12 * 3, 6 * 27].iter() {
            let plan = planner.design_fft_for_len(*len);
            assert!(
                is_mixedradixsmall(&plan),
                "Expected MixedRadixSmall, got {:?}",
                plan
            );
            assert_eq!(plan.len(), *len, "Recipe reports wrong length");
        }
    }

    #[test]
    fn test_plan_scalar_goodthomasbutterfly() {
        let mut planner = FftPlannerScalar::<f64>::new();
        for len in [3 * 5, 3 * 7, 5 * 7, 11 * 13].iter() {
            let plan = planner.design_fft_for_len(*len);
            assert!(
                is_goodthomassmall(&plan),
                "Expected GoodThomasAlgorithmSmall, got {:?}",
                plan
            );
            assert_eq!(plan.len(), *len, "Recipe reports wrong length");
        }
    }

    #[test]
    fn test_plan_scalar_bluestein_vs_rader() {
        let difficultprimes: [usize; 11] = [59, 83, 107, 149, 167, 173, 179, 359, 719, 1439, 2879];
        let easyprimes: [usize; 24] = [
            53, 61, 67, 71, 73, 79, 89, 97, 101, 103, 109, 113, 127, 131, 137, 139, 151, 157, 163,
            181, 191, 193, 197, 199,
        ];

        let mut planner = FftPlannerScalar::<f64>::new();
        for len in difficultprimes.iter() {
            let plan = planner.design_fft_for_len(*len);
            assert!(
                is_bluesteins(&plan),
                "Expected BluesteinsAlgorithm, got {:?}",
                plan
            );
            assert_eq!(plan.len(), *len, "Recipe reports wrong length");
        }
        for len in easyprimes.iter() {
            let plan = planner.design_fft_for_len(*len);
            assert!(is_raders(&plan), "Expected RadersAlgorithm, got {:?}", plan);
            assert_eq!(plan.len(), *len, "Recipe reports wrong length");
        }
    }

    #[test]
    fn test_scalar_fft_cache() {
        {
            // Check that FFTs are reused if they're both forward
            let mut planner = FftPlannerScalar::<f64>::new();
            let fft_a = planner.plan_fft(1234, FftDirection::Forward);
            let fft_b = planner.plan_fft(1234, FftDirection::Forward);
            assert!(Arc::ptr_eq(&fft_a, &fft_b), "Existing fft was not reused");
        }
        {
            // Check that FFTs are reused if they're both inverse
            let mut planner = FftPlannerScalar::<f64>::new();
            let fft_a = planner.plan_fft(1234, FftDirection::Inverse);
            let fft_b = planner.plan_fft(1234, FftDirection::Inverse);
            assert!(Arc::ptr_eq(&fft_a, &fft_b), "Existing fft was not reused");
        }
        {
            // Check that FFTs are NOT resued if they don't both have the same direction
            let mut planner = FftPlannerScalar::<f64>::new();
            let fft_a = planner.plan_fft(1234, FftDirection::Forward);
            let fft_b = planner.plan_fft(1234, FftDirection::Inverse);
            assert!(
                !Arc::ptr_eq(&fft_a, &fft_b),
                "Existing fft was reused, even though directions don't match"
            );
        }
    }

    #[test]
    fn test_scalar_recipe_cache() {
        // Check that all butterflies are used
        let mut planner = FftPlannerScalar::<f64>::new();
        let fft_a = planner.design_fft_for_len(1234);
        let fft_b = planner.design_fft_for_len(1234);
        assert!(
            Arc::ptr_eq(&fft_a, &fft_b),
            "Existing recipe was not reused"
        );
    }

    // We don't need to actually compute anything for a FFT size of zero, but we do need to verify that it doesn't explode
    #[test]
    fn test_plan_zero_scalar() {
        let mut planner32 = FftPlannerScalar::<f32>::new();
        let fft_zero32 = planner32.plan_fft_forward(0);
        fft_zero32.process(&mut []);

        let mut planner64 = FftPlannerScalar::<f64>::new();
        let fft_zero64 = planner64.plan_fft_forward(0);
        fft_zero64.process(&mut []);
    }

    // This test is not designed to be run, only to compile.
    // We cannot make it #[test] since there is a generic parameter.
    #[allow(dead_code)]
    fn test_impl_fft_planner_send<T: FftNum>() {
        fn is_send<T: Send>() {}
        is_send::<FftPlanner<T>>();
        is_send::<FftPlannerScalar<T>>();
        is_send::<FftPlannerSse<T>>();
        is_send::<FftPlannerAvx<T>>();
    }
}
