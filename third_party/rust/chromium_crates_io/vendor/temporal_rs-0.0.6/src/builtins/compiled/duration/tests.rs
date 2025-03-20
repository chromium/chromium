use std::string::ToString;

use crate::{
    options::{
        OffsetDisambiguation, RelativeTo, RoundingIncrement, RoundingOptions, TemporalRoundingMode,
        TemporalUnit,
    },
    partial::PartialDuration,
    primitive::FiniteF64,
    Calendar, DateDuration, PlainDate, TimeDuration, TimeZone, ZonedDateTime,
};

use alloc::vec::Vec;
use core::str::FromStr;

use super::Duration;

fn get_round_result(
    test_duration: &Duration,
    relative_to: RelativeTo,
    options: RoundingOptions,
) -> Vec<i32> {
    test_duration
        .round(options, Some(relative_to))
        .unwrap()
        .fields()
        .iter()
        .map(|f| f.as_date_value().unwrap())
        .collect::<Vec<i32>>()
}

// roundingmode-floor.js
#[test]
fn basic_positive_floor_rounding_v2() {
    let test_duration = Duration::new(
        FiniteF64(5.0),
        FiniteF64(6.0),
        FiniteF64(7.0),
        FiniteF64(8.0),
        FiniteF64(40.0),
        FiniteF64(30.0),
        FiniteF64(20.0),
        FiniteF64(123.0),
        FiniteF64(987.0),
        FiniteF64(500.0),
    )
    .unwrap();
    let forward_date = PlainDate::new(2020, 4, 1, Calendar::from_str("iso8601").unwrap()).unwrap();

    let relative_forward = RelativeTo::PlainDate(forward_date);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: None,
        increment: None,
        rounding_mode: Some(TemporalRoundingMode::Floor),
    };

    let _ = options.smallest_unit.insert(TemporalUnit::Year);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 0, 0, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Month);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Week);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 3, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Day);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Hour);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Minute);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 30, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Second);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 30, 20, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Millisecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 30, 20, 123, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Microsecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 30, 20, 123, 987, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Nanosecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 30, 20, 123, 987, 500],);
}

#[test]
fn basic_negative_floor_rounding_v2() {
    // Test setup
    let test_duration = Duration::new(
        FiniteF64(5.0),
        FiniteF64(6.0),
        FiniteF64(7.0),
        FiniteF64(8.0),
        FiniteF64(40.0),
        FiniteF64(30.0),
        FiniteF64(20.0),
        FiniteF64(123.0),
        FiniteF64(987.0),
        FiniteF64(500.0),
    )
    .unwrap();
    let backward_date =
        PlainDate::new(2020, 12, 1, Calendar::from_str("iso8601").unwrap()).unwrap();

    let relative_backward = RelativeTo::PlainDate(backward_date);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: None,
        increment: None,
        rounding_mode: Some(TemporalRoundingMode::Floor),
    };

    let _ = options.smallest_unit.insert(TemporalUnit::Year);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-6, 0, 0, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Month);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -8, 0, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Week);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, -4, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Day);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -28, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Hour);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -17, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Minute);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -31, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Second);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -30, -21, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Millisecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -30, -20, -124, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Microsecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -30, -20, -123, -988, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Nanosecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -30, -20, -123, -987, -500],);
}

// roundingmode-ceil.js
#[test]
fn basic_positive_ceil_rounding() {
    let test_duration = Duration::new(
        FiniteF64(5.0),
        FiniteF64(6.0),
        FiniteF64(7.0),
        FiniteF64(8.0),
        FiniteF64(40.0),
        FiniteF64(30.0),
        FiniteF64(20.0),
        FiniteF64(123.0),
        FiniteF64(987.0),
        FiniteF64(500.0),
    )
    .unwrap();
    let forward_date = PlainDate::new(2020, 4, 1, Calendar::from_str("iso8601").unwrap()).unwrap();

    let relative_forward = RelativeTo::PlainDate(forward_date);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: None,
        increment: None,
        rounding_mode: Some(TemporalRoundingMode::Ceil),
    };

    let _ = options.smallest_unit.insert(TemporalUnit::Year);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[6, 0, 0, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Month);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 8, 0, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Week);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 4, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Day);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 28, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Hour);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 17, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Minute);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 31, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Second);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 30, 21, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Millisecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 30, 20, 124, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Microsecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 30, 20, 123, 988, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Nanosecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 30, 20, 123, 987, 500],);
}

#[test]
fn basic_negative_ceil_rounding() {
    let test_duration = Duration::new(
        FiniteF64(5.0),
        FiniteF64(6.0),
        FiniteF64(7.0),
        FiniteF64(8.0),
        FiniteF64(40.0),
        FiniteF64(30.0),
        FiniteF64(20.0),
        FiniteF64(123.0),
        FiniteF64(987.0),
        FiniteF64(500.0),
    )
    .unwrap();
    let backward_date =
        PlainDate::new(2020, 12, 1, Calendar::from_str("iso8601").unwrap()).unwrap();
    let relative_backward = RelativeTo::PlainDate(backward_date);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: None,
        increment: None,
        rounding_mode: Some(TemporalRoundingMode::Ceil),
    };

    let _ = options.smallest_unit.insert(TemporalUnit::Year);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, 0, 0, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Month);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Week);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, -3, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Day);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Hour);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Minute);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -30, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Second);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -30, -20, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Millisecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -30, -20, -123, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Microsecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -30, -20, -123, -987, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Nanosecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -30, -20, -123, -987, -500],);
}

// roundingmode-expand.js
#[test]
fn basic_positive_expand_rounding() {
    let test_duration = Duration::new(
        FiniteF64(5.0),
        FiniteF64(6.0),
        FiniteF64(7.0),
        FiniteF64(8.0),
        FiniteF64(40.0),
        FiniteF64(30.0),
        FiniteF64(20.0),
        FiniteF64(123.0),
        FiniteF64(987.0),
        FiniteF64(500.0),
    )
    .unwrap();
    let forward_date = PlainDate::new(2020, 4, 1, Calendar::from_str("iso8601").unwrap()).unwrap();
    let relative_forward = RelativeTo::PlainDate(forward_date);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: None,
        increment: None,
        rounding_mode: Some(TemporalRoundingMode::Expand),
    };

    let _ = options.smallest_unit.insert(TemporalUnit::Year);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[6, 0, 0, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Month);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 8, 0, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Week);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 4, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Day);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 28, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Hour);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 17, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Minute);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 31, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Second);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 30, 21, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Millisecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 30, 20, 124, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Microsecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 30, 20, 123, 988, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Nanosecond);
    let result = get_round_result(&test_duration, relative_forward.clone(), options);
    assert_eq!(&result, &[5, 7, 0, 27, 16, 30, 20, 123, 987, 500],);
}

#[test]
fn basic_negative_expand_rounding() {
    let test_duration = Duration::new(
        FiniteF64(5.0),
        FiniteF64(6.0),
        FiniteF64(7.0),
        FiniteF64(8.0),
        FiniteF64(40.0),
        FiniteF64(30.0),
        FiniteF64(20.0),
        FiniteF64(123.0),
        FiniteF64(987.0),
        FiniteF64(500.0),
    )
    .unwrap();

    let backward_date =
        PlainDate::new(2020, 12, 1, Calendar::from_str("iso8601").unwrap()).unwrap();

    let relative_backward = RelativeTo::PlainDate(backward_date);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: None,
        increment: None,
        rounding_mode: Some(TemporalRoundingMode::Expand),
    };

    let _ = options.smallest_unit.insert(TemporalUnit::Year);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-6, 0, 0, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Month);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -8, 0, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Week);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, -4, 0, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Day);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -28, 0, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Hour);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -17, 0, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Minute);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -31, 0, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Second);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -30, -21, 0, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Millisecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -30, -20, -124, 0, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Microsecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -30, -20, -123, -988, 0],);

    let _ = options.smallest_unit.insert(TemporalUnit::Nanosecond);
    let result = get_round_result(&test_duration.negated(), relative_backward.clone(), options);
    assert_eq!(&result, &[-5, -7, 0, -27, -16, -30, -20, -123, -987, -500],);
}

// test262/test/built-ins/Temporal/Duration/prototype/round/roundingincrement-non-integer.js
#[test]
fn rounding_increment_non_integer() {
    let test_duration = Duration::from(
        DateDuration::new(
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64(1.0),
        )
        .unwrap(),
    );
    let binding = PlainDate::new(2000, 1, 1, Calendar::from_str("iso8601").unwrap()).unwrap();
    let relative_to = RelativeTo::PlainDate(binding);

    let mut options = RoundingOptions {
        largest_unit: None,
        smallest_unit: Some(TemporalUnit::Day),
        increment: None,
        rounding_mode: Some(TemporalRoundingMode::Expand),
    };

    let _ = options
        .increment
        .insert(RoundingIncrement::try_from(2.5).unwrap());
    let result = test_duration
        .round(options, Some(relative_to.clone()))
        .unwrap();

    assert_eq!(
        result.fields(),
        &[
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64(2.0),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default()
        ]
    );

    let _ = options
        .increment
        .insert(RoundingIncrement::try_from(1e9 + 0.5).unwrap());
    let result = test_duration.round(options, Some(relative_to)).unwrap();
    assert_eq!(
        result.fields(),
        &[
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64(1e9),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default()
        ]
    );
}

#[test]
fn basic_add_duration() {
    let base = Duration::new(
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64(1.0),
        FiniteF64::default(),
        FiniteF64(5.0),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
    )
    .unwrap();
    let other = Duration::new(
        FiniteF64(0.0),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64(2.0),
        FiniteF64::default(),
        FiniteF64(5.0),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
    )
    .unwrap();
    let result = base.add(&other).unwrap();
    assert_eq!(result.days(), 3.0);
    assert_eq!(result.minutes(), 10.0);

    let other = Duration::new(
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64(-3.0),
        FiniteF64::default(),
        FiniteF64(-15.0),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
    )
    .unwrap();
    let result = base.add(&other).unwrap();
    assert_eq!(result.days(), -2.0);
    assert_eq!(result.minutes(), -10.0);
}

#[test]
fn basic_subtract_duration() {
    let base = Duration::new(
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64(3.0),
        FiniteF64::default(),
        FiniteF64(15.0),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
    )
    .unwrap();
    let other = Duration::new(
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64(1.0),
        FiniteF64::default(),
        FiniteF64(5.0),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
    )
    .unwrap();
    let result = base.subtract(&other).unwrap();
    assert_eq!(result.days(), 2.0);
    assert_eq!(result.minutes(), 10.0);

    let other = Duration::new(
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64(-3.0),
        FiniteF64::default(),
        FiniteF64(-15.0),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
    )
    .unwrap();
    let result = base.subtract(&other).unwrap();
    assert_eq!(result.days(), 6.0);
    assert_eq!(result.minutes(), 30.0);
}

// days-24-hours-relative-to-zoned-date-time.js
#[test]
fn round_relative_to_zoned_datetime() {
    let duration = Duration::from(
        TimeDuration::new(
            25.into(),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
            FiniteF64::default(),
        )
        .unwrap(),
    );
    let zdt = ZonedDateTime::try_new(
        1_000_000_000_000_000_000,
        Calendar::default(),
        TimeZone::try_from_str("+04:30").unwrap(),
    )
    .unwrap();
    let options = RoundingOptions {
        largest_unit: Some(TemporalUnit::Day),
        smallest_unit: None,
        rounding_mode: None,
        increment: None,
    };
    let result = duration
        .round(options, Some(RelativeTo::ZonedDateTime(zdt)))
        .unwrap();
    // Result duration should be: (0, 0, 0, 1, 1, 0, 0, 0, 0, 0)
    assert_eq!(result.days(), 1.0);
    assert_eq!(result.hours(), 1.0);
}

#[test]
fn test_duration_compare() {
    // TODO(#199): fix this on Windows
    if cfg!(not(windows)) {
        let one = Duration::from_partial_duration(PartialDuration {
            hours: Some(FiniteF64::from(79)),
            minutes: Some(FiniteF64::from(10)),
            ..Default::default()
        })
        .unwrap();
        let two = Duration::from_partial_duration(PartialDuration {
            days: Some(FiniteF64::from(3)),
            hours: Some(FiniteF64::from(7)),
            seconds: Some(FiniteF64::from(630)),
            ..Default::default()
        })
        .unwrap();
        let three = Duration::from_partial_duration(PartialDuration {
            days: Some(FiniteF64::from(3)),
            hours: Some(FiniteF64::from(6)),
            minutes: Some(FiniteF64::from(50)),
            ..Default::default()
        })
        .unwrap();

        let mut arr = [&one, &two, &three];
        arr.sort_by(|a, b| Duration::compare(a, b, None).unwrap());
        assert_eq!(
            arr.map(ToString::to_string),
            [&three, &one, &two].map(ToString::to_string)
        );

        // Sorting relative to a date, taking DST changes into account:
        let zdt = ZonedDateTime::from_str(
            "2020-11-01T00:00-07:00[America/Los_Angeles]",
            Default::default(),
            OffsetDisambiguation::Reject,
        )
        .unwrap();
        arr.sort_by(|a, b| {
            Duration::compare(a, b, Some(RelativeTo::ZonedDateTime(zdt.clone()))).unwrap()
        });
        assert_eq!(
            arr.map(ToString::to_string),
            [&one, &three, &two].map(ToString::to_string)
        )
    }
}

#[test]
fn test_duration_total() {
    let d1 = Duration::from_partial_duration(PartialDuration {
        hours: Some(FiniteF64::from(130)),
        minutes: Some(FiniteF64::from(20)),
        ..Default::default()
    })
    .unwrap();
    assert_eq!(d1.total(TemporalUnit::Second, None).unwrap(), 469200.0);

    // How many 24-hour days is 123456789 seconds?
    let d2 = Duration::from_str("PT123456789S").unwrap();
    assert_eq!(
        d2.total(TemporalUnit::Day, None).unwrap(),
        1428.8980208333332
    );

    // Find totals in months, with and without taking DST into account
    let d3 = Duration::from_partial_duration(PartialDuration {
        hours: Some(FiniteF64::from(2756)),
        ..Default::default()
    })
    .unwrap();
    let relative_to = ZonedDateTime::from_str(
        "2020-01-01T00:00+01:00[Europe/Rome]",
        Default::default(),
        OffsetDisambiguation::Reject,
    )
    .unwrap();
    assert_eq!(
        d3.total(
            TemporalUnit::Month,
            Some(RelativeTo::ZonedDateTime(relative_to))
        )
        .unwrap(),
        3.7958333333333334
    );
    assert_eq!(
        d3.total(
            TemporalUnit::Month,
            Some(RelativeTo::PlainDate(
                PlainDate::new(2020, 1, 1, Calendar::default()).unwrap()
            ))
        )
        .unwrap(),
        3.7944444444444443
    );
}

// balance-subseconds.js
#[test]
fn balance_subseconds() {
    // Test positive
    let pos = Duration::from_partial_duration(PartialDuration {
        milliseconds: Some(FiniteF64::from(999)),
        microseconds: Some(FiniteF64::from(999999)),
        nanoseconds: Some(FiniteF64::from(999999999)),
        ..Default::default()
    })
    .unwrap();
    assert_eq!(pos.total(TemporalUnit::Second, None).unwrap(), 2.998998999);

    // Test negative
    let neg = Duration::from_partial_duration(PartialDuration {
        milliseconds: Some(FiniteF64::from(-999)),
        microseconds: Some(FiniteF64::from(-999999)),
        nanoseconds: Some(FiniteF64::from(-999999999)),
        ..Default::default()
    })
    .unwrap();
    assert_eq!(neg.total(TemporalUnit::Second, None).unwrap(), -2.998998999);
}

// balances-days-up-to-both-years-and-months.js
#[test]
fn balance_days_up_to_both_years_and_months() {
    // Test positive
    let two_years = Duration::from_partial_duration(PartialDuration {
        months: Some(FiniteF64::from(11)),
        days: Some(FiniteF64::from(396)),
        ..Default::default()
    })
    .unwrap();

    let relative_to = PlainDate::new(2017, 1, 1, Calendar::default()).unwrap();

    assert_eq!(
        two_years
            .total(
                TemporalUnit::Year,
                Some(RelativeTo::PlainDate(relative_to.clone()))
            )
            .unwrap(),
        2.0
    );

    // Test negative
    let two_years_negative = Duration::from_partial_duration(PartialDuration {
        months: Some(FiniteF64::from(-11)),
        days: Some(FiniteF64::from(-396)),
        ..Default::default()
    })
    .unwrap();

    assert_eq!(
        two_years_negative
            .total(
                TemporalUnit::Year,
                Some(RelativeTo::PlainDate(relative_to.clone()))
            )
            .unwrap(),
        -2.0
    );
}
