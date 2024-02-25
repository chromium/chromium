use super::*;

use crate::{
    parser::{from_str, to_writer},
    Flags,
};

#[test]
#[cfg(not(miri))] // Very slow in miri
fn roundtrip() {
    let mut s = String::new();

    for a in 0u8..=255 {
        for b in 0u8..=255 {
            let f = TestFlags::from_bits_retain(a | b);

            s.clear();
            to_writer(&f, &mut s).unwrap();

            assert_eq!(f, from_str::<TestFlags>(&s).unwrap());
        }
    }
}

mod from_str {
    use super::*;

    #[test]
    fn valid() {
        assert_eq!(0, from_str::<TestFlags>("").unwrap().bits());

        assert_eq!(1, from_str::<TestFlags>("A").unwrap().bits());
        assert_eq!(1, from_str::<TestFlags>(" A ").unwrap().bits());
        assert_eq!(
            1 | 1 << 1 | 1 << 2,
            from_str::<TestFlags>("A | B | C").unwrap().bits()
        );
        assert_eq!(
            1 | 1 << 1 | 1 << 2,
            from_str::<TestFlags>("A\n|\tB\r\n|   C ").unwrap().bits()
        );
        assert_eq!(
            1 | 1 << 1 | 1 << 2,
            from_str::<TestFlags>("A|B|C").unwrap().bits()
        );

        assert_eq!(1 << 3, from_str::<TestFlags>("0x8").unwrap().bits());
        assert_eq!(1 | 1 << 3, from_str::<TestFlags>("A | 0x8").unwrap().bits());
        assert_eq!(
            1 | 1 << 1 | 1 << 3,
            from_str::<TestFlags>("0x1 | 0x8 | B").unwrap().bits()
        );

        assert_eq!(
            1 | 1 << 1,
            from_str::<TestUnicode>("一 | 二").unwrap().bits()
        );
    }

    #[test]
    fn invalid() {
        assert!(from_str::<TestFlags>("a")
            .unwrap_err()
            .to_string()
            .starts_with("unrecognized named flag"));
        assert!(from_str::<TestFlags>("A & B")
            .unwrap_err()
            .to_string()
            .starts_with("unrecognized named flag"));

        assert!(from_str::<TestFlags>("0xg")
            .unwrap_err()
            .to_string()
            .starts_with("invalid hex flag"));
        assert!(from_str::<TestFlags>("0xffffffffffff")
            .unwrap_err()
            .to_string()
            .starts_with("invalid hex flag"));
    }
}

mod to_writer {
    use super::*;

    #[test]
    fn cases() {
        assert_eq!("", write(TestFlags::empty()));
        assert_eq!("A", write(TestFlags::A));
        assert_eq!("A | B | C", write(TestFlags::all()));
        assert_eq!("0x8", write(TestFlags::from_bits_retain(1 << 3)));
        assert_eq!(
            "A | 0x8",
            write(TestFlags::A | TestFlags::from_bits_retain(1 << 3))
        );

        assert_eq!("", write(TestZero::ZERO));

        assert_eq!("ABC", write(TestFlagsInvert::all()));

        assert_eq!("A", write(TestOverlappingFull::C));
        assert_eq!(
            "A | D",
            write(TestOverlappingFull::C | TestOverlappingFull::D)
        );
    }

    fn write<F: Flags>(value: F) -> String
    where
        F::Bits: crate::parser::WriteHex,
    {
        let mut s = String::new();

        to_writer(&value, &mut s).unwrap();
        s
    }
}
