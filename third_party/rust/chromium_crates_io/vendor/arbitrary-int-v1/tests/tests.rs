#![cfg_attr(feature = "step_trait", feature(step_trait))]

extern crate core;

use arbitrary_int::*;
use std::collections::HashMap;
#[cfg(feature = "step_trait")]
use std::iter::Step;

#[test]
fn constants() {
    // Make a constant to ensure new().value() works in a const-context
    const TEST_CONSTANT: u8 = u7::new(127).value();
    assert_eq!(TEST_CONSTANT, 127u8);

    // Same with widen()
    const TEST_CONSTANT2: u7 = u6::new(63).widen();
    assert_eq!(TEST_CONSTANT2, u7::new(63));

    // Same with widen()
    const TEST_CONSTANT3A: Result<u6, TryNewError> = u6::try_new(62);
    assert_eq!(TEST_CONSTANT3A, Ok(u6::new(62)));
    const TEST_CONSTANT3B: Result<u6, TryNewError> = u6::try_new(64);
    assert!(TEST_CONSTANT3B.is_err());
}

#[test]
fn create_simple() {
    let value7 = u7::new(123);
    let value8 = UInt::<u8, 8>::new(189);

    let value13 = u13::new(123);
    let value16 = UInt::<u16, 16>::new(60000);

    let value23 = u23::new(123);
    let value67 = u67::new(123);

    assert_eq!(value7.value(), 123);
    assert_eq!(value8.value(), 189);

    assert_eq!(value13.value(), 123);
    assert_eq!(value16.value(), 60000);

    assert_eq!(value23.value(), 123);
    assert_eq!(value67.value(), 123);
}

#[test]
fn create_try_new() {
    assert_eq!(u7::new(123).value(), 123);
    assert_eq!(u7::try_new(190).expect_err("No error seen"), TryNewError {});
}

#[test]
#[should_panic]
fn create_panic_u7() {
    u7::new(128);
}

#[test]
#[should_panic]
fn create_panic_u15() {
    u15::new(32768);
}

#[test]
#[should_panic]
fn create_panic_u31() {
    u31::new(2147483648);
}

#[test]
#[should_panic]
fn create_panic_u63() {
    u63::new(0x8000_0000_0000_0000);
}

#[test]
#[should_panic]
fn create_panic_u127() {
    u127::new(0x8000_0000_0000_0000_0000_0000_0000_0000);
}

#[test]
fn add() {
    assert_eq!(u7::new(10) + u7::new(20), u7::new(30));
    assert_eq!(u7::new(100) + u7::new(27), u7::new(127));
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn add_overflow() {
    let _ = u7::new(127) + u7::new(3);
}

#[cfg(not(debug_assertions))]
#[test]
fn add_no_overflow() {
    let _ = u7::new(127) + u7::new(3);
}

#[cfg(feature = "num-traits")]
#[test]
fn num_traits_add_wrapping() {
    let v1 = u7::new(120);
    let v2 = u7::new(10);
    let v3 = num_traits::WrappingAdd::wrapping_add(&v1, &v2);
    assert_eq!(v3, u7::new(2));
}

#[cfg(feature = "num-traits")]
#[test]
fn num_traits_sub_wrapping() {
    let v1 = u7::new(15);
    let v2 = u7::new(20);
    let v3 = num_traits::WrappingSub::wrapping_sub(&v1, &v2);
    assert_eq!(v3, u7::new(123));
}

#[cfg(feature = "num-traits")]
#[test]
fn num_traits_bounded() {
    use num_traits::bounds::Bounded;
    assert_eq!(u7::MAX, u7::max_value());
    assert_eq!(u119::MAX, u119::max_value());
    assert_eq!(u7::new(0), u7::min_value());
    assert_eq!(u119::new(0), u119::min_value());
}

#[test]
fn addassign() {
    let mut value = u9::new(500);
    value += u9::new(11);
    assert_eq!(value, u9::new(511));
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn addassign_overflow() {
    let mut value = u9::new(500);
    value += u9::new(40);
}

#[cfg(not(debug_assertions))]
#[test]
fn addassign_no_overflow() {
    let mut value = u9::new(500);
    value += u9::new(28);
    assert_eq!(value, u9::new(16));
}

#[test]
fn sub() {
    assert_eq!(u7::new(22) - u7::new(10), u7::new(12));
    assert_eq!(u7::new(127) - u7::new(127), u7::new(0));
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn sub_overflow() {
    let _ = u7::new(100) - u7::new(127);
}

#[cfg(not(debug_assertions))]
#[test]
fn sub_no_overflow() {
    let value = u7::new(100) - u7::new(127);
    assert_eq!(value, u7::new(101));
}

#[test]
fn subassign() {
    let mut value = u9::new(500);
    value -= u9::new(11);
    assert_eq!(value, u9::new(489));
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn subassign_overflow() {
    let mut value = u9::new(30);
    value -= u9::new(40);
}

#[cfg(not(debug_assertions))]
#[test]
fn subassign_no_overflow() {
    let mut value = u9::new(30);
    value -= u9::new(40);
    assert_eq!(value, u9::new(502));
}

#[test]
fn mul() {
    assert_eq!(u7::new(22) * u7::new(4), u7::new(88));
    assert_eq!(u7::new(127) * u7::new(0), u7::new(0));
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn mul_overflow() {
    let _ = u7::new(100) * u7::new(2);
}

#[cfg(not(debug_assertions))]
#[test]
fn mul_no_overflow() {
    let result = u7::new(100) * u7::new(2);
    assert_eq!(result, u7::new(72));
}

#[test]
fn mulassign() {
    let mut value = u9::new(240);
    value *= u9::new(2);
    assert_eq!(value, u9::new(480));
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn mulassign_overflow() {
    let mut value = u9::new(500);
    value *= u9::new(2);
}

#[cfg(not(debug_assertions))]
#[test]
fn mulassign_no_overflow() {
    let mut value = u9::new(500);
    value *= u9::new(40);
    assert_eq!(value, u9::new(32));
}

#[test]
fn div() {
    // div just forwards to the underlying type, so there isn't much to do
    assert_eq!(u7::new(22) / u7::new(4), u7::new(5));
    assert_eq!(u7::new(127) / u7::new(1), u7::new(127));
    assert_eq!(u7::new(127) / u7::new(127), u7::new(1));
}

#[should_panic]
#[test]
fn div_by_zero() {
    let _ = u7::new(22) / u7::new(0);
}

#[test]
fn divassign() {
    let mut value = u9::new(240);
    value /= u9::new(2);
    assert_eq!(value, u9::new(120));
}

#[should_panic]
#[test]
fn divassign_by_zero() {
    let mut value = u9::new(240);
    value /= u9::new(0);
}

#[test]
fn bitand() {
    assert_eq!(
        u17::new(0b11001100) & u17::new(0b01101001),
        u17::new(0b01001000)
    );
    assert_eq!(u17::new(0b11001100) & u17::new(0), u17::new(0));
    assert_eq!(
        u17::new(0b11001100) & u17::new(0x1_FFFF),
        u17::new(0b11001100)
    );
}

#[test]
fn bitandassign() {
    let mut value = u4::new(0b0101);
    value &= u4::new(0b0110);
    assert_eq!(value, u4::new(0b0100));
}

#[test]
fn bitor() {
    assert_eq!(
        u17::new(0b11001100) | u17::new(0b01101001),
        u17::new(0b11101101)
    );
    assert_eq!(u17::new(0b11001100) | u17::new(0), u17::new(0b11001100));
    assert_eq!(
        u17::new(0b11001100) | u17::new(0x1_FFFF),
        u17::new(0x1_FFFF)
    );
}

#[test]
fn bitorassign() {
    let mut value = u4::new(0b0101);
    value |= u4::new(0b0110);
    assert_eq!(value, u4::new(0b0111));
}

#[test]
fn bitxor() {
    assert_eq!(
        u17::new(0b11001100) ^ u17::new(0b01101001),
        u17::new(0b10100101)
    );
    assert_eq!(u17::new(0b11001100) ^ u17::new(0), u17::new(0b11001100));
    assert_eq!(
        u17::new(0b11001100) ^ u17::new(0x1_FFFF),
        u17::new(0b1_11111111_00110011)
    );
}

#[test]
fn bitxorassign() {
    let mut value = u4::new(0b0101);
    value ^= u4::new(0b0110);
    assert_eq!(value, u4::new(0b0011));
}

#[test]
fn not() {
    assert_eq!(!u17::new(0), u17::new(0b1_11111111_11111111));
    assert_eq!(!u5::new(0b10101), u5::new(0b01010));
}

#[test]
fn shl() {
    assert_eq!(u17::new(0b1) << 5u8, u17::new(0b100000));
    // Ensure bits on the left are shifted out
    assert_eq!(u9::new(0b11110000) << 3u64, u9::new(0b1_10000000));
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn shl_too_much8() {
    let _ = u53::new(123) << 53u8;
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn shl_too_much16() {
    let _ = u53::new(123) << 53u16;
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn shl_too_much32() {
    let _ = u53::new(123) << 53u32;
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn shl_too_much64() {
    let _ = u53::new(123) << 53u64;
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn shl_too_much128() {
    let _ = u53::new(123) << 53u128;
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn shl_too_much_usize() {
    let _ = u53::new(123) << 53usize;
}

#[test]
fn shlassign() {
    let mut value = u9::new(0b11110000);
    value <<= 3;
    assert_eq!(value, u9::new(0b1_10000000));
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn shlassign_too_much() {
    let mut value = u9::new(0b11110000);
    value <<= 9;
}

#[cfg(debug_assertions)]
#[test]
#[should_panic]
fn shlassign_too_much2() {
    let mut value = u9::new(0b11110000);
    value <<= 10;
}

#[test]
fn shr() {
    assert_eq!(u17::new(0b100110) >> 5usize, u17::new(1));

    // Ensure there's no sign extension
    assert_eq!(u17::new(0b1_11111111_11111111) >> 8, u17::new(0b1_11111111));
}

#[test]
fn shrassign() {
    let mut value = u9::new(0b1_11110000);
    value >>= 6;
    assert_eq!(value, u9::new(0b0_00000111));
}

#[test]
fn compare() {
    assert_eq!(true, u4::new(0b1100) > u4::new(0b0011));
    assert_eq!(true, u4::new(0b1100) >= u4::new(0b0011));
    assert_eq!(false, u4::new(0b1100) < u4::new(0b0011));
    assert_eq!(false, u4::new(0b1100) <= u4::new(0b0011));
    assert_eq!(true, u4::new(0b1100) != u4::new(0b0011));
    assert_eq!(false, u4::new(0b1100) == u4::new(0b0011));

    assert_eq!(false, u4::new(0b1100) > u4::new(0b1100));
    assert_eq!(true, u4::new(0b1100) >= u4::new(0b1100));
    assert_eq!(false, u4::new(0b1100) < u4::new(0b1100));
    assert_eq!(true, u4::new(0b1100) <= u4::new(0b1100));
    assert_eq!(false, u4::new(0b1100) != u4::new(0b1100));
    assert_eq!(true, u4::new(0b1100) == u4::new(0b1100));

    assert_eq!(false, u4::new(0b0011) > u4::new(0b1100));
    assert_eq!(false, u4::new(0b0011) >= u4::new(0b1100));
    assert_eq!(true, u4::new(0b0011) < u4::new(0b1100));
    assert_eq!(true, u4::new(0b0011) <= u4::new(0b1100));
    assert_eq!(true, u4::new(0b0011) != u4::new(0b1100));
    assert_eq!(false, u4::new(0b0011) == u4::new(0b1100));
}

#[test]
fn min_max() {
    assert_eq!(0, u4::MIN.value());
    assert_eq!(0b1111, u4::MAX.value());
    assert_eq!(u4::new(0b1111), u4::MAX);

    assert_eq!(0, u15::MIN.value());
    assert_eq!(32767, u15::MAX.value());
    assert_eq!(u15::new(32767), u15::MAX);

    assert_eq!(0, u31::MIN.value());
    assert_eq!(2147483647, u31::MAX.value());

    assert_eq!(0, u63::MIN.value());
    assert_eq!(0x7FFF_FFFF_FFFF_FFFF, u63::MAX.value());

    assert_eq!(0, u127::MIN.value());
    assert_eq!(0x7FFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF, u127::MAX.value());
}

#[test]
fn bits() {
    assert_eq!(4, u4::BITS);
    assert_eq!(12, u12::BITS);
    assert_eq!(120, u120::BITS);
    assert_eq!(13, UInt::<u128, 13usize>::BITS);

    assert_eq!(8, u8::BITS);
    assert_eq!(16, u16::BITS);
}

#[test]
fn mask() {
    assert_eq!(0x1u8, u1::MASK);
    assert_eq!(0xFu8, u4::MASK);
    assert_eq!(0x3FFFFu32, u18::MASK);
    assert_eq!(0x7FFFFFFF_FFFFFFFF_FFFFFFFF_FFFFFFFFu128, u127::MASK);
    assert_eq!(0x7FFFFFFF_FFFFFFFF_FFFFFFFF_FFFFFFFFu128, u127::MASK);
    assert_eq!(0xFFFFFFFF_FFFFFFFF_FFFFFFFF_FFFFFFFFu128, u128::MAX);
}

#[test]
fn min_max_fullwidth() {
    assert_eq!(u8::MIN, UInt::<u8, 8>::MIN.value());
    assert_eq!(u8::MAX, UInt::<u8, 8>::MAX.value());

    assert_eq!(u16::MIN, UInt::<u16, 16>::MIN.value());
    assert_eq!(u16::MAX, UInt::<u16, 16>::MAX.value());

    assert_eq!(u32::MIN, UInt::<u32, 32>::MIN.value());
    assert_eq!(u32::MAX, UInt::<u32, 32>::MAX.value());

    assert_eq!(u64::MIN, UInt::<u64, 64>::MIN.value());
    assert_eq!(u64::MAX, UInt::<u64, 64>::MAX.value());

    assert_eq!(u128::MIN, UInt::<u128, 128>::MIN.value());
    assert_eq!(u128::MAX, UInt::<u128, 128>::MAX.value());
}

#[allow(deprecated)]
#[test]
fn extract() {
    assert_eq!(u5::new(0b10000), u5::extract(0b11110000, 0));
    assert_eq!(u5::new(0b11100), u5::extract(0b11110000, 2));
    assert_eq!(u5::new(0b11110), u5::extract(0b11110000, 3));

    // Use extract with a custom type (5 bits of u32)
    assert_eq!(
        UInt::<u32, 5>::new(0b11110),
        UInt::<u32, 5>::extract(0b11110000, 3)
    );
    assert_eq!(
        u5::new(0b11110),
        UInt::<u32, 5>::extract(0b11110000, 3).into()
    );
}

#[test]
fn extract_typed() {
    assert_eq!(u5::new(0b10000), u5::extract_u8(0b11110000, 0));
    assert_eq!(u5::new(0b00011), u5::extract_u16(0b11110000_11110110, 6));
    assert_eq!(
        u5::new(0b01011),
        u5::extract_u32(0b11110010_11110110_00000000_00000000, 22)
    );
    assert_eq!(
        u5::new(0b01011),
        u5::extract_u64(
            0b11110010_11110110_00000000_00000000_00000000_00000000_00000000_00000000,
            54
        )
    );
    assert_eq!(u5::new(0b01011), u5::extract_u128(0b11110010_11110110_00000000_00000000_00000000_00000000_00000000_00000000_00000000_00000000_00000000_00000000_00000000_00000000_00000000_00000000, 118));
}

#[test]
fn extract_full_width_typed() {
    assert_eq!(
        0b1010_0011,
        UInt::<u8, 8>::extract_u8(0b1010_0011, 0).value()
    );
    assert_eq!(
        0b1010_0011,
        UInt::<u8, 8>::extract_u16(0b1111_1111_1010_0011, 0).value()
    );
}

#[test]
#[should_panic]
fn extract_not_enough_bits_8() {
    let _ = u5::extract_u8(0b11110000, 4);
}

#[test]
#[should_panic]
fn extract_not_enough_bits_8_full_width() {
    let _ = UInt::<u8, 8>::extract_u8(0b11110000, 1);
}

#[test]
#[should_panic]
fn extract_not_enough_bits_16() {
    let _ = u5::extract_u16(0b11110000, 12);
}

#[test]
#[should_panic]
fn extract_not_enough_bits_32() {
    let _ = u5::extract_u32(0b11110000, 28);
}

#[test]
#[should_panic]
fn extract_not_enough_bits_64() {
    let _ = u5::extract_u64(0b11110000, 60);
}

#[test]
#[should_panic]
fn extract_not_enough_bits_128() {
    let _ = u5::extract_u128(0b11110000, 124);
}

#[test]
fn from_same_bit_widths() {
    assert_eq!(u5::from(UInt::<u8, 5>::new(0b10101)), u5::new(0b10101));
    assert_eq!(u5::from(UInt::<u16, 5>::new(0b10101)), u5::new(0b10101));
    assert_eq!(u5::from(UInt::<u32, 5>::new(0b10101)), u5::new(0b10101));
    assert_eq!(u5::from(UInt::<u64, 5>::new(0b10101)), u5::new(0b10101));
    assert_eq!(u5::from(UInt::<u128, 5>::new(0b10101)), u5::new(0b10101));

    assert_eq!(
        UInt::<u8, 8>::from(UInt::<u128, 8>::new(0b1110_0101)),
        UInt::<u8, 8>::new(0b1110_0101)
    );

    assert_eq!(
        UInt::<u16, 6>::from(UInt::<u8, 5>::new(0b10101)),
        UInt::<u16, 6>::new(0b10101)
    );
    assert_eq!(u15::from(UInt::<u16, 15>::new(0b10101)), u15::new(0b10101));
    assert_eq!(u15::from(UInt::<u32, 15>::new(0b10101)), u15::new(0b10101));
    assert_eq!(u15::from(UInt::<u64, 15>::new(0b10101)), u15::new(0b10101));
    assert_eq!(u15::from(UInt::<u128, 15>::new(0b10101)), u15::new(0b10101));

    assert_eq!(
        UInt::<u32, 6>::from(u6::new(0b10101)),
        UInt::<u32, 6>::new(0b10101)
    );
    assert_eq!(
        UInt::<u32, 14>::from(u14::new(0b10101)),
        UInt::<u32, 14>::new(0b10101)
    );
    assert_eq!(u30::from(UInt::<u32, 30>::new(0b10101)), u30::new(0b10101));
    assert_eq!(u30::from(UInt::<u64, 30>::new(0b10101)), u30::new(0b10101));
    assert_eq!(u30::from(UInt::<u128, 30>::new(0b10101)), u30::new(0b10101));

    assert_eq!(
        UInt::<u64, 7>::from(UInt::<u8, 7>::new(0b10101)),
        UInt::<u64, 7>::new(0b10101)
    );
    assert_eq!(
        UInt::<u64, 12>::from(UInt::<u16, 12>::new(0b10101)),
        UInt::<u64, 12>::new(0b10101)
    );
    assert_eq!(
        UInt::<u64, 28>::from(UInt::<u32, 28>::new(0b10101)),
        UInt::<u64, 28>::new(0b10101)
    );
    assert_eq!(u60::from(u60::new(0b10101)), u60::new(0b10101));
    assert_eq!(u60::from(UInt::<u128, 60>::new(0b10101)), u60::new(0b10101));

    assert_eq!(
        UInt::<u128, 5>::from(UInt::<u8, 5>::new(0b10101)),
        UInt::<u128, 5>::new(0b10101)
    );
    assert_eq!(
        UInt::<u128, 12>::from(UInt::<u16, 12>::new(0b10101)),
        UInt::<u128, 12>::new(0b10101)
    );
    assert_eq!(
        UInt::<u128, 26>::from(UInt::<u32, 26>::new(0b10101)),
        UInt::<u128, 26>::new(0b10101)
    );
    assert_eq!(
        UInt::<u128, 60>::from(UInt::<u64, 60>::new(0b10101)),
        UInt::<u128, 60>::new(0b10101)
    );
    assert_eq!(
        u120::from(UInt::<u128, 120>::new(0b10101)),
        u120::new(0b10101)
    );
}

#[cfg(feature = "num-traits")]
#[test]
fn calculation_with_number_trait() {
    fn increment_by_1<T: num_traits::WrappingAdd + Number>(foo: T) -> T {
        foo.wrapping_add(&T::new(1.into()))
    }

    fn increment_by_512<T: num_traits::WrappingAdd + Number>(
        foo: T,
    ) -> Result<T, <<T as Number>::UnderlyingType as TryFrom<u32>>::Error>
    where
        <<T as Number>::UnderlyingType as TryFrom<u32>>::Error: core::fmt::Debug,
    {
        Ok(foo.wrapping_add(&T::new(512u32.try_into()?)))
    }

    assert_eq!(increment_by_1(0u16), 1u16);
    assert_eq!(increment_by_1(u7::new(3)), u7::new(4));
    assert_eq!(increment_by_1(u15::new(3)), u15::new(4));

    assert_eq!(increment_by_512(0u16), Ok(512u16));
    assert!(increment_by_512(u7::new(3)).is_err());
    assert_eq!(increment_by_512(u15::new(3)), Ok(u15::new(515)));
}

#[test]
fn from_smaller_bit_widths() {
    // The code to get more bits from fewer bits (through From) is the same as the code above
    // for identical bitwidths. Therefore just do a few point checks to ensure things compile

    // There are compile-breakers for the opposite direction (e.g. tryint to do u5 = From(u17),
    // but we can't test compile failures here

    // from is not yet supported if the bitcounts are different but the base data types are the same (need
    // fancier Rust features to support that)
    assert_eq!(u6::from(UInt::<u16, 5>::new(0b10101)), u6::new(0b10101));
    assert_eq!(u6::from(UInt::<u32, 5>::new(0b10101)), u6::new(0b10101));
    assert_eq!(u6::from(UInt::<u64, 5>::new(0b10101)), u6::new(0b10101));
    assert_eq!(u6::from(UInt::<u128, 5>::new(0b10101)), u6::new(0b10101));

    assert_eq!(u15::from(UInt::<u8, 7>::new(0b10101)), u15::new(0b10101));
    //assert_eq!(u15::from(UInt::<u16, 15>::new(0b10101)), u15::new(0b10101));
    assert_eq!(u15::from(UInt::<u32, 14>::new(0b10101)), u15::new(0b10101));
    assert_eq!(u15::from(UInt::<u64, 14>::new(0b10101)), u15::new(0b10101));
    assert_eq!(u15::from(UInt::<u128, 14>::new(0b10101)), u15::new(0b10101));
}

#[allow(non_camel_case_types)]
#[test]
fn from_native_ints_same_bits() {
    use std::primitive;

    type u8 = UInt<primitive::u8, 8>;
    type u16 = UInt<primitive::u16, 16>;
    type u32 = UInt<primitive::u32, 32>;
    type u64 = UInt<primitive::u64, 64>;
    type u128 = UInt<primitive::u128, 128>;

    assert_eq!(u8::from(0x80_u8), u8::new(0x80));
    assert_eq!(u16::from(0x8000_u16), u16::new(0x8000));
    assert_eq!(u32::from(0x8000_0000_u32), u32::new(0x8000_0000));
    assert_eq!(
        u64::from(0x8000_0000_0000_0000_u64),
        u64::new(0x8000_0000_0000_0000)
    );
    assert_eq!(
        u128::from(0x8000_0000_0000_0000_0000_0000_0000_0000_u128),
        u128::new(0x8000_0000_0000_0000_0000_0000_0000_0000)
    );
}

#[test]
fn from_native_ints_fewer_bits() {
    assert_eq!(u9::from(0x80_u8), u9::new(0x80));

    assert_eq!(u17::from(0x80_u8), u17::new(0x80));
    assert_eq!(u17::from(0x8000_u16), u17::new(0x8000));

    assert_eq!(u33::from(0x80_u8), u33::new(0x80));
    assert_eq!(u33::from(0x8000_u16), u33::new(0x8000));
    assert_eq!(u33::from(0x8000_0000_u32), u33::new(0x8000_0000));

    assert_eq!(u65::from(0x80_u8), u65::new(0x80));
    assert_eq!(u65::from(0x8000_u16), u65::new(0x8000));
    assert_eq!(u65::from(0x8000_0000_u32), u65::new(0x8000_0000));
    assert_eq!(
        u65::from(0x8000_0000_0000_0000_u64),
        u65::new(0x8000_0000_0000_0000)
    );
}

#[allow(non_camel_case_types)]
#[test]
fn into_native_ints_same_bits() {
    assert_eq!(u8::from(UInt::<u8, 8>::new(0x80)), 0x80);
    assert_eq!(u16::from(UInt::<u16, 16>::new(0x8000)), 0x8000);
    assert_eq!(u32::from(UInt::<u32, 32>::new(0x8000_0000)), 0x8000_0000);
    assert_eq!(
        u64::from(UInt::<u64, 64>::new(0x8000_0000_0000_0000)),
        0x8000_0000_0000_0000
    );
    assert_eq!(
        u128::from(UInt::<u128, 128>::new(
            0x8000_0000_0000_0000_0000_0000_0000_0000
        )),
        0x8000_0000_0000_0000_0000_0000_0000_0000
    );
}

#[test]
fn into_native_ints_fewer_bits() {
    assert_eq!(u8::from(u7::new(0x40)), 0x40);
    assert_eq!(u16::from(u15::new(0x4000)), 0x4000);
    assert_eq!(u32::from(u31::new(0x4000_0000)), 0x4000_0000);
    assert_eq!(
        u64::from(u63::new(0x4000_0000_0000_0000)),
        0x4000_0000_0000_0000
    );
    assert_eq!(
        u128::from(u127::new(0x4000_0000_0000_0000_0000_0000_0000_0000)),
        0x4000_0000_0000_0000_0000_0000_0000_0000
    );
}

#[test]
fn from_into_bool() {
    assert_eq!(u1::from(true), u1::new(1));
    assert_eq!(u1::from(false), u1::new(0));
    assert_eq!(bool::from(u1::new(1)), true);
    assert_eq!(bool::from(u1::new(0)), false);
}

#[test]
fn widen() {
    // As From() can't be used while keeping the base-data-type, there's widen

    assert_eq!(u5::new(0b11011).widen::<6>(), u6::new(0b11011));
    assert_eq!(u5::new(0b11011).widen::<8>(), UInt::<u8, 8>::new(0b11011));
    assert_eq!(u10::new(0b11011).widen::<11>(), u11::new(0b11011));
    assert_eq!(u20::new(0b11011).widen::<24>(), u24::new(0b11011));
    assert_eq!(u60::new(0b11011).widen::<61>(), u61::new(0b11011));
    assert_eq!(u80::new(0b11011).widen::<127>().value(), 0b11011);
}

#[test]
fn to_string() {
    assert_eq!("Value: 5", format!("Value: {}", 5u32.to_string()));
    assert_eq!("Value: 5", format!("Value: {}", u5::new(5).to_string()));
    assert_eq!("Value: 5", format!("Value: {}", u11::new(5).to_string()));
    assert_eq!("Value: 5", format!("Value: {}", u17::new(5).to_string()));
    assert_eq!("Value: 5", format!("Value: {}", u38::new(5).to_string()));
    assert_eq!("Value: 60", format!("Value: {}", u65::new(60).to_string()));
}

#[test]
fn display() {
    assert_eq!("Value: 5", format!("Value: {}", 5u32));
    assert_eq!("Value: 5", format!("Value: {}", u5::new(5)));
    assert_eq!("Value: 5", format!("Value: {}", u11::new(5)));
    assert_eq!("Value: 5", format!("Value: {}", u17::new(5)));
    assert_eq!("Value: 5", format!("Value: {}", u38::new(5)));
    assert_eq!("Value: 60", format!("Value: {}", u65::new(60)));
}

#[test]
fn debug() {
    assert_eq!("Value: 5", format!("Value: {:?}", 5u32));
    assert_eq!("Value: 5", format!("Value: {:?}", u5::new(5)));
    assert_eq!("Value: 5", format!("Value: {:?}", u11::new(5)));
    assert_eq!("Value: 5", format!("Value: {:?}", u17::new(5)));
    assert_eq!("Value: 5", format!("Value: {:?}", u38::new(5)));
    assert_eq!("Value: 60", format!("Value: {:?}", u65::new(60)));
}

#[test]
fn lower_hex() {
    assert_eq!("Value: a", format!("Value: {:x}", 10u32));
    assert_eq!("Value: a", format!("Value: {:x}", u5::new(10)));
    assert_eq!("Value: a", format!("Value: {:x}", u11::new(10)));
    assert_eq!("Value: a", format!("Value: {:x}", u17::new(10)));
    assert_eq!("Value: a", format!("Value: {:x}", u38::new(10)));
    assert_eq!("Value: 3c", format!("Value: {:x}", 60));
    assert_eq!("Value: 3c", format!("Value: {:x}", u65::new(60)));
}

#[test]
fn upper_hex() {
    assert_eq!("Value: A", format!("Value: {:X}", 10u32));
    assert_eq!("Value: A", format!("Value: {:X}", u5::new(10)));
    assert_eq!("Value: A", format!("Value: {:X}", u11::new(10)));
    assert_eq!("Value: A", format!("Value: {:X}", u17::new(10)));
    assert_eq!("Value: A", format!("Value: {:X}", u38::new(10)));
    assert_eq!("Value: 3C", format!("Value: {:X}", 60));
    assert_eq!("Value: 3C", format!("Value: {:X}", u65::new(60)));
}

#[test]
fn lower_hex_fancy() {
    assert_eq!("Value: 0xa", format!("Value: {:#x}", 10u32));
    assert_eq!("Value: 0xa", format!("Value: {:#x}", u5::new(10)));
    assert_eq!("Value: 0xa", format!("Value: {:#x}", u11::new(10)));
    assert_eq!("Value: 0xa", format!("Value: {:#x}", u17::new(10)));
    assert_eq!("Value: 0xa", format!("Value: {:#x}", u38::new(10)));
    assert_eq!("Value: 0x3c", format!("Value: {:#x}", 60));
    assert_eq!("Value: 0x3c", format!("Value: {:#x}", u65::new(60)));
}

#[test]
fn upper_hex_fancy() {
    assert_eq!("Value: 0xA", format!("Value: {:#X}", 10u32));
    assert_eq!("Value: 0xA", format!("Value: {:#X}", u5::new(10)));
    assert_eq!("Value: 0xA", format!("Value: {:#X}", u11::new(10)));
    assert_eq!("Value: 0xA", format!("Value: {:#X}", u17::new(10)));
    assert_eq!("Value: 0xA", format!("Value: {:#X}", u38::new(10)));
    assert_eq!("Value: 0x3C", format!("Value: {:#X}", 60));
    assert_eq!("Value: 0x3C", format!("Value: {:#X}", u65::new(60)));
}

#[test]
fn debug_lower_hex_fancy() {
    assert_eq!("Value: 0xa", format!("Value: {:#x?}", 10u32));
    assert_eq!("Value: 0xa", format!("Value: {:#x?}", u5::new(10)));
    assert_eq!("Value: 0xa", format!("Value: {:#x?}", u11::new(10)));
    assert_eq!("Value: 0xa", format!("Value: {:#x?}", u17::new(10)));
    assert_eq!("Value: 0xa", format!("Value: {:#x?}", u38::new(10)));
    assert_eq!("Value: 0x3c", format!("Value: {:#x?}", 60));
    assert_eq!("Value: 0x3c", format!("Value: {:#x?}", u65::new(60)));
}

#[test]
fn debug_upper_hex_fancy() {
    assert_eq!("Value: 0xA", format!("Value: {:#X?}", 10u32));
    assert_eq!("Value: 0xA", format!("Value: {:#X?}", u5::new(10)));
    assert_eq!("Value: 0xA", format!("Value: {:#X?}", u11::new(10)));
    assert_eq!("Value: 0xA", format!("Value: {:#X?}", u17::new(10)));
    assert_eq!("Value: 0xA", format!("Value: {:#X?}", u38::new(10)));
    assert_eq!("Value: 0x3C", format!("Value: {:#X?}", 60));
    assert_eq!("Value: 0x3C", format!("Value: {:#X?}", u65::new(60)));
}

#[test]
fn octal() {
    assert_eq!("Value: 12", format!("Value: {:o}", 10u32));
    assert_eq!("Value: 12", format!("Value: {:o}", u5::new(10)));
    assert_eq!("Value: 12", format!("Value: {:o}", u11::new(10)));
    assert_eq!("Value: 12", format!("Value: {:o}", u17::new(10)));
    assert_eq!("Value: 12", format!("Value: {:o}", u38::new(10)));
    assert_eq!("Value: 74", format!("Value: {:o}", 0o74));
    assert_eq!("Value: 74", format!("Value: {:o}", u65::new(0o74)));
}

#[test]
fn binary() {
    assert_eq!("Value: 1010", format!("Value: {:b}", 10u32));
    assert_eq!("Value: 1010", format!("Value: {:b}", u5::new(10)));
    assert_eq!("Value: 1010", format!("Value: {:b}", u11::new(10)));
    assert_eq!("Value: 1010", format!("Value: {:b}", u17::new(10)));
    assert_eq!("Value: 1010", format!("Value: {:b}", u38::new(10)));
    assert_eq!("Value: 111100", format!("Value: {:b}", 0b111100));
    assert_eq!("Value: 111100", format!("Value: {:b}", u65::new(0b111100)));
}

#[test]
fn hash() {
    let mut hashmap = HashMap::<u5, u7>::new();

    hashmap.insert(u5::new(11), u7::new(9));

    assert_eq!(Some(&u7::new(9)), hashmap.get(&u5::new(11)));
    assert_eq!(None, hashmap.get(&u5::new(12)));
}

#[test]
fn swap_bytes() {
    assert_eq!(u24::new(0x12_34_56).swap_bytes(), u24::new(0x56_34_12));
    assert_eq!(
        UInt::<u64, 24>::new(0x12_34_56).swap_bytes(),
        UInt::<u64, 24>::new(0x56_34_12)
    );
    assert_eq!(
        UInt::<u128, 24>::new(0x12_34_56).swap_bytes(),
        UInt::<u128, 24>::new(0x56_34_12)
    );

    assert_eq!(
        u40::new(0x12_34_56_78_9A).swap_bytes(),
        u40::new(0x9A_78_56_34_12)
    );
    assert_eq!(
        UInt::<u128, 40>::new(0x12_34_56_78_9A).swap_bytes(),
        UInt::<u128, 40>::new(0x9A_78_56_34_12)
    );

    assert_eq!(
        u48::new(0x12_34_56_78_9A_BC).swap_bytes(),
        u48::new(0xBC_9A_78_56_34_12)
    );
    assert_eq!(
        UInt::<u128, 48>::new(0x12_34_56_78_9A_BC).swap_bytes(),
        UInt::<u128, 48>::new(0xBC_9A_78_56_34_12)
    );

    assert_eq!(
        u56::new(0x12_34_56_78_9A_BC_DE).swap_bytes(),
        u56::new(0xDE_BC_9A_78_56_34_12)
    );
    assert_eq!(
        UInt::<u128, 56>::new(0x12_34_56_78_9A_BC_DE).swap_bytes(),
        UInt::<u128, 56>::new(0xDE_BC_9A_78_56_34_12)
    );

    assert_eq!(
        u72::new(0x12_34_56_78_9A_BC_DE_FE_DC).swap_bytes(),
        u72::new(0xDC_FE_DE_BC_9A_78_56_34_12)
    );

    assert_eq!(
        u80::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA).swap_bytes(),
        u80::new(0xBA_DC_FE_DE_BC_9A_78_56_34_12)
    );

    assert_eq!(
        u88::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98).swap_bytes(),
        u88::new(0x98_BA_DC_FE_DE_BC_9A_78_56_34_12)
    );

    assert_eq!(
        u96::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76).swap_bytes(),
        u96::new(0x76_98_BA_DC_FE_DE_BC_9A_78_56_34_12)
    );

    assert_eq!(
        u104::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54).swap_bytes(),
        u104::new(0x54_76_98_BA_DC_FE_DE_BC_9A_78_56_34_12)
    );

    assert_eq!(
        u112::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54_32).swap_bytes(),
        u112::new(0x32_54_76_98_BA_DC_FE_DE_BC_9A_78_56_34_12)
    );

    assert_eq!(
        u120::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54_32_10).swap_bytes(),
        u120::new(0x10_32_54_76_98_BA_DC_FE_DE_BC_9A_78_56_34_12)
    );
}

#[test]
fn to_le_and_be_bytes() {
    assert_eq!(u24::new(0x12_34_56).to_le_bytes(), [0x56, 0x34, 0x12]);
    assert_eq!(
        UInt::<u64, 24>::new(0x12_34_56).to_le_bytes(),
        [0x56, 0x34, 0x12]
    );
    assert_eq!(
        UInt::<u128, 24>::new(0x12_34_56).to_le_bytes(),
        [0x56, 0x34, 0x12]
    );

    assert_eq!(u24::new(0x12_34_56).to_be_bytes(), [0x12, 0x34, 0x56]);
    assert_eq!(
        UInt::<u64, 24>::new(0x12_34_56).to_be_bytes(),
        [0x12, 0x34, 0x56]
    );
    assert_eq!(
        UInt::<u128, 24>::new(0x12_34_56).to_be_bytes(),
        [0x12, 0x34, 0x56]
    );

    assert_eq!(
        u40::new(0x12_34_56_78_9A).to_le_bytes(),
        [0x9A, 0x78, 0x56, 0x34, 0x12]
    );
    assert_eq!(
        UInt::<u128, 40>::new(0x12_34_56_78_9A).to_le_bytes(),
        [0x9A, 0x78, 0x56, 0x34, 0x12]
    );

    assert_eq!(
        u40::new(0x12_34_56_78_9A).to_be_bytes(),
        [0x12, 0x34, 0x56, 0x78, 0x9A]
    );
    assert_eq!(
        UInt::<u128, 40>::new(0x12_34_56_78_9A).to_be_bytes(),
        [0x12, 0x34, 0x56, 0x78, 0x9A]
    );

    assert_eq!(
        u48::new(0x12_34_56_78_9A_BC).to_le_bytes(),
        [0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]
    );
    assert_eq!(
        UInt::<u128, 48>::new(0x12_34_56_78_9A_BC).to_le_bytes(),
        [0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]
    );

    assert_eq!(
        u48::new(0x12_34_56_78_9A_BC).to_be_bytes(),
        [0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC]
    );
    assert_eq!(
        UInt::<u128, 48>::new(0x12_34_56_78_9A_BC).to_be_bytes(),
        [0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC]
    );

    assert_eq!(
        u56::new(0x12_34_56_78_9A_BC_DE).to_le_bytes(),
        [0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]
    );
    assert_eq!(
        UInt::<u128, 56>::new(0x12_34_56_78_9A_BC_DE).to_le_bytes(),
        [0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]
    );

    assert_eq!(
        u56::new(0x12_34_56_78_9A_BC_DE).to_be_bytes(),
        [0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE]
    );
    assert_eq!(
        UInt::<u128, 56>::new(0x12_34_56_78_9A_BC_DE).to_be_bytes(),
        [0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE]
    );

    assert_eq!(
        u72::new(0x12_34_56_78_9A_BC_DE_FE_DC).to_le_bytes(),
        [0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]
    );

    assert_eq!(
        u72::new(0x12_34_56_78_9A_BC_DE_FE_DC).to_be_bytes(),
        [0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC]
    );

    assert_eq!(
        u80::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA).to_le_bytes(),
        [0xBA, 0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]
    );

    assert_eq!(
        u80::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA).to_be_bytes(),
        [0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC, 0xBA]
    );

    assert_eq!(
        u88::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98).to_le_bytes(),
        [0x98, 0xBA, 0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]
    );

    assert_eq!(
        u88::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98).to_be_bytes(),
        [0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC, 0xBA, 0x98]
    );

    assert_eq!(
        u96::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76).to_le_bytes(),
        [0x76, 0x98, 0xBA, 0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]
    );

    assert_eq!(
        u96::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76).to_be_bytes(),
        [0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC, 0xBA, 0x98, 0x76]
    );

    assert_eq!(
        u104::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54).to_le_bytes(),
        [0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]
    );

    assert_eq!(
        u104::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54).to_be_bytes(),
        [0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54]
    );

    assert_eq!(
        u112::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54_32).to_le_bytes(),
        [0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]
    );

    assert_eq!(
        u112::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54_32).to_be_bytes(),
        [0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32]
    );

    assert_eq!(
        u120::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54_32_10).to_le_bytes(),
        [
            0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34,
            0x12
        ]
    );

    assert_eq!(
        u120::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54_32_10).to_be_bytes(),
        [
            0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32,
            0x10
        ]
    );
}

#[test]
fn from_le_and_be_bytes() {
    assert_eq!(u24::from_le_bytes([0x56, 0x34, 0x12]), u24::new(0x12_34_56));
    assert_eq!(
        UInt::<u64, 24>::from_le_bytes([0x56, 0x34, 0x12]),
        UInt::<u64, 24>::new(0x12_34_56)
    );
    assert_eq!(
        UInt::<u128, 24>::from_le_bytes([0x56, 0x34, 0x12]),
        UInt::<u128, 24>::new(0x12_34_56)
    );

    assert_eq!(u24::from_be_bytes([0x12, 0x34, 0x56]), u24::new(0x12_34_56));
    assert_eq!(
        UInt::<u64, 24>::from_be_bytes([0x12, 0x34, 0x56]),
        UInt::<u64, 24>::new(0x12_34_56)
    );
    assert_eq!(
        UInt::<u128, 24>::from_be_bytes([0x12, 0x34, 0x56]),
        UInt::<u128, 24>::new(0x12_34_56)
    );

    assert_eq!(
        u40::from_le_bytes([0x9A, 0x78, 0x56, 0x34, 0x12]),
        u40::new(0x12_34_56_78_9A)
    );
    assert_eq!(
        UInt::<u128, 40>::from_le_bytes([0x9A, 0x78, 0x56, 0x34, 0x12]),
        UInt::<u128, 40>::new(0x12_34_56_78_9A)
    );

    assert_eq!(
        u40::from_be_bytes([0x12, 0x34, 0x56, 0x78, 0x9A]),
        u40::new(0x12_34_56_78_9A)
    );
    assert_eq!(
        UInt::<u128, 40>::from_be_bytes([0x12, 0x34, 0x56, 0x78, 0x9A]),
        UInt::<u128, 40>::new(0x12_34_56_78_9A)
    );

    assert_eq!(
        u48::from_le_bytes([0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]),
        u48::new(0x12_34_56_78_9A_BC)
    );
    assert_eq!(
        UInt::<u128, 48>::from_le_bytes([0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]),
        UInt::<u128, 48>::new(0x12_34_56_78_9A_BC)
    );

    assert_eq!(
        u48::from_be_bytes([0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC]),
        u48::new(0x12_34_56_78_9A_BC)
    );
    assert_eq!(
        UInt::<u128, 48>::from_be_bytes([0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC]),
        UInt::<u128, 48>::new(0x12_34_56_78_9A_BC)
    );

    assert_eq!(
        u56::from_le_bytes([0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]),
        u56::new(0x12_34_56_78_9A_BC_DE)
    );
    assert_eq!(
        UInt::<u128, 56>::from_le_bytes([0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]),
        UInt::<u128, 56>::new(0x12_34_56_78_9A_BC_DE)
    );

    assert_eq!(
        u56::from_be_bytes([0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE]),
        u56::new(0x12_34_56_78_9A_BC_DE)
    );
    assert_eq!(
        UInt::<u128, 56>::from_be_bytes([0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE]),
        UInt::<u128, 56>::new(0x12_34_56_78_9A_BC_DE)
    );

    assert_eq!(
        u72::from_le_bytes([0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]),
        u72::new(0x12_34_56_78_9A_BC_DE_FE_DC)
    );

    assert_eq!(
        u72::from_be_bytes([0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC]),
        u72::new(0x12_34_56_78_9A_BC_DE_FE_DC)
    );

    assert_eq!(
        u80::from_le_bytes([0xBA, 0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]),
        u80::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA)
    );

    assert_eq!(
        u80::from_be_bytes([0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC, 0xBA]),
        u80::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA)
    );

    assert_eq!(
        u88::from_le_bytes([0x98, 0xBA, 0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12]),
        u88::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98)
    );

    assert_eq!(
        u88::from_be_bytes([0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC, 0xBA, 0x98]),
        u88::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98)
    );

    assert_eq!(
        u96::from_le_bytes([
            0x76, 0x98, 0xBA, 0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12
        ]),
        u96::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76)
    );

    assert_eq!(
        u96::from_be_bytes([
            0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC, 0xBA, 0x98, 0x76
        ]),
        u96::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76)
    );

    assert_eq!(
        u104::from_le_bytes([
            0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12
        ]),
        u104::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54)
    );

    assert_eq!(
        u104::from_be_bytes([
            0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54
        ]),
        u104::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54)
    );

    assert_eq!(
        u112::from_le_bytes([
            0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12
        ]),
        u112::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54_32)
    );

    assert_eq!(
        u112::from_be_bytes([
            0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32
        ]),
        u112::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54_32)
    );

    assert_eq!(
        u120::from_le_bytes([
            0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34,
            0x12
        ]),
        u120::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54_32_10)
    );

    assert_eq!(
        u120::from_be_bytes([
            0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32,
            0x10
        ]),
        u120::new(0x12_34_56_78_9A_BC_DE_FE_DC_BA_98_76_54_32_10)
    );
}

#[test]
fn to_ne_bytes() {
    if cfg!(target_endian = "little") {
        assert_eq!(
            u40::new(0x12_34_56_78_9A).to_ne_bytes(),
            [0x9A, 0x78, 0x56, 0x34, 0x12]
        );
    } else {
        assert_eq!(
            u40::new(0x12_34_56_78_9A).to_ne_bytes(),
            [0x12, 0x34, 0x56, 0x78, 0x9A]
        );
    }
}

#[test]
fn from_ne_bytes() {
    if cfg!(target_endian = "little") {
        assert_eq!(
            u40::from_ne_bytes([0x9A, 0x78, 0x56, 0x34, 0x12]),
            u40::new(0x12_34_56_78_9A)
        );
    } else {
        assert_eq!(
            u40::from_ne_bytes([0x12, 0x34, 0x56, 0x78, 0x9A]),
            u40::new(0x12_34_56_78_9A)
        );
    }
}

#[test]
fn simple_le_be() {
    const REGULAR: u40 = u40::new(0x12_34_56_78_9A);
    const SWAPPED: u40 = u40::new(0x9A_78_56_34_12);
    if cfg!(target_endian = "little") {
        assert_eq!(REGULAR.to_le(), REGULAR);
        assert_eq!(REGULAR.to_be(), SWAPPED);
        assert_eq!(u40::from_le(REGULAR), REGULAR);
        assert_eq!(u40::from_be(REGULAR), SWAPPED);
    } else {
        assert_eq!(REGULAR.to_le(), SWAPPED);
        assert_eq!(REGULAR.to_be(), REGULAR);
        assert_eq!(u40::from_le(REGULAR), SWAPPED);
        assert_eq!(u40::from_be(REGULAR), REGULAR);
    }
}

#[test]
fn wrapping_add() {
    assert_eq!(u7::new(120).wrapping_add(u7::new(1)), u7::new(121));
    assert_eq!(u7::new(120).wrapping_add(u7::new(10)), u7::new(2));
    assert_eq!(u7::new(127).wrapping_add(u7::new(127)), u7::new(126));
}

#[test]
fn wrapping_sub() {
    assert_eq!(u7::new(120).wrapping_sub(u7::new(1)), u7::new(119));
    assert_eq!(u7::new(10).wrapping_sub(u7::new(20)), u7::new(118));
    assert_eq!(u7::new(0).wrapping_sub(u7::new(1)), u7::new(127));
}

#[test]
fn wrapping_mul() {
    assert_eq!(u7::new(120).wrapping_mul(u7::new(0)), u7::new(0));
    assert_eq!(u7::new(120).wrapping_mul(u7::new(1)), u7::new(120));

    // Overflow u7
    assert_eq!(u7::new(120).wrapping_mul(u7::new(2)), u7::new(112));

    // Overflow the underlying type
    assert_eq!(u7::new(120).wrapping_mul(u7::new(3)), u7::new(104));
}

#[test]
fn wrapping_div() {
    assert_eq!(u7::new(120).wrapping_div(u7::new(1)), u7::new(120));
    assert_eq!(u7::new(120).wrapping_div(u7::new(2)), u7::new(60));
    assert_eq!(u7::new(120).wrapping_div(u7::new(120)), u7::new(1));
    assert_eq!(u7::new(120).wrapping_div(u7::new(121)), u7::new(0));
}

#[should_panic]
#[test]
fn wrapping_div_by_zero() {
    let _ = u7::new(120).wrapping_div(u7::new(0));
}

#[test]
fn wrapping_shl() {
    assert_eq!(u7::new(0b010_1101).wrapping_shl(0), u7::new(0b010_1101));
    assert_eq!(u7::new(0b010_1101).wrapping_shl(1), u7::new(0b101_1010));
    assert_eq!(u7::new(0b010_1101).wrapping_shl(6), u7::new(0b100_0000));
    assert_eq!(u7::new(0b010_1101).wrapping_shl(7), u7::new(0b010_1101));
    assert_eq!(u7::new(0b010_1101).wrapping_shl(8), u7::new(0b101_1010));
    assert_eq!(u7::new(0b010_1101).wrapping_shl(14), u7::new(0b010_1101));
    assert_eq!(u7::new(0b010_1101).wrapping_shl(15), u7::new(0b101_1010));
}

#[test]
fn wrapping_shr() {
    assert_eq!(u7::new(0b010_1101).wrapping_shr(0), u7::new(0b010_1101));
    assert_eq!(u7::new(0b010_1101).wrapping_shr(1), u7::new(0b001_0110));
    assert_eq!(u7::new(0b010_1101).wrapping_shr(5), u7::new(0b000_0001));
    assert_eq!(u7::new(0b010_1101).wrapping_shr(7), u7::new(0b010_1101));
    assert_eq!(u7::new(0b010_1101).wrapping_shr(8), u7::new(0b001_0110));
    assert_eq!(u7::new(0b010_1101).wrapping_shr(14), u7::new(0b010_1101));
    assert_eq!(u7::new(0b010_1101).wrapping_shr(15), u7::new(0b001_0110));
}

#[test]
fn saturating_add() {
    assert_eq!(u7::new(120).saturating_add(u7::new(1)), u7::new(121));
    assert_eq!(u7::new(120).saturating_add(u7::new(10)), u7::new(127));
    assert_eq!(u7::new(127).saturating_add(u7::new(127)), u7::new(127));
    assert_eq!(
        UInt::<u8, 8>::new(250).saturating_add(UInt::<u8, 8>::new(10)),
        UInt::<u8, 8>::new(255)
    );
}

#[test]
fn saturating_sub() {
    assert_eq!(u7::new(120).saturating_sub(u7::new(30)), u7::new(90));
    assert_eq!(u7::new(120).saturating_sub(u7::new(119)), u7::new(1));
    assert_eq!(u7::new(120).saturating_sub(u7::new(120)), u7::new(0));
    assert_eq!(u7::new(120).saturating_sub(u7::new(121)), u7::new(0));
    assert_eq!(u7::new(0).saturating_sub(u7::new(127)), u7::new(0));
}

#[test]
fn saturating_mul() {
    // Fast-path: Only the arbitrary int is bounds checked
    assert_eq!(u4::new(5).saturating_mul(u4::new(2)), u4::new(10));
    assert_eq!(u4::new(5).saturating_mul(u4::new(3)), u4::new(15));
    assert_eq!(u4::new(5).saturating_mul(u4::new(4)), u4::new(15));
    assert_eq!(u4::new(5).saturating_mul(u4::new(5)), u4::new(15));
    assert_eq!(u4::new(5).saturating_mul(u4::new(6)), u4::new(15));
    assert_eq!(u4::new(5).saturating_mul(u4::new(7)), u4::new(15));

    // Slow-path (well, one more comparison)
    assert_eq!(u5::new(5).saturating_mul(u5::new(2)), u5::new(10));
    assert_eq!(u5::new(5).saturating_mul(u5::new(3)), u5::new(15));
    assert_eq!(u5::new(5).saturating_mul(u5::new(4)), u5::new(20));
    assert_eq!(u5::new(5).saturating_mul(u5::new(5)), u5::new(25));
    assert_eq!(u5::new(5).saturating_mul(u5::new(6)), u5::new(30));
    assert_eq!(u5::new(5).saturating_mul(u5::new(7)), u5::new(31));
    assert_eq!(u5::new(30).saturating_mul(u5::new(1)), u5::new(30));
    assert_eq!(u5::new(30).saturating_mul(u5::new(2)), u5::new(31));
    assert_eq!(u5::new(30).saturating_mul(u5::new(10)), u5::new(31));
}

#[test]
fn saturating_div() {
    assert_eq!(u4::new(5).saturating_div(u4::new(1)), u4::new(5));
    assert_eq!(u4::new(5).saturating_div(u4::new(2)), u4::new(2));
    assert_eq!(u4::new(5).saturating_div(u4::new(3)), u4::new(1));
    assert_eq!(u4::new(5).saturating_div(u4::new(4)), u4::new(1));
    assert_eq!(u4::new(5).saturating_div(u4::new(5)), u4::new(1));
}

#[test]
#[should_panic]
fn saturating_divby0() {
    // saturating_div throws an exception on zero
    let _ = u4::new(5).saturating_div(u4::new(0));
}

#[test]
fn saturating_pow() {
    assert_eq!(u7::new(5).saturating_pow(0), u7::new(1));
    assert_eq!(u7::new(5).saturating_pow(1), u7::new(5));
    assert_eq!(u7::new(5).saturating_pow(2), u7::new(25));
    assert_eq!(u7::new(5).saturating_pow(3), u7::new(125));
    assert_eq!(u7::new(5).saturating_pow(4), u7::new(127));
    assert_eq!(u7::new(5).saturating_pow(255), u7::new(127));
}

#[test]
fn checked_add() {
    assert_eq!(u7::new(120).checked_add(u7::new(1)), Some(u7::new(121)));
    assert_eq!(u7::new(120).checked_add(u7::new(7)), Some(u7::new(127)));
    assert_eq!(u7::new(120).checked_add(u7::new(10)), None);
    assert_eq!(u7::new(127).checked_add(u7::new(127)), None);
    assert_eq!(
        UInt::<u8, 8>::new(250).checked_add(UInt::<u8, 8>::new(10)),
        None
    );
}

#[test]
fn checked_sub() {
    assert_eq!(u7::new(120).checked_sub(u7::new(30)), Some(u7::new(90)));
    assert_eq!(u7::new(120).checked_sub(u7::new(119)), Some(u7::new(1)));
    assert_eq!(u7::new(120).checked_sub(u7::new(120)), Some(u7::new(0)));
    assert_eq!(u7::new(120).checked_sub(u7::new(121)), None);
    assert_eq!(u7::new(0).checked_sub(u7::new(127)), None);
}

#[test]
fn checked_mul() {
    // Fast-path: Only the arbitrary int is bounds checked
    assert_eq!(u4::new(5).checked_mul(u4::new(2)), Some(u4::new(10)));
    assert_eq!(u4::new(5).checked_mul(u4::new(3)), Some(u4::new(15)));
    assert_eq!(u4::new(5).checked_mul(u4::new(4)), None);
    assert_eq!(u4::new(5).checked_mul(u4::new(5)), None);
    assert_eq!(u4::new(5).checked_mul(u4::new(6)), None);
    assert_eq!(u4::new(5).checked_mul(u4::new(7)), None);

    // Slow-path (well, one more comparison)
    assert_eq!(u5::new(5).checked_mul(u5::new(2)), Some(u5::new(10)));
    assert_eq!(u5::new(5).checked_mul(u5::new(3)), Some(u5::new(15)));
    assert_eq!(u5::new(5).checked_mul(u5::new(4)), Some(u5::new(20)));
    assert_eq!(u5::new(5).checked_mul(u5::new(5)), Some(u5::new(25)));
    assert_eq!(u5::new(5).checked_mul(u5::new(6)), Some(u5::new(30)));
    assert_eq!(u5::new(5).checked_mul(u5::new(7)), None);
    assert_eq!(u5::new(30).checked_mul(u5::new(1)), Some(u5::new(30)));
    assert_eq!(u5::new(30).checked_mul(u5::new(2)), None);
    assert_eq!(u5::new(30).checked_mul(u5::new(10)), None);
}

#[test]
fn checked_div() {
    // checked_div handles division by zero without exception, unlike saturating_div
    assert_eq!(u4::new(5).checked_div(u4::new(0)), None);
    assert_eq!(u4::new(5).checked_div(u4::new(1)), Some(u4::new(5)));
    assert_eq!(u4::new(5).checked_div(u4::new(2)), Some(u4::new(2)));
    assert_eq!(u4::new(5).checked_div(u4::new(3)), Some(u4::new(1)));
    assert_eq!(u4::new(5).checked_div(u4::new(4)), Some(u4::new(1)));
    assert_eq!(u4::new(5).checked_div(u4::new(5)), Some(u4::new(1)));
}

#[test]
fn checked_shl() {
    assert_eq!(
        u7::new(0b010_1101).checked_shl(0),
        Some(u7::new(0b010_1101))
    );
    assert_eq!(
        u7::new(0b010_1101).checked_shl(1),
        Some(u7::new(0b101_1010))
    );
    assert_eq!(
        u7::new(0b010_1101).checked_shl(6),
        Some(u7::new(0b100_0000))
    );
    assert_eq!(u7::new(0b010_1101).checked_shl(7), None);
    assert_eq!(u7::new(0b010_1101).checked_shl(8), None);
    assert_eq!(u7::new(0b010_1101).checked_shl(14), None);
    assert_eq!(u7::new(0b010_1101).checked_shl(15), None);
}

#[test]
fn checked_shr() {
    assert_eq!(
        u7::new(0b010_1101).checked_shr(0),
        Some(u7::new(0b010_1101))
    );
    assert_eq!(
        u7::new(0b010_1101).checked_shr(1),
        Some(u7::new(0b001_0110))
    );
    assert_eq!(
        u7::new(0b010_1101).checked_shr(5),
        Some(u7::new(0b000_0001))
    );
    assert_eq!(u7::new(0b010_1101).checked_shr(7), None);
    assert_eq!(u7::new(0b010_1101).checked_shr(8), None);
    assert_eq!(u7::new(0b010_1101).checked_shr(14), None);
    assert_eq!(u7::new(0b010_1101).checked_shr(15), None);
}

#[test]
fn overflowing_add() {
    assert_eq!(
        u7::new(120).overflowing_add(u7::new(1)),
        (u7::new(121), false)
    );
    assert_eq!(
        u7::new(120).overflowing_add(u7::new(7)),
        (u7::new(127), false)
    );
    assert_eq!(
        u7::new(120).overflowing_add(u7::new(10)),
        (u7::new(2), true)
    );
    assert_eq!(
        u7::new(127).overflowing_add(u7::new(127)),
        (u7::new(126), true)
    );
    assert_eq!(
        UInt::<u8, 8>::new(250).overflowing_add(UInt::<u8, 8>::new(5)),
        (UInt::<u8, 8>::new(255), false)
    );
    assert_eq!(
        UInt::<u8, 8>::new(250).overflowing_add(UInt::<u8, 8>::new(10)),
        (UInt::<u8, 8>::new(4), true)
    );
}

#[test]
fn overflowing_sub() {
    assert_eq!(
        u7::new(120).overflowing_sub(u7::new(30)),
        (u7::new(90), false)
    );
    assert_eq!(
        u7::new(120).overflowing_sub(u7::new(119)),
        (u7::new(1), false)
    );
    assert_eq!(
        u7::new(120).overflowing_sub(u7::new(120)),
        (u7::new(0), false)
    );
    assert_eq!(
        u7::new(120).overflowing_sub(u7::new(121)),
        (u7::new(127), true)
    );
    assert_eq!(u7::new(0).overflowing_sub(u7::new(127)), (u7::new(1), true));
}

#[test]
fn overflowing_mul() {
    // Fast-path: Only the arbitrary int is bounds checked
    assert_eq!(u4::new(5).overflowing_mul(u4::new(2)), (u4::new(10), false));
    assert_eq!(u4::new(5).overflowing_mul(u4::new(3)), (u4::new(15), false));
    assert_eq!(u4::new(5).overflowing_mul(u4::new(4)), (u4::new(4), true));
    assert_eq!(u4::new(5).overflowing_mul(u4::new(5)), (u4::new(9), true));
    assert_eq!(u4::new(5).overflowing_mul(u4::new(6)), (u4::new(14), true));
    assert_eq!(u4::new(5).overflowing_mul(u4::new(7)), (u4::new(3), true));

    // Slow-path (well, one more comparison)
    assert_eq!(u5::new(5).overflowing_mul(u5::new(2)), (u5::new(10), false));
    assert_eq!(u5::new(5).overflowing_mul(u5::new(3)), (u5::new(15), false));
    assert_eq!(u5::new(5).overflowing_mul(u5::new(4)), (u5::new(20), false));
    assert_eq!(u5::new(5).overflowing_mul(u5::new(5)), (u5::new(25), false));
    assert_eq!(u5::new(5).overflowing_mul(u5::new(6)), (u5::new(30), false));
    assert_eq!(u5::new(5).overflowing_mul(u5::new(7)), (u5::new(3), true));
    assert_eq!(
        u5::new(30).overflowing_mul(u5::new(1)),
        (u5::new(30), false)
    );
    assert_eq!(u5::new(30).overflowing_mul(u5::new(2)), (u5::new(28), true));
    assert_eq!(
        u5::new(30).overflowing_mul(u5::new(10)),
        (u5::new(12), true)
    );
}

#[test]
fn overflowing_div() {
    assert_eq!(u4::new(5).overflowing_div(u4::new(1)), (u4::new(5), false));
    assert_eq!(u4::new(5).overflowing_div(u4::new(2)), (u4::new(2), false));
    assert_eq!(u4::new(5).overflowing_div(u4::new(3)), (u4::new(1), false));
    assert_eq!(u4::new(5).overflowing_div(u4::new(4)), (u4::new(1), false));
    assert_eq!(u4::new(5).overflowing_div(u4::new(5)), (u4::new(1), false));
}

#[should_panic]
#[test]
fn overflowing_div_by_zero() {
    let _ = u4::new(5).overflowing_div(u4::new(0));
}

#[test]
fn overflowing_shl() {
    assert_eq!(
        u7::new(0b010_1101).overflowing_shl(0),
        (u7::new(0b010_1101), false)
    );
    assert_eq!(
        u7::new(0b010_1101).overflowing_shl(1),
        (u7::new(0b101_1010), false)
    );
    assert_eq!(
        u7::new(0b010_1101).overflowing_shl(6),
        (u7::new(0b100_0000), false)
    );
    assert_eq!(
        u7::new(0b010_1101).overflowing_shl(7),
        (u7::new(0b010_1101), true)
    );
    assert_eq!(
        u7::new(0b010_1101).overflowing_shl(8),
        (u7::new(0b101_1010), true)
    );
    assert_eq!(
        u7::new(0b010_1101).overflowing_shl(14),
        (u7::new(0b010_1101), true)
    );
    assert_eq!(
        u7::new(0b010_1101).overflowing_shl(15),
        (u7::new(0b101_1010), true)
    );
}

#[test]
fn overflowing_shr() {
    assert_eq!(
        u7::new(0b010_1101).overflowing_shr(0),
        (u7::new(0b010_1101), false)
    );
    assert_eq!(
        u7::new(0b010_1101).overflowing_shr(1),
        (u7::new(0b001_0110), false)
    );
    assert_eq!(
        u7::new(0b010_1101).overflowing_shr(5),
        (u7::new(0b000_0001), false)
    );
    assert_eq!(
        u7::new(0b010_1101).overflowing_shr(7),
        (u7::new(0b010_1101), true)
    );
    assert_eq!(
        u7::new(0b010_1101).overflowing_shr(8),
        (u7::new(0b001_0110), true)
    );
    assert_eq!(
        u7::new(0b010_1101).overflowing_shr(14),
        (u7::new(0b010_1101), true)
    );
    assert_eq!(
        u7::new(0b010_1101).overflowing_shr(15),
        (u7::new(0b001_0110), true)
    );
}

#[test]
fn reverse_bits() {
    const A: u5 = u5::new(0b11101);
    const B: u5 = A.reverse_bits();
    assert_eq!(B, u5::new(0b10111));

    assert_eq!(
        UInt::<u128, 6>::new(0b101011),
        UInt::<u128, 6>::new(0b110101).reverse_bits()
    );

    assert_eq!(u1::new(1).reverse_bits().value(), 1);
    assert_eq!(u1::new(0).reverse_bits().value(), 0);
}

#[test]
fn count_ones_and_zeros() {
    assert_eq!(4, u5::new(0b10111).count_ones());
    assert_eq!(1, u5::new(0b10111).count_zeros());
    assert_eq!(1, u5::new(0b10111).leading_ones());
    assert_eq!(0, u5::new(0b10111).leading_zeros());
    assert_eq!(3, u5::new(0b10111).trailing_ones());
    assert_eq!(0, u5::new(0b10111).trailing_zeros());

    assert_eq!(2, u5::new(0b10100).trailing_zeros());
    assert_eq!(3, u5::new(0b00011).leading_zeros());

    assert_eq!(0, u5::new(0b00000).count_ones());
    assert_eq!(5, u5::new(0b00000).count_zeros());

    assert_eq!(5, u5::new(0b11111).count_ones());
    assert_eq!(0, u5::new(0b11111).count_zeros());

    assert_eq!(3, u127::new(0b111).count_ones());
    assert_eq!(124, u127::new(0b111).count_zeros());
}

#[test]
fn rotate_left() {
    assert_eq!(u1::new(0b1), u1::new(0b1).rotate_left(1));
    assert_eq!(u2::new(0b01), u2::new(0b10).rotate_left(1));

    assert_eq!(u5::new(0b10111), u5::new(0b10111).rotate_left(0));
    assert_eq!(u5::new(0b01111), u5::new(0b10111).rotate_left(1));
    assert_eq!(u5::new(0b11110), u5::new(0b10111).rotate_left(2));
    assert_eq!(u5::new(0b11101), u5::new(0b10111).rotate_left(3));
    assert_eq!(u5::new(0b11011), u5::new(0b10111).rotate_left(4));
    assert_eq!(u5::new(0b10111), u5::new(0b10111).rotate_left(5));
    assert_eq!(u5::new(0b01111), u5::new(0b10111).rotate_left(6));
    assert_eq!(u5::new(0b01111), u5::new(0b10111).rotate_left(556));

    assert_eq!(u24::new(0x0FFEEC), u24::new(0xC0FFEE).rotate_left(4));
}

#[test]
fn rotate_right() {
    assert_eq!(u1::new(0b1), u1::new(0b1).rotate_right(1));
    assert_eq!(u2::new(0b01), u2::new(0b10).rotate_right(1));

    assert_eq!(u5::new(0b10011), u5::new(0b10011).rotate_right(0));
    assert_eq!(u5::new(0b11001), u5::new(0b10011).rotate_right(1));
    assert_eq!(u5::new(0b11100), u5::new(0b10011).rotate_right(2));
    assert_eq!(u5::new(0b01110), u5::new(0b10011).rotate_right(3));
    assert_eq!(u5::new(0b00111), u5::new(0b10011).rotate_right(4));
    assert_eq!(u5::new(0b10011), u5::new(0b10011).rotate_right(5));
    assert_eq!(u5::new(0b11001), u5::new(0b10011).rotate_right(6));

    assert_eq!(u24::new(0xEC0FFE), u24::new(0xC0FFEE).rotate_right(4));
}

#[cfg(feature = "step_trait")]
#[test]
fn range_agrees_with_underlying() {
    compare_range_32(u19::MIN, u19::MAX);
    compare_range_64(u37::new(95_993), u37::new(1_994_910));
    compare_range_128(u68::new(58_858_348), u68::new(58_860_000));
    compare_range_128(u122::new(111_222_333_444), u122::new(111_222_444_555));
    compare_range_32(u23::MIN, u23::MAX);
    compare_range_64(u48::new(999_444), u48::new(1_005_000));
    compare_range_128(u99::new(12345), u99::new(54321));

    // with the `hint` feature enabled, ::value only exist with primitive types, not on all
    // implementations. This makes some copy & paste necessary here.
    fn compare_range_32<const BITS: usize>(arb_start: UInt<u32, BITS>, arb_end: UInt<u32, BITS>)
    where
        UInt<u32, BITS>: Step,
    {
        let arbint_range = (arb_start..=arb_end).map(UInt::<u32, BITS>::value);
        let underlying_range = arb_start.value()..=arb_end.value();

        assert!(arbint_range.eq(underlying_range));
    }
    fn compare_range_64<const BITS: usize>(arb_start: UInt<u64, BITS>, arb_end: UInt<u64, BITS>)
    where
        UInt<u64, BITS>: Step,
    {
        let arbint_range = (arb_start..=arb_end).map(UInt::<u64, BITS>::value);
        let underlying_range = arb_start.value()..=arb_end.value();

        assert!(arbint_range.eq(underlying_range));
    }
    fn compare_range_128<const BITS: usize>(arb_start: UInt<u128, BITS>, arb_end: UInt<u128, BITS>)
    where
        UInt<u128, BITS>: Step,
    {
        let arbint_range = (arb_start..=arb_end).map(UInt::<u128, BITS>::value);
        let underlying_range = arb_start.value()..=arb_end.value();

        assert!(arbint_range.eq(underlying_range));
    }
}

#[cfg(feature = "step_trait")]
#[test]
fn forward_checked() {
    // In range
    assert_eq!(Some(u7::new(121)), Step::forward_checked(u7::new(120), 1));
    assert_eq!(Some(u7::new(127)), Step::forward_checked(u7::new(120), 7));

    // Out of range
    assert_eq!(None, Step::forward_checked(u7::new(120), 8));

    // Out of range for the underlying type
    assert_eq!(None, Step::forward_checked(u7::new(120), 140));
}

#[cfg(feature = "step_trait")]
#[test]
fn backward_checked() {
    // In range
    assert_eq!(Some(u7::new(1)), Step::backward_checked(u7::new(10), 9));
    assert_eq!(Some(u7::new(0)), Step::backward_checked(u7::new(10), 10));

    // Out of range (for both the arbitrary int and and the underlying type)
    assert_eq!(None, Step::backward_checked(u7::new(10), 11));
}

#[cfg(feature = "step_trait")]
#[test]
fn steps_between() {
    assert_eq!(
        (0, Some(0)),
        Step::steps_between(&u50::new(50), &u50::new(50))
    );

    assert_eq!(
        (4, Some(4)),
        Step::steps_between(&u24::new(5), &u24::new(9))
    );
    assert_eq!((0, None), Step::steps_between(&u24::new(9), &u24::new(5)));

    // this assumes usize is <= 64 bits. a test like this one exists in `core::iter::step`.
    assert_eq!(
        (usize::MAX, Some(usize::MAX)),
        Step::steps_between(&u125::new(0x7), &u125::new(0x1_0000_0000_0000_0006))
    );
    assert_eq!(
        (usize::MAX, None),
        Step::steps_between(&u125::new(0x7), &u125::new(0x1_0000_0000_0000_0007))
    );
}

#[cfg(feature = "serde")]
#[test]
fn serde() {
    use serde_test::{assert_de_tokens_error, assert_tokens, Token};

    let a = u7::new(0b0101_0101);
    assert_tokens(&a, &[Token::U8(0b0101_0101)]);

    let b = u63::new(0x1234_5678_9ABC_DEFE);
    assert_tokens(&b, &[Token::U64(0x1234_5678_9ABC_DEFE)]);

    // This requires https://github.com/serde-rs/test/issues/18 (Add Token::I128 and Token::U128 to serde_test)
    // let c = u127::new(0x1234_5678_9ABC_DEFE_DCBA_9876_5432_1010);
    // assert_tokens(&c, &[Token::U128(0x1234_5678_9ABC_DEFE_DCBA_9876_5432_1010)]);

    assert_de_tokens_error::<u2>(
        &[Token::U8(0b0101_0101)],
        "invalid value: integer `85`, expected a value between `0` and `3`",
    );

    assert_de_tokens_error::<u100>(
        &[Token::I64(-1)],
        "invalid value: integer `-1`, expected u128",
    );
}

#[cfg(feature = "borsh")]
mod borsh_tests {
    use arbitrary_int::{u1, u14, u15, u6, u63, u65, u7, u72, u79, u80, u81, u9, Number, UInt};
    use borsh::schema::BorshSchemaContainer;
    use borsh::{BorshDeserialize, BorshSchema, BorshSerialize};
    use std::fmt::Debug;

    fn test_roundtrip<T: Number + BorshSerialize + BorshDeserialize + PartialEq + Eq + Debug>(
        input: T,
        expected_buffer: &[u8],
    ) {
        let mut buf = Vec::new();

        // Serialize and compare against expected
        input.serialize(&mut buf).unwrap();
        assert_eq!(buf, expected_buffer);

        // Add to the buffer a second time - this is a better test for the deserialization
        // as it ensures we request the correct number of bytes
        input.serialize(&mut buf).unwrap();

        // Deserialize back and compare against input
        let output = T::deserialize(&mut buf.as_ref()).unwrap();
        let output2 = T::deserialize(&mut &buf[buf.len() / 2..]).unwrap();
        assert_eq!(input, output);
        assert_eq!(input, output2);
    }

    #[test]
    fn test_serialize_deserialize() {
        // Run against plain u64 first (not an arbitrary_int)
        test_roundtrip(
            0x12345678_9ABCDEF0u64,
            &[0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12],
        );

        // Now try various arbitrary ints
        test_roundtrip(u1::new(0b0), &[0]);
        test_roundtrip(u1::new(0b1), &[1]);
        test_roundtrip(u6::new(0b101101), &[0b101101]);
        test_roundtrip(u14::new(0b110101_11001101), &[0b11001101, 0b110101]);
        test_roundtrip(
            u72::new(0x36_01234567_89ABCDEF),
            &[0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x36],
        );

        // Pick a byte boundary (80; test one below and one above to ensure we get the right number
        // of bytes)
        test_roundtrip(
            u79::MAX,
            &[0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F],
        );
        test_roundtrip(
            u80::MAX,
            &[0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF],
        );
        test_roundtrip(
            u81::MAX,
            &[
                0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
            ],
        );

        // Test actual u128 and arbitrary u128 (which is a legal one, though not a predefined)
        test_roundtrip(
            u128::MAX,
            &[
                0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                0xFF, 0xFF,
            ],
        );
        test_roundtrip(
            UInt::<u128, 128>::MAX,
            &[
                0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                0xFF, 0xFF,
            ],
        );
    }

    fn verify_byte_count_in_schema<T: BorshSchema + ?Sized>(expected_byte_count: u8, name: &str) {
        let schema = BorshSchemaContainer::for_type::<T>();
        match schema.get_definition(name).expect("exists") {
            borsh::schema::Definition::Primitive(byte_count) => {
                assert_eq!(*byte_count, expected_byte_count);
            }
            _ => panic!("unexpected schema"),
        }
    }

    #[test]
    fn test_schema_byte_count() {
        verify_byte_count_in_schema::<u1>(1, "u1");

        verify_byte_count_in_schema::<u7>(1, "u7");

        verify_byte_count_in_schema::<UInt<u8, 8>>(1, "u8");
        verify_byte_count_in_schema::<UInt<u32, 8>>(1, "u8");

        verify_byte_count_in_schema::<u9>(2, "u9");

        verify_byte_count_in_schema::<u15>(2, "u15");
        verify_byte_count_in_schema::<UInt<u128, 15>>(2, "u15");

        verify_byte_count_in_schema::<u63>(8, "u63");

        verify_byte_count_in_schema::<u65>(9, "u65");
    }
}

#[cfg(feature = "schemars")]
#[test]
fn schemars() {
    use schemars::schema_for;
    let mut u8 = schema_for!(u8);
    let u9 = schema_for!(u9);
    assert_eq!(
        u8.schema.format.clone().unwrap().replace("8", "9"),
        u9.schema.format.clone().unwrap()
    );
    u8.schema.format = u9.schema.format.clone();
    assert_eq!(
        u8.schema
            .metadata
            .clone()
            .unwrap()
            .title
            .unwrap()
            .replace("8", "9"),
        u9.schema.metadata.clone().unwrap().title.unwrap()
    );
    u8.schema.metadata = u9.schema.metadata.clone();
    u8.schema.number = u9.schema.number.clone();
    assert_eq!(u8, u9);
}

#[test]
fn new_and_as_specific_types() {
    let a = u6::new(42);
    let b = u6::from_u8(42);
    let c = u6::from_u16(42);
    let d = u6::from_u32(42);
    let e = u6::from_u64(42);
    let f = u6::from_u128(42);

    assert_eq!(a.as_u8(), 42);
    assert_eq!(a.as_u16(), 42);
    assert_eq!(a.as_u32(), 42);
    assert_eq!(a.as_u64(), 42);
    assert_eq!(a.as_u128(), 42);
    assert_eq!(b.as_u128(), 42);
    assert_eq!(c.as_u128(), 42);
    assert_eq!(d.as_u128(), 42);
    assert_eq!(e.as_u128(), 42);
    assert_eq!(f.as_u128(), 42);
    assert_eq!(f.as_usize(), 42);
}

#[cfg(not(feature = "const_convert_and_const_trait_impl"))]
#[test]
fn from_flexible() {
    let a = u10::new(1000);
    let b = u11::from_(a);

    assert_eq!(a.as_u32(), 1000);
    assert_eq!(b.as_u32(), 1000);
}

#[cfg(not(feature = "const_convert_and_const_trait_impl"))]
#[test]
#[should_panic]
fn from_flexible_catches_out_of_bounds() {
    let a = u28::new(0x8000000);
    let _b = u9::from_(a);
}

#[cfg(not(feature = "const_convert_and_const_trait_impl"))]
#[test]
#[should_panic]
fn from_flexible_catches_out_of_bounds_2() {
    let a = u28::new(0x0000200);
    let _b = u9::from_(a);
}

#[cfg(not(feature = "const_convert_and_const_trait_impl"))]
#[test]
#[should_panic]
fn from_flexible_catches_out_of_bounds_primitive_type() {
    let a = u28::new(0x8000000);
    let _b = u8::from_(a);
}

#[cfg(not(feature = "const_convert_and_const_trait_impl"))]
#[test]
#[should_panic]
fn new_constructors_catch_out_bounds_0() {
    u7::from_u8(0x80u8);
}

#[cfg(not(feature = "const_convert_and_const_trait_impl"))]
#[test]
#[should_panic]
fn new_constructors_catch_out_bounds_1() {
    u7::from_u32(0x80000060u32);
}

#[cfg(not(feature = "const_convert_and_const_trait_impl"))]
#[test]
#[should_panic]
fn new_constructors_catch_out_bounds_2() {
    u7::from_u16(0x8060u16);
}

#[cfg(not(feature = "const_convert_and_const_trait_impl"))]
#[test]
#[should_panic]
fn new_constructors_catch_out_bounds_3() {
    u7::from_u64(0x80000000_00000060u64);
}

#[cfg(not(feature = "const_convert_and_const_trait_impl"))]
#[test]
#[should_panic]
fn new_constructors_catch_out_bounds_4() {
    u7::from_u128(0x80000000_00000000_00000000_00000060u128);
}

#[cfg(not(feature = "const_convert_and_const_trait_impl"))]
#[test]
fn new_masked() {
    let a = u10::new(0b11_01010111);
    let b = u9::masked_new(a);
    assert_eq!(b.as_u32(), 0b1_01010111);
    let c = u7::masked_new(a);
    assert_eq!(c.as_u32(), 0b1010111);
}

#[cfg(not(feature = "const_convert_and_const_trait_impl"))]
#[test]
fn as_flexible() {
    let a: u32 = u14::new(123).as_();
    assert_eq!(a, 123u32);
}
