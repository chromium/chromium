#![allow(rustdoc::bare_urls)]
#![warn(unreachable_pub)]
#![doc = include_str!("../README.md")]
#![cfg_attr(not(feature = "std"), no_std)]

extern crate alloc;
use alloc::vec::Vec;
use core::hash::{BuildHasher, Hash, Hasher};
use core::iter::repeat;
mod hasher;
pub use hasher::DefaultHasher;
use hasher::DoubleHasher;
mod builder;
pub use builder::{
    AtomicBuilderWithBits, AtomicBuilderWithFalsePositiveRate, BuilderWithBits,
    BuilderWithFalsePositiveRate,
};
mod bit_vector;
use bit_vector::{AtomicBitVec, BitVec};
mod math;

#[cfg(feature = "loom")]
pub(crate) use loom::sync::atomic::AtomicU64;

#[cfg(not(feature = "loom"))]
pub(crate) use core::sync::atomic::AtomicU64;

#[cfg(all(feature = "loom", feature = "serde"))]
compile_error!("features `loom` and `serde` are mutually exclusive");

macro_rules! impl_bloom {
    ($name:ident, $builder_bits:ident, $builder_fp:ident, $bitvec:ident, $bits:ty, $ismut:literal) => {
        /// A space efficient approximate membership set data structure.
        /// False positives from [`contains`](Self::contains) are possible, but false negatives
        /// are not, i.e. [`contains`](Self::contains) for all items in the set is guaranteed to return
        /// true, while [`contains`](Self::contains) for all items not in the set probably return false.
        ///
        /// [`Self`] is supported by an underlying bit vector to track item membership.
        /// To insert, a number of bits are set at positions based on the item's hash in the underlying bit vector.
        /// To check membership, a number of bits are checked at positions based on the item's hash in the underlying bit vector.
        ///
        /// Once constructed, neither the Bloom filter's underlying memory usage nor number of bits per item change.
        ///
        /// # Examples
        /// Basic usage:
        /// ```rust
        #[doc = concat!("use fastbloom::", stringify!($name), ";")]
        ///
        #[doc = concat!("let ", $ismut, "filter = ", stringify!($name), "::with_num_bits(1024).expected_items(2);")]
        /// filter.insert("42");
        /// filter.insert("🦀");
        /// ```
        /// Instantiate with a target false positive rate:
        /// ```rust
        #[doc = concat!("use fastbloom::", stringify!($name), ";")]
        ///
        #[doc = concat!("let filter = ", stringify!($name), "::with_false_pos(0.001).items([\"42\", \"🦀\"]);")]
        /// assert!(filter.contains("42"));
        /// assert!(filter.contains("🦀"));
        /// ```
        /// Use any hasher:
        /// ```rust
        #[doc = concat!("use fastbloom::", stringify!($name), ";")]
        /// use ahash::RandomState;
        ///
        #[doc = concat!("let filter = ", stringify!($name), "::with_num_bits(1024)")]
        ///     .hasher(RandomState::default())
        ///     .items(["42", "🦀"]);
        /// ```
        #[derive(Debug, Clone)]
        #[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
        pub struct $name<S = DefaultHasher> {
            bits: $bitvec,
            num_hashes: u32,
            hasher: S,
        }

        impl $name {
            fn new_builder(num_bits: usize) -> $builder_bits {
                assert!(num_bits > 0);
                // Only available in rust 1.73+
                // let num_u64s = num_bits.div_ceil(64);
                let num_u64s = (num_bits + 64 - 1) / 64;
                $builder_bits {
                    data: repeat(0).take(num_u64s).collect(),
                    hasher: Default::default(),
                }
            }

            fn new_from_vec(vec: Vec<u64>) -> $builder_bits {
                assert!(!vec.is_empty());
                $builder_bits {
                    data: vec,
                    hasher: Default::default(),
                }
            }

            fn new_with_false_pos(fp: f64) -> $builder_fp {
                assert!(fp > 0.0);
                $builder_fp {
                    desired_fp_rate: fp,
                    hasher: Default::default(),
                }
            }

            /// Creates a new builder instance to construct a [`Self`] with a target false positive rate of `fp`.
            /// The memory size of the underlying bit vector is dependent on the false positive rate and the expected number of items.
            /// # Panics
            /// Panics if the false positive rate, `fp`, is 0.
            ///
            /// # Examples
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($name), ";")]
            #[doc = concat!("let filter = ", stringify!($name), "::with_false_pos(0.001).expected_items(1000);")]
            /// ```
            pub fn with_false_pos(fp: f64) -> $builder_fp {
                $name::new_with_false_pos(fp)
            }

            /// Creates a builder instance to construct a [`Self`] with `num_bits` number of bits for tracking item membership.
            /// # Panics
            /// Panics if the number of bits, `num_bits`, is 0.
            ///
            /// # Examples
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($name), ";")]
            #[doc = concat!("let filter = ", stringify!($name), "::with_num_bits(1024).hashes(4);")]
            /// ```
            pub fn with_num_bits(num_bits: usize) -> $builder_bits {
                $name::new_builder(num_bits)
            }

            /// Creates a builder instance to construct a [`Self`] initialized with bit vector `bit_vec`.
            ///
            /// # Panics
            /// Panics if the bit vector, `bit_vec`, is empty.
            /// # Examples
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($name), ";")]
            ///
            #[doc = concat!("let orig = ", stringify!($name), "::with_false_pos(0.001).seed(&42).items([1, 2]);")]
            /// let num_hashes = orig.num_hashes();
            #[doc = concat!("let new = ", stringify!($name), "::from_vec(orig.iter().collect()).seed(&42).hashes(num_hashes);")]
            ///
            /// assert!(new.contains(&1));
            /// assert!(new.contains(&2));
            /// ```
            pub fn from_vec(bit_vec: Vec<u64>) -> $builder_bits {
                $name::new_from_vec(bit_vec)
            }
        }

        impl<S: BuildHasher> $name<S> {
            /// Checks if an element is possibly in the Bloom filter.
            ///
            /// # Returns
            ///
            /// `true` if the item is possibly in the Bloom filter, `false` otherwise.
            ///
            /// # Examples
            ///
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($name), ";")]
            ///
            #[doc = concat!("let bloom = ", stringify!($name), "::with_num_bits(1024).items([1, 2, 3]);")]
            /// assert!(bloom.contains(&1));
            /// ```
            #[inline]
            pub fn contains(&self, val: &(impl Hash + ?Sized)) -> bool {
                self.contains_hash(self.source_hash(val))
            }

            /// Checks if the hash of an element is possibly in the Bloom filter.
            /// That is the element is pre-hashed and all subsequent hashes are derived from this "source" hash.
            ///
            /// # Returns
            ///
            /// `true` if the item is possibly in the Bloom filter, `false` otherwise.
            #[inline]
            pub fn contains_hash(&self, hash: u64) -> bool {
                let mut hasher = DoubleHasher::new(hash);
                (0..self.num_hashes).all(|_| {
                    let h = hasher.next();
                    self.bits.check(index(self.num_bits(), h))
                })

            }

            /// Returns the number of hashes per item.
            #[inline]
            pub fn num_hashes(&self) -> u32 {
                self.num_hashes
            }

            /// Returns the total number of in-memory bits supporting the Bloom filter.
            pub fn num_bits(&self) -> usize {
                self.bits.num_bits()
            }

            /// Returns an iterator over the raw bit values of this Bloom filter.
            #[inline]
            pub fn iter(&self) -> impl Iterator<Item = u64> + '_ {
                self.bits.iter()
            }

            /// Returns the underlying slice of this Bloom filter's bit contents.
            #[inline]
            pub fn as_slice(&self) -> &[$bits] {
                self.bits.as_slice()
            }

            /// Returns the hash of `val` using this Bloom filter's hasher.
            /// The resulting value can be used in [`Self::contains_hash`] or [`Self::insert_hash`].
            /// All subsequent hashes are derived from this source hash.
            /// This is useful for pre-computing hash values in order to store them or send them over the network.
            #[inline]
            pub fn source_hash(&self, val: &(impl Hash + ?Sized)) -> u64 {
                let mut state = self.hasher.build_hasher();
                val.hash(&mut state);
                state.finish()
            }
        }

        impl<T, S: BuildHasher> Extend<T> for $name<S>
        where
            T: Hash,
        {
            #[inline]
            fn extend<I: IntoIterator<Item = T>>(&mut self, iter: I) {
                for val in iter {
                    self.insert(&val);
                }
            }
        }

        impl<S: BuildHasher> PartialEq for $name<S> {
            fn eq(&self, other: &Self) -> bool {
                self.bits == other.bits && self.num_hashes == other.num_hashes
            }
        }
        impl<S: BuildHasher> Eq for $name<S> {}
    };
}

impl_bloom!(
    BloomFilter,
    BuilderWithBits,
    BuilderWithFalsePositiveRate,
    BitVec,
    u64,
    "mut "
);
impl_bloom!(
    AtomicBloomFilter,
    AtomicBuilderWithBits,
    AtomicBuilderWithFalsePositiveRate,
    AtomicBitVec,
    AtomicU64,
    ""
);

impl<S: BuildHasher> BloomFilter<S> {
    /// Inserts an element into the Bloom filter.
    ///
    /// # Returns
    ///
    /// `true` if the item may have been previously in the Bloom filter (indicating a potential false positive),
    /// `false` otherwise.
    ///
    /// # Examples
    /// ```
    /// use fastbloom::BloomFilter;
    ///
    /// let mut bloom = BloomFilter::with_num_bits(1024).hashes(4);
    /// bloom.insert(&2);
    /// assert!(bloom.contains(&2));
    /// ```
    #[inline]
    pub fn insert(&mut self, val: &(impl Hash + ?Sized)) -> bool {
        self.insert_hash(self.source_hash(val))
    }

    /// Inserts the hash of an element into the Bloom filter.
    /// That is the element is pre-hashed and all subsequent hashes are derived from this "source" hash.
    ///
    /// # Returns
    ///
    /// `true` if the item may have been previously in the Bloom filter (indicating a potential false positive),
    /// `false` otherwise.
    #[inline]
    pub fn insert_hash(&mut self, hash: u64) -> bool {
        let mut hasher = DoubleHasher::new(hash);
        let mut previously_contained = true;
        for _ in 0..self.num_hashes {
            let h = hasher.next();
            previously_contained &= self.bits.set(index(self.num_bits(), h));
        }
        previously_contained
    }

    /// Inserts all the items in `iter` into the `self`.
    #[inline]
    pub fn insert_all<T: Hash, I: IntoIterator<Item = T>>(&mut self, iter: I) {
        for val in iter {
            self.insert(&val);
        }
    }

    /// Clear all of the bits in the Bloom filter, removing all items.
    #[inline]
    pub fn clear(&mut self) {
        self.bits.clear();
    }

    /// Unions `other` into `self`. The hashers of both Bloomfilters must be identical (this is not enforced!).
    ///
    /// # Panics
    /// Panics if the other Bloomfilter has a different number of bits or hashes than `self`.
    ///
    /// # Example
    /// ```
    /// use fastbloom::BloomFilter;
    ///
    /// let mut bloom = BloomFilter::with_num_bits(4096).seed(&1).hashes(4);
    /// let mut other = BloomFilter::with_num_bits(4096).seed(&1).hashes(4);
    /// bloom.insert_all(0..=1000);
    /// other.insert_all(500..=1500);
    /// bloom.union(&other);
    ///
    /// for x in 0..=2000 {
    ///     assert_eq!(bloom.contains(&x), bloom.contains(&x) || other.contains(&x));
    /// }
    /// ```
    #[inline]
    pub fn union(&mut self, other: &BloomFilter<S>) {
        assert_eq!(
            self.num_hashes(),
            other.num_hashes(),
            "expected same number of hashes"
        );
        self.bits.union(&other.bits);
    }

    /// Intersects `other` onto `self`. The hashers of both Bloomfilters must be identical (this is not enforced!).
    ///
    /// # Panics
    /// Panics if the other Bloomfilter has a different number of bits or hashes than `self`.
    ///
    /// # Example
    /// ```
    /// use fastbloom::BloomFilter;
    ///
    /// let mut bloom = BloomFilter::with_num_bits(4096).seed(&1).hashes(4);
    /// let mut other = BloomFilter::with_num_bits(4096).seed(&1).hashes(4);
    /// bloom.insert_all(0..=1000);
    /// other.insert_all(500..=1500);
    /// bloom.intersect(&other);
    ///
    /// for x in 0..=2000 {
    ///     assert_eq!(bloom.contains(&x), bloom.contains(&x) && other.contains(&x));
    /// }
    /// ```
    #[inline]
    pub fn intersect(&mut self, other: &BloomFilter<S>) {
        assert_eq!(
            self.num_hashes(),
            other.num_hashes(),
            "expected same number of hashes"
        );
        self.bits.intersect(&other.bits);
    }
}

impl<S: BuildHasher> AtomicBloomFilter<S> {
    /// Inserts an element into the Bloom filter.
    ///
    /// # Returns
    ///
    /// `true` if the item may have been previously in the Bloom filter (indicating a potential false positive),
    /// `false` otherwise.
    ///
    /// # Examples
    /// ```
    /// use fastbloom::AtomicBloomFilter;
    ///
    /// let bloom = AtomicBloomFilter::with_num_bits(1024).hashes(4);
    /// bloom.insert(&2);
    /// assert!(bloom.contains(&2));
    /// ```
    #[inline]
    pub fn insert(&self, val: &(impl Hash + ?Sized)) -> bool {
        let source_hash = self.source_hash(val);
        self.insert_hash(source_hash)
    }

    /// Inserts the hash of an element into the Bloom filter.
    /// That is the element is pre-hashed and all subsequent hashes are derived from this "source" hash.
    ///
    /// # Returns
    ///
    /// `true` if the item may have been previously in the Bloom filter (indicating a potential false positive),
    /// `false` otherwise.
    #[inline]
    pub fn insert_hash(&self, hash: u64) -> bool {
        let mut hasher = DoubleHasher::new(hash);
        let mut previously_contained = true;
        for _ in 0..self.num_hashes {
            let h = hasher.next();
            previously_contained &= self.bits.set(index(self.num_bits(), h));
        }
        previously_contained
    }

    /// Inserts all the items in `iter` into the `self`. Immutable version of [`Self::extend`].
    #[inline]
    pub fn insert_all<T: Hash, I: IntoIterator<Item = T>>(&self, iter: I) {
        for val in iter {
            self.insert(&val);
        }
    }

    /// Clear all of the bits in the Bloom filter, removing all items.
    #[inline]
    pub fn clear(&self) {
        self.bits.clear();
    }

    /// Unions `other` into `self`. The hashers of both Bloomfilters must be identical (this is not enforced!).
    ///
    /// # Panics
    /// Panics if the other Bloomfilter has a different number of bits or hashes than `self`.
    ///
    /// # Example
    /// ```
    /// use fastbloom::AtomicBloomFilter;
    ///
    /// let bloom = AtomicBloomFilter::with_num_bits(4096).seed(&1).hashes(4);
    /// let other = AtomicBloomFilter::with_num_bits(4096).seed(&1).hashes(4);
    /// bloom.insert_all(0..=1000);
    /// other.insert_all(500..=1500);
    /// bloom.union(&other);
    ///
    /// for x in 0..=2000 {
    ///     assert_eq!(bloom.contains(&x), bloom.contains(&x) || other.contains(&x));
    /// }
    /// ```
    #[inline]
    pub fn union(&self, other: &AtomicBloomFilter<S>) {
        assert_eq!(
            self.num_hashes(),
            other.num_hashes(),
            "expected same number of hashes"
        );
        self.bits.union(&other.bits);
    }

    /// Intersects `other` onto `self`. The hashers of both Bloomfilters must be identical (this is not enforced!).
    ///
    /// # Panics
    /// Panics if the other Bloomfilter has a different number of bits or hashes than `self`.
    ///
    /// # Example
    /// ```
    /// use fastbloom::AtomicBloomFilter;
    ///
    /// let bloom = AtomicBloomFilter::with_num_bits(4096).seed(&1).hashes(4);
    /// let other = AtomicBloomFilter::with_num_bits(4096).seed(&1).hashes(4);
    /// bloom.insert_all(0..=1000);
    /// other.insert_all(500..=1500);
    /// bloom.intersect(&other);
    ///
    /// for x in 0..=2000 {
    ///     assert_eq!(bloom.contains(&x), bloom.contains(&x) && other.contains(&x));
    /// }
    /// ```
    #[inline]
    pub fn intersect(&self, other: &AtomicBloomFilter<S>) {
        assert_eq!(
            self.num_hashes(),
            other.num_hashes(),
            "expected same number of hashes"
        );
        self.bits.intersect(&other.bits);
    }
}

/// Returns a the block index for an item's hash.
/// The block index must be in the range `0..num_blocks`.
/// This implementation is a more performant alternative to `hash % num_blocks`:
/// <https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/>
#[inline]
pub(crate) fn index(num_blocks: usize, hash: u64) -> usize {
    (((hash >> 32).wrapping_mul(num_blocks as u64)) >> 32) as usize
}

macro_rules! impl_tests {
    ($modname:ident, $name:ident) => {
        #[allow(unused_mut)]
        #[cfg(not(feature = "loom"))]
        #[cfg(test)]
        mod $modname {
            use super::*;
            use alloc::format;
            use rand::{rngs::StdRng, Rng, SeedableRng};

            trait Seeded: BuildHasher {
                fn seeded(seed: &[u8; 16]) -> Self;
            }
            impl Seeded for DefaultHasher {
                fn seeded(seed: &[u8; 16]) -> Self {
                    Self::seeded(seed)
                }
            }

            const TRIALS: usize = 1_000_000;

            fn false_pos_rate<H: BuildHasher>(filter: &$name<H>) -> f64 {
                let mut total = 0;
                let mut false_positives = 0;
                for x in non_member_nums() {
                    total += 1;
                    false_positives += filter.contains(&x) as usize;
                }
                (false_positives as f64) / (total as f64)
            }

            fn member_nums(num: usize) -> impl Iterator<Item = u64> {
                random_numbers(num, 5)
            }

            fn non_member_nums() -> impl Iterator<Item = u64> {
                random_numbers(TRIALS, 7).map(|x| x + u32::MAX as u64)
            }

            fn random_numbers(num: usize, seed: u64) -> impl Iterator<Item = u64> {
                let mut rng = StdRng::seed_from_u64(seed);
                (0..=num).map(move |_| rng.random::<u32>() as u64)
            }

            #[test]
            fn test_to_from_vec() {
                fn to_from_(size: usize) {
                    let mut b = $name::new_builder(size).seed(&1).hashes(3);
                    b.extend(member_nums(1000));
                    let b2 = $name::new_from_vec(b.iter().collect())
                        .seed(&1)
                        .hashes(3);
                    assert_eq!(b, b2);
                    assert_eq!(b.num_bits(), b.bits.len() * 64);
                    assert!(size <= b.bits.len() * 64);
                    assert!((size + u64::BITS as usize) > b.bits.len() * 64);
                }
                for size in 1..=10009 {
                    to_from_(size);
                }
            }

            #[test]
            fn first_insert_false() {
                let mut filter = $name::with_num_bits(1202).expected_items(4);
                assert!(!filter.insert(&5));
            }

            #[test]
            fn target_fp_is_accurate() {
                let thresh = 2.0f64;
                for mag in 1..=6 {
                    let fp = 1.0f64 / 10u64.pow(mag) as f64;
                    for num_items_mag in 1..7 {
                        let num_items = 10usize.pow(num_items_mag);
                        let mut filter = $name::new_with_false_pos(fp)
                            .seed(&42)
                            .expected_items(num_items);
                        filter.extend(member_nums(num_items));
                        let sample_fp = false_pos_rate(&filter);
                        let err = (fp - sample_fp).abs() / fp;
                        assert!(sample_fp < fp || err < thresh,  "err {err:}, thresh {thresh:}, num_items: {num_items:}, fp: {fp:}, sample fp: {sample_fp:}");
                    }
                }
            }

            #[test]
            fn nothing_after_clear() {
                for mag in 1..6 {
                    let size = 10usize.pow(mag);
                    for bloom_size_mag in 6..10 {
                        let num_blocks_bytes = 1 << bloom_size_mag;
                        let num_bits = num_blocks_bytes * 8;
                        let mut filter = $name::new_builder(num_bits)
                            .seed(&7)
                            .expected_items(size);
                        filter.extend(member_nums(size));
                        assert!(filter.num_hashes() > 0);
                        filter.clear();
                        assert!(member_nums(size).all(|x| !filter.contains(&x)));
                    }
                }
            }

            #[test]
            fn random_inserts_always_contained() {
                for mag in 1..6 {
                    let size = 10usize.pow(mag);
                    for bloom_size_mag in 6..10 {
                        let num_blocks_bytes = 1 << bloom_size_mag;
                        let num_bits = num_blocks_bytes * 8;
                        let mut filter = $name::new_builder(num_bits).expected_items(size);
                        filter.extend(member_nums(size));
                        assert!(member_nums(size).all(|x| filter.contains(&x)));
                        assert!(member_nums(size).all(|x| filter.insert(&x)));
                    }
                }
            }

            #[test]
            fn test_optimal_hashes_is_optimal() {
                fn test_optimal_hashes_is_optimal_<H: Seeded>() {
                    let sizes = [1000, 2000, 5000, 6000, 8000, 10000];
                    for num_items in sizes {
                        let num_bits = 65000 * 8;
                        let mut filter = $name::new_builder(num_bits)
                            .hasher(H::seeded(&[42; 16]))
                            .expected_items(num_items);
                        filter.extend(member_nums(num_items));

                        let fp_to_beat = false_pos_rate(&filter);
                        let optimal_hashes = filter.num_hashes();

                        for num_hashes in [optimal_hashes - 1, optimal_hashes + 1] {
                            let mut test_filter = $name::new_builder(num_bits)
                                .hasher(H::seeded(&[42; 16]))
                                .hashes(num_hashes);
                            test_filter.extend(member_nums(num_items));
                            let fp = false_pos_rate(&test_filter);
                            assert!(fp_to_beat <= fp);
                        }
                    }
                }
                test_optimal_hashes_is_optimal_::<DefaultHasher>();
            }

            #[test]
            fn seeded_is_same() {
                let num_bits = 1 << 10;
                let sample_vals = member_nums(1000).collect::<Vec<_>>();
                for x in 0u8..32 {
                    let seed = x as u128;
                    assert_eq!(
                        $name::new_builder(num_bits)
                            .seed(&seed)
                            .items(sample_vals.iter()),
                        $name::new_builder(num_bits)
                            .seed(&seed)
                            .items(sample_vals.iter())
                    );
                    assert!(
                        !($name::new_builder(num_bits)
                            .seed(&(seed + 1))
                            .items(sample_vals.iter())
                            == $name::new_builder(num_bits)
                                .seed(&seed)
                                .items(sample_vals.iter()))
                    );
                }
            }

            #[test]
            fn false_pos_decrease_with_size() {
                for mag in 5..6 {
                    let size = 10usize.pow(mag);
                    let mut prev_fp = 1.0;
                    for num_bits_mag in 9..22 {
                        let num_bits = 1 << num_bits_mag;

                        let mut filter = $name::new_builder(num_bits).expected_items(size);
                        filter.extend(member_nums(size));

                        let fp = false_pos_rate(&filter);

                        let err = format!(
                            "size: {size:}, num_bits: {num_bits:}, {:.6}, {:?}",
                            fp,
                            filter.num_hashes(),
                        );
                        assert!(
                            fp <= prev_fp,
                            "{}",
                            err
                        );
                        prev_fp = fp;
                    }
                }
            }

            fn assert_even_distribution(distr: &[u64], err: f64) {
                assert!(err > 0.0 && err < 1.0);
                let expected: i64 = (distr.iter().sum::<u64>() / (distr.len() as u64)) as i64;
                let thresh = (expected as f64 * err) as i64;
                for x in distr {
                    let diff = (*x as i64 - expected).abs();
                    assert!(
                        diff <= thresh,
                        "{x:?} deviates from {expected:?}\nDistribution: {distr:?}"
                    );
                }
            }

            #[test]
            fn test_seeded_hash_from_hashes_depth() {
                for size in [1, 10, 100, 1000] {
                    let mut rng = StdRng::seed_from_u64(524323);
                    let mut hasher = DoubleHasher::new(rng.random_range(0..u64::MAX));
                    let mut seeded_hash_counts: Vec<_> = repeat(0).take(size).collect();
                    for _ in 0..(size * 10_000) {
                        let hi = hasher.next();
                        seeded_hash_counts[(hi as usize) % size] += 1;
                    }
                    assert_even_distribution(&seeded_hash_counts, 0.05);
                }
            }

            #[test]
            fn test_debug() {
                let filter = $name::with_num_bits(1).hashes(1);
                assert!(!format!("{:?}", filter).is_empty());
            }

            #[test]
            fn test_clone() {
                let filter = $name::with_num_bits(4).hashes(4);
                let mut cloned = filter.clone();
                assert_eq!(filter, cloned);
                cloned.insert(&42);
                assert!(filter != cloned);
            }

            #[test]
            fn eq_constructors_num_bits() {
                assert_eq!(
                    $name::with_num_bits(4).hashes(4),
                    $name::new_builder(4).hashes(4),
                );
            }

            #[test]
            fn eq_constructors_false_pos() {
                assert_eq!(
                    $name::with_false_pos(0.4),
                    $name::new_with_false_pos(0.4),
                );
            }

            #[test]
            fn eq_constructors_from_vec() {
                assert_eq!(
                    $name::from_vec(repeat(42).take(42).collect()),
                    $name::new_from_vec(repeat(42).take(42).collect()),
                );
            }

            #[test]
            fn test_rebuilt_from_vec() {
                for num in [1, 10, 1000, 100_000] {
                    for fp in [0.1, 0.01, 0.0001, 0.0000001] {
                        let mut b = $name::with_false_pos(fp)
                            .seed(&42)
                            .expected_items(num);
                        b.extend(member_nums(num));
                        let orig_hashes = b.num_hashes();
                        let new = $name::from_vec(b.iter().collect())
                            .seed(&42)
                            .hashes(orig_hashes);
                        assert!(member_nums(num).all(|x| new.contains(&x)));
                    }
                }
            }

            #[cfg(feature = "serde")]
            #[test]
            fn test_serde() {
                for num in [1, 10, 1000, 100_000] {
                    for fp in [0.1, 0.01, 0.0001, 0.0000001] {
                        let mut before = $name::with_false_pos(fp)
                            .seed(&42)
                            .expected_items(num);
                        before.extend(member_nums(num));

                        let s = serde_cbor::to_vec(&before).unwrap();
                        let mut after: $name = serde_cbor::from_slice(&s).unwrap();
                        assert_eq!(before, after);

                        before.extend(member_nums(num * 2));
                        after.extend(member_nums(num * 2));
                        assert_eq!(before, after);
                    }
                }
            }
        }
    };
}

impl_tests!(non_atomic, BloomFilter);
impl_tests!(atomic, AtomicBloomFilter);

#[cfg(not(feature = "loom"))]
#[cfg(test)]
mod atomic_parity_tests {
    #[cfg(feature = "serde")]
    #[test]
    fn serde_parity() {
        use super::*;

        for num_bits in [64, 1024, 4096, 1 << 16] {
            for seed in 4..=18 {
                let mut non = BloomFilter::with_num_bits(num_bits)
                    .seed(&seed)
                    .expected_items(100);
                non.insert_all(0..=100);
                let atomic = AtomicBloomFilter::with_num_bits(num_bits)
                    .seed(&seed)
                    .expected_items(100);
                atomic.insert_all(0..=100);

                let non_bytes = serde_cbor::to_vec(&non).unwrap();
                let atomic_bytes = serde_cbor::to_vec(&atomic).unwrap();
                assert_eq!(non_bytes, atomic_bytes);

                let non_from_atomic: BloomFilter = serde_cbor::from_slice(&atomic_bytes).unwrap();
                let atomic_from_non: AtomicBloomFilter =
                    serde_cbor::from_slice(&non_bytes).unwrap();
                assert_eq!(non_from_atomic, non);
                assert_eq!(atomic_from_non, atomic);
            }
        }
    }
}

#[cfg(feature = "loom")]
#[cfg(test)]
mod loom_tests {
    use super::*;

    #[test]
    fn test_loom() {
        loom::model(|| {
            let b = loom::sync::Arc::new(AtomicBloomFilter::with_num_bits(128).seed(&42).hashes(2));
            let expected = AtomicBloomFilter::with_num_bits(128).seed(&42).hashes(2);
            expected.insert_all(1..=3);

            let handles: Vec<_> = [(1..=2), (2..=3)]
                .into_iter()
                .map(|data| {
                    let v = b.clone();
                    loom::thread::spawn(move || v.insert_all(data))
                })
                .collect();

            for handle in handles {
                handle.join().unwrap();
            }

            let res = b.iter().collect::<Vec<_>>();
            assert_eq!(res, expected.iter().collect::<Vec<_>>());
        });
    }
}
