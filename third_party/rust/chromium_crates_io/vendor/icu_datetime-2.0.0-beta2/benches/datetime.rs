// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod fixtures;

use criterion::{criterion_group, criterion_main, Criterion};
use icu_datetime::FixedCalendarDateTimeFormatter;

use icu_calendar::{Date, Gregorian};
use icu_locale_core::Locale;
use icu_time::{zone::TimeZoneVariant, DateTime, Time, TimeZoneInfo, ZonedDateTime};
use writeable::Writeable;

#[path = "../tests/mock.rs"]
mod mock;

fn datetime_benches(c: &mut Criterion) {
    let mut group = c.benchmark_group("datetime");

    let mut bench_neoneo_datetime_with_fixture = |name, file, has_zones| {
        let fxs = serde_json::from_str::<fixtures::Fixture>(file).unwrap();
        group.bench_function(format!("semantic/{name}"), |b| {
            b.iter(|| {
                for fx in &fxs.0 {
                    let datetimes: Vec<ZonedDateTime<Gregorian, _>> = fx
                        .values
                        .iter()
                        .map(move |value| {
                            if has_zones {
                                mock::parse_zoned_gregorian_from_str(value)
                            } else {
                                let DateTime { date, time } =
                                    DateTime::try_from_str(value, Gregorian).unwrap();
                                ZonedDateTime {
                                    date,
                                    time,
                                    // zone is unused but we need to make the types match
                                    zone: TimeZoneInfo::utc()
                                        .at_time((
                                            Date::try_new_iso(2024, 1, 1).unwrap(),
                                            Time::midnight(),
                                        ))
                                        .with_zone_variant(TimeZoneVariant::Standard),
                                }
                            }
                        })
                        .collect();
                    for setup in &fx.setups {
                        let locale: Locale = setup.locale.parse().expect("Failed to parse locale.");
                        let fset = setup
                            .options
                            .semantic
                            .clone()
                            .unwrap()
                            .build_composite()
                            .unwrap();

                        let dtf = {
                            FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(
                                locale.into(),
                                fset,
                            )
                            .expect("Failed to create FixedCalendarDateTimeFormatter.")
                        };

                        let mut result = String::new();

                        for dt in &datetimes {
                            let fdt = dtf.format(dt);
                            fdt.write_to(&mut result).unwrap();
                            result.clear();
                        }
                    }
                }
            })
        });
    };

    bench_neoneo_datetime_with_fixture(
        "lengths",
        include_str!("fixtures/tests/lengths.json"),
        false,
    );

    bench_neoneo_datetime_with_fixture(
        "components",
        include_str!("fixtures/tests/components.json"),
        false,
    );

    bench_neoneo_datetime_with_fixture(
        "lengths_with_zones",
        include_str!("fixtures/tests/lengths_with_zones.json"),
        true,
    );

    group.finish();
}

criterion_group!(benches, datetime_benches);
criterion_main!(benches);
