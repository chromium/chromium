// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_calendar::{Date, DateTime, Gregorian, Time};
use icu_datetime::{
    fields::components,
    fieldsets::{self, enums::*},
    options::{Alignment, FractionalSecondDigits, TimePrecision, YearStyle},
    FixedCalendarDateTimeFormatter,
};
use icu_locale_core::Locale;
use icu_locale_core::{locale, preferences::extensions::unicode::keywords::HourCycle};

fn assert_resolved_components(
    skeleton: CompositeDateTimeFieldSet,
    bag: &components::Bag,
    locale: Locale,
) {
    let dtf =
        FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(locale.into(), skeleton).unwrap();
    let datetime = DateTime {
        date: Date::try_new_gregorian(2024, 1, 1).unwrap(),
        time: Time::midnight(),
    };
    let resolved_pattern = dtf.format(&datetime).pattern();
    assert_eq!(components::Bag::from(&resolved_pattern), *bag);
}

#[test]
fn test_length_date() {
    let skeleton = CompositeDateTimeFieldSet::Date(DateFieldSet::YMD(fieldsets::YMD::medium()));

    let mut components_bag = components::Bag::default();
    components_bag.year = Some(components::Year::Numeric);
    components_bag.month = Some(components::Month::Short);
    components_bag.day = Some(components::Day::NumericDayOfMonth);

    assert_resolved_components(skeleton, &components_bag, locale!("en"));
}

#[test]
fn test_length_time() {
    let skeleton = CompositeDateTimeFieldSet::Time(TimeFieldSet::T(fieldsets::T::medium()));

    let mut components_bag = components::Bag::default();
    components_bag.hour = Some(components::Numeric::Numeric);
    components_bag.minute = Some(components::Numeric::TwoDigit);
    components_bag.second = Some(components::Numeric::TwoDigit);
    components_bag.hour_cycle = Some(HourCycle::H12);

    assert_resolved_components(
        skeleton,
        &components_bag,
        "en-u-hc-h12".parse::<Locale>().unwrap(),
    );
}

#[test]
fn test_length_time_preferences() {
    let skeleton = CompositeDateTimeFieldSet::Time(TimeFieldSet::T(
        fieldsets::T::medium().with_alignment(Alignment::Column),
    ));

    let mut components_bag = components::Bag::default();
    components_bag.hour = Some(components::Numeric::TwoDigit);
    components_bag.minute = Some(components::Numeric::TwoDigit);
    components_bag.second = Some(components::Numeric::TwoDigit);
    components_bag.hour_cycle = Some(HourCycle::H24);

    assert_resolved_components(
        skeleton,
        &components_bag,
        "en-u-hc-h24".parse::<Locale>().unwrap(),
    );
}

#[test]
fn test_date_and_time() {
    let skeleton = CompositeDateTimeFieldSet::DateTime(DateAndTimeFieldSet::YMDET(
        fieldsets::YMDET::medium()
            .with_year_style(YearStyle::Always)
            .with_alignment(Alignment::Column)
            .with_time_precision(TimePrecision::SecondExact(FractionalSecondDigits::F4)),
    ));

    let mut input_bag = components::Bag::default();
    input_bag.era = Some(components::Text::Short);
    input_bag.year = Some(components::Year::Numeric);
    input_bag.month = Some(components::Month::Numeric);
    input_bag.day = Some(components::Day::TwoDigitDayOfMonth);
    input_bag.weekday = Some(components::Text::Short);
    input_bag.hour = Some(components::Numeric::TwoDigit);
    input_bag.minute = Some(components::Numeric::TwoDigit);
    input_bag.second = Some(components::Numeric::TwoDigit);
    input_bag.fractional_second = Some(FractionalSecondDigits::F4);
    input_bag.hour_cycle = None;
    let mut output_bag = input_bag; // make a copy
    output_bag.month = Some(components::Month::Short);
    output_bag.hour_cycle = Some(HourCycle::H23);

    assert_resolved_components(
        skeleton,
        &output_bag,
        "en-u-hc-h23".parse::<Locale>().unwrap(),
    );
}
