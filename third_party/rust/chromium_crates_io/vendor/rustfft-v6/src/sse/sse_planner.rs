use num_integer::gcd;
use std::any::TypeId;
use std::collections::HashMap;

use std::sync::Arc;

use crate::{common::FftNum, fft_cache::FftCache, FftDirection};

use crate::algorithm::*;
use crate::sse::sse_butterflies::*;
use crate::sse::sse_prime_butterflies;
use crate::sse::sse_radix4::*;
use crate::Fft;

use crate::math_utils::{PrimeFactor, PrimeFactors};

const MIN_RADIX4_BITS: u32 = 6; // smallest size to consider radix 4 an option is 2^6 = 64
const MAX_RADER_PRIME_FACTOR: usize = 23; // don't use Raders if the inner fft length has prime factor larger than this

/// A Recipe is a structure that describes the design of a FFT, without actually creating it.
/// It is used as a middle step in the planning process.
#[derive(Debug, PartialEq, Clone)]
pub enum Recipe {
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
    Radix4 {
        k: u32,
        base_fft: Arc<Recipe>,
    },
    Butterfly1,
    Butterfly2,
    Butterfly3,
    Butterfly4,
    Butterfly5,
    Butterfly6,
    Butterfly8,
    Butterfly9,
    Butterfly10,
    Butterfly12,
    Butterfly15,
    Butterfly16,
    Butterfly24,
    Butterfly32,
    PrimeButterfly {
        len: usize,
    },
}

impl Recipe {
    pub fn len(&self) -> usize {
        match self {
            Recipe::Dft(length) => *length,
            Recipe::Radix4 { k, base_fft } => base_fft.len() * (1 << (k * 2)),
            Recipe::Butterfly1 => 1,
            Recipe::Butterfly2 => 2,
            Recipe::Butterfly3 => 3,
            Recipe::Butterfly4 => 4,
            Recipe::Butterfly5 => 5,
            Recipe::Butterfly6 => 6,
            Recipe::Butterfly8 => 8,
            Recipe::Butterfly9 => 9,
            Recipe::Butterfly10 => 10,
            Recipe::Butterfly12 => 12,
            Recipe::Butterfly15 => 15,
            Recipe::Butterfly16 => 16,
            Recipe::Butterfly24 => 24,
            Recipe::Butterfly32 => 32,
            Recipe::PrimeButterfly { len } => *len,
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

/// The SSE FFT planner creates new FFT algorithm instances using a mix of scalar and SSE accelerated algorithms.
/// It requires at least SSE4.1, which is available on all reasonably recent x86_64 cpus.
///
/// RustFFT has several FFT algorithms available. For a given FFT size, the `FftPlannerSse` decides which of the
/// available FFT algorithms to use and then initializes them.
///
/// ~~~
/// // Perform a forward Fft of size 1234
/// use std::sync::Arc;
/// use rustfft::{FftPlannerSse, num_complex::Complex};
///
/// if let Ok(mut planner) = FftPlannerSse::new() {
///   let fft = planner.plan_fft_forward(1234);
///
///   let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 1234];
///   fft.process(&mut buffer);
///
///   // The FFT instance returned by the planner has the type `Arc<dyn Fft<T>>`,
///   // where T is the numeric type, ie f32 or f64, so it's cheap to clone
///   let fft_clone = Arc::clone(&fft);
/// }
/// ~~~
///
/// If you plan on creating multiple FFT instances, it is recommended to re-use the same planner for all of them. This
/// is because the planner re-uses internal data across FFT instances wherever possible, saving memory and reducing
/// setup time. (FFT instances created with one planner will never re-use data and buffers with FFT instances created
/// by a different planner)
///
/// Each FFT instance owns [`Arc`s](std::sync::Arc) to its internal data, rather than borrowing it from the planner, so it's perfectly
/// safe to drop the planner after creating Fft instances.
pub struct FftPlannerSse<T: FftNum> {
    algorithm_cache: FftCache<T>,
    recipe_cache: HashMap<usize, Arc<Recipe>>,
    all_butterflies: Box<[usize]>,
}

impl<T: FftNum> FftPlannerSse<T> {
    /// Creates a new `FftPlannerSse` instance.
    ///
    /// Returns `Ok(planner_instance)` if we're compiling for X86_64, SSE support was enabled in feature flags, and the current CPU supports the `sse4.1` CPU feature.
    /// Returns `Err(())` if SSE support is not available.
    pub fn new() -> Result<Self, ()> {
        if is_x86_feature_detected!("sse4.1") {
            // Ideally, we would implement the planner with specialization.
            // Specialization won't be on stable rust for a long time though, so in the meantime, we can hack around it.
            //
            // We use TypeID to determine if T is f32, f64, or neither. If neither, we don't want to do any SSE acceleration
            // If it's f32 or f64, then construct and return a SSE planner instance.
            //
            // All SSE accelerated algorithms come in separate versions for f32 and f64. The type is checked when a new one is created, and if it does not
            // match the type the FFT is meant for, it will panic. This will never be a problem if using a planner to construct the FFTs.
            //
            // An annoying snag with this setup is that we frequently have to transmute buffers from &mut [Complex<T>] to &mut [Complex<f32 or f64>] or vice versa.
            // We know this is safe because we assert everywhere that Type(f32 or f64)==Type(T), so it's just a matter of "doing it right" every time.
            // These transmutes are required because the FFT algorithm's input will come through the FFT trait, which may only be bounded by FftNum.
            // So the buffers will have the type &mut [Complex<T>].
            let id_f32 = TypeId::of::<f32>();
            let id_f64 = TypeId::of::<f64>();
            let id_t = TypeId::of::<T>();

            if id_t == id_f32 || id_t == id_f64 {
                let hand_butterflies: [usize; 13] = [2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16, 24, 32];
                let prime_butterflies = sse_prime_butterflies::prime_butterfly_lens();
                let mut all_butterflies: Box<[usize]> = hand_butterflies
                    .iter()
                    .chain(prime_butterflies.iter())
                    .copied()
                    .collect();
                all_butterflies.sort();

                // make sure we didn't get any duplicate butterflies from multiple sources
                // since it's sorted, any duplicates will be adjacent
                for window in all_butterflies.windows(2) {
                    assert_ne!(window[0], window[1]);
                }

                return Ok(Self {
                    algorithm_cache: FftCache::new(),
                    recipe_cache: HashMap::new(),
                    all_butterflies,
                });
            }
        }
        Err(())
    }

    /// Returns a `Fft` instance which uses SSE4.1 instructions to compute FFTs of size `len`.
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

    /// Returns a `Fft` instance which uses SSE4.1 instructions to compute forward FFTs of size `len`
    ///
    /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
    pub fn plan_fft_forward(&mut self, len: usize) -> Arc<dyn Fft<T>> {
        self.plan_fft(len, FftDirection::Forward)
    }

    /// Returns a `Fft` instance which uses SSE4.1 instructions to compute inverse FFTs of size `len.
    ///
    /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
    pub fn plan_fft_inverse(&mut self, len: usize) -> Arc<dyn Fft<T>> {
        self.plan_fft(len, FftDirection::Inverse)
    }

    // Make a recipe for a length
    fn design_fft_for_len(&mut self, len: usize) -> Arc<Recipe> {
        if len < 1 {
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
        let id_f32 = TypeId::of::<f32>();
        let id_f64 = TypeId::of::<f64>();
        let id_t = TypeId::of::<T>();

        match recipe {
            Recipe::Dft(len) => Arc::new(Dft::new(*len, direction)) as Arc<dyn Fft<T>>,
            Recipe::Radix4 { k, base_fft } => {
                let base_fft = self.build_fft(&base_fft, direction);
                if id_t == id_f32 {
                    Arc::new(SseRadix4::<f32, T>::new(*k, base_fft).unwrap()) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseRadix4::<f64, T>::new(*k, base_fft).unwrap()) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly1 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly1::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly1::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly2 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly2::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly2::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly3 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly3::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly3::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly4 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly4::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly4::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly5 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly5::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly5::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly6 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly6::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly6::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly8 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly8::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly8::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly9 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly9::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly9::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly10 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly10::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly10::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly12 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly12::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly12::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly15 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly15::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly15::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly16 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly16::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly16::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly24 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly24::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly24::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::Butterfly32 => {
                if id_t == id_f32 {
                    Arc::new(SseF32Butterfly32::new(direction)) as Arc<dyn Fft<T>>
                } else if id_t == id_f64 {
                    Arc::new(SseF64Butterfly32::new(direction)) as Arc<dyn Fft<T>>
                } else {
                    panic!("Not f32 or f64");
                }
            }
            Recipe::PrimeButterfly { len } => {
                // Safety: construct_prime_butterfly() requires the sse4.1 instruction set, and we checked that in the constructor
                unsafe { sse_prime_butterflies::construct_prime_butterfly(*len, direction) }
            }
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
        } else if len.trailing_zeros() >= MIN_RADIX4_BITS {
            if factors.get_other_factors().is_empty() && factors.get_power_of_three() < 2 {
                self.design_radix4(factors)
            } else {
                let non_power_of_two = factors
                    .remove_factors(PrimeFactor {
                        value: 2,
                        count: len.trailing_zeros(),
                    })
                    .unwrap();
                let power_of_two = PrimeFactors::compute(1 << len.trailing_zeros());
                self.design_mixed_radix(power_of_two, non_power_of_two)
            }
        } else {
            // Can we do this as a mixed radix with just two butterflies?
            // Loop through and find all combinations
            // If more than one is found, keep the one where the factors are closer together.
            // For example length 20 where 10x2 and 5x4 are possible, we use 5x4.
            let mut bf_left = 0;
            let mut bf_right = 0;
            // If the length is below 14, or over 1024 we don't need to try this.
            if len > 13 && len <= 1024 {
                for (n, bf_l) in self.all_butterflies.iter().enumerate() {
                    if len % bf_l == 0 {
                        let bf_r = len / bf_l;
                        if self.all_butterflies.iter().skip(n).any(|&m| m == bf_r) {
                            bf_left = *bf_l;
                            bf_right = bf_r;
                        }
                    }
                }
                if bf_left > 0 {
                    let fact_l = PrimeFactors::compute(bf_left);
                    let fact_r = PrimeFactors::compute(bf_right);
                    return self.design_mixed_radix(fact_l, fact_r);
                }
            }
            // Not possible with just butterflies, go with the general solution.
            let (left_factors, right_factors) = factors.partition_factors();
            self.design_mixed_radix(left_factors, right_factors)
        }
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
        if left_len < 33 && right_len < 33 {
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

    // Returns Some(instance) if we have a butterfly available for this size. Returns None if there is no butterfly available for this size
    fn design_butterfly_algorithm(&mut self, len: usize) -> Option<Arc<Recipe>> {
        match len {
            1 => Some(Arc::new(Recipe::Butterfly1)),
            2 => Some(Arc::new(Recipe::Butterfly2)),
            3 => Some(Arc::new(Recipe::Butterfly3)),
            4 => Some(Arc::new(Recipe::Butterfly4)),
            5 => Some(Arc::new(Recipe::Butterfly5)),
            6 => Some(Arc::new(Recipe::Butterfly6)),
            8 => Some(Arc::new(Recipe::Butterfly8)),
            9 => Some(Arc::new(Recipe::Butterfly9)),
            10 => Some(Arc::new(Recipe::Butterfly10)),
            12 => Some(Arc::new(Recipe::Butterfly12)),
            15 => Some(Arc::new(Recipe::Butterfly15)),
            16 => Some(Arc::new(Recipe::Butterfly16)),
            24 => Some(Arc::new(Recipe::Butterfly24)),
            32 => Some(Arc::new(Recipe::Butterfly32)),
            _ => {
                if sse_prime_butterflies::prime_butterfly_lens().contains(&len) {
                    Some(Arc::new(Recipe::PrimeButterfly { len }))
                } else {
                    None
                }
            }
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

    fn design_radix4(&mut self, factors: PrimeFactors) -> Arc<Recipe> {
        // We can eventually relax this restriction -- it's not instrinsic to radix4, it's just that anything besides 2^n and 3*2^n hasn't been measured yet
        assert!(factors.get_other_factors().is_empty() && factors.get_power_of_three() < 2);

        let p2 = factors.get_power_of_two();
        let base_len: usize = if factors.get_power_of_three() == 0 {
            // pure power of 2
            match p2 {
                // base cases. we shouldn't hit these but we might as well be ready for them
                0 => 1,
                1 => 2,
                2 => 4,
                3 => 8,
                // main case: if len is a power of 4, use a base of 16, otherwise use a base of 32
                _ => {
                    if p2 % 2 == 1 {
                        32
                    } else {
                        16
                    }
                }
            }
        } else {
            // we have a factor 3 that we're going to stick into the butterflies
            match p2 {
                // base cases. we shouldn't hit these but we might as well be ready for them
                0 => 3,
                1 => 6,
                // main case: if len is 3*4^k, use a base of 12, otherwise use a base of 24
                _ => {
                    if p2 % 2 == 1 {
                        24
                    } else {
                        12
                    }
                }
            }
        };

        // now that we know the base length, divide it out get what radix4 needs to compute
        let cross_len = factors.get_product() / base_len;
        assert!(cross_len.is_power_of_two());

        let cross_bits = cross_len.trailing_zeros();
        assert!(cross_bits % 2 == 0);
        let k = cross_bits / 2;

        let base_fft = self.design_fft_for_len(base_len);
        Arc::new(Recipe::Radix4 { k, base_fft })
    }
}

#[cfg(test)]
mod unit_tests {
    use super::*;

    fn is_mixedradix(plan: &Recipe) -> bool {
        match plan {
            &Recipe::MixedRadix { .. } => true,
            _ => false,
        }
    }

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
    fn test_plan_sse_trivial() {
        // Length 0 and 1 should use Dft
        let mut planner = FftPlannerSse::<f64>::new().unwrap();
        for len in 0..1 {
            let plan = planner.design_fft_for_len(len);
            assert_eq!(*plan, Recipe::Dft(len));
            assert_eq!(plan.len(), len, "Recipe reports wrong length");
        }
    }

    #[test]
    fn test_plan_sse_largepoweroftwo() {
        // Powers of 2 above 6 should use Radix4
        let mut planner = FftPlannerSse::<f64>::new().unwrap();
        for pow in 6..32 {
            let len = 1 << pow;
            let plan = planner.design_fft_for_len(len);
            assert!(matches!(*plan, Recipe::Radix4 { k: _, base_fft: _ }));
            assert_eq!(plan.len(), len, "Recipe reports wrong length");
        }
    }

    #[test]
    fn test_plan_sse_butterflies() {
        // Check that all butterflies are used
        let mut planner = FftPlannerSse::<f64>::new().unwrap();
        assert_eq!(*planner.design_fft_for_len(2), Recipe::Butterfly2);
        assert_eq!(*planner.design_fft_for_len(3), Recipe::Butterfly3);
        assert_eq!(*planner.design_fft_for_len(4), Recipe::Butterfly4);
        assert_eq!(*planner.design_fft_for_len(5), Recipe::Butterfly5);
        assert_eq!(*planner.design_fft_for_len(6), Recipe::Butterfly6);
        assert_eq!(*planner.design_fft_for_len(8), Recipe::Butterfly8);
        assert_eq!(*planner.design_fft_for_len(9), Recipe::Butterfly9);
        assert_eq!(*planner.design_fft_for_len(10), Recipe::Butterfly10);
        assert_eq!(*planner.design_fft_for_len(12), Recipe::Butterfly12);
        assert_eq!(*planner.design_fft_for_len(15), Recipe::Butterfly15);
        assert_eq!(*planner.design_fft_for_len(16), Recipe::Butterfly16);
        assert_eq!(*planner.design_fft_for_len(24), Recipe::Butterfly24);
        assert_eq!(*planner.design_fft_for_len(32), Recipe::Butterfly32);
        for len in sse_prime_butterflies::prime_butterfly_lens() {
            assert_eq!(
                *planner.design_fft_for_len(*len),
                Recipe::PrimeButterfly { len: *len }
            );
        }
    }

    #[test]
    fn test_plan_sse_mixedradix() {
        // Products of several different primes should become MixedRadix
        let mut planner = FftPlannerSse::<f64>::new().unwrap();
        for pow2 in 2..5 {
            for pow3 in 2..5 {
                for pow5 in 2..5 {
                    for pow7 in 2..5 {
                        let len = 2usize.pow(pow2)
                            * 3usize.pow(pow3)
                            * 5usize.pow(pow5)
                            * 7usize.pow(pow7);
                        let plan = planner.design_fft_for_len(len);
                        assert!(is_mixedradix(&plan), "Expected MixedRadix, got {:?}", plan);
                        assert_eq!(plan.len(), len, "Recipe reports wrong length");
                    }
                }
            }
        }
    }

    #[test]
    fn test_plan_sse_mixedradixsmall() {
        // Products of two "small" lengths < 31 that have a common divisor >1, and isn't a power of 2 should be MixedRadixSmall
        let mut planner = FftPlannerSse::<f64>::new().unwrap();
        for len in [5 * 20, 5 * 25].iter() {
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
    fn test_plan_sse_goodthomasbutterfly() {
        let mut planner = FftPlannerSse::<f64>::new().unwrap();
        for len in [3 * 7, 5 * 7, 11 * 13, 2 * 29].iter() {
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
    fn test_plan_sse_bluestein_vs_rader() {
        let difficultprimes: [usize; 11] = [59, 83, 107, 149, 167, 173, 179, 359, 719, 1439, 2879];
        let easyprimes: [usize; 24] = [
            53, 61, 67, 71, 73, 79, 89, 97, 101, 103, 109, 113, 127, 131, 137, 139, 151, 157, 163,
            181, 191, 193, 197, 199,
        ];

        let mut planner = FftPlannerSse::<f64>::new().unwrap();
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
    fn test_sse_fft_cache() {
        {
            // Check that FFTs are reused if they're both forward
            let mut planner = FftPlannerSse::<f64>::new().unwrap();
            let fft_a = planner.plan_fft(1234, FftDirection::Forward);
            let fft_b = planner.plan_fft(1234, FftDirection::Forward);
            assert!(Arc::ptr_eq(&fft_a, &fft_b), "Existing fft was not reused");
        }
        {
            // Check that FFTs are reused if they're both inverse
            let mut planner = FftPlannerSse::<f64>::new().unwrap();
            let fft_a = planner.plan_fft(1234, FftDirection::Inverse);
            let fft_b = planner.plan_fft(1234, FftDirection::Inverse);
            assert!(Arc::ptr_eq(&fft_a, &fft_b), "Existing fft was not reused");
        }
        {
            // Check that FFTs are NOT resued if they don't both have the same direction
            let mut planner = FftPlannerSse::<f64>::new().unwrap();
            let fft_a = planner.plan_fft(1234, FftDirection::Forward);
            let fft_b = planner.plan_fft(1234, FftDirection::Inverse);
            assert!(
                !Arc::ptr_eq(&fft_a, &fft_b),
                "Existing fft was reused, even though directions don't match"
            );
        }
    }

    #[test]
    fn test_sse_recipe_cache() {
        // Check that all butterflies are used
        let mut planner = FftPlannerSse::<f64>::new().unwrap();
        let fft_a = planner.design_fft_for_len(1234);
        let fft_b = planner.design_fft_for_len(1234);
        assert!(
            Arc::ptr_eq(&fft_a, &fft_b),
            "Existing recipe was not reused"
        );
    }
}
