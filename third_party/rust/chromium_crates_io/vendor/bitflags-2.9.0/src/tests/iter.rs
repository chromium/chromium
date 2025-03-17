use super::*;

use crate::Flags;

#[test]
#[cfg(not(miri))] // Very slow in miri
fn roundtrip() {
    for a in 0u8..=255 {
        for b in 0u8..=255 {
            let f = TestFlags::from_bits_retain(a | b);

            assert_eq!(f, f.iter().collect::<TestFlags>());
            assert_eq!(
                TestFlags::from_bits_truncate(f.bits()),
                f.iter_names().map(|(_, f)| f).collect::<TestFlags>()
            );

            let f = TestExternal::from_bits_retain(a | b);

            assert_eq!(f, f.iter().collect::<TestExternal>());
        }
    }
}

mod collect {
    use super::*;

    #[test]
    fn cases() {
        assert_eq!(0, [].into_iter().collect::<TestFlags>().bits());

        assert_eq!(1, [TestFlags::A,].into_iter().collect::<TestFlags>().bits());

        assert_eq!(
            1 | 1 << 1 | 1 << 2,
            [TestFlags::A, TestFlags::B | TestFlags::C,]
                .into_iter()
                .collect::<TestFlags>()
                .bits()
        );

        assert_eq!(
            1 | 1 << 3,
            [
                TestFlags::from_bits_retain(1 << 3),
                TestFlags::empty(),
                TestFlags::A,
            ]
            .into_iter()
            .collect::<TestFlags>()
            .bits()
        );

        assert_eq!(
            1 << 5 | 1 << 7,
            [
                TestExternal::empty(),
                TestExternal::from_bits_retain(1 << 5),
                TestExternal::from_bits_retain(1 << 7),
            ]
            .into_iter()
            .collect::<TestExternal>()
            .bits()
        );
    }
}

mod iter {
    use super::*;

    #[test]
    fn cases() {
        case(&[], TestFlags::empty(), TestFlags::iter);

        case(&[1], TestFlags::A, TestFlags::iter);
        case(&[1, 1 << 1], TestFlags::A | TestFlags::B, TestFlags::iter);
        case(
            &[1, 1 << 1, 1 << 3],
            TestFlags::A | TestFlags::B | TestFlags::from_bits_retain(1 << 3),
            TestFlags::iter,
        );

        case(&[1, 1 << 1, 1 << 2], TestFlags::ABC, TestFlags::iter);
        case(
            &[1, 1 << 1, 1 << 2, 1 << 3],
            TestFlags::ABC | TestFlags::from_bits_retain(1 << 3),
            TestFlags::iter,
        );

        case(
            &[1 | 1 << 1 | 1 << 2],
            TestFlagsInvert::ABC,
            TestFlagsInvert::iter,
        );

        case(&[], TestZero::ZERO, TestZero::iter);

        case(
            &[1, 1 << 1, 1 << 2, 0b1111_1000],
            TestExternal::all(),
            TestExternal::iter,
        );
    }

    #[track_caller]
    fn case<T: Flags + std::fmt::Debug + IntoIterator<Item = T> + Copy>(
        expected: &[T::Bits],
        value: T,
        inherent: impl FnOnce(&T) -> crate::iter::Iter<T>,
    ) where
        T::Bits: std::fmt::Debug + PartialEq,
    {
        assert_eq!(
            expected,
            inherent(&value).map(|f| f.bits()).collect::<Vec<_>>(),
            "{:?}.iter()",
            value
        );
        assert_eq!(
            expected,
            Flags::iter(&value).map(|f| f.bits()).collect::<Vec<_>>(),
            "Flags::iter({:?})",
            value
        );
        assert_eq!(
            expected,
            value.into_iter().map(|f| f.bits()).collect::<Vec<_>>(),
            "{:?}.into_iter()",
            value
        );
    }
}

mod iter_names {
    use super::*;

    #[test]
    fn cases() {
        case(&[], TestFlags::empty(), TestFlags::iter_names);

        case(&[("A", 1)], TestFlags::A, TestFlags::iter_names);
        case(
            &[("A", 1), ("B", 1 << 1)],
            TestFlags::A | TestFlags::B,
            TestFlags::iter_names,
        );
        case(
            &[("A", 1), ("B", 1 << 1)],
            TestFlags::A | TestFlags::B | TestFlags::from_bits_retain(1 << 3),
            TestFlags::iter_names,
        );

        case(
            &[("A", 1), ("B", 1 << 1), ("C", 1 << 2)],
            TestFlags::ABC,
            TestFlags::iter_names,
        );
        case(
            &[("A", 1), ("B", 1 << 1), ("C", 1 << 2)],
            TestFlags::ABC | TestFlags::from_bits_retain(1 << 3),
            TestFlags::iter_names,
        );

        case(
            &[("ABC", 1 | 1 << 1 | 1 << 2)],
            TestFlagsInvert::ABC,
            TestFlagsInvert::iter_names,
        );

        case(&[], TestZero::ZERO, TestZero::iter_names);

        case(
            &[("A", 1)],
            TestOverlappingFull::A,
            TestOverlappingFull::iter_names,
        );
        case(
            &[("A", 1), ("D", 1 << 1)],
            TestOverlappingFull::A | TestOverlappingFull::D,
            TestOverlappingFull::iter_names,
        );
    }

    #[track_caller]
    fn case<T: Flags + std::fmt::Debug>(
        expected: &[(&'static str, T::Bits)],
        value: T,
        inherent: impl FnOnce(&T) -> crate::iter::IterNames<T>,
    ) where
        T::Bits: std::fmt::Debug + PartialEq,
    {
        assert_eq!(
            expected,
            inherent(&value)
                .map(|(n, f)| (n, f.bits()))
                .collect::<Vec<_>>(),
            "{:?}.iter_names()",
            value
        );
        assert_eq!(
            expected,
            Flags::iter_names(&value)
                .map(|(n, f)| (n, f.bits()))
                .collect::<Vec<_>>(),
            "Flags::iter_names({:?})",
            value
        );
    }
}
