//! Tests for the `block` module items
use rand_core::{
    SeedableRng,
    block::{BlockRng, Generator},
};

const RESULTS_LEN: usize = 16;

#[derive(Debug, Clone)]
struct DummyRng {
    counter: u32,
}

impl Generator for DummyRng {
    type Output = [u32; RESULTS_LEN];

    fn generate(&mut self, output: &mut Self::Output) {
        for item in output {
            *item = self.counter;
            self.counter = self.counter.wrapping_add(3511615421);
        }
    }
}

impl SeedableRng for DummyRng {
    type Seed = [u8; 4];

    fn from_seed(seed: Self::Seed) -> Self {
        DummyRng {
            counter: u32::from_le_bytes(seed),
        }
    }
}

#[test]
fn blockrng_next_u32_vs_next_u64() {
    let mut rng1 = BlockRng::new(DummyRng::from_seed([1, 2, 3, 4]));
    let mut rng2 = rng1.clone();
    let mut rng3 = rng1.clone();

    let mut a = [0; 16];
    a[..4].copy_from_slice(&rng1.next_word().to_le_bytes());
    a[4..12].copy_from_slice(&rng1.next_u64_from_u32().to_le_bytes());
    a[12..].copy_from_slice(&rng1.next_word().to_le_bytes());

    let mut b = [0; 16];
    b[..4].copy_from_slice(&rng2.next_word().to_le_bytes());
    b[4..8].copy_from_slice(&rng2.next_word().to_le_bytes());
    b[8..].copy_from_slice(&rng2.next_u64_from_u32().to_le_bytes());
    assert_eq!(a, b);

    let mut c = [0; 16];
    c[..8].copy_from_slice(&rng3.next_u64_from_u32().to_le_bytes());
    c[8..12].copy_from_slice(&rng3.next_word().to_le_bytes());
    c[12..].copy_from_slice(&rng3.next_word().to_le_bytes());
    assert_eq!(a, c);
}

#[test]
fn blockrng_next_u64() {
    let mut rng = BlockRng::new(DummyRng::from_seed([1, 2, 3, 4]));
    let result_size = RESULTS_LEN;
    for _i in 0..result_size / 2 - 1 {
        rng.next_u64_from_u32();
    }
    rng.next_word();

    let _ = rng.next_u64_from_u32();
    assert_eq!(rng.word_offset(), 1);
}
