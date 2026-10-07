//! Main `rand_core` tests
use rand_core::{CryptoRng, Infallible, Rng, SeedableRng, TryCryptoRng, TryRng, UnwrapErr, utils};

#[test]
fn test_seed_from_u64() {
    struct SeedableNum(u64);
    impl SeedableRng for SeedableNum {
        type Seed = [u8; 8];

        fn from_seed(seed: Self::Seed) -> Self {
            let x: [u64; 1] = utils::read_words(&seed);
            SeedableNum(x[0])
        }
    }

    const N: usize = 8;
    const SEEDS: [u64; N] = [0u64, 1, 2, 3, 4, 8, 16, -1i64 as u64];
    let mut results = [0u64; N];
    for (i, seed) in SEEDS.iter().enumerate() {
        let SeedableNum(x) = SeedableNum::seed_from_u64(*seed);
        results[i] = x;
    }

    for (i1, r1) in results.iter().enumerate() {
        let weight = r1.count_ones();
        // This is the binomial distribution B(64, 0.5), so chance of
        // weight < 20 is binocdf(19, 64, 0.5) = 7.8e-4, and same for
        // weight > 44.
        assert!((20..=44).contains(&weight));

        for (i2, r2) in results.iter().enumerate() {
            if i1 == i2 {
                continue;
            }
            let diff_weight = (r1 ^ r2).count_ones();
            assert!(diff_weight >= 20);
        }
    }

    // value-breakage test:
    assert_eq!(results[0], 5029875928683246316);
}

// A stub RNG.
struct SomeRng;

impl TryRng for SomeRng {
    type Error = Infallible;

    fn try_next_u32(&mut self) -> Result<u32, Self::Error> {
        unimplemented!()
    }
    fn try_next_u64(&mut self) -> Result<u64, Self::Error> {
        unimplemented!()
    }
    fn try_fill_bytes(&mut self, _dst: &mut [u8]) -> Result<(), Self::Error> {
        unimplemented!()
    }
}

impl TryCryptoRng for SomeRng {}

#[test]
fn dyn_rng_to_tryrng() {
    // Illustrates the need for `+ ?Sized` bound in `impl<R: Rng> TryRng for R`.

    // A method in another crate taking a fallible RNG
    fn third_party_api(_rng: &mut (impl TryRng + ?Sized)) -> bool {
        true
    }

    // A method in our crate requiring an infallible RNG
    fn my_api(rng: &mut dyn Rng) -> bool {
        // We want to call the method above
        third_party_api(rng)
    }

    assert!(my_api(&mut SomeRng));
}

#[test]
fn dyn_cryptorng_to_trycryptorng() {
    // Illustrates the need for `+ ?Sized` bound in `impl<R: CryptoRng> TryCryptoRng for R`.

    // A method in another crate taking a fallible RNG
    fn third_party_api(_rng: &mut (impl TryCryptoRng + ?Sized)) -> bool {
        true
    }

    // A method in our crate requiring an infallible RNG
    fn my_api(rng: &mut dyn CryptoRng) -> bool {
        // We want to call the method above
        third_party_api(rng)
    }

    assert!(my_api(&mut SomeRng));
}

#[test]
fn dyn_unwrap_mut_tryrng() {
    // Illustrates that UnwrapMut may be used over &mut R where R: TryRng

    fn third_party_api(_rng: &mut impl Rng) -> bool {
        true
    }

    fn my_api(rng: &mut (impl TryRng + ?Sized)) -> bool {
        let mut infallible_rng = UnwrapErr(rng);
        third_party_api(&mut infallible_rng)
    }

    assert!(my_api(&mut SomeRng));
}

#[test]
fn dyn_unwrap_mut_trycryptorng() {
    // Crypto variant of the above

    fn third_party_api(_rng: &mut impl CryptoRng) -> bool {
        true
    }

    fn my_api(rng: &mut (impl TryCryptoRng + ?Sized)) -> bool {
        let mut infallible_rng = UnwrapErr(rng);
        third_party_api(&mut infallible_rng)
    }

    assert!(my_api(&mut SomeRng));
}

#[test]
fn reborrow_unwrap_mut() {
    struct FourRng;

    impl TryRng for FourRng {
        type Error = Infallible;
        fn try_next_u32(&mut self) -> Result<u32, Self::Error> {
            Ok(4)
        }
        fn try_next_u64(&mut self) -> Result<u64, Self::Error> {
            unimplemented!()
        }
        fn try_fill_bytes(&mut self, _: &mut [u8]) -> Result<(), Self::Error> {
            unimplemented!()
        }
    }

    let mut rng = FourRng;
    let mut rng = UnwrapErr(&mut rng);

    assert_eq!(rng.next_u32(), 4);
    {
        let mut rng2 = rng.re();
        assert_eq!(rng2.next_u32(), 4);
        // Make sure rng2 is dropped.
    }
    assert_eq!(rng.next_u32(), 4);
}
