use crate::{math::*, AtomicBloomFilter, BloomFilter, BuildHasher, DefaultHasher};
use alloc::vec::Vec;
use core::{cmp::max, f64::consts::LN_2, hash::Hash};

macro_rules! builder_with_bits {
    ($name:ident, $bloom:ident) => {
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
            /// use ahash::RandomState;
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
            /// # Examples
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
            ///
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_num_bits(1024).hashes(4);")]
            /// ```
            pub fn hashes(self, num_hashes: u32) -> $bloom<S> {
                $bloom {
                    bits: self.data.into_iter().collect(),
                    num_hashes,
                    hasher: self.hasher,
                }
            }

            /// "Consumes" this builder, using the provided `expected_num_items` to return an
            #[doc = concat!("empty [`", stringify!($bloom), "`]. The number of hashes is optimized based on `expected_num_items`")]
            #[doc = concat!("to maximize Bloom filter accuracy (minimize false positives chance on [`", stringify!($bloom), "::contains`]).")]
            /// More or less than `expected_num_items` may be inserted into Bloom filter.
            ///
            /// # Examples
            ///
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
            ///
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_num_bits(1024).expected_items(500);")]
            /// ```
            pub fn expected_items(self, expected_num_items: usize) -> $bloom<S> {
                let hashes = optimal_hashes_f(self.data.len(), expected_num_items);
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
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_num_bits(1024).items([1, 2, 3]);")]
            /// ```
            pub fn items<I: IntoIterator<IntoIter = impl ExactSizeIterator<Item = impl Hash>>>(
                self,
                items: I,
            ) -> $bloom<S> {
                let into_iter = items.into_iter();
                let mut filter = self.expected_items(into_iter.len());
                filter.extend(into_iter);
                filter
            }
        }
    };
}

builder_with_bits!(BuilderWithBits, BloomFilter);
builder_with_bits!(AtomicBuilderWithBits, AtomicBloomFilter);

macro_rules! builder_with_fp {
    ($name:ident, $bloom:ident) => {
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
            /// use ahash::RandomState;
            ///
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_false_pos(0.001).hasher(RandomState::default()).expected_items(100);")]
            /// ```
            pub fn hasher<H: BuildHasher>(self, hasher: H) -> $name<H> {
                $name::<H> {
                    desired_fp_rate: self.desired_fp_rate,
                    hasher,
                }
            }

            /// "Consumes" this builder, using the provided `expected_num_items` to return an
            #[doc = concat!("empty [`", stringify!($bloom), "`]. The number of hashes is optimized based on `expected_num_items`")]
            #[doc = concat!("to maximize Bloom filter accuracy (minimize false positives chance on [`", stringify!($bloom), "::contains`]).")]
            /// More or less than `expected_num_items` may be inserted into Bloom filter.
            ///
            /// # Examples
            ///
            /// ```
            #[doc = concat!("use fastbloom::", stringify!($bloom), ";")]
            ///
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_false_pos(0.001).expected_items(500);")]
            /// ```
            pub fn expected_items(self, expected_num_items: usize) -> $bloom<S> {
                let num_bits = optimal_size(expected_num_items as f64, self.desired_fp_rate);
                $bloom::new_builder(num_bits)
                    .hasher(self.hasher)
                    .expected_items(expected_num_items)
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
            #[doc = concat!("let bloom = ", stringify!($bloom), "::with_false_pos(0.001).items([1, 2, 3]);")]
            /// ```
            pub fn items<I: IntoIterator<IntoIter = impl ExactSizeIterator<Item = impl Hash>>>(
                self,
                items: I,
            ) -> $bloom<S> {
                let into_iter = items.into_iter();
                let mut filter = self.expected_items(into_iter.len());
                filter.extend(into_iter);
                filter
            }
        }
    };
}

builder_with_fp!(BuilderWithFalsePositiveRate, BloomFilter);
builder_with_fp!(AtomicBuilderWithFalsePositiveRate, AtomicBloomFilter);

/// The optimal number of hashes to perform for an item given the expected number of items in the bloom filter.
/// Proof: <https://gopiandcode.uk/logs/log-bloomfilters-debunked.html>.
#[inline]
fn optimal_hashes_f(num_u64s: usize, num_items: usize) -> u32 {
    let num_bits = (num_u64s * 64) as f64;
    let hashes = LN_2 * num_bits / num_items as f64;
    max(hashes as u32, 1)
}

fn optimal_size(items_count: f64, fp_p: f64) -> usize {
    let log2_2 = LN_2 * LN_2;
    let result = 8 * ceil((items_count) * ln(fp_p) / (-8.0 * log2_2)) as usize;
    max(result, 512)
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
    use crate::BloomFilter;

    #[test]
    fn test_size() {
        let _: BloomFilter = BloomFilter::new_with_false_pos(0.0001).expected_items(10000);
    }
}
