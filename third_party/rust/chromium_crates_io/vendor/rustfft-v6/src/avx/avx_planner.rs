use std::sync::Arc;
use std::{any::TypeId, cmp::min};

use primal_check::miller_rabin;

use crate::algorithm::*;
use crate::common::FftNum;
use crate::math_utils::PartialFactors;
use crate::Fft;
use crate::{algorithm::butterflies::*, fft_cache::FftCache};

use super::*;

fn wrap_fft<T: FftNum>(butterfly: impl Fft<T> + 'static) -> Arc<dyn Fft<T>> {
    Arc::new(butterfly) as Arc<dyn Fft<T>>
}

#[derive(Debug)]
enum MixedRadixBase {
    // The base will be a butterfly algorithm
    ButterflyBase(usize),

    // The base will be an instance of Rader's Algorithm. That will require its own plan for the internal FFT, which we'll handle separately
    RadersBase(usize),

    // The base will be an instance of Bluestein's Algorithm. That will require its own plan for the internal FFT, which we'll handle separately.
    // First usize is the base length, second usize is the inner FFT length
    BluesteinsBase(usize, usize),

    // The "base" is a FFT instance we already have cached
    CacheBase(usize),
}
impl MixedRadixBase {
    fn base_len(&self) -> usize {
        match self {
            Self::ButterflyBase(len) => *len,
            Self::RadersBase(len) => *len,
            Self::BluesteinsBase(len, _) => *len,
            Self::CacheBase(len) => *len,
        }
    }
}

/// repreesnts a FFT plan, stored as a base FFT and a stack of MixedRadix*xn on top of it.
#[derive(Debug)]
pub struct MixedRadixPlan {
    len: usize,       // product of base and radixes
    radixes: Vec<u8>, // stored from innermost to outermost
    base: MixedRadixBase,
}
impl MixedRadixPlan {
    fn new(base: MixedRadixBase, radixes: Vec<u8>) -> Self {
        Self {
            len: base.base_len() * radixes.iter().map(|r| *r as usize).product::<usize>(),
            base,
            radixes,
        }
    }
    fn cached(cached_len: usize) -> Self {
        Self {
            len: cached_len,
            base: MixedRadixBase::CacheBase(cached_len),
            radixes: Vec::new(),
        }
    }
    fn butterfly(butterfly_len: usize, radixes: Vec<u8>) -> Self {
        Self::new(MixedRadixBase::ButterflyBase(butterfly_len), radixes)
    }
    fn push_radix(&mut self, radix: u8) {
        self.radixes.push(radix);
        self.len *= radix as usize;
    }
    fn push_radix_power(&mut self, radix: u8, power: u32) {
        self.radixes
            .extend(std::iter::repeat(radix).take(power as usize));
        self.len *= (radix as usize).pow(power);
    }
}

/// The AVX FFT planner creates new FFT algorithm instances which take advantage of the AVX instruction set.
///
/// Creating an instance of `FftPlannerAvx` requires the `avx` and `fma` instructions to be available on the current machine, and it requires RustFFT's
///  `avx` feature flag to be set. A few algorithms will use `avx2` if it's available, but it isn't required.
///
/// For the time being, AVX acceleration is black box, and AVX accelerated algorithms are not available without a planner. This may change in the future.
///
/// ~~~
/// // Perform a forward Fft of size 1234, accelerated by AVX
/// use std::sync::Arc;
/// use rustfft::{FftPlannerAvx, num_complex::Complex};
///
/// // If FftPlannerAvx::new() returns Ok(), we'll know AVX algorithms are available
/// // on this machine, and that RustFFT was compiled with the `avx` feature flag
/// if let Ok(mut planner) = FftPlannerAvx::new() {
///     let fft = planner.plan_fft_forward(1234);
///
///     let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 1234];
///     fft.process(&mut buffer);
///
///     // The FFT instance returned by the planner has the type `Arc<dyn Fft<T>>`,
///     // where T is the numeric type, ie f32 or f64, so it's cheap to clone
///     let fft_clone = Arc::clone(&fft);
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
pub struct FftPlannerAvx<T: FftNum> {
    internal_planner: Box<dyn AvxPlannerInternalAPI<T>>,
}
impl<T: FftNum> FftPlannerAvx<T> {
    /// Constructs a new `FftPlannerAvx` instance.
    ///
    /// Returns `Ok(planner_instance)` if we're compiling for X86_64, AVX support was enabled in feature flags, and the current CPU supports the `avx` and `fma` CPU features.
    /// Returns `Err(())` if AVX support is not available.
    pub fn new() -> Result<Self, ()> {
        // Eventually we might make AVX algorithms that don't also require FMA.
        // If that happens, we can only check for AVX here? seems like a pretty low-priority addition
        let has_avx = is_x86_feature_detected!("avx");
        let has_fma = is_x86_feature_detected!("fma");
        if has_avx && has_fma {
            // Ideally, we would implement the planner with specialization.
            // Specialization won't be on stable rust for a long time tohugh, so in the meantime, we can hack around it.
            //
            // The first step of the hack is to use TypeID to determine if T is f32, f64, or neither. If neither, we don't want to di any AVX acceleration
            // If it's f32 or f64, then construct an internal type that has two generic parameters, one bounded on AvxNum, the other bounded on FftNum
            //
            // - A is bounded on the AvxNum trait, and is the type we use for any AVX computations. It has associated types for AVX vectors,
            //      associated constants for the number of elements per vector, etc.
            // - T is bounded on the FftNum trait, and thus is the type that every FFT algorithm will recieve its input/output buffers in.
            //
            // An important snag relevant to the planner is that we have to box and type-erase the AvxNum bound,
            // since the only other option is making the AvxNum bound a part of this struct's external API
            //
            // Another annoying snag with this setup is that we frequently have to transmute buffers from &mut [Complex<T>] to &mut [Complex<A>] or vice versa.
            // We know this is safe because we assert everywhere that Type(A)==Type(T), so it's just a matter of "doing it right" every time.
            // These transmutes are required because the FFT algorithm's input will come through the FFT trait, which may only be bounded by FftNum.
            // So the buffers will have the type &mut [Complex<T>]. The problem comes in that all of our AVX computation tools are on the AvxNum trait.
            //
            // If we had specialization, we could easily convince the compilr that AvxNum and FftNum were different bounds on the same underlying type (IE f32 or f64)
            // but without it, the compiler is convinced that they are different. So we use the transmute as a last-resort way to overcome this limitation.
            //
            // We keep both the A and T types around in all of our AVX-related structs so that we can cast between A and T whenever necessary.
            let id_f32 = TypeId::of::<f32>();
            let id_f64 = TypeId::of::<f64>();
            let id_t = TypeId::of::<T>();

            if id_t == id_f32 {
                return Ok(Self {
                    internal_planner: Box::new(AvxPlannerInternal::<f32, T>::new()),
                });
            } else if id_t == id_f64 {
                return Ok(Self {
                    internal_planner: Box::new(AvxPlannerInternal::<f64, T>::new()),
                });
            }
        }
        Err(())
    }

    /// Returns a `Fft` instance which uses AVX instructions to compute FFTs of size `len`.
    ///
    /// If the provided `direction` is `FftDirection::Forward`, the returned instance will compute forward FFTs. If it's `FftDirection::Inverse`, it will compute inverse FFTs.
    ///
    /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
    pub fn plan_fft(&mut self, len: usize, direction: FftDirection) -> Arc<dyn Fft<T>> {
        self.internal_planner.plan_and_construct_fft(len, direction)
    }
    /// Returns a `Fft` instance which uses AVX instructions to compute forward FFTs of size `len`.
    ///
    /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
    pub fn plan_fft_forward(&mut self, len: usize) -> Arc<dyn Fft<T>> {
        self.plan_fft(len, FftDirection::Forward)
    }
    /// Returns a `Fft` instance which uses AVX instructions to compute inverse FFTs of size `len.
    ///
    /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
    pub fn plan_fft_inverse(&mut self, len: usize) -> Arc<dyn Fft<T>> {
        self.plan_fft(len, FftDirection::Inverse)
    }

    /// Returns a FFT plan without constructing it
    #[allow(unused)]
    pub(crate) fn debug_plan_fft(&self, len: usize, direction: FftDirection) -> MixedRadixPlan {
        self.internal_planner.debug_plan_fft(len, direction)
    }
}

trait AvxPlannerInternalAPI<T: FftNum>: Send + Sync {
    fn plan_and_construct_fft(&mut self, len: usize, direction: FftDirection) -> Arc<dyn Fft<T>>;
    fn debug_plan_fft(&self, len: usize, direction: FftDirection) -> MixedRadixPlan;
}

struct AvxPlannerInternal<A: AvxNum, T: FftNum> {
    cache: FftCache<T>,
    _phantom: std::marker::PhantomData<A>,
}

impl<T: FftNum> AvxPlannerInternalAPI<T> for AvxPlannerInternal<f32, T> {
    fn plan_and_construct_fft(&mut self, len: usize, direction: FftDirection) -> Arc<dyn Fft<T>> {
        // Step 1: Create a plan for this FFT length.
        let plan = self.plan_fft(len, direction, Self::plan_mixed_radix_base);

        // Step 2: Construct the plan. If the base is rader's algorithm or bluestein's algorithm, this may call self.plan_and_construct_fft recursively!
        self.construct_plan(
            plan,
            direction,
            Self::construct_butterfly,
            Self::plan_and_construct_fft,
        )
    }
    fn debug_plan_fft(&self, len: usize, direction: FftDirection) -> MixedRadixPlan {
        self.plan_fft(len, direction, Self::plan_mixed_radix_base)
    }
}
impl<T: FftNum> AvxPlannerInternalAPI<T> for AvxPlannerInternal<f64, T> {
    fn plan_and_construct_fft(&mut self, len: usize, direction: FftDirection) -> Arc<dyn Fft<T>> {
        // Step 1: Create a plan for this FFT length.
        let plan = self.plan_fft(len, direction, Self::plan_mixed_radix_base);

        // Step 2: Construct the plan. If the base is rader's algorithm or bluestein's algorithm, this may call self.plan_and_construct_fft recursively!
        self.construct_plan(
            plan,
            direction,
            Self::construct_butterfly,
            Self::plan_and_construct_fft,
        )
    }
    fn debug_plan_fft(&self, len: usize, direction: FftDirection) -> MixedRadixPlan {
        self.plan_fft(len, direction, Self::plan_mixed_radix_base)
    }
}

//-------------------------------------------------------------------
// f32-specific planning stuff
//-------------------------------------------------------------------
impl<T: FftNum> AvxPlannerInternal<f32, T> {
    pub fn new() -> Self {
        // Internal sanity check: Make sure that T == f32.
        // This struct has two generic parameters A and T, but they must always be the same, and are only kept separate to help work around the lack of specialization.
        // It would be cool if we could do this as a static_assert instead
        let id_f32 = TypeId::of::<f32>();
        let id_t = TypeId::of::<T>();
        assert_eq!(id_f32, id_t);

        Self {
            cache: FftCache::new(),
            _phantom: std::marker::PhantomData,
        }
    }

    fn plan_mixed_radix_base(&self, len: usize, factors: &PartialFactors) -> MixedRadixPlan {
        // if we have non-fast-path factors, use them as our base FFT length, and we will have to use either rader's algorithm or bluestein's algorithm as our base
        if factors.get_other_factors() > 1 {
            let other_factors = factors.get_other_factors();

            // First, if the "other factors" are a butterfly, use that as the butterfly
            if self.is_butterfly(other_factors) {
                return MixedRadixPlan::butterfly(other_factors, vec![]);
            }

            // We can only use rader's if `other_factors` is prime
            if miller_rabin(other_factors as u64) {
                // len is prime, so we can use Rader's Algorithm as a base. Whether or not that's a good idea is a different story
                // Rader's Algorithm is only faster in a few narrow cases.
                // as a heuristic, only use rader's algorithm if its inner FFT can be computed entirely without bluestein's or rader's
                // We're intentionally being too conservative here. Otherwise we'd be recursively applying a heuristic, and repeated heuristic failures could stack to make a rader's chain significantly slower.
                // If we were writing a measuring planner, expanding this heuristic and measuring its effectiveness would be an opportunity for up to 2x performance gains.
                let inner_factors = PartialFactors::compute(other_factors - 1);
                if self.is_butterfly(inner_factors.get_other_factors()) {
                    // We only have factors of 2,3,5,7, and 11. If we don't have AVX2, we also have to exclude factors of 5 and 7 and 11, because avx2 gives us enough headroom for the overhead of those to not be a problem
                    if is_x86_feature_detected!("avx2")
                        || (inner_factors.product_power2power3() == len - 1)
                    {
                        return MixedRadixPlan::new(
                            MixedRadixBase::RadersBase(other_factors),
                            vec![],
                        );
                    }
                }
            }

            // At this point, we know we're using bluestein's algorithm for the base. Next step is to plan the inner size we'll use for bluestein's algorithm.
            let inner_bluesteins_len =
                self.plan_bluesteins(other_factors, |(_len, factor2, factor3)| {
                    if *factor2 > 16 && *factor3 < 3 {
                        // surprisingly, pure powers of 2 have a pretty steep dropoff in speed after 65536.
                        // the algorithm is designed to generate candidadtes larger than baseline_candidate, so if we hit a large power of 2, there should be more after it that we can skip to
                        return false;
                    }
                    true
                });
            return MixedRadixPlan::new(
                MixedRadixBase::BluesteinsBase(other_factors, inner_bluesteins_len),
                vec![],
            );
        }

        // If this FFT size is a butterfly, use that
        if self.is_butterfly(len) {
            return MixedRadixPlan::butterfly(len, vec![]);
        }

        // If the power2 * power3 component of this FFT is a butterfly and not too small, return that
        let power2power3 = factors.product_power2power3();
        if power2power3 > 4 && self.is_butterfly(power2power3) {
            return MixedRadixPlan::butterfly(power2power3, vec![]);
        }

        // most of this code is heuristics assuming FFTs of a minimum size. if the FFT is below that minimum size, the heuristics break down.
        // so the first thing we're going to do is hardcode the plan for osme specific sizes where we know the heuristics won't be enough
        let hardcoded_base = match power2power3 {
            // 3 * 2^n special cases
            96 => Some(MixedRadixPlan::butterfly(32, vec![3])), // 2^5 * 3
            192 => Some(MixedRadixPlan::butterfly(48, vec![4])), // 2^6 * 3
            1536 => Some(MixedRadixPlan::butterfly(48, vec![8, 4])), // 2^8 * 3

            // 9 * 2^n special cases
            18 => Some(MixedRadixPlan::butterfly(3, vec![6])), // 2 * 3^2
            144 => Some(MixedRadixPlan::butterfly(36, vec![4])), // 2^4 * 3^2

            _ => None,
        };
        if let Some(hardcoded) = hardcoded_base {
            return hardcoded;
        }

        if factors.get_power2() >= 5 {
            match factors.get_power3() {
                // if this FFT is a power of 2, our strategy here is to tweak the butterfly to free us up to do an 8xn chain
                0 => match factors.get_power2() % 3 {
                    0 => MixedRadixPlan::butterfly(512, vec![]),
                    1 => MixedRadixPlan::butterfly(256, vec![]),
                    2 => MixedRadixPlan::butterfly(256, vec![]),
                    _ => unreachable!(),
                },
                // if this FFT is 3 times a power of 2, our strategy here is to tweak butterflies to make it easier to set up a 8xn chain
                1 => match factors.get_power2() % 3 {
                    0 => MixedRadixPlan::butterfly(64, vec![12, 16]),
                    1 => MixedRadixPlan::butterfly(48, vec![]),
                    2 => MixedRadixPlan::butterfly(64, vec![]),
                    _ => unreachable!(),
                },
                // if this FFT is 9 or greater times a power of 2, just use 72. As you might expect, in this vast field of options, what is optimal becomes a lot more muddy and situational
                // but across all the benchmarking i've done, 72 seems like the best default that will get us the best plan in 95% of the cases
                // 64, 54, and 48 are occasionally faster, although i haven't been able to discern a pattern.
                _ => MixedRadixPlan::butterfly(72, vec![]),
            }
        } else if factors.get_power3() >= 3 {
            // Our FFT is a power of 3 times a low power of 2. A high level summary of our strategy is that we want to pick a base that will
            // A: consume all factors of 2, and B: leave us with an even power of 3, so that we can do a 9xn chain.
            match factors.get_power2() {
                0 => MixedRadixPlan::butterfly(27, vec![]),
                1 => MixedRadixPlan::butterfly(54, vec![]),
                2 => match factors.get_power3() % 2 {
                    0 => MixedRadixPlan::butterfly(36, vec![]),
                    1 => MixedRadixPlan::butterfly(if len < 1000 { 36 } else { 12 }, vec![]),
                    _ => unreachable!(),
                },
                3 => match factors.get_power3() % 2 {
                    0 => MixedRadixPlan::butterfly(72, vec![]),
                    1 => MixedRadixPlan::butterfly(
                        if factors.get_power3() > 7 { 24 } else { 72 },
                        vec![],
                    ),
                    _ => unreachable!(),
                },
                4 => match factors.get_power3() % 2 {
                    0 => MixedRadixPlan::butterfly(
                        if factors.get_power3() > 6 { 16 } else { 72 },
                        vec![],
                    ),
                    1 => MixedRadixPlan::butterfly(
                        if factors.get_power3() > 9 { 48 } else { 72 },
                        vec![],
                    ),
                    _ => unreachable!(),
                },
                // if this FFT is 32 or greater times a power of 3, just use 72. As you might expect, in this vast field of options, what is optimal becomes a lot more muddy and situational
                // but across all the benchmarking i've done, 72 seems like the best default that will get us the best plan in 95% of the cases
                // 64, 54, and 48 are occasionally faster, although i haven't been able to discern a pattern.
                _ => MixedRadixPlan::butterfly(72, vec![]),
            }
        }
        // If this FFT has powers of 11, 7, or 5, use that
        else if factors.get_power11() > 0 {
            MixedRadixPlan::butterfly(11, vec![])
        } else if factors.get_power7() > 0 {
            MixedRadixPlan::butterfly(7, vec![])
        } else if factors.get_power5() > 0 {
            MixedRadixPlan::butterfly(5, vec![])
        } else {
            panic!(
                "Couldn't find a base for FFT size {}, factors={:?}",
                len, factors
            )
        }
    }

    fn is_butterfly(&self, len: usize) -> bool {
        [
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 16, 17, 19, 23, 24, 27, 29, 31, 32, 36, 48,
            54, 64, 72, 128, 256, 512,
        ]
        .contains(&len)
    }

    fn construct_butterfly(&self, len: usize, direction: FftDirection) -> Arc<dyn Fft<T>> {
        match len {
            0 | 1 => wrap_fft(Dft::new(len, direction)),
            2 => wrap_fft(Butterfly2::new(direction)),
            3 => wrap_fft(Butterfly3::new(direction)),
            4 => wrap_fft(Butterfly4::new(direction)),
            5 => wrap_fft(Butterfly5Avx::new(direction).unwrap()),
            6 => wrap_fft(Butterfly6::new(direction)),
            7 => wrap_fft(Butterfly7Avx::new(direction).unwrap()),
            8 => wrap_fft(Butterfly8Avx::new(direction).unwrap()),
            9 => wrap_fft(Butterfly9Avx::new(direction).unwrap()),
            11 => wrap_fft(Butterfly11Avx::new(direction).unwrap()),
            12 => wrap_fft(Butterfly12Avx::new(direction).unwrap()),
            13 => wrap_fft(Butterfly13::new(direction)),
            16 => wrap_fft(Butterfly16Avx::new(direction).unwrap()),
            17 => wrap_fft(Butterfly17::new(direction)),
            19 => wrap_fft(Butterfly19::new(direction)),
            23 => wrap_fft(Butterfly23::new(direction)),
            24 => wrap_fft(Butterfly24Avx::new(direction).unwrap()),
            27 => wrap_fft(Butterfly27Avx::new(direction).unwrap()),
            29 => wrap_fft(Butterfly29::new(direction)),
            31 => wrap_fft(Butterfly31::new(direction)),
            32 => wrap_fft(Butterfly32Avx::new(direction).unwrap()),
            36 => wrap_fft(Butterfly36Avx::new(direction).unwrap()),
            48 => wrap_fft(Butterfly48Avx::new(direction).unwrap()),
            54 => wrap_fft(Butterfly54Avx::new(direction).unwrap()),
            64 => wrap_fft(Butterfly64Avx::new(direction).unwrap()),
            72 => wrap_fft(Butterfly72Avx::new(direction).unwrap()),
            128 => wrap_fft(Butterfly128Avx::new(direction).unwrap()),
            256 => wrap_fft(Butterfly256Avx::new(direction).unwrap()),
            512 => wrap_fft(Butterfly512Avx::new(direction).unwrap()),
            _ => panic!("Invalid butterfly len: {}", len),
        }
    }
}

//-------------------------------------------------------------------
// f64-specific planning stuff
//-------------------------------------------------------------------
impl<T: FftNum> AvxPlannerInternal<f64, T> {
    pub fn new() -> Self {
        // Internal sanity check: Make sure that T == f64.
        // This struct has two generic parameters A and T, but they must always be the same, and are only kept separate to help work around the lack of specialization.
        // It would be cool if we could do this as a static_assert instead
        let id_f64 = TypeId::of::<f64>();
        let id_t = TypeId::of::<T>();
        assert_eq!(id_f64, id_t);

        Self {
            cache: FftCache::new(),
            _phantom: std::marker::PhantomData,
        }
    }

    fn plan_mixed_radix_base(&self, len: usize, factors: &PartialFactors) -> MixedRadixPlan {
        // if we have a factor that can't be computed with 2xn 3xn etc, we'll have to compute it with bluestein's or rader's, so use that as the base
        if factors.get_other_factors() > 1 {
            let other_factors = factors.get_other_factors();

            // First, if the "other factors" are a butterfly, use that as the butterfly
            if self.is_butterfly(other_factors) {
                return MixedRadixPlan::butterfly(other_factors, vec![]);
            }

            // We can only use rader's if `other_factors` is prime
            if miller_rabin(other_factors as u64) {
                // len is prime, so we can use Rader's Algorithm as a base. Whether or not that's a good idea is a different story
                // Rader's Algorithm is only faster in a few narrow cases.
                // as a heuristic, only use rader's algorithm if its inner FFT can be computed entirely without bluestein's or rader's
                // We're intentionally being too conservative here. Otherwise we'd be recursively applying a heuristic, and repeated heuristic failures could stack to make a rader's chain significantly slower.
                // If we were writing a measuring planner, expanding this heuristic and measuring its effectiveness would be an opportunity for up to 2x performance gains.
                let inner_factors = PartialFactors::compute(other_factors - 1);
                if self.is_butterfly(inner_factors.get_other_factors()) {
                    // We only have factors of 2,3,5,7, and 11. If we don't have AVX2, we also have to exclude factors of 5 and 7 and 11, because avx2 gives us enough headroom for the overhead of those to not be a problem
                    if is_x86_feature_detected!("avx2")
                        || (inner_factors.product_power2power3() == len - 1)
                    {
                        return MixedRadixPlan::new(
                            MixedRadixBase::RadersBase(other_factors),
                            vec![],
                        );
                    }
                }
            }

            // At this point, we know we're using bluestein's algorithm for the base. Next step is to plan the inner size we'll use for bluestein's algorithm.
            let inner_bluesteins_len =
                self.plan_bluesteins(other_factors, |(_len, factor2, factor3)| {
                    if *factor3 < 1 && *factor2 > 13 {
                        return false;
                    }
                    if *factor3 < 4 && *factor2 > 14 {
                        return false;
                    }
                    true
                });
            return MixedRadixPlan::new(
                MixedRadixBase::BluesteinsBase(other_factors, inner_bluesteins_len),
                vec![],
            );
        }

        // If this FFT size is a butterfly, use that
        if self.is_butterfly(len) {
            return MixedRadixPlan::butterfly(len, vec![]);
        }

        // If the power2 * power3 component of this FFT is a butterfly and not too small, return that
        let power2power3 = factors.product_power2power3();
        if power2power3 > 4 && self.is_butterfly(power2power3) {
            return MixedRadixPlan::butterfly(power2power3, vec![]);
        }

        // most of this code is heuristics assuming FFTs of a minimum size. if the FFT is below that minimum size, the heuristics break down.
        // so the first thing we're going to do is hardcode the plan for osme specific sizes where we know the heuristics won't be enough
        let hardcoded_base = match power2power3 {
            // 2^n special cases
            64 => Some(MixedRadixPlan::butterfly(16, vec![4])), // 2^6

            // 3 * 2^n special cases
            48 => Some(MixedRadixPlan::butterfly(12, vec![4])), // 3 * 2^4
            96 => Some(MixedRadixPlan::butterfly(12, vec![8])), // 3 * 2^5
            768 => Some(MixedRadixPlan::butterfly(12, vec![8, 8])), // 3 * 2^8

            // 9 * 2^n special cases
            72 => Some(MixedRadixPlan::butterfly(24, vec![3])), // 2^3 * 3^2
            288 => Some(MixedRadixPlan::butterfly(32, vec![9])), // 2^5 * 3^2

            // 4 * 3^n special cases
            108 => Some(MixedRadixPlan::butterfly(18, vec![6])), // 2^4 * 3^2
            _ => None,
        };
        if let Some(hardcoded) = hardcoded_base {
            return hardcoded;
        }

        if factors.get_power2() >= 4 {
            match factors.get_power3() {
                // if this FFT is a power of 2, our strategy here is to tweak the butterfly to free us up to do an 8xn chain
                0 => match factors.get_power2() % 3 {
                    0 => MixedRadixPlan::butterfly(512, vec![]),
                    1 => MixedRadixPlan::butterfly(128, vec![]),
                    2 => MixedRadixPlan::butterfly(256, vec![]),
                    _ => unreachable!(),
                },
                // if this FFT is 3 times a power of 2, our strategy here is to tweak butterflies to make it easier to set up a 8xn chain
                1 => match factors.get_power2() % 3 {
                    0 => MixedRadixPlan::butterfly(24, vec![]),
                    1 => MixedRadixPlan::butterfly(32, vec![12]),
                    2 => MixedRadixPlan::butterfly(32, vec![12, 16]),
                    _ => unreachable!(),
                },
                // if this FFT is 9 times a power of 2, our strategy here is to tweak butterflies to make it easier to set up a 8xn chain
                2 => match factors.get_power2() % 3 {
                    0 => MixedRadixPlan::butterfly(36, vec![16]),
                    1 => MixedRadixPlan::butterfly(36, vec![]),
                    2 => MixedRadixPlan::butterfly(18, vec![]),
                    _ => unreachable!(),
                },
                // this FFT is 27 or greater times a power of two. As you might expect, in this vast field of options, what is optimal becomes a lot more muddy and situational
                // but across all the benchmarking i've done, 36 seems like the best default that will get us the best plan in 95% of the cases
                // 32 is rarely faster, although i haven't been able to discern a pattern.
                _ => MixedRadixPlan::butterfly(36, vec![]),
            }
        } else if factors.get_power3() >= 3 {
            // Our FFT is a power of 3 times a low power of 2
            match factors.get_power2() {
                0 => match factors.get_power3() % 2 {
                    0 => MixedRadixPlan::butterfly(
                        if factors.get_power3() > 10 { 9 } else { 27 },
                        vec![],
                    ),
                    1 => MixedRadixPlan::butterfly(27, vec![]),
                    _ => unreachable!(),
                },
                1 => MixedRadixPlan::butterfly(18, vec![]),
                2 => match factors.get_power3() % 2 {
                    0 => MixedRadixPlan::butterfly(36, vec![]),
                    1 => MixedRadixPlan::butterfly(
                        if factors.get_power3() > 10 { 36 } else { 18 },
                        vec![],
                    ),
                    _ => unreachable!(),
                },
                3 => MixedRadixPlan::butterfly(18, vec![]),
                // this FFT is 16 or greater times a power of three. As you might expect, in this vast field of options, what is optimal becomes a lot more muddy and situational
                // but across all the benchmarking i've done, 36 seems like the best default that will get us the best plan in 95% of the cases
                // 32 is rarely faster, although i haven't been able to discern a pattern.
                _ => MixedRadixPlan::butterfly(36, vec![]),
            }
        }
        // If this FFT has powers of 11, 7, or 5, use that
        else if factors.get_power11() > 0 {
            MixedRadixPlan::butterfly(11, vec![])
        } else if factors.get_power7() > 0 {
            MixedRadixPlan::butterfly(7, vec![])
        } else if factors.get_power5() > 0 {
            MixedRadixPlan::butterfly(5, vec![])
        } else {
            panic!(
                "Couldn't find a base for FFT size {}, factors={:?}",
                len, factors
            )
        }
    }

    fn is_butterfly(&self, len: usize) -> bool {
        [
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 16, 17, 18, 19, 23, 24, 27, 29, 31, 32, 36,
            64, 128, 256, 512,
        ]
        .contains(&len)
    }

    fn construct_butterfly(&self, len: usize, direction: FftDirection) -> Arc<dyn Fft<T>> {
        match len {
            0 | 1 => wrap_fft(Dft::new(len, direction)),
            2 => wrap_fft(Butterfly2::new(direction)),
            3 => wrap_fft(Butterfly3::new(direction)),
            4 => wrap_fft(Butterfly4::new(direction)),
            5 => wrap_fft(Butterfly5Avx64::new(direction).unwrap()),
            6 => wrap_fft(Butterfly6::new(direction)),
            7 => wrap_fft(Butterfly7Avx64::new(direction).unwrap()),
            8 => wrap_fft(Butterfly8Avx64::new(direction).unwrap()),
            9 => wrap_fft(Butterfly9Avx64::new(direction).unwrap()),
            11 => wrap_fft(Butterfly11Avx64::new(direction).unwrap()),
            12 => wrap_fft(Butterfly12Avx64::new(direction).unwrap()),
            13 => wrap_fft(Butterfly13::new(direction)),
            16 => wrap_fft(Butterfly16Avx64::new(direction).unwrap()),
            17 => wrap_fft(Butterfly17::new(direction)),
            18 => wrap_fft(Butterfly18Avx64::new(direction).unwrap()),
            19 => wrap_fft(Butterfly19::new(direction)),
            23 => wrap_fft(Butterfly23::new(direction)),
            24 => wrap_fft(Butterfly24Avx64::new(direction).unwrap()),
            27 => wrap_fft(Butterfly27Avx64::new(direction).unwrap()),
            29 => wrap_fft(Butterfly29::new(direction)),
            31 => wrap_fft(Butterfly31::new(direction)),
            32 => wrap_fft(Butterfly32Avx64::new(direction).unwrap()),
            36 => wrap_fft(Butterfly36Avx64::new(direction).unwrap()),
            64 => wrap_fft(Butterfly64Avx64::new(direction).unwrap()),
            128 => wrap_fft(Butterfly128Avx64::new(direction).unwrap()),
            256 => wrap_fft(Butterfly256Avx64::new(direction).unwrap()),
            512 => wrap_fft(Butterfly512Avx64::new(direction).unwrap()),
            _ => panic!("Invalid butterfly len: {}", len),
        }
    }
}

//-------------------------------------------------------------------
// type-agnostic planning stuff
//-------------------------------------------------------------------
impl<A: AvxNum, T: FftNum> AvxPlannerInternal<A, T> {
    // Given a length, return a plan for how this FFT should be computed
    fn plan_fft(
        &self,
        len: usize,
        direction: FftDirection,
        base_fn: impl FnOnce(&Self, usize, &PartialFactors) -> MixedRadixPlan,
    ) -> MixedRadixPlan {
        // First step: If this size is already cached, return it directly
        if self.cache.contains_fft(len, direction) {
            return MixedRadixPlan::cached(len);
        }

        // We have butterflies for everything below 10, so if it's below 10, just skip the factorization etc
        // Notably, this step is *required* if the len is 0, since we can't compute a prime factorization for zero
        if len < 10 {
            return MixedRadixPlan::butterfly(len, Vec::new());
        }

        // This length is not cached, so we have to come up with a new plan. The first step is to find a suitable base.
        let factors = PartialFactors::compute(len);
        let base = base_fn(self, len, &factors);

        // it's possible that the base planner plans out the whole FFT. it's guaranteed if `len` is a prime number, or if it's a butterfly, for example
        let uncached_plan = if base.len == len {
            base
        } else {
            // We have some mixed radix steps to compute! Compute the factors that need to computed by mixed radix steps,
            let radix_factors = factors
                .divide_by(&PartialFactors::compute(base.len))
                .unwrap_or_else(|| {
                    panic!(
                        "Invalid base for FFT length={}, base={:?}, base radixes={:?}",
                        len, base.base, base.radixes
                    )
                });
            self.plan_mixed_radix(radix_factors, base)
        };

        // Last step: We have a full FFT plan, but some of the steps of that plan may have been cached. If they have, use the largest cached step as the base.
        self.replan_with_cache(uncached_plan, direction)
    }

    // Takes a plan and an algorithm cache, and replaces steps of the plan with cached steps, if possible
    fn replan_with_cache(&self, plan: MixedRadixPlan, direction: FftDirection) -> MixedRadixPlan {
        enum CacheLocation {
            None,
            Base,
            Radix(usize, usize), // First value is the length of the cached FFT, and second value is the index in the radix array
        }

        let mut largest_cached_len = CacheLocation::None;
        let base_len = plan.base.base_len();
        let mut current_len = base_len;

        // Check if the cache contains the base
        if self.cache.contains_fft(current_len, direction) {
            largest_cached_len = CacheLocation::Base;
        }

        // Walk up the radix chain, checking if rthe cache contains each step
        for (i, radix) in plan.radixes.iter().enumerate() {
            current_len *= *radix as usize;

            if self.cache.contains_fft(current_len, direction) {
                largest_cached_len = CacheLocation::Radix(current_len, i);
            }
        }

        // If we found a cached length within the plan, update the plan to account for the cache
        match largest_cached_len {
            CacheLocation::None => plan,
            CacheLocation::Base => {
                MixedRadixPlan::new(MixedRadixBase::CacheBase(base_len), plan.radixes)
            }
            CacheLocation::Radix(cached_len, cached_index) => {
                // We know that `plan.radixes[cached_index]` is the largest cache value, and `cached_len` will be our new base legth
                // Drop every element from `plan.radixes` from up to and including cached_index
                let mut chain = plan.radixes;
                chain.drain(0..=cached_index);
                MixedRadixPlan::new(MixedRadixBase::CacheBase(cached_len), chain)
            }
        }
    }
    // given a set of factors, compute how many iterations of 12xn and 16xn we should plan for. Returns (k, j) for 12^k and 6^j
    fn plan_power12_power6(radix_factors: &PartialFactors) -> (u32, u32) {
        // it's helpful to think of this process as rewriting the FFT length as powers of our radixes
        // the fastest FFT we could possibly compute is 8^n, because the 8xn algorithm is blazing fast. 9xn and 12xn are also in the top tier for speed, so those 3 algorithms are what we will aim for
        // Specifically, we want to find a combination of 8, 9, and 12, that will "consume" all factors of 2 and 3, without having any leftovers

        // Unfortunately, most FFTs don't come in the form 8^n * 9^m * 12^k
        // Thankfully, 6xn is also reasonably fast, so we can use 6xn to strip away factors.
        // This function's job will be to divide radix_factors into 8^n * 9^m * 12^k * 6^j, which minimizes j, then maximizes k

        // we're going to hypothetically add as many 12's to our plan as possible, keeping track of how many 6's were required to balance things out
        // we can also compute this analytically with modular arithmetic, but that technique only works when the FFT is above a minimum size, but this loop+array technique always works
        let max_twelves = min(radix_factors.get_power2() / 2, radix_factors.get_power3());
        let mut required_sixes = [None; 4]; // only track 6^0 through 6^3. 6^4 can be converted into 12^2 * 9, and 6^5 can be converted into 12 * 8 * 9 * 9
        for hypothetical_twelve_power in 0..(max_twelves + 1) {
            let hypothetical_twos = radix_factors.get_power2() - hypothetical_twelve_power * 2;
            let hypothetical_threes = radix_factors.get_power3() - hypothetical_twelve_power;

            // figure out how many sixes we would need to leave our FFT at 8^n * 9^m via modular arithmetic, and write to that index of our twelves_per_sixes array
            let sixes = match (hypothetical_twos % 3, hypothetical_threes % 2) {
                (0, 0) => Some(0),
                (1, 1) => Some(1),
                (2, 0) => Some(2),
                (0, 1) => Some(3),
                (1, 0) => None, // it would take 4 sixes, which can be replaced by 2 twelves, so we'll hit it in a later loop (if we have that many factors)
                (2, 1) => None, // it would take 5 sixes, but note that 12 is literally 2^2 * 3^1, so instead of applying 5 sixes, we can apply a single 12
                (_, _) => unreachable!(),
            };

            // if we can bring the FFT into range for the fast path with sixes, record so in the required_sixes array
            // but make sure the number of sixes we're going to apply actually fits into our available factors
            if let Some(sixes) = sixes {
                if sixes <= hypothetical_twos && sixes <= hypothetical_threes {
                    required_sixes[sixes as usize] = Some(hypothetical_twelve_power)
                }
            }
        }

        // required_sixes[i] now contains the largest power of twelve that we can apply, given that we also apply 6^i
        // we want to apply as many of 12 as possible, so take the array element with the largest non-None element
        // note that it's possible (and very likely) that either power_twelve or power_six is zero, or both of them are zero! this will happen for a pure power of 2 or power of 3 FFT, for example
        let (power_twelve, mut power_six) = required_sixes
            .iter()
            .enumerate()
            .filter_map(|(i, maybe_twelve)| maybe_twelve.map(|twelve| (twelve, i as u32)))
            .fold(
                (0, 0),
                |best, current| if current.0 >= best.0 { current } else { best },
            );

        // special case: if we have exactly one factor of 2 and at least one factor of 3, unconditionally apply a factor of 6 to get rid of the 2
        if radix_factors.get_power2() == 1 && radix_factors.get_power3() > 0 {
            power_six = 1;
        }
        // special case: if we have a single factor of 3 and more than one factor of 2 (and we don't have any twelves), unconditionally apply a factor of 6 to get rid of the 3
        if radix_factors.get_power2() > 1 && radix_factors.get_power3() == 1 && power_twelve == 0 {
            power_six = 1;
        }

        (power_twelve, power_six)
    }

    fn plan_mixed_radix(
        &self,
        mut radix_factors: PartialFactors,
        mut plan: MixedRadixPlan,
    ) -> MixedRadixPlan {
        // if we can complete the FFT with a single radix, do it
        if [2, 3, 4, 5, 6, 7, 8, 9, 12, 16].contains(&radix_factors.product()) {
            plan.push_radix(radix_factors.product() as u8)
        } else {
            // Compute how many powers of 12 and powers of 6 we want to strip away
            let (power_twelve, power_six) = Self::plan_power12_power6(&radix_factors);

            // divide our powers of 12 and 6 out of our radix factors
            radix_factors = radix_factors
                .divide_by(&PartialFactors::compute(
                    6usize.pow(power_six) * 12usize.pow(power_twelve),
                ))
                .unwrap();

            // now that we know the 12 and 6 factors, the plan array can be computed in descending radix size
            if radix_factors.get_power2() % 3 == 1 && radix_factors.get_power2() > 1 {
                // our factors of 2 might not quite be a power of 8 -- our plan_power12_power6 function tried its best, but if there are very few factors of 3, it can't help.
                // if we're 2 * 8^N, benchmarking shows that applying a 16 before our chain of 8s is very fast.
                plan.push_radix(16);
                radix_factors = radix_factors
                    .divide_by(&PartialFactors::compute(16))
                    .unwrap();
            }
            plan.push_radix_power(12, power_twelve);
            plan.push_radix_power(11, radix_factors.get_power11());
            plan.push_radix_power(9, radix_factors.get_power3() / 2);
            plan.push_radix_power(8, radix_factors.get_power2() / 3);
            plan.push_radix_power(7, radix_factors.get_power7());
            plan.push_radix_power(6, power_six);
            plan.push_radix_power(5, radix_factors.get_power5());
            if radix_factors.get_power2() % 3 == 2 {
                // our factors of 2 might not quite be a power of 8 -- our plan_power12_power6 function tried its best, but if we are a power of 2, it can't help.
                // if we're 4 * 8^N, benchmarking shows that applying a 4 to the end our chain of 8s is very fast.
                plan.push_radix(4);
            }
            if radix_factors.get_power3() % 2 == 1 {
                // our factors of 3 might not quite be a power of 9 -- our plan_power12_power6 function tried its best, but if we are a power of 3, it can't help.
                // if we're 3 * 9^N, our only choice is to add an 8xn step
                plan.push_radix(3);
            }
            if radix_factors.get_power2() % 3 == 1 {
                // our factors of 2 might not quite be a power of 8. We tried to correct this with a 16 radix and 4 radix, but as a last resort, apply a 2. 2 is very slow, but it's better than not computing the FFT
                plan.push_radix(2);
            }

            // measurement opportunity: is it faster to let the plan_power12_power6 function put a 4 on the end instead of relying on all 8's?
            // measurement opportunity: is it faster to slap a 16 on top of the stack?
            // measurement opportunity: if our plan_power12_power6 function adds both 12s and sixes, is it faster to drop combinations of 12+6 down to 8+9?
        };
        plan
    }

    // Constructs and returns a FFT instance from a FFT plan.
    // If the base is a butterfly, it will call the provided `construct_butterfly_fn` to do so.
    // If constructing the base requires constructing an inner FFT (IE bluetein's or rader's algorithm), it will call the provided `inner_fft_fn` to construct it
    fn construct_plan(
        &mut self,
        plan: MixedRadixPlan,
        direction: FftDirection,
        construct_butterfly_fn: impl FnOnce(&Self, usize, FftDirection) -> Arc<dyn Fft<T>>,
        inner_fft_fn: impl FnOnce(&mut Self, usize, FftDirection) -> Arc<dyn Fft<T>>,
    ) -> Arc<dyn Fft<T>> {
        let mut fft = match plan.base {
            MixedRadixBase::CacheBase(len) => self.cache.get(len, direction).unwrap(),
            MixedRadixBase::ButterflyBase(len) => {
                let butterfly_instance = construct_butterfly_fn(self, len, direction);

                // Cache this FFT instance for future calls to `plan_fft`
                self.cache.insert(&butterfly_instance);

                butterfly_instance
            }
            MixedRadixBase::RadersBase(len) => {
                // Rader's Algorithm requires an inner FFT of size len - 1
                let inner_fft = inner_fft_fn(self, len - 1, direction);

                // try to construct our AVX2 rader's algorithm. If that fails (probably because the machine we're running on doesn't have AVX2), fall back to scalar
                let raders_instance =
                    if let Ok(raders_avx) = RadersAvx2::<A, T>::new(Arc::clone(&inner_fft)) {
                        wrap_fft(raders_avx)
                    } else {
                        wrap_fft(RadersAlgorithm::new(inner_fft))
                    };

                // Cache this FFT instance for future calls to `plan_fft`
                self.cache.insert(&raders_instance);

                raders_instance
            }
            MixedRadixBase::BluesteinsBase(len, inner_fft_len) => {
                // Bluestein's has an inner FFT of arbitrary size. But we've already planned it, so just use what we planned
                let inner_fft = inner_fft_fn(self, inner_fft_len, direction);

                // try to construct our AVX2 rader's algorithm. If that fails (probably because the machine we're running on doesn't have AVX2), fall back to scalar
                let bluesteins_instance =
                    wrap_fft(BluesteinsAvx::<A, T>::new(len, inner_fft).unwrap());

                // Cache this FFT instance for future calls to `plan_fft`
                self.cache.insert(&bluesteins_instance);

                bluesteins_instance
            }
        };

        // We have constructed our base. Now, construct the radix chain.
        for radix in plan.radixes {
            fft = match radix {
                2 => wrap_fft(MixedRadix2xnAvx::<A, T>::new(fft).unwrap()),
                3 => wrap_fft(MixedRadix3xnAvx::<A, T>::new(fft).unwrap()),
                4 => wrap_fft(MixedRadix4xnAvx::<A, T>::new(fft).unwrap()),
                5 => wrap_fft(MixedRadix5xnAvx::<A, T>::new(fft).unwrap()),
                6 => wrap_fft(MixedRadix6xnAvx::<A, T>::new(fft).unwrap()),
                7 => wrap_fft(MixedRadix7xnAvx::<A, T>::new(fft).unwrap()),
                8 => wrap_fft(MixedRadix8xnAvx::<A, T>::new(fft).unwrap()),
                9 => wrap_fft(MixedRadix9xnAvx::<A, T>::new(fft).unwrap()),
                11 => wrap_fft(MixedRadix11xnAvx::<A, T>::new(fft).unwrap()),
                12 => wrap_fft(MixedRadix12xnAvx::<A, T>::new(fft).unwrap()),
                16 => wrap_fft(MixedRadix16xnAvx::<A, T>::new(fft).unwrap()),
                _ => unreachable!(),
            };

            // Cache this FFT instance for future calls to `plan_fft`
            self.cache.insert(&fft);
        }

        fft
    }

    // Plan and return the inner size to be used with Bluestein's Algorithm
    // Calls `filter_fn` on result candidates, giving the caller the opportunity to reject certain sizes
    fn plan_bluesteins(
        &self,
        len: usize,
        filter_fn: impl FnMut(&&(usize, u32, u32)) -> bool,
    ) -> usize {
        assert!(len > 1); // Internal consistency check: The logic in this method doesn't work for a length of 1

        // Bluestein's computes a FFT of size `len` by reorganizing it as a FFT of ANY size greater than or equal to len * 2 - 1
        // an obvious choice is the next power of two larger than  len * 2 - 1, but if we can find a smaller FFT that will go faster, we can save a lot of time.
        // We can very efficiently compute almost any 2^n * 3^m, so we're going to search for all numbers of the form 2^n * 3^m that lie between len * 2 - 1 and the next power of two.
        let min_len = len * 2 - 1;
        let baseline_candidate = min_len.checked_next_power_of_two().unwrap();

        // our algorithm here is to start with our next power of 2, and repeatedly divide by 2 and multiply by 3, trying to keep our value in range
        let mut bluesteins_candidates = Vec::new();
        let mut candidate = baseline_candidate;
        let mut factor2 = candidate.trailing_zeros();
        let mut factor3 = 0;

        let min_factor2 = 2; // benchmarking shows that while 3^n and  2 * 3^n are fast, they're typically slower than the next-higher candidate, so don't bother generating them
        while factor2 >= min_factor2 {
            // if this candidate length isn't too small, add it to our candidates list
            if candidate >= min_len {
                bluesteins_candidates.push((candidate, factor2, factor3));
            }
            // if the candidate is too large, divide it by 2. if it's too small, divide it by 3
            if candidate >= baseline_candidate {
                candidate >>= 1;
                factor2 -= 1;
            } else {
                candidate *= 3;
                factor3 += 1;
            }
        }
        bluesteins_candidates.sort();

        // we now have a list of candidates to choosse from. some 2^n * 3^m FFTs are faster than others, so apply a filter, which will let us skip sizes that benchmarking has shown to be slow
        let (chosen_size, _, _) =
            bluesteins_candidates
                .iter()
                .find(filter_fn)
                .unwrap_or_else(|| {
                    panic!(
                        "Failed to find a bluestein's candidate for len={}, candidates: {:?}",
                        len, bluesteins_candidates
                    )
                });

        *chosen_size
    }
}

#[cfg(test)]
mod unit_tests {
    use super::*;

    // We don't need to actually compute anything for a FFT size of zero, but we do need to verify that it doesn't explode
    #[test]
    fn test_plan_zero_avx() {
        let mut planner32 = FftPlannerAvx::<f32>::new().unwrap();
        let fft_zero32 = planner32.plan_fft_forward(0);
        fft_zero32.process(&mut []);

        let mut planner64 = FftPlannerAvx::<f64>::new().unwrap();
        let fft_zero64 = planner64.plan_fft_forward(0);
        fft_zero64.process(&mut []);
    }
}
