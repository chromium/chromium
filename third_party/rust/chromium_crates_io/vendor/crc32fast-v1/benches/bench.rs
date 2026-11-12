use std::hint::black_box;

use crc32fast::Hasher;
use criterion::{criterion_group, criterion_main, BenchmarkId, Criterion, Throughput};
use rand::{rngs::StdRng, RngExt, SeedableRng};

/// Benchmark inputs are drawn from a fixed seed so that runs are comparable against a stored
/// baseline (`--save-baseline` / `--baseline`). With a per-run seed, `combine` in particular
/// reports large spurious changes, because its timing depends on the random lengths it is given.
const SEED: u64 = 0x_c0ff_ee00_1dea;

const BASELINE_SIZES: &[usize] = &[1024, 16 * 1024, 1024 * 1024];

// Small and non-16-multiple ("awkward") sizes: these exercise the small-input clmul path and the
// partial-block tail fold, and are where the pre-clmul table/scalar fallback was slowest.
const SPECIALIZED_SIZES: &[usize] = &[
    16,
    63,
    127,
    255,
    511,
    1000,
    1023,
    1024,
    2047,
    2048,
    2175,
    2176,
    2303,
    2304,
    16 * 1024,
    1024 * 1024,
];

/// Benchmark `update` over a set of input sizes.
///
/// `hasher_init` is built once by the caller and cloned per iteration: constructing a specialized
/// `Hasher` runs CPU feature detection, which we don't want to measure.
fn bench_update(c: &mut Criterion, group_name: &str, sizes: &[usize], hasher_init: Hasher) {
    let mut group = c.benchmark_group(group_name);
    let mut rng = StdRng::seed_from_u64(SEED);

    for &size in sizes {
        let mut bytes = vec![0u8; size];
        rng.fill(&mut bytes[..]);

        group.throughput(Throughput::Bytes(size as u64));
        group.bench_with_input(BenchmarkId::from_parameter(size), &bytes, |b, bytes| {
            b.iter(|| {
                let mut hasher = hasher_init.clone();
                hasher.update(bytes);
                black_box(hasher.finalize())
            })
        });
    }

    group.finish();
}

fn baseline(c: &mut Criterion) {
    bench_update(
        c,
        "baseline",
        BASELINE_SIZES,
        Hasher::internal_new_baseline(0, 0),
    );
}

fn specialized(c: &mut Criterion) {
    let Some(hasher) = Hasher::internal_new_specialized(0, 0) else {
        eprintln!("skipping specialized benches: no SIMD implementation for this target");
        return;
    };

    bench_update(c, "specialized", SPECIALIZED_SIZES, hasher);
}

fn combine(c: &mut Criterion) {
    let mut group = c.benchmark_group("combine");
    let mut rng = StdRng::seed_from_u64(SEED);

    // Parameterized by the bit width of the second hasher's length, which drives how much work
    // `combine` does.
    for bits in [16u32, 32, 64] {
        let (i1, l1, i2): (u32, u64, u32) = rng.random();
        let l2: u64 = match bits {
            16 => u64::from(rng.random::<u16>()),
            32 => u64::from(rng.random::<u32>()),
            _ => rng.random::<u64>(),
        };

        let h1 = Hasher::new_with_initial_len(i1, l1);
        let h2 = Hasher::new_with_initial_len(i2, l2);

        group.bench_function(BenchmarkId::from_parameter(bits), |b| {
            b.iter(|| {
                let mut h = h1.clone();
                h.combine(&h2);
                black_box(h);
            })
        });
    }

    group.finish();
}

criterion_group!(benches, baseline, specialized, combine);
criterion_main!(benches);
