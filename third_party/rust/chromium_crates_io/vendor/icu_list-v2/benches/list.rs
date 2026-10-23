// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use criterion::{Criterion, black_box, criterion_group, criterion_main};
use icu_list::{
    ListFormatter,
    options::{ListFormatterOptions, ListLength},
};
use icu_locale_core::locale;
use writeable::Writeable;

fn list_benches(c: &mut Criterion) {
    let mut group = c.benchmark_group("list");

    let locales = [locale!("en"), locale!("es"), locale!("th")];

    // Benchmark construction
    group.bench_function("construct/and/wide", |b| {
        b.iter(|| {
            for locale in black_box(&locales) {
                let _ = ListFormatter::try_new_and(
                    locale.into(),
                    ListFormatterOptions::default().with_length(black_box(ListLength::Wide)),
                );
            }
        })
    });

    group.bench_function("construct/or/short", |b| {
        b.iter(|| {
            for locale in black_box(&locales) {
                let _ = ListFormatter::try_new_or(
                    locale.into(),
                    ListFormatterOptions::default().with_length(black_box(ListLength::Short)),
                );
            }
        })
    });

    group.bench_function("construct/unit/narrow", |b| {
        b.iter(|| {
            for locale in black_box(&locales) {
                let _ = ListFormatter::try_new_unit(
                    locale.into(),
                    ListFormatterOptions::default().with_length(black_box(ListLength::Narrow)),
                );
            }
        })
    });

    // Benchmark formatting
    fn generate_list(n: usize) -> Vec<String> {
        (0..n).map(|i| format!("Item {}", i)).collect()
    }

    let list_2 = generate_list(2);
    let list_5 = generate_list(5);
    let list_1000 = generate_list(1000);

    let locale = locale!("en");
    let mut result = String::with_capacity(10000);

    let formatter_and = ListFormatter::try_new_and(
        locale.clone().into(),
        ListFormatterOptions::default().with_length(ListLength::Wide),
    )
    .unwrap();

    let formatter_or = ListFormatter::try_new_or(
        locale.clone().into(),
        ListFormatterOptions::default().with_length(ListLength::Wide),
    )
    .unwrap();

    let formatter_unit = ListFormatter::try_new_unit(
        locale.clone().into(),
        ListFormatterOptions::default().with_length(ListLength::Wide),
    )
    .unwrap();

    let styles = [
        ("and", &formatter_and),
        ("or", &formatter_or),
        ("unit", &formatter_unit),
    ];

    let lists = [
        ("2_items", &list_2),
        ("5_items", &list_5),
        ("1000_items", &list_1000),
    ];

    for (style_str, formatter) in &styles {
        for (size_str, list) in &lists {
            group.bench_function(format!("format/{style_str}/wide/{size_str}"), |b| {
                b.iter(|| {
                    result.clear();
                    let _ = black_box(formatter)
                        .format(black_box(list).iter())
                        .write_to(&mut result);
                })
            });
        }
    }

    for (size_str, list) in &lists {
        group.bench_function(format!("format/baseline/{size_str}"), |b| {
            b.iter(|| {
                result.clear();
                for item in black_box(list).iter() {
                    result.push_str(item);
                }
            })
        });
    }

    // Adversarial Spanish Benchmarks (Testing "or" pattern)
    let formatter_or = ListFormatter::try_new_or(
        locale!("es").into(),
        ListFormatterOptions::default().with_length(ListLength::Wide),
    )
    .unwrap();

    let list_match_word = vec!["casa".to_string(), "otro".to_string()];
    let list_no_match_word = vec!["casa".to_string(), "perro".to_string()];
    let list_long_number = vec![
        "10".to_string(),
        // The whole number needs to be processed by the DFA, as it
        // needs to figure out if it starts with "11" (once), 110
        // (ciento diez), or "1100" (mil cien).
        "11000000000000000000000000000000".to_string(),
    ];

    group.bench_function("format/or/wide/es/match_word", |b| {
        b.iter(|| {
            result.clear();
            let _ = black_box(&formatter_or)
                .format(black_box(&list_match_word).iter())
                .write_to(&mut result);
        })
    });

    group.bench_function("format/or/wide/es/no_match_word", |b| {
        b.iter(|| {
            result.clear();
            let _ = black_box(&formatter_or)
                .format(black_box(&list_no_match_word).iter())
                .write_to(&mut result);
        })
    });

    group.bench_function("format/or/wide/es/long_number", |b| {
        b.iter(|| {
            result.clear();
            let _ = black_box(&formatter_or)
                .format(black_box(&list_long_number).iter())
                .write_to(&mut result);
        })
    });

    group.finish();
}

criterion_group!(benches, list_benches);
criterion_main!(benches);
