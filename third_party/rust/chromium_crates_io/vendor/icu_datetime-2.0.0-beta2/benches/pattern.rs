// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod fixtures;

use criterion::{criterion_group, criterion_main, Criterion};

use icu_datetime::provider::pattern::reference::Pattern;

fn pattern_benches(c: &mut Criterion) {
    let patterns = serde_json::from_str::<fixtures::PatternsFixture>(include_str!(
        "fixtures/tests/patterns.json"
    ))
    .unwrap()
    .0;

    {
        let mut group = c.benchmark_group("pattern");

        group.bench_function("parse", |b| {
            b.iter(|| {
                for input in &patterns {
                    let _ = input.parse::<Pattern>().unwrap();
                }
            })
        });

        group.finish();
    }
}

criterion_group!(benches, pattern_benches,);
criterion_main!(benches);
