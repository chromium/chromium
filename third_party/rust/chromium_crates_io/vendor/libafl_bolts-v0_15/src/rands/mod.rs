//! The random number generators of `LibAFL`

#[cfg(all(not(feature = "std"), target_has_atomic = "ptr"))]
use core::sync::atomic::{AtomicUsize, Ordering};
use core::{
    debug_assert,
    fmt::Debug,
    num::{NonZero, NonZeroUsize},
};

#[cfg(feature = "rand_trait")]
use rand_core::{RngCore, SeedableRng};
use serde::{Deserialize, Serialize};

#[cfg(feature = "alloc")]
pub mod loaded_dice;

#[cfg(all(not(feature = "std"), target_has_atomic = "ptr"))]
static SEED_COUNTER: AtomicUsize = AtomicUsize::new(0);

/// Return a pseudo-random seed. For `no_std` environments, a single deterministic sequence is used.
#[must_use]
#[allow(unreachable_code)] // cfg dependent
pub fn random_seed() -> u64 {
    #[cfg(feature = "std")]
    return random_seed_from_random_state();
    #[cfg(all(not(feature = "std"), target_has_atomic = "ptr"))]
    return random_seed_deterministic();
    // no_std and no atomics; https://xkcd.com/221/
    4
}

#[cfg(all(not(feature = "std"), target_has_atomic = "ptr"))]
fn random_seed_deterministic() -> u64 {
    let mut seed = SEED_COUNTER.fetch_add(1, Ordering::Relaxed) as u64;
    splitmix64(&mut seed)
}

#[cfg(feature = "std")]
fn random_seed_from_random_state() -> u64 {
    use core::hash::{BuildHasher, Hasher};
    use std::collections::hash_map::RandomState;
    RandomState::new().build_hasher().finish()
}

// https://prng.di.unimi.it/splitmix64.c
fn splitmix64(x: &mut u64) -> u64 {
    *x = x.wrapping_add(0x9e3779b97f4a7c15);
    let mut z = *x;
    z = (z ^ (z >> 30)).wrapping_mul(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)).wrapping_mul(0x94d049bb133111eb);
    z ^ (z >> 31)
}

/// The standard [`Rand`] implementation for `LibAFL`.
///
/// It is usually the right choice, with very good speed and a reasonable randomness.
/// Not cryptographically secure (which is not what you want during fuzzing ;) )
pub type StdRand = RomuDuoJrRand;

/// Choose an item at random from the given iterator, sampling uniformly.
///
/// Will only return `None` for an empty iterator.
///
/// Note: the runtime cost is bound by the iterator's [`nth`][`Iterator::nth`] implementation
///  * For `Vec`, slice, array, this is O(1)
///  * For `HashMap`, `HashSet`, this is O(n)
pub fn choose<I>(from: I, rand: u64) -> Option<I::Item>
where
    I: IntoIterator,
    I::IntoIter: ExactSizeIterator,
{
    // create iterator
    let mut iter = from.into_iter();

    let len = NonZero::new(iter.len())?;

    // pick a random, valid index
    let index = fast_bound(rand, len);

    // return the item chosen
    Some(iter.nth(index).unwrap())
}

/// Faster and almost unbiased alternative to `rand % n`.
///
/// For N-bit bound, probability of getting a biased value is 1/2^(64-N).
/// At least 2^2*(64-N) samples are required to detect this amount of bias.
///
/// See: [An optimal algorithm for bounded random integers](https://github.com/apple/swift/pull/39143).
#[inline]
#[must_use]
pub fn fast_bound(rand: u64, n: NonZeroUsize) -> usize {
    let mul = u128::from(rand).wrapping_mul(u128::from(n.get() as u64));
    (mul >> 64) as usize
}

#[inline]
#[must_use]
fn fast_bound_usize(rand: u64, n: usize) -> usize {
    let mul = u128::from(rand).wrapping_mul(u128::from(n as u64));
    (mul >> 64) as usize
}

/// Ways to get random around here.
/// Please note that these are not cryptographically secure.
/// Or, even if some might be by accident, at least they are not seeded in a cryptographically secure fashion.
pub trait Rand {
    /// Sets the seed of this Rand
    fn set_seed(&mut self, seed: u64);

    /// Gets the next 64 bit value
    fn next(&mut self) -> u64;

    /// Gets a value between 0.0 (inclusive) and 1.0 (exclusive)
    #[inline]
    #[expect(clippy::cast_precision_loss)]
    fn next_float(&mut self) -> f64 {
        // both 2^53 and 2^-53 can be represented in f64 exactly
        const MAX: u64 = 1u64 << 53;
        const MAX_DIV: f64 = 1.0 / (MAX as f64);
        let u = self.next() & MAX.wrapping_sub(1);
        u as f64 * MAX_DIV
    }

    /// Returns true with specified probability
    #[inline]
    fn coinflip(&mut self, success_prob: f64) -> bool {
        debug_assert!((0.0..=1.0).contains(&success_prob));
        self.next_float() < success_prob
    }

    /// Gets a value below the given bound (exclusive)
    #[inline]
    fn below(&mut self, upper_bound_excl: NonZeroUsize) -> usize {
        fast_bound(self.next(), upper_bound_excl)
    }

    /// Gets a value below the given one or zero
    fn below_or_zero(&mut self, n: usize) -> usize {
        fast_bound_usize(self.next(), n)
    }

    /// Gets a value between the given lower bound (inclusive) and upper bound (inclusive)
    #[inline]
    fn between(&mut self, lower_bound_incl: usize, upper_bound_incl: usize) -> usize {
        debug_assert!(lower_bound_incl <= upper_bound_incl);
        // # Safety
        // We check that the upper_bound_incl <= lower_bound_incl above (alas only in debug), so the below is fine.
        // Even if we encounter a 0 in release here, the worst-case scenario should be an invalid return value.
        lower_bound_incl
            + self.below(unsafe {
                NonZero::new(upper_bound_incl - lower_bound_incl + 1).unwrap_unchecked()
            })
    }

    /// Convenient variant of [`choose`].
    ///
    /// This method uses [`Iterator::size_hint`] for optimization. With an
    /// accurate hint and where [`Iterator::nth`] is a constant-time operation
    /// this method can offer `O(1)` performance. Where no size hint is
    /// available, complexity is `O(n)` where `n` is the iterator length.
    /// Partial hints (where `lower > 0`) also improve performance.
    ///
    /// Copy&paste from [`rand::IteratorRandom`](https://docs.rs/rand/0.8.5/rand/seq/trait.IteratorRandom.html#method.choose)
    fn choose<I>(&mut self, from: I) -> Option<I::Item>
    where
        I: IntoIterator,
    {
        let mut iter = from.into_iter();
        let (mut lower, mut upper) = iter.size_hint();
        let mut consumed = 0;
        let mut result = None;

        // Handling for this condition outside the loop allows the optimizer to eliminate the loop
        // when the Iterator is an ExactSizeIterator. This has a large performance impact on e.g.
        // seq_iter_choose_from_1000.
        if upper == Some(lower) {
            return if let Some(lower) = NonZero::new(lower) {
                iter.nth(self.below(lower))
            } else {
                None
            };
        }

        // Continue until the iterator is exhausted
        loop {
            if lower > 1 {
                // # Safety
                // lower is > 1, we don't consume more than usize elements, so this should always be non-0.
                let ix = self.below(unsafe { NonZero::new(lower + consumed).unwrap_unchecked() });
                let skip = if ix < lower {
                    result = iter.nth(ix);
                    lower - (ix + 1)
                } else {
                    lower
                };
                if upper == Some(lower) {
                    return result;
                }
                consumed += lower;
                if skip > 0 {
                    iter.nth(skip - 1);
                }
            } else {
                let elem = iter.next();
                if elem.is_none() {
                    return result;
                }
                consumed += 1;
                // # SAFETY
                // `consumed` can never be 0 here. We just increased it by 1 above.
                if self.below(unsafe { NonZero::new(consumed).unwrap_unchecked() }) == 0 {
                    result = elem;
                }
            }

            let hint = iter.size_hint();
            lower = hint.0;
            upper = hint.1;
        }
    }
}

#[cfg(feature = "rand_trait")]
impl<T> Rand for T
where
    T: RngCore + SeedableRng + Serialize + for<'de> Deserialize<'de> + Debug,
{
    fn set_seed(&mut self, seed: u64) {
        *self = Self::seed_from_u64(seed);
    }

    fn next(&mut self) -> u64 {
        self.next_u64()
    }
}

macro_rules! impl_default_new {
    ($rand:ty) => {
        impl Default for $rand {
            /// Creates a generator seeded with [`random_seed`].
            fn default() -> Self {
                Self::with_seed(random_seed())
            }
        }

        impl $rand {
            /// Creates a generator seeded with [`random_seed`].
            #[must_use]
            pub fn new() -> Self {
                Self::with_seed(random_seed())
            }
        }
    };
}

impl_default_new!(Xoshiro256PlusPlusRand);
impl_default_new!(XorShift64Rand);
impl_default_new!(Lehmer64Rand);
impl_default_new!(RomuTrioRand);
impl_default_new!(RomuDuoJrRand);
impl_default_new!(Sfc64Rand);

macro_rules! impl_rng_core {
    ($rand:ty) => {
        #[cfg(feature = "rand_trait")]
        impl rand_core::RngCore for $rand {
            fn next_u32(&mut self) -> u32 {
                self.next() as u32
            }

            fn next_u64(&mut self) -> u64 {
                self.next()
            }

            fn fill_bytes(&mut self, dest: &mut [u8]) {
                rand_core::impls::fill_bytes_via_next(self, dest)
            }
        }
    };
}

impl_rng_core!(Xoshiro256PlusPlusRand);
impl_rng_core!(XorShift64Rand);
impl_rng_core!(Lehmer64Rand);
impl_rng_core!(RomuTrioRand);
impl_rng_core!(RomuDuoJrRand);
impl_rng_core!(Sfc64Rand);

/// xoshiro256++ PRNG: <https://prng.di.unimi.it/>
#[derive(Debug, Copy, Clone, Serialize, Deserialize)]
pub struct Xoshiro256PlusPlusRand {
    s: [u64; 4],
}

impl Rand for Xoshiro256PlusPlusRand {
    fn set_seed(&mut self, mut seed: u64) {
        self.s[0] = splitmix64(&mut seed);
        self.s[1] = splitmix64(&mut seed);
        self.s[2] = splitmix64(&mut seed);
        self.s[3] = splitmix64(&mut seed);
    }

    #[inline]
    fn next(&mut self) -> u64 {
        let ret: u64 = self.s[0]
            .wrapping_add(self.s[3])
            .rotate_left(23)
            .wrapping_add(self.s[0]);
        let t: u64 = self.s[1] << 17;

        self.s[2] ^= self.s[0];
        self.s[3] ^= self.s[1];
        self.s[1] ^= self.s[2];
        self.s[0] ^= self.s[3];

        self.s[2] ^= t;

        self.s[3] = self.s[3].rotate_left(45);

        ret
    }
}

impl Xoshiro256PlusPlusRand {
    /// Creates a new xoshiro256++ rand with the given seed
    #[must_use]
    pub fn with_seed(seed: u64) -> Self {
        let mut rand = Self { s: [0; 4] };
        rand.set_seed(seed);
        rand
    }
}

/// Xorshift64 PRNG
#[derive(Debug, Copy, Clone, Serialize, Deserialize)]
pub struct XorShift64Rand {
    s: u64,
}

impl Rand for XorShift64Rand {
    fn set_seed(&mut self, mut seed: u64) {
        self.s = splitmix64(&mut seed) | 1;
    }

    #[inline]
    fn next(&mut self) -> u64 {
        let mut x = self.s;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        self.s = x;
        x
    }
}

impl XorShift64Rand {
    /// Creates a new xorshift64 rand with the given seed
    #[must_use]
    pub fn with_seed(seed: u64) -> Self {
        let mut ret: Self = Self { s: 0 };
        ret.set_seed(seed);
        ret
    }
}

/// Lehmer64 PRNG
#[derive(Debug, Copy, Clone, Serialize, Deserialize)]
pub struct Lehmer64Rand {
    s: u128,
}

impl Rand for Lehmer64Rand {
    fn set_seed(&mut self, mut seed: u64) {
        let hi = splitmix64(&mut seed);
        let lo = splitmix64(&mut seed) | 1;
        self.s = (u128::from(hi) << 64) | u128::from(lo);
    }

    #[inline]
    #[expect(clippy::unreadable_literal)]
    fn next(&mut self) -> u64 {
        self.s = self.s.wrapping_mul(0xda942042e4dd58b5);
        (self.s >> 64) as u64
    }
}

impl Lehmer64Rand {
    /// Creates a new Lehmer rand with the given seed
    #[must_use]
    pub fn with_seed(seed: u64) -> Self {
        let mut ret: Self = Self { s: 0 };
        ret.set_seed(seed);
        ret
    }
}

/// Extremely quick rand implementation
/// see <https://arxiv.org/pdf/2002.11331.pdf>
#[derive(Debug, Copy, Clone, Serialize, Deserialize)]
pub struct RomuTrioRand {
    x_state: u64,
    y_state: u64,
    z_state: u64,
}

impl RomuTrioRand {
    /// Creates a new `RomuTrioRand` with the given seed.
    #[must_use]
    pub fn with_seed(seed: u64) -> Self {
        let mut rand = Self {
            x_state: 0,
            y_state: 0,
            z_state: 0,
        };
        rand.set_seed(seed);
        rand
    }
}

impl Rand for RomuTrioRand {
    fn set_seed(&mut self, mut seed: u64) {
        self.x_state = splitmix64(&mut seed);
        self.y_state = splitmix64(&mut seed);
        self.z_state = splitmix64(&mut seed);
    }

    #[inline]
    #[expect(clippy::unreadable_literal)]
    fn next(&mut self) -> u64 {
        let xp = self.x_state;
        let yp = self.y_state;
        let zp = self.z_state;
        self.x_state = 15241094284759029579_u64.wrapping_mul(zp);
        self.y_state = yp.wrapping_sub(xp).rotate_left(12);
        self.z_state = zp.wrapping_sub(yp).rotate_left(44);
        xp
    }
}

/// see <https://arxiv.org/pdf/2002.11331.pdf>
#[derive(Debug, Copy, Clone, Serialize, Deserialize)]
pub struct RomuDuoJrRand {
    x_state: u64,
    y_state: u64,
}

impl RomuDuoJrRand {
    /// Creates a new `RomuDuoJrRand` with the given seed.
    #[must_use]
    pub fn with_seed(seed: u64) -> Self {
        let mut rand = Self {
            x_state: 0,
            y_state: 0,
        };
        rand.set_seed(seed);
        rand
    }
}

impl Rand for RomuDuoJrRand {
    fn set_seed(&mut self, mut seed: u64) {
        self.x_state = splitmix64(&mut seed);
        self.y_state = splitmix64(&mut seed);
    }

    #[inline]
    #[expect(clippy::unreadable_literal)]
    fn next(&mut self) -> u64 {
        let xp = self.x_state;
        self.x_state = 15241094284759029579_u64.wrapping_mul(self.y_state);
        self.y_state = self.y_state.wrapping_sub(xp).rotate_left(27);
        xp
    }
}

/// [SFC64][1] algorithm by Chris Doty-Humphrey.
///
/// [1]: https://numpy.org/doc/stable/reference/random/bit_generators/sfc64.html
#[derive(Debug, Copy, Clone, Serialize, Deserialize)]
pub struct Sfc64Rand {
    a: u64,
    b: u64,
    c: u64,
    w: u64,
}

impl Sfc64Rand {
    /// Creates a new [`Sfc64Rand`] with the given seed.
    #[must_use]
    pub fn with_seed(seed: u64) -> Self {
        let mut s = Sfc64Rand {
            a: 0,
            b: 0,
            c: 0,
            w: 0,
        };
        s.set_seed(seed);
        s
    }
}

impl Rand for Sfc64Rand {
    fn set_seed(&mut self, seed: u64) {
        self.a = seed;
        self.b = seed;
        self.c = seed;
        self.w = 1;
        for _ in 0..12 {
            self.next();
        }
    }

    #[inline]
    fn next(&mut self) -> u64 {
        let out = self.a.wrapping_add(self.b).wrapping_add(self.w);
        self.w = self.w.wrapping_add(1);
        self.a = self.b ^ (self.b >> 11);
        self.b = self.c.wrapping_add(self.c << 3);
        self.c = self.c.rotate_left(24).wrapping_add(out);
        out
    }
}

/// fake rand, for testing purposes
#[derive(Debug, Copy, Clone, Default, Serialize, Deserialize)]
pub struct XkcdRand {
    val: u64,
}

impl Rand for XkcdRand {
    fn set_seed(&mut self, mut seed: u64) {
        self.val = splitmix64(&mut seed);
    }

    fn next(&mut self) -> u64 {
        self.val
    }
}

/// A test rng that will return the same value (chose by fair dice roll) for testing.
impl XkcdRand {
    /// Creates a new [`XkcdRand`] with the rand of 4, [chosen by fair dice roll, guaranteed to be random](https://xkcd.com/221/).
    #[must_use]
    pub fn new() -> Self {
        Self { val: 4 }
    }

    /// Creates a new [`XkcdRand`] with the given seed.
    #[must_use]
    pub fn with_seed(seed: u64) -> Self {
        let mut rand = XkcdRand { val: 0 };
        rand.set_seed(seed);
        rand
    }
}

#[cfg(feature = "python")]
/// `Rand` Python bindings
pub mod pybind {
    use pyo3::prelude::*;
    use serde::{Deserialize, Serialize};

    use super::{Rand, StdRand, random_seed};

    #[pyclass(unsendable, name = "StdRand")]
    #[expect(clippy::unsafe_derive_deserialize)]
    #[derive(Serialize, Deserialize, Debug, Clone)]
    /// Python class for `StdRand`
    pub struct PythonStdRand {
        /// Rust wrapped `StdRand` object
        pub inner: StdRand,
    }

    #[pymethods]
    impl PythonStdRand {
        #[staticmethod]
        fn with_random_seed() -> Self {
            Self {
                inner: StdRand::with_seed(random_seed()),
            }
        }

        #[staticmethod]
        fn with_seed(seed: u64) -> Self {
            Self {
                inner: StdRand::with_seed(seed),
            }
        }

        fn as_rand(slf: Py<Self>) -> PythonRand {
            PythonRand::new_std(slf)
        }
    }

    #[derive(Serialize, Deserialize, Debug)]
    enum PythonRandWrapper {
        Std(Py<PythonStdRand>),
    }

    /// Rand Trait binding
    #[pyclass(unsendable, name = "Rand")]
    #[expect(clippy::unsafe_derive_deserialize)]
    #[derive(Serialize, Deserialize, Debug)]
    pub struct PythonRand {
        wrapper: PythonRandWrapper,
    }

    macro_rules! unwrap_me_mut {
        ($wrapper:expr, $name:ident, $body:block) => {
            crate::unwrap_me_mut_body!($wrapper, $name, $body, PythonRandWrapper, { Std })
        };
    }

    #[pymethods]
    impl PythonRand {
        #[staticmethod]
        fn new_std(py_std_rand: Py<PythonStdRand>) -> Self {
            Self {
                wrapper: PythonRandWrapper::Std(py_std_rand),
            }
        }
    }

    impl Rand for PythonRand {
        fn set_seed(&mut self, seed: u64) {
            unwrap_me_mut!(self.wrapper, r, { r.set_seed(seed) });
        }

        #[inline]
        fn next(&mut self) -> u64 {
            unwrap_me_mut!(self.wrapper, r, { r.next() })
        }
    }

    /// Register the classes to the python module
    pub fn register(m: &Bound<'_, PyModule>) -> PyResult<()> {
        m.add_class::<PythonStdRand>()?;
        m.add_class::<PythonRand>()?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use crate::{
        nonzero,
        rands::{
            Rand, RomuDuoJrRand, RomuTrioRand, Sfc64Rand, StdRand, XorShift64Rand,
            Xoshiro256PlusPlusRand,
        },
    };

    fn test_single_rand<R: Rand>(rand: &mut R) {
        assert_ne!(rand.next(), rand.next());
        assert!(rand.below(nonzero!(100)) < 100);
        assert_eq!(rand.below(nonzero!(1)), 0);
        assert_eq!(rand.between(10, 10), 10);
        assert!(rand.between(11, 20) > 10);
    }

    #[test]
    fn test_rands() {
        // see cargo bench for speed comparisons
        test_single_rand(&mut StdRand::with_seed(0));
        test_single_rand(&mut RomuTrioRand::with_seed(0));
        test_single_rand(&mut RomuDuoJrRand::with_seed(0));
        test_single_rand(&mut XorShift64Rand::with_seed(0));
        test_single_rand(&mut Xoshiro256PlusPlusRand::with_seed(0));
        test_single_rand(&mut Sfc64Rand::with_seed(0));
    }

    #[test]
    fn test_romutrio_golden() {
        // https://github.com/ziglang/zig/blob/130fb5cb0fb9039e79450c9db58d6590c5bee3b3/lib/std/Random/RomuTrio.zig#L75-L95
        let golden: [u64; 10] = [
            16294208416658607535,
            13964609475759908645,
            4703697494102998476,
            3425221541186733346,
            2285772463536419399,
            9454187757529463048,
            13695907680080547496,
            8328236714879408626,
            12323357569716880909,
            12375466223337721820,
        ];

        let mut s = RomuTrioRand::with_seed(0);
        for v in golden {
            let u = s.next();
            assert_eq!(v, u);
        }
    }

    #[test]
    fn test_romuduojr_golden() {
        // https://github.com/eqv/rand_romu/blob/c0379dc3c21ffac8440197e2f8fe95c226c44bfe/src/lib.rs#L65-L79
        let golden: [u64; 9] = [
            0x3c91b13ee3913664,
            0xdc1980b78df3115,
            0x1c163b704996d2ad,
            0xa000c594bb28313b,
            0xfb6c42e69a523526,
            0x1fcebd6988ab21d8,
            0x5e0a8abf025f8f02,
            0x29554b00ffab0263,
            0xff5b6bb1551cf66,
        ];

        let mut s = RomuDuoJrRand {
            x_state: 0x3c91b13ee3913664u64,
            y_state: 0x863f0e37c2637d1fu64,
        };
        for v in golden {
            let u = s.next();
            assert_eq!(v, u);
        }
    }

    #[test]
    fn test_xoshiro256pp_golden() {
        // https://github.com/ziglang/zig/blob/130fb5cb0fb9039e79450c9db58d6590c5bee3b3/lib/std/Random/Xoshiro256.zig#L96-L103
        let golden: [u64; 6] = [
            0x53175d61490b23df,
            0x61da6f3dc380d507,
            0x5c0fdf91ec9a7bfc,
            0x02eebf8c3bbe5e1a,
            0x7eca04ebaf4a5eea,
            0x0543c37757f08d9a,
        ];

        let mut s = Xoshiro256PlusPlusRand::with_seed(0);
        for v in golden {
            let u = s.next();
            assert_eq!(v, u);
        }
    }

    #[test]
    fn test_sfc64_golden() {
        // https://github.com/ziglang/zig/blob/130fb5cb0fb9039e79450c9db58d6590c5bee3b3/lib/std/Random/Sfc64.zig#L73-L99
        let golden: [u64; 16] = [
            0x3acfa029e3cc6041,
            0xf5b6515bf2ee419c,
            0x1259635894a29b61,
            0xb6ae75395f8ebd6,
            0x225622285ce302e2,
            0x520d28611395cb21,
            0xdb909c818901599d,
            0x8ffd195365216f57,
            0xe8c4ad5e258ac04a,
            0x8f8ef2c89fdb63ca,
            0xf9865b01d98d8e2f,
            0x46555871a65d08ba,
            0x66868677c6298fcd,
            0x2ce15a7e6329f57d,
            0xb2f1833ca91ca79,
            0x4b0890ac9bf453ca,
        ];

        let mut s = Sfc64Rand::with_seed(0);
        for v in golden {
            let u = s.next();
            assert_eq!(v, u);
        }
    }

    #[test]
    #[cfg(feature = "rand_trait")]
    fn test_rand_trait() {
        use rand_core::{RngCore, SeedableRng};
        use serde::{Deserialize, Serialize};

        #[derive(Debug, Serialize, Deserialize)]
        struct CountingRng(u64);

        impl RngCore for CountingRng {
            fn next_u32(&mut self) -> u32 {
                self.next_u64() as u32
            }

            fn next_u64(&mut self) -> u64 {
                self.0 += 1;
                self.0
            }

            fn fill_bytes(&mut self, dst: &mut [u8]) {
                rand_core::impls::fill_bytes_via_next(self, dst);
            }
        }

        impl SeedableRng for CountingRng {
            type Seed = [u8; 8];

            fn from_seed(seed: Self::Seed) -> Self {
                Self(u64::from_le_bytes(seed))
            }
        }

        // LibAFL's Rand trait is auto-implemented for all SeedableRng + RngCore types.
        assert!(CountingRng(0).coinflip(0.1));
    }
}
