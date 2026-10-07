//! The [`Generator`] trait and [`BlockRng`]
//!
//! Trait [`Generator`] may be implemented by block-generators; that is PRNGs
//! whose output is a *block* of words, such as `[u32; 16]`.
//!
//! The struct [`BlockRng`] wraps such a [`Generator`] together with an output
//! buffer and implements several methods (e.g. [`BlockRng::next_word`]) to
//! assist in the implementation of [`TryRng`]. Note that (unlike in earlier
//! versions of `rand_core`) [`BlockRng`] itself does not implement [`TryRng`]
//! since in practice we found it was always beneficial to use a wrapper type
//! over [`BlockRng`].
//!
//! # Example
//!
//! ```
//! use core::convert::Infallible;
//! use rand_core::{Rng, SeedableRng, TryRng};
//! use rand_core::block::{Generator, BlockRng};
//!
//! struct MyRngCore {
//!     // Generator state ...
//! #    state: [u32; 8],
//! }
//!
//! impl Generator for MyRngCore {
//!     type Output = [u32; 8];
//!
//!     fn generate(&mut self, output: &mut Self::Output) {
//!         // Write a new block to output...
//! #        *output = self.state;
//!     }
//! }
//!
//! // Our RNG is a wrapper over BlockRng
//! pub struct MyRng(BlockRng<MyRngCore>);
//!
//! impl SeedableRng for MyRng {
//!     type Seed = [u8; 32];
//!     fn from_seed(seed: Self::Seed) -> Self {
//!         let core = MyRngCore {
//!             // ...
//! #            state: rand_core::utils::read_words(&seed),
//!         };
//!         MyRng(BlockRng::new(core))
//!     }
//! }
//!
//! impl TryRng for MyRng {
//!     type Error = Infallible;
//!
//!     #[inline]
//!     fn try_next_u32(&mut self) -> Result<u32, Infallible> {
//!         Ok(self.0.next_word())
//!     }
//!
//!     #[inline]
//!     fn try_next_u64(&mut self) -> Result<u64, Infallible> {
//!         Ok(self.0.next_u64_from_u32())
//!     }
//!
//!     #[inline]
//!     fn try_fill_bytes(&mut self, bytes: &mut [u8]) -> Result<(), Infallible> {
//!         Ok(self.0.fill_bytes(bytes))
//!     }
//! }
//!
//! // And if applicable: impl TryCryptoRng for MyRng {}
//!
//! let mut rng = MyRng::seed_from_u64(0);
//! println!("First value: {}", rng.next_u32());
//! # assert_eq!(rng.next_u32(), 1171109249);
//! ```
//!
//! [`TryRng`]: crate::TryRng
//! [`SeedableRng`]: crate::SeedableRng

use crate::utils::Word;
use core::fmt;

/// A random (block) generator
pub trait Generator {
    /// The output type.
    ///
    /// For use with [`rand_core::block`](crate::block) code this must be `[u32; _]` or `[u64; _]`.
    type Output;

    /// Generate a new block of `output`.
    ///
    /// This must fill `output` with random data.
    fn generate(&mut self, output: &mut Self::Output);

    /// Destruct the output buffer
    ///
    /// This method is called on [`Drop`] of the [`Self::Output`] buffer.
    /// The default implementation does nothing.
    #[inline]
    fn drop(&mut self, output: &mut Self::Output) {
        let _ = output;
    }
}

/// RNG functionality for a block [`Generator`]
///
/// This type encompasses a [`Generator`] [`core`](Self::core) and a buffer.
/// It provides optimized implementations of methods required by an [`Rng`].
///
/// All values are consumed in-order of generation. No whole words (e.g. `u32`
/// or `u64`) are discarded, though where a word is partially used (e.g. for a
/// byte-fill whose length is not a multiple of the word size) the rest of the
/// word is discarded.
///
/// [`Rng`]: crate::Rng
#[derive(Clone)]
pub struct BlockRng<G: Generator> {
    results: G::Output,
    /// The *core* part of the RNG, implementing the `generate` function.
    pub core: G,
}

// Custom Debug implementation that does not expose the contents of `results`.
impl<G> fmt::Debug for BlockRng<G>
where
    G: Generator + fmt::Debug,
{
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("BlockRng")
            .field("core", &self.core)
            .finish_non_exhaustive()
    }
}

impl<G: Generator> Drop for BlockRng<G> {
    fn drop(&mut self) {
        self.core.drop(&mut self.results);
    }
}

impl<W: Word + Default, const N: usize, G: Generator<Output = [W; N]>> BlockRng<G> {
    /// Create a new `BlockRng` from an existing RNG implementing
    /// `Generator`. Results will be generated on first use.
    #[inline]
    pub fn new(core: G) -> BlockRng<G> {
        let mut results = [W::default(); N];
        results[0] = W::from_usize(N);
        BlockRng { core, results }
    }

    /// Reconstruct from a core and a remaining-results buffer.
    ///
    /// This may be used to deserialize using a `core` and the output of
    /// [`Self::remaining_results`].
    ///
    /// Returns `None` if `remaining_results` is too long.
    pub fn reconstruct(core: G, remaining_results: &[W]) -> Option<Self> {
        let mut results = [W::default(); N];
        if remaining_results.len() < N {
            let index = N - remaining_results.len();
            results[index..].copy_from_slice(remaining_results);
            results[0] = W::from_usize(index);
            Some(BlockRng { results, core })
        } else {
            None
        }
    }
}

impl<W: Word, const N: usize, G: Generator<Output = [W; N]>> BlockRng<G> {
    /// Get the index into the result buffer.
    ///
    /// If this is equal to or larger than the size of the result buffer then
    /// the buffer is "empty" and `generate()` must be called to produce new
    /// results.
    #[inline(always)]
    fn index(&self) -> usize {
        self.results[0].into_usize()
    }

    #[inline(always)]
    fn set_index(&mut self, index: usize) {
        debug_assert!(0 < index && index <= N);
        self.results[0] = W::from_usize(index);
    }

    /// Re-generate buffer contents, skipping the first `n` words
    ///
    /// Existing buffer contents are discarded. A new set of results is
    /// generated (either immediately or when next required). The first `n`
    /// words are skipped (this may be used to set a specific word position).
    ///
    /// # Panics
    ///
    /// This method will panic if `n >= N` where `N` is the buffer size (in
    /// words).
    #[inline]
    pub fn reset_and_skip(&mut self, n: usize) {
        if n == 0 {
            self.set_index(N);
            return;
        }

        assert!(n < N);
        self.core.generate(&mut self.results);
        self.set_index(n);
    }

    /// Get the number of words consumed since the start of the block
    ///
    /// The result is in the range `0..N` where `N` is the buffer size (in
    /// words).
    #[inline]
    pub fn word_offset(&self) -> usize {
        let index = self.index();
        if index >= N { 0 } else { index }
    }

    /// Access the unused part of the results buffer
    ///
    /// The length of the returned slice is guaranteed to be less than the
    /// length of `<Self as Generator>::Output` (i.e. less than `N` where
    /// `Output = [W; N]`).
    ///
    /// This is a low-level interface intended for serialization.
    /// Results are not marked as consumed.
    #[inline]
    pub fn remaining_results(&self) -> &[W] {
        let index = self.index();
        &self.results[index..]
    }

    /// Generate the next word (e.g. `u32`)
    #[inline]
    pub fn next_word(&mut self) -> W {
        let mut index = self.index();
        if index >= N {
            self.core.generate(&mut self.results);
            index = 0;
        }

        let value = self.results[index];
        self.set_index(index + 1);
        value
    }
}

impl<const N: usize, G: Generator<Output = [u32; N]>> BlockRng<G> {
    /// Generate a `u64` from two `u32` words
    #[inline]
    pub fn next_u64_from_u32(&mut self) -> u64 {
        let index = self.index();
        let mut new_index;
        let (mut lo, mut hi);
        if index < N - 1 {
            lo = self.results[index];
            hi = self.results[index + 1];
            new_index = index + 2;
        } else {
            lo = self.results[N - 1];
            self.core.generate(&mut self.results);
            hi = self.results[0];
            new_index = 1;
            if index >= N {
                lo = hi;
                hi = self.results[1];
                new_index = 2;
            }
        }
        self.set_index(new_index);
        (u64::from(hi) << 32) | u64::from(lo)
    }
}

impl<W: Word, const N: usize, G: Generator<Output = [W; N]>> BlockRng<G> {
    /// Fill `dest`
    #[inline]
    pub fn fill_bytes(&mut self, dest: &mut [u8]) {
        let mut read_len = 0;
        let mut index = self.index();
        while read_len < dest.len() {
            if index >= N {
                self.core.generate(&mut self.results);
                index = 0;
            }

            let size = core::mem::size_of::<W>();
            let mut chunks = dest[read_len..].chunks_exact_mut(size);
            let mut src = self.results[index..].iter();

            let zipped = chunks.by_ref().zip(src.by_ref());
            let num_chunks = zipped.len();
            zipped.for_each(|(chunk, src)| chunk.copy_from_slice(src.to_le_bytes().as_ref()));
            index += num_chunks;
            read_len += num_chunks * size;

            if let Some(src) = src.next() {
                // We have consumed all full chunks of dest, but not src.
                let dest_rem = chunks.into_remainder();
                let n = dest_rem.len();
                if n > 0 {
                    dest_rem.copy_from_slice(&src.to_le_bytes().as_ref()[..n]);
                    index += 1;
                    debug_assert_eq!(read_len + n, dest.len());
                }
                break;
            }
        }
        self.set_index(index);
    }
}
