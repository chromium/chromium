use crate::{Rng, TryRng};

/// A random number generator that can be explicitly seeded.
///
/// This trait encapsulates the low-level functionality common to all
/// pseudo-random number generators (PRNGs, or algorithmic generators).
///
/// A generator implementing `SeedableRng` will usually be deterministic, but
/// beware that portability and reproducibility of results **is not implied**.
/// Refer to documentation of the generator, noting that generators named after
/// a specific algorithm are usually tested for reproducibility against a
/// reference vector, while `SmallRng` and `StdRng` specifically opt out of
/// reproducibility guarantees.
pub trait SeedableRng: Sized {
    /// Seed type, which is restricted to types mutably-dereferenceable as `u8`
    /// arrays (we recommend `[u8; N]` for some `N`).
    ///
    /// It is recommended to seed PRNGs with a seed of at least circa 100 bits,
    /// which means an array of `[u8; 12]` or greater to avoid picking RNGs with
    /// partially overlapping periods.
    ///
    /// For cryptographic RNG's a seed of 256 bits is recommended, `[u8; 32]`.
    ///
    ///
    /// # Implementing `SeedableRng` for RNGs with large seeds
    ///
    /// Note that [`Default`] is not implemented for large arrays `[u8; N]` with
    /// `N` > 32. To be able to implement the traits required by `SeedableRng`
    /// for RNGs with such large seeds, the newtype pattern can be used:
    ///
    /// ```
    /// use rand_core::SeedableRng;
    ///
    /// const N: usize = 64;
    /// #[derive(Clone)]
    /// pub struct MyRngSeed(pub [u8; N]);
    /// # #[allow(dead_code)]
    /// pub struct MyRng(MyRngSeed);
    ///
    /// impl Default for MyRngSeed {
    ///     fn default() -> MyRngSeed {
    ///         MyRngSeed([0; N])
    ///     }
    /// }
    ///
    /// impl AsRef<[u8]> for MyRngSeed {
    ///     fn as_ref(&self) -> &[u8] {
    ///         &self.0
    ///     }
    /// }
    ///
    /// impl AsMut<[u8]> for MyRngSeed {
    ///     fn as_mut(&mut self) -> &mut [u8] {
    ///         &mut self.0
    ///     }
    /// }
    ///
    /// impl SeedableRng for MyRng {
    ///     type Seed = MyRngSeed;
    ///
    ///     fn from_seed(seed: MyRngSeed) -> MyRng {
    ///         MyRng(seed)
    ///     }
    /// }
    /// ```
    type Seed: Clone + Default + AsRef<[u8]> + AsMut<[u8]>;

    /// Create a new PRNG using the given seed.
    ///
    /// PRNG implementations are allowed to assume that bits in the seed are
    /// well distributed. That means usually that the number of one and zero
    /// bits are roughly equal, and values like 0, 1 and (size - 1) are unlikely.
    /// Note that many non-cryptographic PRNGs will show poor quality output
    /// if this is not adhered to. If you wish to seed from simple numbers, use
    /// `seed_from_u64` instead.
    ///
    /// All PRNG implementations should be reproducible unless otherwise noted:
    /// given a fixed `seed`, the same sequence of output should be produced
    /// on all runs, library versions and architectures (e.g. check endianness).
    /// Any "value-breaking" changes to the generator should require bumping at
    /// least the minor version and documentation of the change.
    ///
    /// It is not required that this function yield the same state as a
    /// reference implementation of the PRNG given equivalent seed; if necessary
    /// another constructor replicating behaviour from a reference
    /// implementation can be added.
    ///
    /// PRNG implementations should make sure `from_seed` never panics. In the
    /// case that some special values (like an all zero seed) are not viable
    /// seeds it is preferable to map these to alternative constant value(s),
    /// for example `0xBAD5EEDu32` or `0x0DDB1A5E5BAD5EEDu64` ("odd biases? bad
    /// seed"). This is assuming only a small number of values must be rejected.
    fn from_seed(seed: Self::Seed) -> Self;

    /// Create a new PRNG using a `u64` seed.
    ///
    /// This is a convenience-wrapper around `from_seed` to allow construction
    /// of any `SeedableRng` from a simple `u64` value. It is designed such that
    /// low Hamming Weight numbers like 0 and 1 can be used and should still
    /// result in good, independent seeds to the PRNG which is returned.
    ///
    /// This **is not suitable for cryptography**, as should be clear given that
    /// the input size is only 64 bits.
    ///
    /// Implementations for PRNGs *may* provide their own implementations of
    /// this function, but the default implementation should be good enough for
    /// all purposes. *Changing* the implementation of this function should be
    /// considered a value-breaking change.
    fn seed_from_u64(mut state: u64) -> Self {
        let mut seed = Self::Seed::default();
        let mut iter = seed.as_mut().chunks_exact_mut(4);
        for chunk in &mut iter {
            chunk.copy_from_slice(&pcg32(&mut state));
        }
        let rem = iter.into_remainder();
        if !rem.is_empty() {
            rem.copy_from_slice(&pcg32(&mut state)[..rem.len()]);
        }

        Self::from_seed(seed)
    }

    /// Create a new PRNG seeded from an infallible `Rng`.
    ///
    /// This may be useful when needing to rapidly seed many PRNGs from a master
    /// PRNG, and to allow forking of PRNGs. It may be considered deterministic.
    ///
    /// The master PRNG should be at least as high quality as the child PRNGs.
    /// When seeding non-cryptographic child PRNGs, we recommend using a
    /// different algorithm for the master PRNG (ideally a CSPRNG) to avoid
    /// correlations between the child PRNGs. If this is not possible (e.g.
    /// forking using small non-crypto PRNGs) ensure that your PRNG has a good
    /// mixing function on the output or consider use of a hash function with
    /// `from_seed`.
    ///
    /// Note that seeding `XorShiftRng` from another `XorShiftRng` provides an
    /// extreme example of what can go wrong: the new PRNG will be a clone
    /// of the parent.
    ///
    /// PRNG implementations are allowed to assume that a good RNG is provided
    /// for seeding, and that it is cryptographically secure when appropriate.
    /// As of `rand` 0.7 / `rand_core` 0.5, implementations overriding this
    /// method should ensure the implementation satisfies reproducibility
    /// (in prior versions this was not required).
    ///
    /// [`rand`]: https://docs.rs/rand
    fn from_rng<R: Rng + ?Sized>(rng: &mut R) -> Self {
        let mut seed = Self::Seed::default();
        rng.fill_bytes(seed.as_mut());
        Self::from_seed(seed)
    }

    /// Create a new PRNG seeded from a potentially fallible `Rng`.
    ///
    /// See [`from_rng`][SeedableRng::from_rng] docs for more information.
    fn try_from_rng<R: TryRng + ?Sized>(rng: &mut R) -> Result<Self, R::Error> {
        let mut seed = Self::Seed::default();
        rng.try_fill_bytes(seed.as_mut())?;
        Ok(Self::from_seed(seed))
    }

    /// Fork this PRNG
    ///
    /// This creates a new PRNG from the current one by initializing a new one and
    /// seeding it from the current one.
    ///
    /// This is useful when initializing a PRNG for a thread
    fn fork(&mut self) -> Self
    where
        Self: Rng,
    {
        Self::from_rng(self)
    }

    /// Fork this PRNG
    ///
    /// This creates a new PRNG from the current one by initializing a new one and
    /// seeding it from the current one.
    ///
    /// This is useful when initializing a PRNG for a thread.
    ///
    /// This is the failable equivalent to [`SeedableRng::fork`]
    fn try_fork(&mut self) -> Result<Self, Self::Error>
    where
        Self: TryRng,
    {
        Self::try_from_rng(self)
    }
}

/// PCG32 generator function
fn pcg32(state: &mut u64) -> [u8; 4] {
    const MUL: u64 = 0x5851_F42D_4C95_7F2D;
    const INC: u64 = 0xA176_54E4_6FBE_17F3;

    // We advance the state first (to get away from the input value,
    // in case it has low Hamming Weight).
    *state = state.wrapping_mul(MUL).wrapping_add(INC);
    let state = *state;

    // Use PCG output function with to_le to generate x:
    let xorshifted = (((state >> 18) ^ state) >> 27) as u32;
    let rot = (state >> 59) as u32;
    let x = xorshifted.rotate_right(rot);
    x.to_le_bytes()
}
