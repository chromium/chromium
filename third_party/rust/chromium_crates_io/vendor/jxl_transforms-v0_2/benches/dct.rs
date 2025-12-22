// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::identity_op)]

use criterion::measurement::Measurement;
use criterion::{criterion_group, criterion_main, BenchmarkGroup, BenchmarkId, Criterion};
use jxl_simd::{bench_all_instruction_sets, SimdDescriptor};
use jxl_transforms::transform_map::MAX_COEFF_AREA;
use jxl_transforms::*;

fn bench_idct2d<D: SimdDescriptor>(d: D, c: &mut BenchmarkGroup<'_, impl Measurement>, name: &str) {
    let mut data = vec![1.0; MAX_COEFF_AREA];

    macro_rules! run {
        ($fun: ident, $name: literal, $sz: expr) => {
            let id = BenchmarkId::new(name, format_args!("{}", $name));
            c.bench_function(id, |b| {
                b.iter(|| {
                    d.call(
                        #[inline(always)]
                        |d| $fun(d, &mut data[..$sz]),
                    );
                })
            });
        };
    }

    run!(idct2d_2_2, "2x2", 2 * 2);
    run!(idct2d_4_4, "4x4", 4 * 4);
    run!(idct2d_4_8, "4x8", 4 * 8);
    run!(idct2d_8_4, "8x4", 4 * 8);
    run!(idct2d_8_8, "8x8", 8 * 8);
    run!(idct2d_16_8, "16x8", 16 * 8);
    run!(idct2d_8_16, "8x16", 8 * 16);
    run!(idct2d_16_16, "16x16", 16 * 16);
    run!(idct2d_32_8, "32x8", 32 * 8);
    run!(idct2d_8_32, "8x32", 8 * 32);
    run!(idct2d_32_16, "32x16", 32 * 16);
    run!(idct2d_16_32, "16x32", 16 * 32);
    run!(idct2d_32_32, "32x32", 32 * 32);
    run!(idct2d_64_32, "64x32", 64 * 32);
    run!(idct2d_32_64, "32x64", 32 * 64);
    run!(idct2d_64_64, "64x64", 64 * 64);
    run!(idct2d_128_64, "128x64", 128 * 64);
    run!(idct2d_64_128, "64x128", 64 * 128);
    run!(idct2d_128_128, "128x128", 128 * 128);
    run!(idct2d_256_128, "256x128", 256 * 128);
    run!(idct2d_128_256, "128x256", 128 * 256);
    run!(idct2d_256_256, "256x256", 256 * 256);
}

fn bench_reinterpreting_dct<D: SimdDescriptor>(
    d: D,
    c: &mut BenchmarkGroup<'_, impl Measurement>,
    name: &str,
) {
    let mut data = vec![1.0; MAX_COEFF_AREA];
    let mut output = vec![0.0; MAX_COEFF_AREA];

    macro_rules! run {
        ($fun: ident, $name: literal, $sz: expr) => {
            let id = BenchmarkId::new(name, format_args!("{}", $name));

            c.bench_function(id, |b| {
                b.iter(|| {
                    d.call(
                        #[inline(always)]
                        |d| $fun(d, &mut data[..$sz], &mut output),
                    )
                })
            });
        };
    }

    run!(reinterpreting_dct2d_1_2, "1x2", 1 * 2);
    run!(reinterpreting_dct2d_2_1, "2x1", 2 * 1);
    run!(reinterpreting_dct2d_2_2, "2x2", 2 * 2);
    run!(reinterpreting_dct2d_1_4, "1x4", 1 * 4);
    run!(reinterpreting_dct2d_4_1, "4x1", 4 * 1);
    run!(reinterpreting_dct2d_2_4, "2x4", 2 * 4);
    run!(reinterpreting_dct2d_4_2, "4x2", 4 * 2);
    run!(reinterpreting_dct2d_4_4, "4x4", 4 * 4);
    run!(reinterpreting_dct2d_8_4, "8x4", 8 * 4);
    run!(reinterpreting_dct2d_4_8, "4x8", 4 * 8);
    run!(reinterpreting_dct2d_8_8, "8x8", 8 * 8);
    run!(reinterpreting_dct2d_8_16, "8x16", 8 * 16);
    run!(reinterpreting_dct2d_16_8, "16x8", 16 * 8);
    run!(reinterpreting_dct2d_16_16, "16x16", 16 * 16);
    run!(reinterpreting_dct2d_32_16, "32x16", 32 * 16);
    run!(reinterpreting_dct2d_16_32, "16x32", 16 * 32);
    run!(reinterpreting_dct2d_32_32, "32x32", 32 * 32);
}

fn idct_benches(c: &mut Criterion) {
    let mut group = c.benchmark_group("idct2d");
    let g = &mut group;

    bench_all_instruction_sets!(bench_idct2d, g);

    group.finish();
}

fn reinterpreting_dct_benches(c: &mut Criterion) {
    let mut group = c.benchmark_group("reinterpreting_dct");
    let g = &mut group;

    bench_all_instruction_sets!(bench_reinterpreting_dct, g);

    group.finish();
}

criterion_group!(
    name = benches;
    config = Criterion::default().sample_size(50);
    targets = idct_benches, reinterpreting_dct_benches
);
criterion_main!(benches);
