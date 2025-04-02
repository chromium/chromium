// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod fixtures;

use criterion::{
    black_box, criterion_group, criterion_main, measurement::WallTime, BenchmarkGroup, Criterion,
};
use fixtures::DateFixture;
use icu_calendar::{AsCalendar, Calendar, Date, DateDuration};

fn bench_date<A: AsCalendar>(date: &mut Date<A>) {
    // black_box used to avoid compiler optimization.
    // Arithmetic
    date.add(DateDuration::new(
        black_box(1),
        black_box(2),
        black_box(3),
        black_box(4),
    ));

    // Retrieving vals
    let _ = black_box(date.year().era_year_or_extended());
    let _ = black_box(date.month().ordinal);
    let _ = black_box(date.day_of_month().0);

    // Conversion to ISO.
    let _ = black_box(date.to_iso());
}

fn bench_calendar<C: Clone + Calendar>(
    group: &mut BenchmarkGroup<WallTime>,
    name: &str,
    fxs: &DateFixture,
    calendar: C,
    calendar_date_init: impl Fn(i32, u8, u8) -> Date<C>,
) {
    group.bench_function(name, |b| {
        b.iter(|| {
            for fx in &fxs.0 {
                // Instantion from int
                let mut instantiated_date_calendar = calendar_date_init(fx.year, fx.month, fx.day);

                // Conversion from ISO
                let date_iso = Date::try_new_iso(fx.year, fx.month, fx.day).unwrap();
                let mut converted_date_calendar = Date::new_from_iso(date_iso, calendar.clone());

                bench_date(&mut instantiated_date_calendar);
                bench_date(&mut converted_date_calendar);
            }
        })
    });
}

fn date_benches(c: &mut Criterion) {
    let mut group = c.benchmark_group("date");
    let fxs = serde_json::from_str::<DateFixture>(include_str!("fixtures/datetimes.json")).unwrap();

    bench_calendar(
        &mut group,
        "calendar/overview",
        &fxs,
        icu::calendar::cal::Iso,
        |y, m, d| Date::try_new_iso(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/buddhist",
        &fxs,
        icu::calendar::cal::Buddhist,
        |y, m, d| Date::try_new_buddhist(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/coptic",
        &fxs,
        icu::calendar::cal::Coptic,
        |y, m, d| Date::try_new_coptic(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/ethiopic",
        &fxs,
        icu::calendar::cal::Ethiopian::new(),
        |y, m, d| {
            Date::try_new_ethiopian(icu::calendar::cal::EthiopianEraStyle::AmeteMihret, y, m, d)
                .unwrap()
        },
    );

    bench_calendar(
        &mut group,
        "calendar/indian",
        &fxs,
        icu::calendar::cal::Indian,
        |y, m, d| Date::try_new_indian(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/persian",
        &fxs,
        icu::calendar::cal::Persian,
        |y, m, d| Date::try_new_persian(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/roc",
        &fxs,
        icu::calendar::cal::Roc,
        |y, m, d| Date::try_new_roc(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/julian",
        &fxs,
        icu::calendar::cal::Julian,
        |y, m, d| Date::try_new_julian(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/chinese_calculating",
        &fxs,
        icu::calendar::cal::Chinese::new_always_calculating(),
        |y, m, d| {
            Date::try_new_chinese_with_calendar(
                y,
                m,
                d,
                icu::calendar::cal::Chinese::new_always_calculating(),
            )
            .unwrap()
        },
    );

    bench_calendar(
        &mut group,
        "calendar/chinese_cached",
        &fxs,
        icu::calendar::cal::Chinese::new(),
        |y, m, d| {
            Date::try_new_chinese_with_calendar(y, m, d, icu::calendar::cal::Chinese::new())
                .unwrap()
        },
    );

    bench_calendar(
        &mut group,
        "calendar/dangi_calculating",
        &fxs,
        icu::calendar::cal::Dangi::new_always_calculating(),
        |y, m, d| {
            Date::try_new_dangi_with_calendar(
                y,
                m,
                d,
                icu::calendar::cal::Dangi::new_always_calculating(),
            )
            .unwrap()
        },
    );

    bench_calendar(
        &mut group,
        "calendar/dangi_cached",
        &fxs,
        icu::calendar::cal::Dangi::new(),
        |y, m, d| {
            Date::try_new_dangi_with_calendar(y, m, d, icu::calendar::cal::Dangi::new()).unwrap()
        },
    );

    bench_calendar(
        &mut group,
        "calendar/hebrew",
        &fxs,
        icu::calendar::cal::Hebrew,
        |y, m, d| Date::try_new_hebrew(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/gregorian",
        &fxs,
        icu::calendar::cal::Gregorian,
        |y, m, d| Date::try_new_gregorian(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/islamic/civil",
        &fxs,
        icu::calendar::cal::IslamicCivil::new(),
        |y, m, d| {
            Date::try_new_islamic_civil_with_calendar(
                y,
                m,
                d,
                icu::calendar::cal::IslamicCivil::new(),
            )
            .unwrap()
        },
    );

    bench_calendar(
        &mut group,
        "calendar/islamic/tabular",
        &fxs,
        icu::calendar::cal::IslamicTabular::new(),
        |y, m, d| {
            Date::try_new_islamic_tabular_with_calendar(
                y,
                m,
                d,
                icu::calendar::cal::IslamicTabular::new(),
            )
            .unwrap()
        },
    );

    bench_calendar(
        &mut group,
        "calendar/islamic/ummalqura",
        &fxs,
        icu::calendar::cal::IslamicUmmAlQura::new_always_calculating(),
        |y, m, d| {
            Date::try_new_ummalqura_with_calendar(
                y,
                m,
                d,
                icu::calendar::cal::IslamicUmmAlQura::new_always_calculating(),
            )
            .unwrap()
        },
    );

    bench_calendar(
        &mut group,
        "calendar/islamic/observational",
        &fxs,
        icu::calendar::cal::IslamicObservational::new_always_calculating(),
        |y, m, d| {
            Date::try_new_observational_islamic_with_calendar(
                y,
                m,
                d,
                icu::calendar::cal::IslamicObservational::new_always_calculating(),
            )
            .unwrap()
        },
    );

    group.finish();
}

criterion_group!(benches, date_benches);
criterion_main!(benches);
