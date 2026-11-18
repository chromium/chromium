//! Benchmark that compares naive iteration over glyph names with optimized
//! iteration using `Post::glyph_names`.

use core::hint::black_box;
use criterion::{criterion_group, criterion_main, Criterion};
use font_test_data::post::GlyphNameOrder;
use read_fonts::{tables::post::Post, FontRead};

const NUM_GLYPHS: u16 = 8_000;
const BASE_NAME_LEN: u8 = 20;

fn mode_name(mode: GlyphNameOrder) -> &'static str {
    match mode {
        GlyphNameOrder::Monotonic => "monotonic",
        GlyphNameOrder::MostlyMonotonicWithBackrefs => "mostly_monotonic_with_backrefs",
        GlyphNameOrder::AllPointToLast => "all_point_to_last",
    }
}

/// Iterate glyph names by looping over indices and calling `Post::glyph_name`.
pub fn glyph_names_iter_naive(c: &mut Criterion) {
    let modes = [
        GlyphNameOrder::Monotonic,
        GlyphNameOrder::MostlyMonotonicWithBackrefs,
        GlyphNameOrder::AllPointToLast,
    ];
    for mode in modes {
        let post_data =
            font_test_data::post::v2_with_varied_glyph_names(NUM_GLYPHS, BASE_NAME_LEN, mode).0;
        let post = Post::read(post_data.as_slice().into()).unwrap();
        let bench_name = format!("glyph_names_iter_naive_synth_post_v2/{}", mode_name(mode));
        c.bench_function(&bench_name, |b| {
            b.iter(|| {
                let total_len: usize = (0..post.num_glyphs().unwrap())
                    .filter_map(|gid| post.glyph_name(gid.into()))
                    .map(str::len)
                    .sum();
                black_box(total_len)
            });
        });
    }
}

/// Use optimized `Post::glyph_names` iterator to iterate glyph names.
pub fn glyph_names_iter(c: &mut Criterion) {
    let modes = [
        GlyphNameOrder::Monotonic,
        GlyphNameOrder::MostlyMonotonicWithBackrefs,
        GlyphNameOrder::AllPointToLast,
    ];
    for mode in modes {
        let post_data =
            font_test_data::post::v2_with_varied_glyph_names(NUM_GLYPHS, BASE_NAME_LEN, mode).0;
        let post = Post::read(post_data.as_slice().into()).unwrap();
        let bench_name = format!("glyph_names_iter_cached_synth_post_v2/{}", mode_name(mode));
        c.bench_function(&bench_name, |b| {
            b.iter(|| {
                let total_len: usize = post.glyph_names().map(|(_, name)| name.len()).sum();
                black_box(total_len)
            });
        });
    }
}

criterion_group!(benches, glyph_names_iter_naive, glyph_names_iter);
criterion_main!(benches);
