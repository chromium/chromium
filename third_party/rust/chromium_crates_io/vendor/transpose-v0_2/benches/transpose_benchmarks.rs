#[macro_use]
extern crate criterion;

extern crate transpose;

use criterion::{Criterion, ParameterizedBenchmark, Throughput};
use std::mem;
use std::time::Duration;

fn bench_oop_transpose<T: Copy + Default>(c: &mut Criterion, tyname: &str) {
    let ref sizes = [(4, 4), (8, 8), (16, 16), (64, 64), (256, 256), (1024, 1024), (2048, 2048), (4096, 4096)];

    let bench = ParameterizedBenchmark::new(tyname,
        |b, &&(width, height)| {
            let mut buffer = vec![T::default(); width * height];
            let mut scratch = vec![T::default(); width * height];

            b.iter(|| { transpose::transpose(&mut buffer, &mut scratch, width, height); });
        },
        sizes)
        .throughput(|&&(width, height)| Throughput::Bytes((width * height * mem::size_of::<T>()) as u32))
        .warm_up_time(Duration::from_secs(1));

    c.bench("square transposes out-of-place", bench);
}

fn bench_oop_u32(c: &mut Criterion) { bench_oop_transpose::<u32>(c, "u32") }
fn bench_oop_u64(c: &mut Criterion) { bench_oop_transpose::<u64>(c, "u64") }

criterion_group!(out_of_place_benches, bench_oop_u32, bench_oop_u64);





fn bench_inplace_transpose<T: Copy + Default>(c: &mut Criterion, tyname: &str) {
    let ref sizes = [(4, 4), (8, 8), (16, 16), (64, 64), (256, 256), (1024, 1024)];

    let bench = ParameterizedBenchmark::new(tyname,
        |b, &&(width, height)| {
            let mut buffer = vec![T::default(); width * height];
            let mut scratch = vec![T::default(); std::cmp::max(width, height)];

            b.iter(|| { transpose::transpose_inplace(&mut buffer, &mut scratch, width, height); });
        },
        sizes)
        .throughput(|&&(width, height)| Throughput::Bytes((width * height * mem::size_of::<T>()) as u32))
        .warm_up_time(Duration::from_secs(1));

    c.bench("square transposes inplace", bench);
}

fn bench_inplace_u32(c: &mut Criterion) { bench_inplace_transpose::<u32>(c, "u32") }
fn bench_inplace_u64(c: &mut Criterion) { bench_inplace_transpose::<u64>(c, "u64") }

criterion_group!(inplace_benches, bench_inplace_u32, bench_inplace_u64);

criterion_main!(out_of_place_benches, inplace_benches);
