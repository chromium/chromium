// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use criterion::{black_box, criterion_group, criterion_main, Criterion};

use icu_segmenter::LineBreakOptions;
use icu_segmenter::LineBreakStrictness;
use icu_segmenter::LineBreakWordOption;
use icu_segmenter::LineSegmenter;

// Example is MIT license.
const TEST_STR_EN: &str = "Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.";
const TEST_STR_TH: &str =
    "ภาษาไทยภาษาไทย ภาษาไทยภาษาไทย ภาษาไทยภาษาไทย ภาษาไทยภาษาไทย ภาษาไทยภาษาไทย ภาษาไทยภาษาไทย";

fn line_break_iter_latin1(c: &mut Criterion) {
    let mut group = c.benchmark_group("Line Break/Latin1");

    let segmenter = LineSegmenter::new_dictionary();

    let mut options = LineBreakOptions::default();
    options.strictness = LineBreakStrictness::Anywhere;
    options.word_option = LineBreakWordOption::BreakAll;
    let segmenter_css = LineSegmenter::new_dictionary_with_options(options);

    group.bench_function("En", |b| {
        b.iter(|| {
            black_box(&segmenter)
                .segment_latin1(black_box(TEST_STR_EN).as_bytes())
                .count()
        })
    });

    group.bench_function("En CSS", |b| {
        b.iter(|| {
            black_box(&segmenter_css)
                .segment_latin1(black_box(TEST_STR_EN).as_bytes())
                .count()
        })
    });
}

fn line_break_iter_utf8(c: &mut Criterion) {
    let mut group = c.benchmark_group("Line Break/UTF8");

    let segmenter_auto = LineSegmenter::new_auto();
    let segmenter_lstm = LineSegmenter::new_lstm();
    let segmenter_dictionary = LineSegmenter::new_dictionary();

    let mut options = LineBreakOptions::default();
    options.strictness = LineBreakStrictness::Anywhere;
    options.word_option = LineBreakWordOption::BreakAll;
    let segmenter_css_dictionary = LineSegmenter::new_dictionary_with_options(options);

    // No need to test "auto", "lstm", or "dictionary" constructor variants since English uses only
    // UAX14 rules for line breaking.
    group.bench_function("En", |b| {
        b.iter(|| {
            black_box(&segmenter_dictionary)
                .segment_str(black_box(TEST_STR_EN))
                .count()
        })
    });

    group.bench_function("En CSS", |b| {
        b.iter(|| {
            black_box(&segmenter_css_dictionary)
                .segment_str(black_box(TEST_STR_EN))
                .count()
        })
    });

    let segmenters = [
        (&segmenter_auto, "auto"),
        (&segmenter_lstm, "lstm"),
        (&segmenter_dictionary, "dictionary"),
    ];
    for (segmenter, variant) in segmenters {
        group.bench_function("Th/".to_string() + variant, |b| {
            b.iter(|| {
                black_box(&segmenter)
                    .segment_str(black_box(TEST_STR_TH))
                    .count()
            })
        });
    }
}

fn line_break_iter_utf16(c: &mut Criterion) {
    let mut group = c.benchmark_group("Line Break/UTF16");

    let utf16_en: Vec<u16> = TEST_STR_EN.encode_utf16().collect();
    let utf16_th: Vec<u16> = TEST_STR_TH.encode_utf16().collect();

    let segmenter_auto = LineSegmenter::new_auto();
    let segmenter_lstm = LineSegmenter::new_lstm();
    let segmenter_dictionary = LineSegmenter::new_dictionary();

    let mut options = LineBreakOptions::default();
    options.strictness = LineBreakStrictness::Anywhere;
    options.word_option = LineBreakWordOption::BreakAll;
    let segmenter_css_dictionary = LineSegmenter::new_dictionary_with_options(options);

    // No need to test "auto", "lstm", or "dictionary" constructor variants since English uses only
    // UAX14 rules for line breaking.
    group.bench_function("En", |b| {
        b.iter(|| {
            black_box(&segmenter_dictionary)
                .segment_utf16(black_box(&utf16_en))
                .count()
        })
    });

    group.bench_function("En CSS", |b| {
        b.iter(|| {
            black_box(&segmenter_css_dictionary)
                .segment_utf16(black_box(&utf16_en))
                .count()
        })
    });

    let segmenters = [
        (&segmenter_auto, "auto"),
        (&segmenter_lstm, "lstm"),
        (&segmenter_dictionary, "dictionary"),
    ];
    for (segmenter, variant) in segmenters {
        group.bench_function("Th/".to_string() + variant, |b| {
            b.iter(|| {
                black_box(&segmenter)
                    .segment_utf16(black_box(&utf16_th))
                    .count()
            })
        });
    }
}

criterion_group!(
    benches,
    line_break_iter_latin1,
    line_break_iter_utf8,
    line_break_iter_utf16
);
criterion_main!(benches);
