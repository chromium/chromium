//! Type1 benchmarks.

use core::hint::black_box;
use criterion::{criterion_group, criterion_main, Criterion};
use read_fonts::ps::type1::Type1Font;

/// Basic parsing benchmark for Type1 fonts.
pub fn parse(c: &mut Criterion) {
    let fonts = [
        ("pfa", font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA),
        ("pfb", font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB),
    ];
    for (kind, font) in fonts {
        let bench_name = format!("type1_parse/{}", kind);
        c.bench_function(&bench_name, |b| {
            b.iter(|| {
                let _ = black_box(Type1Font::new(font).unwrap());
            });
        });
    }
}

criterion_group!(benches, parse);
criterion_main!(benches);
