use crate::{Decimal, Error, Integer, error};

// Expect a floating point error less than the smallest allowed value of 0.001
const ABSERR: f64 = 0.000_1_f64;

#[test]
fn test_display() {
    for (expected, input) in [
        ("0.1", 100),
        ("-0.1", -100),
        ("0.01", 10),
        ("-0.01", -10),
        ("0.001", 1),
        ("-0.001", -1),
        ("0.12", 120),
        ("-0.12", -120),
        ("0.124", 124),
        ("-0.124", -124),
        ("0.125", 125),
        ("-0.125", -125),
        ("0.126", 126),
        ("-0.126", -126),
    ] {
        let decimal = Decimal::from_integer_scaled_1000(Integer::constant(input));
        assert_eq!(expected, decimal.to_string());
    }

    assert_eq!("0.0", Decimal::ZERO.to_string());
    assert_eq!("-999999999999.999", Decimal::MIN.to_string());
    assert_eq!("999999999999.999", Decimal::MAX.to_string());
}

#[test]
fn test_into_f64() {
    for (expected, input) in [
        (0.0, 0),
        (0.001, 1),
        (0.01, 10),
        (0.1, 100),
        (1.0, 1000),
        (10.0, 10000),
        (0.123, 123),
        (-0.001, -1),
        (-0.01, -10),
        (-0.1, -100),
        (-1.0, -1000),
        (-10.0, -10000),
        (-0.123, -123),
    ] {
        assert!(
            (expected - f64::from(Decimal::from_integer_scaled_1000(input.into()))).abs() < ABSERR
        );
    }

    assert!((-999_999_999_999.999 - f64::from(Decimal::MIN)).abs() < ABSERR);
    assert!((999_999_999_999.999 - f64::from(Decimal::MAX)).abs() < ABSERR);
}

#[test]
fn test_try_from_f64() {
    for (expected, input) in [
        (Err(error::Repr::NaN), f64::NAN),
        (Err(error::Repr::OutOfRange), f64::INFINITY),
        (Err(error::Repr::OutOfRange), f64::NEG_INFINITY),
        (Err(error::Repr::OutOfRange), 2_f64.powi(65)),
        (Err(error::Repr::OutOfRange), -(2_f64.powi(65))),
        (Err(error::Repr::OutOfRange), -1_000_000_000_000.0),
        (Err(error::Repr::OutOfRange), 1_000_000_000_000.0),
        (Ok(Decimal::MIN), -999_999_999_999.999),
        (Ok(Decimal::MIN), -999_999_999_999.999_1),
        (Err(error::Repr::OutOfRange), -999_999_999_999.999_5),
        (Ok(Decimal::MAX), 999_999_999_999.999),
        (Ok(Decimal::MAX), 999_999_999_999.999_1),
        (Err(error::Repr::OutOfRange), 999_999_999_999.999_5),
        (Ok(Decimal::ZERO), 0.0),
        (Ok(Decimal::ZERO), -0.0),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(123))),
            0.1234,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(124))),
            0.1235,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(124))),
            0.1236,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(-123))),
            -0.1234,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(-124))),
            -0.1235,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(-124))),
            -0.1236,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(124))),
            0.1245,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(-124))),
            -0.1245,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(502))),
            0.5015,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(-502))),
            -0.5015,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(504))),
            0.5035,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(-504))),
            -0.5035,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(2000))),
            2.0005,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(-2000))),
            -2.0005,
        ),
        (Ok(Decimal::ZERO), 0.0005),
        (Ok(Decimal::ZERO), -0.0005),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(1))),
            0.00051,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(-1))),
            -0.00051,
        ),
    ] {
        assert_eq!(expected.map_err(Error::from), Decimal::try_from(input));
    }
}

#[test]
fn test_from_integer() {
    let expected = Decimal::from_integer_scaled_1000(Integer::constant(123_000));
    assert_eq!(Decimal::from(123_i8), expected);
    assert_eq!(Decimal::from(123_i16), expected);
    assert_eq!(Decimal::from(123_i32), expected);
    assert_eq!(Decimal::try_from(123_i64), Ok(expected));
    assert_eq!(Decimal::try_from(123_i128), Ok(expected));
    assert_eq!(Decimal::try_from(123_isize), Ok(expected));

    assert_eq!(Decimal::from(123_u8), expected);
    assert_eq!(Decimal::from(123_u16), expected);
    assert_eq!(Decimal::from(123_u32), expected);
    assert_eq!(Decimal::try_from(123_u64), Ok(expected));
    assert_eq!(Decimal::try_from(123_u128), Ok(expected));
    assert_eq!(Decimal::try_from(123_usize), Ok(expected));
    assert_eq!(Decimal::try_from(Integer::constant(123)), Ok(expected));

    let expected_neg = Decimal::from_integer_scaled_1000(Integer::constant(-123_000));
    assert_eq!(Decimal::from(-123_i8), expected_neg);
    assert_eq!(Decimal::from(-123_i16), expected_neg);
    assert_eq!(Decimal::from(-123_i32), expected_neg);
    assert_eq!(Decimal::try_from(-123_i64), Ok(expected_neg));
    assert_eq!(Decimal::try_from(-123_i128), Ok(expected_neg));
    assert_eq!(Decimal::try_from(-123_isize), Ok(expected_neg));
    assert_eq!(Decimal::try_from(Integer::constant(-123)), Ok(expected_neg));

    assert_eq!(
        Decimal::from(i8::MIN),
        Decimal::from_integer_scaled_1000(Integer::constant(-128_000)),
    );
    assert_eq!(
        Decimal::from(i8::MAX),
        Decimal::from_integer_scaled_1000(Integer::constant(127_000)),
    );
    assert_eq!(
        Decimal::from(i16::MIN),
        Decimal::from_integer_scaled_1000(Integer::constant(-32_768_000)),
    );
    assert_eq!(
        Decimal::from(i16::MAX),
        Decimal::from_integer_scaled_1000(Integer::constant(32_767_000)),
    );
    assert_eq!(
        Decimal::from(i32::MIN),
        Decimal::from_integer_scaled_1000(Integer::constant(-2_147_483_648_000)),
    );
    assert_eq!(
        Decimal::from(i32::MAX),
        Decimal::from_integer_scaled_1000(Integer::constant(2_147_483_647_000)),
    );

    assert_eq!(Decimal::from(u8::MIN), Decimal::ZERO);
    assert_eq!(
        Decimal::from(u8::MAX),
        Decimal::from_integer_scaled_1000(Integer::constant(255_000)),
    );
    assert_eq!(Decimal::from(u16::MIN), Decimal::ZERO);
    assert_eq!(
        Decimal::from(u16::MAX),
        Decimal::from_integer_scaled_1000(Integer::constant(65_535_000)),
    );
    assert_eq!(Decimal::from(u32::MIN), Decimal::ZERO);
    assert_eq!(
        Decimal::from(u32::MAX),
        Decimal::from_integer_scaled_1000(Integer::constant(4_294_967_295_000)),
    );

    assert_eq!(
        Decimal::try_from(-999_999_999_999_i64),
        Ok(Decimal::from_integer_scaled_1000(Integer::constant(
            -999_999_999_999_000
        ))),
    );
    assert_eq!(
        Decimal::try_from(999_999_999_999_i64),
        Ok(Decimal::from_integer_scaled_1000(Integer::constant(
            999_999_999_999_000
        ))),
    );
    assert_eq!(
        Decimal::try_from(-1_000_000_000_000_i64),
        Err(error::Repr::OutOfRange.into()),
    );
    assert_eq!(
        Decimal::try_from(1_000_000_000_000_i64),
        Err(error::Repr::OutOfRange.into()),
    );
    assert_eq!(
        Decimal::try_from(i64::MIN),
        Err(error::Repr::OutOfRange.into()),
    );
    assert_eq!(
        Decimal::try_from(i64::MAX),
        Err(error::Repr::OutOfRange.into()),
    );
    assert_eq!(
        Decimal::try_from(Integer::MIN),
        Err(error::Repr::OutOfRange.into()),
    );
    assert_eq!(
        Decimal::try_from(Integer::MAX),
        Err(error::Repr::OutOfRange.into()),
    );
}
