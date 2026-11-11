use crate::{Error, Integer};

#[test]
#[should_panic = "out of range"]
fn test_constant_too_small() {
    let _ = Integer::constant(-1_000_000_000_000_000);
}

#[test]
#[should_panic = "out of range"]
fn test_constant_too_large() {
    let _ = Integer::constant(1_000_000_000_000_000);
}

#[test]
fn test_conversions() -> Result<(), Error> {
    assert_eq!(Integer::MIN, Integer::constant(-999_999_999_999_999));
    assert_eq!(Integer::MAX, Integer::constant(999_999_999_999_999));

    assert!(Integer::try_from(-1_000_000_000_000_000_i64).is_err());
    assert!(Integer::try_from(1_000_000_000_000_000_i64).is_err());

    assert_eq!(i8::try_from(Integer::from(123_i8)), Ok(123));
    assert_eq!(i16::try_from(Integer::from(123_i16)), Ok(123));
    assert_eq!(i32::try_from(Integer::from(123_i32)), Ok(123));
    assert_eq!(i64::from(Integer::try_from(123_i64)?), 123);
    assert_eq!(i128::from(Integer::try_from(123_i128)?), 123);
    assert_eq!(isize::try_from(Integer::try_from(123_isize)?), Ok(123));

    assert_eq!(u8::try_from(Integer::from(123_u8)), Ok(123));
    assert_eq!(u16::try_from(Integer::from(123_u16)), Ok(123));
    assert_eq!(u32::try_from(Integer::from(123_u32)), Ok(123));
    assert_eq!(u64::try_from(Integer::try_from(123_u64)?), Ok(123));
    assert_eq!(u128::try_from(Integer::try_from(123_u128)?), Ok(123));
    assert_eq!(usize::try_from(Integer::try_from(123_usize)?), Ok(123));

    assert_eq!(i8::try_from(Integer::from(-123_i8)), Ok(-123));
    assert_eq!(i16::try_from(Integer::from(-123_i16)), Ok(-123));
    assert_eq!(i32::try_from(Integer::from(-123_i32)), Ok(-123));
    assert_eq!(i64::from(Integer::try_from(-123_i64)?), -123);
    assert_eq!(i128::from(Integer::try_from(-123_i128)?), -123);
    assert_eq!(isize::try_from(Integer::try_from(-123_isize)?), Ok(-123));

    assert!(u8::try_from(Integer::constant(-123)).is_err());
    assert!(u16::try_from(Integer::constant(-123)).is_err());
    assert!(u32::try_from(Integer::constant(-123)).is_err());
    assert!(u64::try_from(Integer::constant(-123)).is_err());
    assert!(u128::try_from(Integer::constant(-123)).is_err());
    assert!(usize::try_from(Integer::constant(-123)).is_err());

    assert_eq!(Integer::from(i8::MIN), Integer::constant(-128));
    assert_eq!(Integer::from(i8::MAX), Integer::constant(127));
    assert_eq!(Integer::from(i16::MIN), Integer::constant(-32_768));
    assert_eq!(Integer::from(i16::MAX), Integer::constant(32_767));
    assert_eq!(Integer::from(i32::MIN), Integer::constant(-2_147_483_648));
    assert_eq!(Integer::from(i32::MAX), Integer::constant(2_147_483_647));

    assert_eq!(Integer::from(u8::MIN), Integer::ZERO);
    assert_eq!(Integer::from(u8::MAX), Integer::constant(255));
    assert_eq!(Integer::from(u16::MIN), Integer::ZERO);
    assert_eq!(Integer::from(u16::MAX), Integer::constant(65_535));
    assert_eq!(Integer::from(u32::MIN), Integer::ZERO);
    assert_eq!(Integer::from(u32::MAX), Integer::constant(4_294_967_295));

    assert_eq!(i64::from(Integer::MIN), -999_999_999_999_999);
    assert_eq!(i64::from(Integer::MAX), 999_999_999_999_999);
    assert_eq!(i128::from(Integer::MIN), -999_999_999_999_999);
    assert_eq!(i128::from(Integer::MAX), 999_999_999_999_999);

    Ok(())
}
