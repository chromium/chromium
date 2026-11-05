//! Compare Unicode mapping on font files named by CHARMAP_BENCH_FONTS.
//!
//! Set the environment variable to a semicolon-separated list of paths.

use core::hint::black_box;
use criterion::{criterion_group, criterion_main, Criterion, Throughput};
use read_fonts::{model::Font, tables::cmap::CmapSubtable, types::GlyphId, FontRef, TableProvider};
use std::{env, fs, path::Path};

const BATCH_SIZE: usize = 65_536;

fn bench_system_fonts(c: &mut Criterion) {
    let Ok(paths) = env::var("CHARMAP_BENCH_FONTS") else {
        eprintln!("Set CHARMAP_BENCH_FONTS to benchmark local font files");
        return;
    };
    for path in paths.split(';').filter(|path| !path.is_empty()) {
        let path = Path::new(path);
        let data = fs::read(path).expect("read font file");
        let format = {
            let font = FontRef::new(&data).expect("parse font file");
            let subtable = font
                .cmap()
                .expect("read cmap")
                .best_subtable()
                .expect("select Unicode cmap")
                .2;
            match subtable {
                CmapSubtable::Format4(_) => 4,
                CmapSubtable::Format12(_) => 12,
                _ => continue,
            }
        };
        let font = Font::new(data, 0).expect("parse font model");
        let charmap = font.charmap();
        let covered: Vec<_> = charmap.unicodes().map(|(codepoint, _)| codepoint).collect();
        if covered.is_empty() {
            continue;
        }
        let mut codepoints: Vec<_> = (0..BATCH_SIZE)
            .map(|i| {
                if i % 8 == 7 {
                    0x10ffff - (i as u32 % 4096)
                } else {
                    covered[i % covered.len()]
                }
            })
            .collect();
        for i in 0..BATCH_SIZE {
            let j = ((i as u64 * 1_103_515_245 + 12_345) % BATCH_SIZE as u64) as usize;
            codepoints.swap(i, j);
        }

        let expected: Vec<_> = codepoints
            .iter()
            .map(|&codepoint| charmap.map_unicode(codepoint).unwrap_or(GlyphId::NOTDEF))
            .collect();
        let mut outputs = vec![GlyphId::NOTDEF; BATCH_SIZE];
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

        let filename = path.file_name().unwrap().to_string_lossy();
        let name = format!("cmap{format}_{filename}");
        eprintln!(
            "{name}: {} covered codepoints, {} mapped inputs",
            covered.len(),
            expected
                .iter()
                .filter(|&&glyph| glyph != GlyphId::NOTDEF)
                .count()
        );
        let mut group = c.benchmark_group(name);
        group.throughput(Throughput::Elements(BATCH_SIZE as u64));
        group.bench_function("scalar", |b| {
            b.iter(|| {
                let mut mapped = 0;
                let mut initial = true;
                for (&codepoint, out) in black_box(&codepoints).iter().zip(outputs.iter_mut()) {
                    *out = charmap.map_unicode(codepoint).unwrap_or(GlyphId::NOTDEF);
                    initial &= *out != GlyphId::NOTDEF;
                    mapped += usize::from(initial);
                }
                black_box(mapped);
                black_box(&outputs);
            });
        });
        group.bench_function("batched", |b| {
            b.iter(|| {
                let mapped = charmap.map_unicode_batched(
                    black_box(&codepoints)
                        .iter()
                        .copied()
                        .zip(outputs.iter_mut()),
                );
                black_box(mapped);
                black_box(&outputs);
            });
        });
        group.finish();
    }
}

criterion_group!(benches, bench_system_fonts);
criterion_main!(benches);
