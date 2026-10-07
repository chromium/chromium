use criterion::{BatchSize, Criterion, criterion_group, criterion_main};
use std::hint::black_box;
use wide::{f32x4, f32x8, f64x2, f64x4, f64x8};

// Benchmark chains for testing FMA throughput
fn bench_f32x4_mul_add_chain(c: &mut Criterion) {
  c.bench_function("f32x4_mul_add_chain", |b| {
    b.iter_batched(
      || {
        (
          f32x4::from([1.0, 2.0, 3.0, 4.0]),
          f32x4::from([1.1, 1.2, 1.3, 1.4]),
          f32x4::from([0.1, 0.2, 0.3, 0.4]),
        )
      },
      |(mut acc, mul, add)| {
        // Chain of FMA operations to test throughput
        for _ in 0..100 {
          acc = acc.mul_add(mul, add);
        }
        black_box(acc)
      },
      BatchSize::SmallInput,
    );
  });
}

fn bench_f32x8_mul_add_chain(c: &mut Criterion) {
  c.bench_function("f32x8_mul_add_chain", |b| {
    b.iter_batched(
      || {
        (
          f32x8::from([1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]),
          f32x8::from([1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8]),
          f32x8::from([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8]),
        )
      },
      |(mut acc, mul, add)| {
        for _ in 0..100 {
          acc = acc.mul_add(mul, add);
        }
        black_box(acc)
      },
      BatchSize::SmallInput,
    );
  });
}

fn bench_f64x2_mul_add_chain(c: &mut Criterion) {
  c.bench_function("f64x2_mul_add_chain", |b| {
    b.iter_batched(
      || {
        (
          f64x2::from([1.0, 2.0]),
          f64x2::from([1.1, 1.2]),
          f64x2::from([0.1, 0.2]),
        )
      },
      |(mut acc, mul, add)| {
        for _ in 0..100 {
          acc = acc.mul_add(mul, add);
        }
        black_box(acc)
      },
      BatchSize::SmallInput,
    );
  });
}

fn bench_f64x4_mul_add_chain(c: &mut Criterion) {
  c.bench_function("f64x4_mul_add_chain", |b| {
    b.iter_batched(
      || {
        (
          f64x4::from([1.0, 2.0, 3.0, 4.0]),
          f64x4::from([1.1, 1.2, 1.3, 1.4]),
          f64x4::from([0.1, 0.2, 0.3, 0.4]),
        )
      },
      |(mut acc, mul, add)| {
        for _ in 0..100 {
          acc = acc.mul_add(mul, add);
        }
        black_box(acc)
      },
      BatchSize::SmallInput,
    );
  });
}

fn bench_f64x8_mul_add_chain(c: &mut Criterion) {
  c.bench_function("f64x8_mul_add_chain", |b| {
    b.iter_batched(
      || {
        (
          f64x8::from([1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]),
          f64x8::from([1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8]),
          f64x8::from([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8]),
        )
      },
      |(mut acc, mul, add)| {
        for _ in 0..100 {
          acc = acc.mul_add(mul, add);
        }
        black_box(acc)
      },
      BatchSize::SmallInput,
    );
  });
}

criterion_group!(
  benches,
  bench_f32x4_mul_add_chain,
  bench_f32x8_mul_add_chain,
  bench_f64x2_mul_add_chain,
  bench_f64x4_mul_add_chain,
  bench_f64x8_mul_add_chain,
);
criterion_main!(benches);
