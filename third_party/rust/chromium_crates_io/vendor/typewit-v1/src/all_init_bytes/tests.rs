use super::{as_bytes, slice_as_bytes};


macro_rules! test_int {
    ($vals:expr) => ({
        for val in $vals {
            assert_eq!(as_bytes(&val), val.to_ne_bytes());

            let mut vec = arrayvec::ArrayVec::<u8, 32>::new();
            vec.try_extend_from_slice(&val.to_ne_bytes()).unwrap();
            vec.try_extend_from_slice(&val.to_ne_bytes()).unwrap();

            assert_eq!(slice_as_bytes(&[val, val]), &vec[..]);
        }
    });
}

macro_rules! test_signed_int {
    ($vals:expr) => ({
        for val in $vals {
            test_int!{[val, -val]}
        }
    });
}


#[test]
fn test_bool_as_bytes() {
    for b in [false, true] {
        assert_eq!(as_bytes(&b), &[b as u8][..]);
        assert_eq!(slice_as_bytes(&[b]), &[b as u8][..]);
    }
}

#[test]
fn test_char_as_bytes() {
    for c in ['f', 'o', '\n', '\t', 'ñ', '个', '\u{100000}', char::MAX] {
        assert_eq!(as_bytes(&c), (c as u32).to_ne_bytes());
        assert_eq!(slice_as_bytes(&[c]), (c as u32).to_ne_bytes());
    }
}

#[test]
fn test_u8_as_bytes() {
    test_int!{[0u8, 233u8]}
}

#[test]
fn test_u16_as_bytes() {
    test_int!{[0u16, 500u16, 1000u16]}
}

#[test]
fn test_u32_as_bytes() {
    test_int!{[0u32, 500u32, 1000u32, 1_000_000u32, 1_000_000_000u32]}
}

#[test]
fn test_u64_as_bytes() {
    test_int!{[
        0u64, 500u64, 1000u64, 1_000_000u64, 1_000_000_000u64, 
        1_000_000_000_000_000_000u64,
    ]}
}

#[test]
fn test_u128_as_bytes() {
    test_int!{[
        0u128, 500u128, 1000u128, 1_000_000u128, 1_000_000_000u128, 
        1_000_000_000_000_000_000u128, 1_000_000_000_000_000_000_000_000_000u128, 
        1_000_000_000_000_000_000_000_000_000_000_000_000u128,
    ]}
}

#[test]
fn test_usize_as_bytes() {
    test_int!{[0usize, 500usize, 1000usize, 12345usize]}
}


#[test]
fn test_i8_as_bytes() {
    test_signed_int!{[0i8, 111i8]}
}

#[test]
fn test_i16_as_bytes() {
    test_signed_int!{[0i16, 500i16, 1000i16]}
}

#[test]
fn test_i32_as_bytes() {
    test_signed_int!{[0i32, 500i32, 1000i32, 1_000_000i32, 1_000_000_000i32]}
}

#[test]
fn test_i64_as_bytes() {
    test_signed_int!{[
        0i64, 500i64, 1000i64, 1_000_000i64, 1_000_000_000i64, 
        1_000_000_000_000_000_000i64,
    ]}
}

#[test]
fn test_i128_as_bytes() {
    test_signed_int!{[
        0i128, 500i128, 1000i128, 1_000_000i128, 1_000_000_000i128, 
        1_000_000_000_000_000_000i128, 1_000_000_000_000_000_000_000_000_000i128, 
        1_000_000_000_000_000_000_000_000_000_000_000_000i128,
    ]}
}

#[test]
fn test_isize_as_bytes() {
    test_signed_int!{[0isize, 500isize, 1000isize, 12345isize]}
}
