// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod fixtures;

use criterion::{
    black_box, criterion_group, criterion_main, measurement::WallTime, BenchmarkGroup, Criterion,
};
use fixtures::DateFixture;
use icu_calendar::{AsCalendar, Calendar, DateDuration, DateTime, Time};

fn bench_datetime<A: AsCalendar>(datetime: &mut DateTime<A>) {
    // black_box used to avoid compiler optimization.
    // Arithmetic.
    datetime.date.add(DateDuration::new(
        black_box(1),
        black_box(2),
        black_box(3),
        black_box(4),
    ));
    datetime.time = Time::try_new(black_box(14), black_box(30), black_box(0), black_box(0))
        .expect("Failed to initialize Time instance.");

    // Retrieving vals
    let _ = black_box(datetime.date.year().era_year_or_extended());
    let _ = black_box(datetime.date.month().ordinal);
    let _ = black_box(datetime.date.day_of_month().0);
    let _ = black_box(datetime.time.hour);
    let _ = black_box(datetime.time.minute);
    let _ = black_box(datetime.time.second);

    // Conversion to ISO.
    let _ = black_box(datetime.to_iso());
}

fn bench_calendar<C: Clone + Calendar>(
    group: &mut BenchmarkGroup<WallTime>,
    name: &str,
    fxs: &DateFixture,
    calendar: C,
    calendar_datetime_init: impl Fn(i32, u8, u8, u8, u8, u8) -> DateTime<C>,
) {
    group.bench_function(name, |b| {
        b.iter(|| {
            for fx in &fxs.0 {
                // Instantion from int
                let mut instantiated_datetime_calendar = calendar_datetime_init(
                    fx.year, fx.month, fx.day, fx.hour, fx.minute, fx.second,
                );

                // Conversion from ISO
                let datetime_iso =
                    DateTime::try_new_iso(fx.year, fx.month, fx.day, fx.hour, fx.minute, fx.second)
                        .unwrap();
                let mut converted_datetime_calendar =
                    DateTime::new_from_iso(datetime_iso, calendar.clone());

                bench_datetime(&mut instantiated_datetime_calendar);
                bench_datetime(&mut converted_datetime_calendar);
            }
        })
    });
}

fn datetime_benches(c: &mut Criterion) {
    let mut group = c.benchmark_group("datetime");
    let fxs = serde_json::from_str::<DateFixture>(include_str!("fixtures/datetimes.json")).unwrap();

    bench_calendar(
        &mut group,
        "calendar/overview",
        &fxs,
        icu::calendar::cal::Iso,
        |y, m, d, h, min, s| DateTime::try_new_iso(y, m, d, h, min, s).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/buddhist",
        &fxs,
        icu::calendar::cal::Buddhist,
        |y, m, d, h, min, s| DateTime::try_new_buddhist(y, m, d, h, min, s).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/coptic",
        &fxs,
        icu::calendar::cal::Coptic,
        |y, m, d, h, min, s| DateTime::try_new_coptic(y, m, d, h, min, s).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/ethiopic",
        &fxs,
        icu::calendar::cal::Ethiopian::new(),
        |y, m, d, h, min, s| {
            DateTime::try_new_ethiopian(
                icu::calendar::cal::EthiopianEraStyle::AmeteMihret,
                y,
                m,
                d,
                h,
                min,
                s,
            )
            .unwrap()
        },
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/chinese_calculating",
        &fxs,
        icu::calendar::cal::Chinese::new_always_calculating(),
        |y, m, d, h, min, s| {
            DateTime::try_new_chinese_with_calendar(
                y,
                m,
                d,
                h,
                min,
                s,
                icu::calendar::cal::Chinese::new_always_calculating(),
            )
            .unwrap()
        },
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/chinese_cached",
        &fxs,
        icu::calendar::cal::Chinese::new(),
        |y, m, d, h, min, s| {
            DateTime::try_new_chinese_with_calendar(
                y,
                m,
                d,
                h,
                min,
                s,
                icu::calendar::cal::Chinese::new(),
            )
            .unwrap()
        },
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/gregorian",
        &fxs,
        icu::calendar::cal::Gregorian,
        |y, m, d, h, min, s| DateTime::try_new_gregorian(y, m, d, h, min, s).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/indian",
        &fxs,
        icu::calendar::cal::Indian,
        |y, m, d, h, min, s| DateTime::try_new_indian(y, m, d, h, min, s).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/julian",
        &fxs,
        icu::calendar::cal::Julian,
        |y, m, d, h, min, s| DateTime::try_new_julian(y, m, d, h, min, s).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/islamic/civil",
        &fxs,
        icu::calendar::cal::IslamicCivil::new(),
        |y, m, d, h, min, s| {
            DateTime::try_new_islamic_civil_with_calendar(
                y,
                m,
                d,
                h,
                min,
                s,
                icu::calendar::cal::IslamicCivil::new(),
            )
            .unwrap()
        },
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/islamic/tabular",
        &fxs,
        icu::calendar::cal::IslamicTabular::new(),
        |y, m, d, h, min, s| {
            DateTime::try_new_islamic_tabular_with_calendar(
                y,
                m,
                d,
                h,
                min,
                s,
                icu::calendar::cal::IslamicTabular::new(),
            )
            .unwrap()
        },
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/islamic/ummalqura",
        &fxs,
        icu::calendar::cal::IslamicUmmAlQura::new_always_calculating(),
        |y, m, d, h, min, s| {
            DateTime::try_new_ummalqura_with_calendar(
                y,
                m,
                d,
                h,
                min,
                s,
                icu::calendar::cal::IslamicUmmAlQura::new_always_calculating(),
            )
            .unwrap()
        },
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/islamic/observational",
        &fxs,
        icu::calendar::cal::IslamicObservational::new_always_calculating(),
        |y, m, d, h, min, s| {
            DateTime::try_new_observational_islamic_with_calendar(
                y,
                m,
                d,
                h,
                min,
                s,
                icu::calendar::cal::IslamicObservational::new_always_calculating(),
            )
            .unwrap()
        },
    );

    group.finish();
}

criterion_group!(benches, datetime_benches);
criterion_main!(benches);
