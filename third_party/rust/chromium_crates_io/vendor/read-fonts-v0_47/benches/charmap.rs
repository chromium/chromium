//! Compare scalar and batched Unicode mapping on real font fixtures.

use core::hint::black_box;
use criterion::{criterion_group, criterion_main, BenchmarkId, Criterion, Throughput};
use read_fonts::{model::Font, tables::cmap::CmapSubtable, types::GlyphId, FontRef, TableProvider};

fn bench_charmap(c: &mut Criterion) {
    for (font_name, data, format, base, span) in [
        (
            "noto_serif_display_cmap4",
            font_test_data::NOTO_SERIF_DISPLAY_TRIMMED,
            4,
            0x20_u32,
            0x400_u32,
        ),
        (
            "noto_color_emoji_cmap12",
            font_test_data::NOTO_COLOR_EMOJI_FLAGS,
            12,
            0x1f1e6,
            26,
        ),
    ] {
        let selected = FontRef::new(data)
            .unwrap()
            .cmap()
            .unwrap()
            .best_subtable()
            .unwrap()
            .2;
        assert!(matches!(
            (format, selected),
            (4, CmapSubtable::Format4(_)) | (12, CmapSubtable::Format12(_))
        ));

        let font = Font::new(data, 0).unwrap();
        let charmap = font.charmap();
        let mut group = c.benchmark_group(font_name);
        for size in [4096, 65536] {
            let ordered: Vec<u32> = (0..size).map(|i| base + i as u32 % span).collect();
            let mut mixed = ordered.clone();
            for i in 0..size {
                let j = ((i as u64 * 1_103_515_245 + 12_345) % size as u64) as usize;
                mixed.swap(i, j);
            }
            for (pattern, codepoints) in [("ordered", ordered), ("mixed", mixed)] {
                let expected: Vec<_> = codepoints
                    .iter()
                    .map(|&codepoint| charmap.map_unicode(codepoint).unwrap_or(GlyphId::NOTDEF))
                    .collect();
                let mut outputs = vec![GlyphId::NOTDEF; size];
                let mapped =
                    charmap.map_unicode_batched(codepoints.iter().copied().zip(outputs.iter_mut()));
                assert_eq!(outputs, expected);
                assert_eq!(
                    mapped,
                    expected
                        .iter()
                        .take_while(|&&glyph| glyph != GlyphId::NOTDEF)
                        .count()
                );

                group.throughput(Throughput::Elements(size as u64));
                let label = format!("{pattern}_{size}");
                group.bench_with_input(
                    BenchmarkId::new("scalar", &label),
                    &codepoints,
                    |b, codes| {
                        b.iter(|| {
                            for (&codepoint, out) in black_box(codes).iter().zip(outputs.iter_mut())
                            {
                                *out = charmap.map_unicode(codepoint).unwrap_or(GlyphId::NOTDEF);
                            }
                            black_box(&outputs);
                        });
                    },
                );
                group.bench_with_input(
                    BenchmarkId::new("batched", &label),
                    &codepoints,
                    |b, codes| {
                        b.iter(|| {
                            let mapped = charmap.map_unicode_batched(
                                black_box(codes).iter().copied().zip(outputs.iter_mut()),
                            );
                            black_box(mapped);
                            black_box(&outputs);
                        });
                    },
                );
            }
        }
        group.finish();
    }
}

criterion_group!(benches, bench_charmap);
criterion_main!(benches);
