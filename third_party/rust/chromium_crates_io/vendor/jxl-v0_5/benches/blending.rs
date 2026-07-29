// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use criterion::{BatchSize, BenchmarkId, Criterion, Throughput, criterion_group, criterion_main};
use jxl::features::blending::perform_blending;
use jxl::features::patches::{PatchBlendMode, PatchBlending};
use jxl::headers::bit_depth::BitDepth;
use jxl::headers::extra_channels::{ExtraChannel, ExtraChannelInfo};

/// Deterministic values in (0.05, 0.95): a valid alpha range with no subnormals.
fn fill(seed: u64, buf: &mut [f32]) {
    let mut state = seed.wrapping_mul(0x9E3779B97F4A7C15) | 1;
    for v in buf.iter_mut() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        *v = 0.05 + 0.9 * ((state >> 40) as f32 / (1u64 << 24) as f32);
    }
}

fn blending_benches(c: &mut Criterion) {
    let mut group = c.benchmark_group("perform_blending");

    // Every case blends 3 color channels and one alpha extra channel, with the
    // same blend mode for both. `replace` covers the copy fast path.
    let cases = [
        ("blend_below_straight", PatchBlendMode::BlendBelow, false),
        ("blend_below_premul", PatchBlendMode::BlendBelow, true),
        ("blend_above_straight", PatchBlendMode::BlendAbove, false),
        ("blend_above_premul", PatchBlendMode::BlendAbove, true),
        (
            "alpha_weighted_add_below",
            PatchBlendMode::AlphaWeightedAddBelow,
            false,
        ),
        ("add", PatchBlendMode::Add, false),
        ("mul", PatchBlendMode::Mul, false),
        ("replace", PatchBlendMode::Replace, false),
    ];

    for (label, mode, alpha_associated) in cases {
        for xsize in [256usize, 1024, 4096] {
            let extra_channel_info = [ExtraChannelInfo::new(
                false,
                ExtraChannel::Alpha,
                BitDepth::integer_samples(8),
                0,
                "alpha".to_string(),
                alpha_associated,
                None,
                None,
            )];
            let num_channels = 3 + extra_channel_info.len();

            let blending = PatchBlending {
                mode,
                alpha_channel: 0,
                clamp: false,
            };

            let mut bg_data = vec![0f32; num_channels * xsize];
            let mut fg_data = vec![0f32; num_channels * xsize];
            fill(1, &mut bg_data);
            fill(2, &mut fg_data);
            let fg: Vec<&[f32]> = fg_data.chunks_exact(xsize).collect();
            let mut tmp = Vec::new();

            group.throughput(Throughput::Elements((num_channels * xsize) as u64));
            group.bench_function(BenchmarkId::new(label, xsize), |b| {
                b.iter_batched_ref(
                    || bg_data.clone(),
                    |bg_data| {
                        let mut bg: Vec<&mut [f32]> = bg_data.chunks_exact_mut(xsize).collect();
                        perform_blending(
                            &mut bg,
                            &fg,
                            &blending,
                            std::slice::from_ref(&blending),
                            &extra_channel_info,
                            &mut tmp,
                        );
                    },
                    BatchSize::SmallInput,
                )
            });
        }
    }

    group.finish();
}

criterion_group!(blending, blending_benches);
criterion_main!(blending);
