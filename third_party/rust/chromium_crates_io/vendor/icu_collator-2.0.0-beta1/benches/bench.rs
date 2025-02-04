// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use criterion::{black_box, criterion_group, criterion_main, BatchSize, BenchmarkId, Criterion};

use icu::collator::*;
use icu::locale::locale;

pub fn collator_with_locale(criterion: &mut Criterion) {
    // Load file content in reverse order vector.
    let content_latin: (&str, Vec<&str>) = (
        "TestNames_Latin",
        include_str!("data/TestNames_Latin.txt")
            .lines()
            .filter(|&s| !s.starts_with('#'))
            .rev()
            .collect::<Vec<&str>>(),
    );
    let content_asian: (&str, Vec<&str>) = (
        "TestNames_Asian",
        include_str!("data/TestNames_Asian.txt")
            .lines()
            .filter(|&s| !s.starts_with('#'))
            .rev()
            .collect(),
    );
    let content_russian: (&str, Vec<&str>) = (
        "TestNames_Russian",
        include_str!("data/TestNames_Russian.txt")
            .lines()
            .filter(|&s| !s.starts_with('#'))
            .rev()
            .collect(),
    );
    let content_chinese: (&str, Vec<&str>) = (
        "TestNames_Chinese",
        include_str!("data/TestNames_Chinese.txt")
            .lines()
            .filter(|&s| !s.starts_with('#'))
            .rev()
            .collect(),
    );
    let content_jp_h: (&str, Vec<&str>) = (
        "TestNames_Japanese_h",
        include_str!("data/TestNames_Japanese_h.txt")
            .lines()
            .filter(|&s| !s.starts_with('#'))
            .rev()
            .collect::<Vec<&str>>(),
    );
    let content_jp_k: (&str, Vec<&str>) = (
        "TestNames_Japanese_k",
        include_str!("data/TestNames_Japanese_k.txt")
            .lines()
            .filter(|&s| !s.starts_with('#'))
            .rev()
            .collect::<Vec<&str>>(),
    );
    let content_korean: (&str, Vec<&str>) = (
        "TestNames_Korean",
        include_str!("data/TestNames_Korean.txt")
            .lines()
            .filter(|&s| !s.starts_with('#'))
            .rev()
            .collect::<Vec<&str>>(),
    );
    let content_thai: (&str, Vec<&str>) = (
        "TestNames_Thai",
        include_str!("data/TestNames_Thai.txt")
            .lines()
            .filter(|&s| !s.starts_with('#'))
            .rev()
            .collect::<Vec<&str>>(),
    );

    // hsivonen@ : All five strengths are benched.
    // The default is tertiary, so it makes sense to bench that.
    //
    // Also, it's particularly interesting to bench quaternary
    // and identical with Japanese due to the performance remarks ICU4C docs make about Japanese.
    //
    // In particular, to get full Japanese standard behavior, you need the identical strength.
    // Furthermore, CLDR used to default to quaternary for Japanese but now defaults to tertiary
    // as for every other language for performance reasons.
    let all_strength = [
        Strength::Primary,
        Strength::Secondary,
        Strength::Tertiary,
        Strength::Quaternary,
        Strength::Identical,
    ];
    let performance_parameters = [
        (locale!("en-US"), vec![&content_latin], &all_strength),
        (locale!("da-DK"), vec![&content_latin], &all_strength),
        (locale!("fr-CA"), vec![&content_latin], &all_strength),
        (
            locale!("ja-JP"),
            vec![&content_latin, &content_jp_h, &content_jp_k, &content_asian],
            &all_strength,
        ),
        (
            locale!("zh-u-co-pinyin"),
            vec![&content_latin, &content_chinese],
            &all_strength,
        ), // zh_CN
        (
            locale!("zh-u-co-stroke"),
            vec![&content_latin, &content_chinese],
            &all_strength,
        ), // zh_TW
        (
            locale!("ru-RU"),
            vec![&content_latin, &content_russian],
            &all_strength,
        ),
        (
            locale!("th"),
            vec![&content_latin, &content_thai],
            &all_strength,
        ),
        (
            locale!("ko-KR"),
            vec![&content_latin, &content_korean],
            &all_strength,
        ),
    ];
    for perf_parameter in performance_parameters {
        let (locale_under_bench, files_under_bench, benched_strength) = perf_parameter;

        let mut group = criterion.benchmark_group(locale_under_bench.to_string());

        for content_under_bench in files_under_bench {
            let (file_name, elements) = black_box(content_under_bench);
            // baseline performance, locale-unaware code point sort done by Rust (0 for ordering in the html report)
            group.bench_function(
                BenchmarkId::new(format!("{}/0_rust_sort", file_name), "default"),
                |bencher| {
                    bencher.iter_batched(
                        || elements.clone(),
                        |mut lines| lines.sort_unstable(),
                        BatchSize::SmallInput,
                    )
                },
            );

            // index to keep order of strength in the html report
            for (index, strength) in benched_strength.iter().enumerate() {
                let mut options = CollatorOptions::default();
                options.strength = Some(*strength);
                let collator =
                    Collator::try_new(CollatorPreferences::from(&locale_under_bench), options)
                        .unwrap();
                // ICU4X collator performance, sort is locale-aware
                group.bench_function(
                    BenchmarkId::new(
                        format!("{}/1_icu4x_sort", file_name),
                        format!("{}_{:?}", index, strength),
                    ),
                    |bencher| {
                        bencher.iter_batched(
                            || elements.clone(),
                            |mut lines| {
                                lines.sort_unstable_by(|left, right| collator.compare(left, right))
                            },
                            BatchSize::SmallInput,
                        )
                    },
                );
            }
        }
        group.finish();
    }
}

criterion_group!(
    name = benches;
    config = Criterion::default();
    targets = collator_with_locale
);

criterion_main!(benches);
