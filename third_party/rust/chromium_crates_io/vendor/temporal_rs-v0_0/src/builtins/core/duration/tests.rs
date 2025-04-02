use core::str::FromStr;

use crate::{
    options::ToStringRoundingOptions, parsers::Precision, partial::PartialDuration,
    primitive::FiniteF64,
};

use super::Duration;

#[test]
fn partial_duration_empty() {
    let err = Duration::from_partial_duration(PartialDuration::default());
    assert!(err.is_err())
}

#[test]
fn partial_duration_values() {
    let mut partial = PartialDuration::default();
    let _ = partial.years.insert(FiniteF64(20.0));
    let result = Duration::from_partial_duration(partial).unwrap();
    assert_eq!(result.years(), 20.0);
}

#[test]
fn default_duration_string() {
    let duration = Duration::default();

    let options = ToStringRoundingOptions {
        precision: Precision::Auto,
        smallest_unit: None,
        rounding_mode: None,
    };
    let result = duration.as_temporal_string(options).unwrap();
    assert_eq!(&result, "PT0S");

    let options = ToStringRoundingOptions {
        precision: Precision::Digit(0),
        smallest_unit: None,
        rounding_mode: None,
    };
    let result = duration.as_temporal_string(options).unwrap();
    assert_eq!(&result, "PT0S");

    let options = ToStringRoundingOptions {
        precision: Precision::Digit(1),
        smallest_unit: None,
        rounding_mode: None,
    };
    let result = duration.as_temporal_string(options).unwrap();
    assert_eq!(&result, "PT0.0S");

    let options = ToStringRoundingOptions {
        precision: Precision::Digit(3),
        smallest_unit: None,
        rounding_mode: None,
    };
    let result = duration.as_temporal_string(options).unwrap();
    assert_eq!(&result, "PT0.000S");
}

#[test]
fn duration_to_string_auto_precision() {
    let duration = Duration::new(
        1.into(),
        2.into(),
        3.into(),
        4.into(),
        5.into(),
        6.into(),
        7.into(),
        FiniteF64::default(),
        FiniteF64::default(),
        FiniteF64::default(),
    )
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "P1Y2M3W4DT5H6M7S");

    let duration = Duration::new(
        1.into(),
        2.into(),
        3.into(),
        4.into(),
        5.into(),
        6.into(),
        7.into(),
        987.into(),
        650.into(),
        FiniteF64::default(),
    )
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "P1Y2M3W4DT5H6M7.98765S");
}

#[test]
fn empty_date_duration() {
    let duration = Duration::from_partial_duration(PartialDuration {
        hours: Some(1.into()),
        ..Default::default()
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "PT1H");
}

#[test]
fn negative_fields_to_string() {
    let duration = Duration::from_partial_duration(PartialDuration {
        years: Some(FiniteF64::from(-1)),
        months: Some(FiniteF64::from(-1)),
        weeks: Some(FiniteF64::from(-1)),
        days: Some(FiniteF64::from(-1)),
        hours: Some(FiniteF64::from(-1)),
        minutes: Some(FiniteF64::from(-1)),
        seconds: Some(FiniteF64::from(-1)),
        milliseconds: Some(FiniteF64::from(-1)),
        microseconds: Some(FiniteF64::from(-1)),
        nanoseconds: Some(FiniteF64::from(-1)),
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "-P1Y1M1W1DT1H1M1.001001001S");

    let duration = Duration::from_partial_duration(PartialDuration {
        milliseconds: Some(FiniteF64::from(-250)),
        ..Default::default()
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "-PT0.25S");

    let duration = Duration::from_partial_duration(PartialDuration {
        milliseconds: Some(FiniteF64::from(-3500)),
        ..Default::default()
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "-PT3.5S");

    let duration = Duration::from_partial_duration(PartialDuration {
        milliseconds: Some(FiniteF64::from(-3500)),
        ..Default::default()
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();
    assert_eq!(&result, "-PT3.5S");

    let duration = Duration::from_partial_duration(PartialDuration {
        weeks: Some(FiniteF64::from(-1)),
        days: Some(FiniteF64::from(-1)),
        ..Default::default()
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();

    assert_eq!(&result, "-P1W1D");
}

#[test]
fn preserve_precision_loss() {
    const MAX_SAFE_INT: f64 = 9_007_199_254_740_991.0;
    let duration = Duration::from_partial_duration(PartialDuration {
        milliseconds: Some(FiniteF64::try_from(MAX_SAFE_INT).unwrap()),
        microseconds: Some(FiniteF64::try_from(MAX_SAFE_INT).unwrap()),
        ..Default::default()
    })
    .unwrap();
    let result = duration
        .as_temporal_string(ToStringRoundingOptions::default())
        .unwrap();

    assert_eq!(&result, "PT9016206453995.731991S");
}

#[test]
fn duration_from_str() {
    let duration = Duration::from_str("PT0.999999999H").unwrap();
    assert_eq!(duration.minutes(), FiniteF64(59.0));
    assert_eq!(duration.seconds(), FiniteF64(59.0));
    assert_eq!(duration.milliseconds(), FiniteF64(999.0));
    assert_eq!(duration.microseconds(), FiniteF64(996.0));
    assert_eq!(duration.nanoseconds(), FiniteF64(400.0));

    let duration = Duration::from_str("PT0.000000011H").unwrap();
    assert_eq!(duration.minutes(), FiniteF64(0.0));
    assert_eq!(duration.seconds(), FiniteF64(0.0));
    assert_eq!(duration.milliseconds(), FiniteF64(0.0));
    assert_eq!(duration.microseconds(), FiniteF64(39.0));
    assert_eq!(duration.nanoseconds(), FiniteF64(600.0));

    let duration = Duration::from_str("PT0.999999999M").unwrap();
    assert_eq!(duration.seconds(), FiniteF64(59.0));
    assert_eq!(duration.milliseconds(), FiniteF64(999.0));
    assert_eq!(duration.microseconds(), FiniteF64(999.0));
    assert_eq!(duration.nanoseconds(), FiniteF64(940.0));
}
