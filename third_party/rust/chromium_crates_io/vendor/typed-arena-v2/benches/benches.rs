#[macro_use]
extern crate criterion;
extern crate typed_arena;

use criterion::{BenchmarkId, Criterion};

#[derive(Default)]
struct Small(usize);

#[derive(Default)]
struct Big([usize; 32]);

fn allocate<T: Default>(n: usize) {
    let arena = typed_arena::Arena::new();
    for _ in 0..n {
        let val: &mut T = arena.alloc(Default::default());
        criterion::black_box(val);
    }
}

fn criterion_benchmark(c: &mut Criterion) {
    let mut group = c.benchmark_group("allocate");
    for n in 1..5 {
        let n = n * 1000;
        group.throughput(criterion::Throughput::Elements(n as u64));
        group.bench_with_input(BenchmarkId::new("allocate-small", n), &n, |b, &n| {
            b.iter(|| allocate::<Small>(n))
        });
        group.bench_with_input(BenchmarkId::new("allocate-big", n), &n, |b, &n| {
            b.iter(|| allocate::<Big>(n))
        });
    }
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);
