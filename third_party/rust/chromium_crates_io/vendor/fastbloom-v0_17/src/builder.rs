use crate::{math::*, AtomicBloomFilter, BloomFilter, BuildHasher, DefaultHasher};
use alloc::vec::Vec;
use core::{cmp::max, f64::consts::LN_2, hash::Hash};

macro_rules! builder_with_bits {
    ($name:ident, $($m:ident)?, $bloom:ident) => {
        /// A Bloom filter builder with an immutable number of bits.
        ///
        #[doc = concat!("This type can be used to construct an instance of [`", stringify!($bloom), "`] via the builder pattern.")]
        ///
        /// # Examples
        /// ```
        #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
        ///
        #[doc = concat!("let builder = ", stringify!($bloom), "::with_num_bits(1024);")]
        #[doc = concat!("let builder = ", stringify!($bloom), "::from_vec(vec![0; 8]);")]
        /// ```
        #[derive(Debug, Clone)]
        pub struct $name<S = DefaultHasher> {
            pub(crate) data: Vec<u64>,
            pub(crate) hasher: S,
        }

        impl<S: BuildHasher> PartialEq for $name<S> {
            fn eq(&self, other: &Self) -> bool {
                self.data == other.data
            }
        }
        impl<S: BuildHasher> Eq for $name<S> {}

        impl $name {
            /// Sets the seed for this builder. The later constructed Bloom filter
            /// will use this seed when hashing items.
            ///
            /// # Examples
            ///
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
            ///
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_num_bits(1024).seed(&1).hashes(4);")]
            /// ```
            pub fn seed(mut self, seed: &u128) -> Self {
                self.hasher = DefaultHasher::seeded(&seed.to_be_bytes());
                self
            }
        }

        impl<S: BuildHasher> $name<S> {
            /// Sets the hasher for this builder. The later constructed Bloom filter will use
            /// this hasher when inserting and checking items.
            ///
            /// # Examples
            ///
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
            /// use foldhash::fast::RandomState;
            ///
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_num_bits(1024).hasher(RandomState::default()).hashes(4);")]
            /// ```
            pub fn hasher<H: BuildHasher>(self, hasher: H) -> $name<H> {
                $name::<H> {
                    data: self.data,
                    hasher,
                }
            }

            /// "Consumes" this builder, using the provided `num_hashes` to return an
            #[doc = concat!("empty [`", stringify!($bloom), "`].")]
            ///
            /// Note: if `num_hashes` is 0, it is treated as 1. Bloom filters with 0
            /// hashes per item are practically useless, and disallowing this case
            /// enables further optimizations.
            ///
            /// # Examples
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
            ///
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_num_bits(1024).hashes(4);")]
            /// ```
            pub fn hashes(self, num_hashes: u32) -> $bloom<S> {
                $bloom {
                    bits: self.data.into_iter().collect(),
                    num_hashes_minus_one: max(1, num_hashes) - 1,
                    hasher: self.hasher,
                }
            }

            /// "Consumes" this builder, using the provided `expected_items` to return an
            #[doc = concat!("empty [`", stringify!($bloom), "`]. The number of hashes is optimized based on `expected_items`")]
            #[doc = concat!("to maximize Bloom filter accuracy (minimize false positives chance on [`", stringify!($bloom), "::contains`]).")]
            /// More or less than `expected_items` may be inserted into Bloom filter.
            ///
            /// Note: `expected_items` will internally be set to 1 if 0 is specified.
            ///
            /// # Examples
            ///
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
            ///
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_num_bits(1024).expected_items(500);")]
            /// ```
            pub fn expected_items(self, expected_items: usize) -> $bloom<S> {
                let expected_items = max(1, expected_items);
                let hashes = optimal_hashes(self.data.len() * 64, expected_items);
                self.hashes(hashes)
            }

            #[doc = concat!("\"Consumes\" this builder and constructs a [`", stringify!($bloom), "`] containing")]
            /// all values in `items`. The number of hashes per item
            /// is optimized based on `items.len()` to maximize Bloom filter accuracy
            #[doc = concat!("(minimize false positives chance on [`", stringify!($bloom), "::contains`]).")]
            ///
            /// # Examples
            ///
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
            ///
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_num_bits(1024).items([1, 2, 3].iter());")]
            /// ```
            pub fn items<'a, H: Hash + 'a, I: IntoIterator<IntoIter = impl ExactSizeIterator<Item = &'a H>>>(
                self,
                items: I,
            ) -> $bloom<S> {
                let into_iter = items.into_iter();
                let $($m)? filter = self.expected_items(into_iter.len());
                filter.insert_all(into_iter);
                filter
            }
        }
    };
}

builder_with_bits!(BuilderWithBits, mut, BloomFilter);
builder_with_bits!(AtomicBuilderWithBits, , AtomicBloomFilter);

macro_rules! builder_with_fp {
    ($name:ident, $($m:ident)?, $bloom:ident) => {
        /// A Bloom filter builder with an immutable false positive rate.
        ///
        /// This type can be used to construct an instance of [`BloomFilter`] via the builder pattern.
        ///
        /// # Examples
        ///
        /// ```
        #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
        ///
        #[doc = concat!("let builder = ", stringify!($bloom), "::with_false_pos(0.01);")]
        /// ```
        #[derive(Debug, Clone)]
        pub struct $name<S = DefaultHasher> {
            pub(crate) desired_fp_rate: f64,
            pub(crate) hasher: S,
        }

        impl<S: BuildHasher> PartialEq for $name<S> {
            fn eq(&self, other: &Self) -> bool {
                self.desired_fp_rate == other.desired_fp_rate
            }
        }
        impl<S: BuildHasher> Eq for $name<S> {}

        impl $name {
            /// Sets the seed for this builder. The later constructed Bloom filter
            /// will use this seed when hashing items.
            ///
            /// # Examples
            ///
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
            ///
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_false_pos(0.001).seed(&1).expected_items(100);")]
            /// ```
            pub fn seed(mut self, seed: &u128) -> Self {
                self.hasher = DefaultHasher::seeded(&seed.to_be_bytes());
                self
            }
        }

        impl<S: BuildHasher> $name<S> {
            #[doc = concat!("Sets the hasher for this builder. The later constructed [`", stringify!($bloom), "`] will use")]
            /// this hasher when inserting and checking items.
            ///
            /// # Examples
            ///
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
            /// use foldhash::fast::RandomState;
            ///
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_false_pos(0.001).hasher(RandomState::default()).expected_items(100);")]
            /// ```
            pub fn hasher<H: BuildHasher>(self, hasher: H) -> $name<H> {
                $name::<H> {
                    desired_fp_rate: self.desired_fp_rate,
                    hasher,
                }
            }

            /// "Consumes" this builder, using the provided `expected_items` to return an
            #[doc = concat!("empty [`", stringify!($bloom), "`]. The number of hashes is optimized based on `expected_items`")]
            #[doc = concat!("to maximize Bloom filter accuracy (minimize false positives chance on [`", stringify!($bloom), "::contains`]).")]
            /// More or less than `expected_items` may be inserted into Bloom filter.
            ///
            /// Note: `expected_items` will internally be set to 1 if 0 is specified.
            ///
            /// # Examples
            ///
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
            ///
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_false_pos(0.001).expected_items(500);")]
            /// ```
            pub fn expected_items(self, expected_items: usize) -> $bloom<S> {
                let expected_items = max(1, expected_items);
                let num_bits = optimal_size(expected_items, self.desired_fp_rate);
                $bloom::new_builder(num_bits)
                    .hasher(self.hasher)
                    .expected_items(expected_items)
            }

            #[doc = concat!("\"Consumes\" this builder and constructs a [`", stringify!($bloom), "`] containing")]
            /// all values in `items`. The number of hashes per item and underlying memory
            /// is optimized based on `items.len()` to meet the desired false positive rate.
            ///
            /// # Examples
            ///
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
            ///
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_false_pos(0.001).items([1, 2, 3].iter());")]
            /// ```
            pub fn items<'a, H: Hash + 'a, I: IntoIterator<IntoIter = impl ExactSizeIterator<Item = &'a H>>>(
                self,
                items: I,
            ) -> $bloom<S> {
                let into_iter = items.into_iter();
                let $($m)? filter = self.expected_items(into_iter.len());
                filter.insert_all(into_iter);
                filter
            }
        }
    };
}

builder_with_fp!(BuilderWithFalsePositiveRate, mut, BloomFilter);
builder_with_fp!(AtomicBuilderWithFalsePositiveRate, , AtomicBloomFilter);

/// Returns the optimal (for false positive rate) number of hashes to perform for an item given the expected number of items in the bloom filter.
pub fn optimal_hashes(num_bits: usize, num_items: usize) -> u32 {
    // Proof: <https://gopiandcode.uk/logs/log-bloomfilters-debunked.html>.
    let num_bits = num_bits as f64;
    let hashes = LN_2 * num_bits / num_items as f64;
    max(round(hashes) as u32, 1)
}

/// Returns the smallest size in bits of a Bloom filter containing `num_items` items to achieve the target false positive rate.
pub fn optimal_size(num_items: usize, fp: f64) -> usize {
    let num_items = num_items as f64;
    let log2_2 = LN_2 * LN_2;
    let result = 8 * ceil(num_items * ln(fp) / (-8.0 * log2_2)) as usize;
    max(result, 64)
}

/// Returns the probability of a "1" bit in the Bloom filter.
pub fn expected_density(hashes: u32, bits: usize, items: usize) -> f64 {
    let total_sets = (items * hashes as usize) as f64;
    let bits = bits as f64;
    let prob_set = 1.0 / bits;
    let prob_not_set = 1.0 - prob_set;
    let prob_all_not_set = crate::math::pow(prob_not_set, total_sets);
    1.0 - prob_all_not_set
}

/// Returns the expected false positive rate of a Bloom filter.
pub fn expected_false_pos(hashes: u32, density: f64) -> f64 {
    crate::math::pow(density, hashes as f64)
}

#[cfg(test)]
mod tests {
    use super::*;

    // optimal size should produce a FP the same.

    #[test]
    fn test_expected_false_pos() {
        for items_mag in 1..=32 {
            let items = 2usize.pow(items_mag);
            for fp_mag in 1..=16 {
                let target_fp = 1.0f64 / 10u64.pow(fp_mag) as f64;
                let size = optimal_size(items, target_fp);

                let thresh = if size < 256 {
                    0.1 // If size is tool small results too sensitive
                } else {
                    0.01
                };

                let h = optimal_hashes(size, items);
                let density = expected_density(h, size, items);
                let expected_fp = expected_false_pos(h, density);
                let err = (expected_fp - target_fp) / target_fp;
                assert!(err < thresh);
            }
        }
    }

    fn density_err(d: f64) -> f64 {
        (0.5 - d).abs()
    }

    #[test]
    fn test_optimal_hashes() {
        for bits_mag in 6..=16 {
            let bits = 2usize.pow(bits_mag);
            for items_mag in 1..=16 {
                let items = 2usize.pow(items_mag);

                let h = optimal_hashes(bits, items);

                // Too sensitive to rounding errors
                if h > 1000 {
                    continue;
                }
                let density = expected_density(h, bits, items);
                assert!(density_err(density) <= density_err(expected_density(h + 1, bits, items)));
                assert!(density_err(density) <= density_err(expected_density(h - 1, bits, items)));
            }
        }
    }
}

#[cfg(test)]
mod for_accuracy_tests {
    use crate::BloomFilter;

    #[test]
    fn data_size() {
        let size_bits = 512 * 1000;
        let bloom = BloomFilter::with_num_bits(size_bits).hashes(4);
        assert_eq!(bloom.num_bits(), size_bits);
    }

    #[test]
    fn specified_hashes() {
        for num_hashes in 1..1000 {
            assert_eq!(
                num_hashes,
                BloomFilter::with_num_bits(1)
                    .hashes(num_hashes)
                    .num_hashes()
            );
            assert_eq!(
                num_hashes,
                BloomFilter::with_num_bits(1)
                    .hashes(num_hashes)
                    .num_hashes()
            );
        }
    }
}

#[cfg(test)]
mod for_size_tests {
    use crate::{AtomicBloomFilter, BloomFilter};

    #[test]
    fn test_size() {
        let _: BloomFilter = BloomFilter::new_with_false_pos(0.0001).expected_items(10000);
    }

    #[test]
    fn test_zero_hashes() {
        let bloom = BloomFilter::with_num_bits(512).hashes(0);
        assert_eq!(bloom.num_hashes(), 1);
        let bloom = AtomicBloomFilter::with_num_bits(512).hashes(0);
        assert_eq!(bloom.num_hashes(), 1);
    }
}
