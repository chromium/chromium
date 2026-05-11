use criterion::{criterion_group, criterion_main, Criterion};
use sdd::Guard;

fn guard_single(c: &mut Criterion) {
    c.bench_function("EBR: guard", |b| {
        b.iter(|| {
            let _guard = Guard::new();
        })
    });
}

fn guard_superposed(c: &mut Criterion) {
    let _guard = Guard::new();
    c.bench_function("EBR: superposed guard", |b| {
        b.iter(|| {
            let _guard = Guard::new();
        })
    });
}

criterion_group!(ebr, guard_single, guard_superposed);
criterion_main!(ebr);
