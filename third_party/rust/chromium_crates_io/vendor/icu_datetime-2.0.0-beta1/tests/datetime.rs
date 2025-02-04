// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod fixtures;
mod patterns;

use fixtures::TestOutputItem;
use icu_calendar::cal::{
    Buddhist, Chinese, Coptic, Dangi, Gregorian, Hebrew, Indian, IslamicCivil,
    IslamicObservational, IslamicTabular, IslamicUmmAlQura, Iso, Persian, Roc,
    {Ethiopian, EthiopianEraStyle}, {Japanese, JapaneseExtended},
};
use icu_calendar::{
    any_calendar::{AnyCalendarKind, IntoAnyCalendar},
    AsCalendar, Calendar, DateTime,
};
use icu_datetime::scaffold::CldrCalendar;
use icu_datetime::{fieldsets::enums::*, DateTimeFormatterPreferences};
use icu_datetime::{
    pattern::DateTimePattern, pattern::TypedDateTimeNames, DateTimeFormatter,
    FixedCalendarDateTimeFormatter,
};
use icu_locale_core::{
    extensions::unicode::{key, value, Value},
    locale,
    preferences::extensions::unicode::keywords::HourCycle,
    Locale,
};
use icu_provider::prelude::*;
use icu_timezone::{CustomZonedDateTime, TimeZoneIdMapper, TimeZoneInfo, UtcOffset};
use patterns::{
    dayperiods::{DayPeriodExpectation, DayPeriodTests},
    time_zones::TimeZoneTests,
};
use writeable::{assert_try_writeable_eq, assert_writeable_eq};

mod mock;

fn apply_preference_bag_to_locale(hour_cycle: HourCycle, locale: &mut Locale) {
    const H11: Value = value!("h11");
    const H12: Value = value!("h12");
    const H23: Value = value!("h23");
    const H24: Value = value!("h24");
    let value = match hour_cycle {
        HourCycle::H11 => H11,
        HourCycle::H12 => H12,
        HourCycle::H23 => H23,
        HourCycle::H24 => H24,
        _ => unreachable!(),
    };
    locale.extensions.unicode.keywords.set(key!("hc"), value);
}

fn test_fixture(fixture_name: &str, file: &str) {
    for fx in serde_json::from_str::<fixtures::Fixture>(file)
        .expect("Unable to get fixture.")
        .0
    {
        let japanese = Japanese::new();
        let japanext = JapaneseExtended::new();
        let skeleton = match fx.input.options.semantic {
            Some(semantic) => {
                let semantic = CompositeFieldSet::try_from(semantic).unwrap();
                match CompositeDateTimeFieldSet::try_from_composite_field_set(semantic) {
                    Some(v) => v,
                    None => {
                        panic!("Cannot handle field sets with time zones in this fn: {semantic:?}");
                    }
                }
            }
            None => {
                eprintln!("Warning: Skipping test with no semantic skeleton: {fx:?}");
                continue;
            }
        };
        let input_value = mock::parse_gregorian_from_str(&fx.input.value);
        let input_buddhist = input_value.to_calendar(Buddhist);
        let input_chinese = input_value.to_calendar(Chinese::new());
        let input_coptic = input_value.to_calendar(Coptic);
        let input_dangi = input_value.to_calendar(Dangi::new());
        let input_ethiopian = input_value.to_calendar(Ethiopian::new());
        let input_ethioaa =
            input_value.to_calendar(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem));
        let input_hebrew = input_value.to_calendar(Hebrew);
        let input_indian = input_value.to_calendar(Indian);
        let input_islamic_civil = input_value.to_calendar(IslamicCivil);
        let input_islamic_observational =
            input_value.to_calendar(IslamicObservational::new_always_calculating());
        let input_islamic_tabular = input_value.to_calendar(IslamicTabular);
        let input_islamic_umm_al_qura =
            input_value.to_calendar(IslamicUmmAlQura::new_always_calculating());
        let input_iso = input_value.to_calendar(Iso);
        let input_japanese = input_value.to_calendar(japanese);
        let input_japanext = input_value.to_calendar(japanext);
        let input_persian = input_value.to_calendar(Persian);
        let input_roc = input_value.to_calendar(Roc);

        let description = match fx.description {
            Some(description) => {
                format!("\n  test: {description:?}\n  file: {fixture_name}.json\n")
            }
            None => format!("\n  file: {fixture_name}.json\n"),
        };
        for (locale, output_value) in fx.output.values {
            let mut locale =
                Locale::try_from_str(&locale).expect("Expected parseable locale in fixture");
            if let Some(hour_cycle) = fx.input.options.hour_cycle {
                apply_preference_bag_to_locale(hour_cycle.into(), &mut locale);
            }
            if let Some(kind) = AnyCalendarKind::get_for_locale(&locale) {
                match kind {
                    AnyCalendarKind::Buddhist => assert_fixture_element(
                        &locale,
                        &input_buddhist,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::Chinese => assert_fixture_element(
                        &locale,
                        &input_chinese,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::Coptic => assert_fixture_element(
                        &locale,
                        &input_coptic,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::Dangi => assert_fixture_element(
                        &locale,
                        &input_dangi,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::Ethiopian => assert_fixture_element(
                        &locale,
                        &input_ethiopian,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::EthiopianAmeteAlem => assert_fixture_element(
                        &locale,
                        &input_ethioaa,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::Hebrew => assert_fixture_element(
                        &locale,
                        &input_hebrew,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::Indian => assert_fixture_element(
                        &locale,
                        &input_indian,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::IslamicCivil => assert_fixture_element(
                        &locale,
                        &input_islamic_civil,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::IslamicObservational => assert_fixture_element(
                        &locale,
                        &input_islamic_observational,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::IslamicTabular => assert_fixture_element(
                        &locale,
                        &input_islamic_tabular,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::IslamicUmmAlQura => assert_fixture_element(
                        &locale,
                        &input_islamic_umm_al_qura,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::Japanese => assert_fixture_element(
                        &locale,
                        &input_japanese,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::JapaneseExtended => assert_fixture_element(
                        &locale,
                        &input_japanext,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::Persian => assert_fixture_element(
                        &locale,
                        &input_persian,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    AnyCalendarKind::Roc => assert_fixture_element(
                        &locale,
                        &input_roc,
                        &input_iso,
                        &output_value,
                        skeleton,
                        &description,
                    ),
                    _ => panic!("datetime test does not support locale {locale:?}"),
                }
            } else {
                assert_fixture_element(
                    &locale,
                    &input_value,
                    &input_iso,
                    &output_value,
                    skeleton,
                    &description,
                )
            }
        }
    }
}

fn assert_fixture_element<A>(
    locale: &Locale,
    input_value: &DateTime<A>,
    input_iso: &DateTime<Iso>,
    output_value: &TestOutputItem,
    skeleton: CompositeDateTimeFieldSet,
    description: &str,
) where
    A: AsCalendar + Clone,
    A::Calendar: CldrCalendar,
    A::Calendar: IntoAnyCalendar,
    icu_datetime::provider::Baked: DataProvider<<A::Calendar as CldrCalendar>::YearNamesV1Marker>,
    icu_datetime::provider::Baked: DataProvider<<A::Calendar as CldrCalendar>::MonthNamesV1Marker>,
    icu_datetime::provider::Baked: DataProvider<<A::Calendar as CldrCalendar>::SkeletaV1Marker>,
{
    assert!(
        input_value.date.calendar().any_calendar_kind().is_some(),
        "{} does not specify its AsCalendarKind",
        input_value.date.calendar().debug_name()
    );

    let input_value = CustomZonedDateTime {
        date: input_value.date.clone(),
        time: input_value.time,
        zone: TimeZoneInfo::utc(),
    };
    let input_iso = CustomZonedDateTime {
        date: input_iso.date,
        time: input_iso.time,
        zone: TimeZoneInfo::utc(),
    };

    let any_input = CustomZonedDateTime {
        date: input_value.date.to_any(),
        time: input_value.time,
        zone: TimeZoneInfo::utc(),
    };
    let iso_any_input = CustomZonedDateTime {
        date: input_iso.date.to_any(),
        time: input_iso.time,
        zone: TimeZoneInfo::utc(),
    };

    let dtf = FixedCalendarDateTimeFormatter::try_new(locale.into(), skeleton).expect(description);

    let any_dtf = DateTimeFormatter::try_new(locale.into(), skeleton).expect(description);

    let actual1 = dtf.format(&input_value);
    assert_writeable_eq!(actual1, output_value.expectation(), "{}", description);

    let actual2 = any_dtf.format_same_calendar(&any_input).unwrap();
    assert_writeable_eq!(
        actual2,
        output_value.expectation(),
        "(DateTimeFormatter) {}",
        description
    );

    let actual3 = any_dtf.format_any_calendar(&iso_any_input);
    assert_writeable_eq!(
        actual3,
        output_value.expectation(),
        "(DateTimeFormatter iso conversion) {}",
        description
    );

    let pattern = actual1.pattern();
    assert_eq!(pattern, actual2.pattern());
    assert_eq!(pattern, actual3.pattern());

    if let Some(expected_pattern) = output_value.pattern() {
        assert_writeable_eq!(pattern, expected_pattern);
    }
}

fn test_fixture_with_time_zones(fixture_name: &str, file: &str) {
    for fx in serde_json::from_str::<fixtures::Fixture>(file)
        .expect("Unable to get fixture.")
        .0
    {
        let skeleton = match fx.input.options.semantic {
            Some(semantic) => CompositeFieldSet::try_from(semantic).unwrap(),
            None => {
                eprintln!("Warning: Skipping test with no semantic skeleton: {fx:?}");
                continue;
            }
        };

        let zoned_datetime = mock::parse_zoned_gregorian_from_str(&fx.input.value);

        let description = match fx.description {
            Some(description) => {
                format!("\n  test: {description:?}\n  file: {fixture_name}.json\n")
            }
            None => format!("\n  file: {fixture_name}.json\n"),
        };
        for (locale, output_value) in fx.output.values {
            let mut locale: Locale = locale.parse().unwrap();
            if let Some(hour_cycle) = fx.input.options.hour_cycle {
                apply_preference_bag_to_locale(hour_cycle.into(), &mut locale);
            }
            let dtf = {
                FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(locale.into(), skeleton)
                    .unwrap()
            };
            assert_writeable_eq!(
                dtf.format(&zoned_datetime),
                output_value.expectation(),
                "{}",
                description
            );
        }
    }
}

#[test]
fn test_dayperiod_patterns() {
    for test in
        serde_json::from_str::<DayPeriodTests>(include_str!("patterns/tests/dayperiods.json"))
            .unwrap()
            .0
    {
        let locale: Locale = test.locale.parse().unwrap();
        for test_case in &test.test_cases {
            for dt_input in &test_case.datetimes {
                let datetime = mock::parse_gregorian_from_str(dt_input);
                for DayPeriodExpectation { patterns, expected } in &test_case.expectations {
                    for pattern_input in patterns {
                        let parsed_pattern =
                            DateTimePattern::try_from_pattern_str(pattern_input).unwrap();
                        let mut pattern_formatter =
                            TypedDateTimeNames::<Gregorian, CompositeDateTimeFieldSet>::try_new(
                                (&locale).into(),
                            )
                            .unwrap();
                        let formatted_datetime = pattern_formatter
                            .include_for_pattern(&parsed_pattern)
                            .unwrap()
                            .format(&datetime);
                        assert_try_writeable_eq!(
                            formatted_datetime,
                            *expected,
                            Ok(()),
                            "\n\
                            locale:   `{}`,\n\
                            datetime: `{}`,\n\
                            pattern:  `{}`",
                            locale,
                            dt_input,
                            pattern_input,
                        );
                    }
                }
            }
        }
    }
}

#[test]
fn test_time_zone_format_configs() {
    for test in
        serde_json::from_str::<TimeZoneTests>(include_str!("patterns/tests/time_zones.json"))
            .unwrap()
            .0
    {
        let prefs: DateTimeFormatterPreferences = test.locale.parse::<Locale>().unwrap().into();
        let zoned_datetime = mock::parse_zoned_gregorian_from_str(&test.datetime);
        for (pattern_input, expect) in &test.expectations {
            let Some(skeleton) = patterns::time_zones::pattern_to_semantic_skeleton(pattern_input)
            else {
                continue;
            };
            let tzf =
                FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(prefs, skeleton).unwrap();
            assert_writeable_eq!(
                tzf.format(&zoned_datetime.zone),
                *expect,
                "\n\
                    prefs:  `{:?}`,\n\
                    datetime: `{}`,\n\
                    config: `{:?}`,\n
                    ",
                prefs,
                test.datetime,
                pattern_input,
            );
        }
    }
}

#[test]
fn test_time_zone_format_offset_seconds() {
    use icu_datetime::fieldsets::O;

    let tzf = FixedCalendarDateTimeFormatter::<(), _>::try_new(locale!("en").into(), O::medium())
        .unwrap();
    assert_writeable_eq!(
        tzf.format(&UtcOffset::try_from_seconds(12).unwrap()),
        "GMT+0:00:12",
    );
}

#[test]
fn test_time_zone_format_offset_fallback() {
    use icu_datetime::fieldsets::O;

    let tzf = FixedCalendarDateTimeFormatter::<(), _>::try_new(locale!("en").into(), O::medium())
        .unwrap();
    assert_writeable_eq!(
        tzf.format(
            &TimeZoneIdMapper::new()
                .iana_to_bcp47("America/Los_Angeles")
                .with_offset(None)
        ),
        "GMT+?",
    );
}

#[test]
fn test_time_zone_patterns() {
    for test in
        serde_json::from_str::<TimeZoneTests>(include_str!("patterns/tests/time_zones.json"))
            .unwrap()
            .0
    {
        let prefs: DateTimeFormatterPreferences = test.locale.parse::<Locale>().unwrap().into();
        let zoned_datetime = mock::parse_zoned_gregorian_from_str(&test.datetime);

        for (pattern_input, expect) in &test.expectations {
            if pattern_input == "VVV" {
                // TODO(#5658): 'VVV' format not yet supported
                continue;
            }
            let parsed_pattern = DateTimePattern::try_from_pattern_str(pattern_input).unwrap();
            let mut pattern_formatter =
                TypedDateTimeNames::<Gregorian, ZoneFieldSet>::try_new(prefs).unwrap();
            let formatted_datetime = pattern_formatter
                .include_for_pattern(&parsed_pattern)
                .unwrap()
                .format(&zoned_datetime);
            assert_writeable_eq!(
                writeable::adapters::LossyWrap(formatted_datetime),
                *expect,
                "\n\
                    prefs:  `{:?}`,\n\
                    datetime: `{}`,\n\
                    pattern:  `{}`",
                prefs,
                test.datetime,
                pattern_input,
            );
        }
    }
}

#[test]
fn test_length_fixtures() {
    test_fixture("lengths", include_str!("fixtures/tests/lengths.json"));
    test_fixture_with_time_zones(
        "lengths_with_zones",
        include_str!("fixtures/tests/lengths_with_zones.json"),
    );
    test_fixture_with_time_zones(
        "lengths_with_zones_from_pdt",
        include_str!("fixtures/tests/lengths_with_zones_from_pdt.json"),
    );
}

#[test]
fn test_japanese() {
    test_fixture("japanese", include_str!("fixtures/tests/japanese.json"));
}

#[test]
fn test_lengths_with_preferences() {
    test_fixture(
        "lengths_with_preferences",
        include_str!("fixtures/tests/lengths_with_preferences.json"),
    );
}

/// Tests simple component::Bag.
#[test]
fn test_components() {
    test_fixture("components", include_str!("fixtures/tests/components.json"));
}

/// Tests component::Bag configurations that have exact matches to CLDR skeletons.
#[test]
fn test_components_exact_matches() {
    test_fixture(
        "components-exact-matches",
        include_str!("fixtures/tests/components-exact-matches.json"),
    );
}

#[test]
fn test_components_hour_cycle() {
    test_fixture(
        "components_hour_cycle",
        include_str!("fixtures/tests/components_hour_cycle.json"),
    );
}

/// Tests that time zones are included, which rely on the append items mechanism.
#[test]
fn test_components_with_zones() {
    test_fixture_with_time_zones(
        "components_with_zones",
        include_str!("fixtures/tests/components_with_zones.json"),
    );
}

/// Tests that component::Bags can adjust for width differences in the final pattern.
#[test]
fn test_components_width_differences() {
    test_fixture(
        "components-width-differences",
        include_str!("fixtures/tests/components-width-differences.json"),
    );
}

/// Tests that combine component::Bags options that don't exactly match a pattern.
#[test]
fn test_components_partial_matches() {
    test_fixture(
        "components-partial-matches",
        include_str!("fixtures/tests/components-partial-matches.json"),
    );
}

/// Tests that component::Bags can combine a date skeleton, and a time skeleton.
#[test]
fn test_components_combine_datetime() {
    test_fixture(
        "components-combine-datetime",
        include_str!("fixtures/tests/components-combine-datetime.json"),
    );
}
