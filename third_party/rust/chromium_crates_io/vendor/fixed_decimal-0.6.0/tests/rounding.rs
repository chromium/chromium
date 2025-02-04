// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::ops::RangeInclusive;
use fixed_decimal::FixedDecimal;
use fixed_decimal::RoundingMode;
use fixed_decimal::Sign;
use writeable::Writeable;

#[test]
pub fn test_ecma402_table() {
    // Source: <https://tc39.es/ecma402/#table-intl-rounding-modes>
    let cases: [(_, _, _, _, _, _, _); 9] = [
        ("ceil", RoundingMode::Ceil, -1, 1, 1, 1, 2),
        ("floor", RoundingMode::Floor, -2, 0, 0, 0, 1),
        ("expand", RoundingMode::Expand, -2, 1, 1, 1, 2),
        ("trunc", RoundingMode::Trunc, -1, 0, 0, 0, 1),
        ("half_ceil", RoundingMode::HalfCeil, -1, 0, 1, 1, 2),
        ("half_floor", RoundingMode::HalfFloor, -2, 0, 0, 1, 1),
        ("half_expand", RoundingMode::HalfExpand, -2, 0, 1, 1, 2),
        ("half_trunc", RoundingMode::HalfTrunc, -1, 0, 0, 1, 1),
        ("half_even", RoundingMode::HalfEven, -2, 0, 0, 1, 2),
    ];
    for (name, mode, e1, e2, e3, e4, e5) in cases {
        let mut fd1: FixedDecimal = "-1.5".parse().unwrap();
        let mut fd2: FixedDecimal = "0.4".parse().unwrap();
        let mut fd3: FixedDecimal = "0.5".parse().unwrap();
        let mut fd4: FixedDecimal = "0.6".parse().unwrap();
        let mut fd5: FixedDecimal = "1.5".parse().unwrap();
        fd1.round_with_mode(0, mode);
        fd2.round_with_mode(0, mode);
        fd3.round_with_mode(0, mode);
        fd4.round_with_mode(0, mode);
        fd5.round_with_mode(0, mode);
        assert_eq!(
            fd1.write_to_string(),
            e1.write_to_string(),
            "-1.5 failed for {name}"
        );
        assert_eq!(
            fd2.write_to_string(),
            e2.write_to_string(),
            "0.4 failed for {name}"
        );
        assert_eq!(
            fd3.write_to_string(),
            e3.write_to_string(),
            "0.5 failed for {name}"
        );
        assert_eq!(
            fd4.write_to_string(),
            e4.write_to_string(),
            "0.6 failed for {name}"
        );
        assert_eq!(
            fd5.write_to_string(),
            e5.write_to_string(),
            "1.5 failed for {name}"
        );
    }
}

#[test]
pub fn test_within_ranges() {
    struct TestCase {
        rounding_mode_name: &'static str,
        rounding_mode: RoundingMode,
        range_n2000: RangeInclusive<i32>,
        range_n1000: RangeInclusive<i32>,
        range_0: RangeInclusive<i32>,
        range_1000: RangeInclusive<i32>,
        range_2000: RangeInclusive<i32>,
    }
    let cases: [TestCase; 9] = [
        TestCase {
            rounding_mode_name: "ceil",
            rounding_mode: RoundingMode::Ceil,
            range_n2000: -2999..=-2000,
            range_n1000: -1999..=-1000,
            range_0: -999..=0,
            range_1000: 1..=1000,
            range_2000: 1001..=2000,
        },
        TestCase {
            rounding_mode_name: "floor",
            rounding_mode: RoundingMode::Floor,
            range_n2000: -2000..=-1001,
            range_n1000: -1000..=-1,
            range_0: 0..=999,
            range_1000: 1000..=1999,
            range_2000: 2000..=2999,
        },
        TestCase {
            rounding_mode_name: "expand",
            rounding_mode: RoundingMode::Expand,
            range_n2000: -2000..=-1001,
            range_n1000: -1000..=-1,
            range_0: 0..=0,
            range_1000: 1..=1000,
            range_2000: 1001..=2000,
        },
        TestCase {
            rounding_mode_name: "trunc",
            rounding_mode: RoundingMode::Trunc,
            range_n2000: -2999..=-2000,
            range_n1000: -1999..=-1000,
            range_0: -999..=999,
            range_1000: 1000..=1999,
            range_2000: 2000..=2999,
        },
        TestCase {
            rounding_mode_name: "half_ceil",
            rounding_mode: RoundingMode::HalfCeil,
            range_n2000: -2500..=-1501,
            range_n1000: -1500..=-501,
            range_0: -500..=449,
            range_1000: 500..=1449,
            range_2000: 1500..=2449,
        },
        TestCase {
            rounding_mode_name: "half_floor",
            rounding_mode: RoundingMode::HalfFloor,
            range_n2000: -2449..=-1500,
            range_n1000: -1449..=-500,
            range_0: -449..=500,
            range_1000: 501..=1500,
            range_2000: 1501..=2500,
        },
        TestCase {
            rounding_mode_name: "half_expand",
            rounding_mode: RoundingMode::HalfExpand,
            range_n2000: -2449..=-1500,
            range_n1000: -1449..=-500,
            range_0: -449..=449,
            range_1000: 500..=1449,
            range_2000: 1500..=2449,
        },
        TestCase {
            rounding_mode_name: "half_trunc",
            rounding_mode: RoundingMode::HalfTrunc,
            range_n2000: -2500..=-1501,
            range_n1000: -1500..=-501,
            range_0: -500..=500,
            range_1000: 501..=1500,
            range_2000: 1501..=2500,
        },
        TestCase {
            rounding_mode_name: "half_even",
            rounding_mode: RoundingMode::HalfEven,
            range_n2000: -2500..=-1500,
            range_n1000: -1449..=-501,
            range_0: -500..=500,
            range_1000: 501..=1449,
            range_2000: 1500..=2500,
        },
    ];
    for TestCase {
        rounding_mode_name,
        rounding_mode,
        range_n2000,
        range_n1000,
        range_0,
        range_1000,
        range_2000,
    } in cases
    {
        for n in range_n2000 {
            let mut fd = FixedDecimal::from(n);
            fd.round_with_mode(3, rounding_mode);
            assert_eq!(fd.write_to_string(), "-2000", "{rounding_mode_name}: {n}");
            let mut fd = FixedDecimal::from(n - 1000000).multiplied_pow10(-5);
            fd.round_with_mode(-2, rounding_mode);
            assert_eq!(
                fd.write_to_string(),
                "-10.02",
                "{rounding_mode_name}: {n} ÷ 10^5 ± 10"
            );
        }
        for n in range_n1000 {
            let mut fd = FixedDecimal::from(n);
            fd.round_with_mode(3, rounding_mode);
            assert_eq!(fd.write_to_string(), "-1000", "{rounding_mode_name}: {n}");
            let mut fd = FixedDecimal::from(n - 1000000).multiplied_pow10(-5);
            fd.round_with_mode(-2, rounding_mode);
            assert_eq!(
                fd.write_to_string(),
                "-10.01",
                "{rounding_mode_name}: {n} ÷ 10^5 ± 10"
            );
        }
        for n in range_0 {
            let mut fd = FixedDecimal::from(n);
            fd.round_with_mode(3, rounding_mode);
            fd.set_sign(Sign::None); // get rid of -0
            assert_eq!(fd.write_to_string(), "000", "{rounding_mode_name}: {n}");
            let (mut fd, expected) = if n < 0 {
                (
                    FixedDecimal::from(n - 1000000).multiplied_pow10(-5),
                    "-10.00",
                )
            } else {
                (
                    FixedDecimal::from(n + 1000000).multiplied_pow10(-5),
                    "10.00",
                )
            };
            fd.round_with_mode(-2, rounding_mode);
            assert_eq!(
                fd.write_to_string(),
                expected,
                "{rounding_mode_name}: {n} ÷ 10^5 ± 10"
            );
        }
        for n in range_1000 {
            let mut fd = FixedDecimal::from(n);
            fd.round_with_mode(3, rounding_mode);
            assert_eq!(fd.write_to_string(), "1000", "{rounding_mode_name}: {n}");
            let mut fd = FixedDecimal::from(n + 1000000).multiplied_pow10(-5);
            fd.round_with_mode(-2, rounding_mode);
            assert_eq!(
                fd.write_to_string(),
                "10.01",
                "{rounding_mode_name}: {n} ÷ 10^5 ± 10"
            );
        }
        for n in range_2000 {
            let mut fd = FixedDecimal::from(n);
            fd.round_with_mode(3, rounding_mode);
            assert_eq!(fd.write_to_string(), "2000", "{rounding_mode_name}: {n}");
            let mut fd = FixedDecimal::from(n + 1000000).multiplied_pow10(-5);
            fd.round_with_mode(-2, rounding_mode);
            assert_eq!(
                fd.write_to_string(),
                "10.02",
                "{rounding_mode_name}: {n} ÷ 10^5 ± 10"
            );
        }
    }
}

#[test]
pub fn extra_rounding_mode_cases() {
    struct TestCase {
        input: &'static str,
        position: i16,
        // ceil, floor, expand, trunc, half_ceil, half_floor, half_expand, half_trunc, half_even
        all_expected: [&'static str; 9],
    }
    let cases: [TestCase; 8] = [
        TestCase {
            input: "505.050",
            position: -3,
            all_expected: [
                "505.050", "505.050", "505.050", "505.050", "505.050", "505.050", "505.050",
                "505.050", "505.050",
            ],
        },
        TestCase {
            input: "505.050",
            position: -2,
            all_expected: [
                "505.05", "505.05", "505.05", "505.05", "505.05", "505.05", "505.05", "505.05",
                "505.05",
            ],
        },
        TestCase {
            input: "505.050",
            position: -1,
            all_expected: [
                "505.1", "505.0", "505.1", "505.0", "505.1", "505.0", "505.1", "505.0", "505.0",
            ],
        },
        TestCase {
            input: "505.050",
            position: 0,
            all_expected: [
                "506", "505", "506", "505", "505", "505", "505", "505", "505",
            ],
        },
        TestCase {
            input: "505.050",
            position: 1,
            all_expected: [
                "510", "500", "510", "500", "510", "510", "510", "510", "510",
            ],
        },
        TestCase {
            input: "505.050",
            position: 2,
            all_expected: [
                "600", "500", "600", "500", "500", "500", "500", "500", "500",
            ],
        },
        TestCase {
            input: "505.050",
            position: 3,
            all_expected: [
                "1000", "000", "1000", "000", "1000", "1000", "1000", "1000", "1000",
            ],
        },
        TestCase {
            input: "505.050",
            position: 4,
            all_expected: [
                "10000", "0000", "10000", "0000", "0000", "0000", "0000", "0000", "0000",
            ],
        },
    ];
    #[allow(clippy::type_complexity)] // most compact representation in code
    let rounding_modes: [(&'static str, RoundingMode); 9] = [
        ("ceil", RoundingMode::Ceil),
        ("floor", RoundingMode::Floor),
        ("expand", RoundingMode::Expand),
        ("trunc", RoundingMode::Trunc),
        ("half_ceil", RoundingMode::HalfCeil),
        ("half_floor", RoundingMode::HalfFloor),
        ("half_expand", RoundingMode::HalfExpand),
        ("half_trunc", RoundingMode::HalfTrunc),
        ("half_even", RoundingMode::HalfEven),
    ];
    for TestCase {
        input,
        position,
        all_expected,
    } in cases
    {
        for ((rounding_mode_name, rounding_mode), expected) in
            rounding_modes.iter().zip(all_expected.iter())
        {
            let mut fd: FixedDecimal = input.parse().unwrap();
            fd.round_with_mode(position, *rounding_mode);
            assert_eq!(
                &*fd.write_to_string(),
                *expected,
                "{input}: {rounding_mode_name} @ {position}"
            )
        }
    }
}

#[test]
pub fn test_ecma402_table_with_increments() {
    use fixed_decimal::RoundingIncrement;

    #[rustfmt::skip] // Don't split everything on its own line. Makes it look a lot nicer.
    #[allow(clippy::type_complexity)]
    let cases: [(_, _, [(_, _, _, _, _, _, _); 9]); 3] = [
        ("two", RoundingIncrement::MultiplesOf2, [
            ("ceil", RoundingMode::Ceil, "-1.4", "0.4", "0.6", "0.6", "1.6"),
            ("floor", RoundingMode::Floor, "-1.6", "0.4", "0.4", "0.6", "1.4"),
            ("expand", RoundingMode::Expand, "-1.6", "0.4", "0.6", "0.6", "1.6"),
            ("trunc", RoundingMode::Trunc, "-1.4", "0.4", "0.4", "0.6", "1.4"),
            ("half_ceil", RoundingMode::HalfCeil, "-1.4", "0.4", "0.6", "0.6", "1.6"),
            ("half_floor", RoundingMode::HalfFloor, "-1.6", "0.4", "0.4", "0.6", "1.4"),
            ("half_expand", RoundingMode::HalfExpand, "-1.6", "0.4", "0.6", "0.6", "1.6"),
            ("half_trunc", RoundingMode::HalfTrunc, "-1.4", "0.4", "0.4", "0.6", "1.4"),
            ("half_even", RoundingMode::HalfEven, "-1.6", "0.4", "0.4", "0.6", "1.6"),
        ]),
        ("five", RoundingIncrement::MultiplesOf5, [
            ("ceil", RoundingMode::Ceil, "-1.5", "0.5", "0.5", "1.0", "1.5"),
            ("floor", RoundingMode::Floor, "-1.5", "0.0", "0.5", "0.5", "1.5"),
            ("expand", RoundingMode::Expand, "-1.5", "0.5", "0.5", "1.0", "1.5"),
            ("trunc", RoundingMode::Trunc, "-1.5", "0.0", "0.5", "0.5", "1.5"),
            ("half_ceil", RoundingMode::HalfCeil, "-1.5", "0.5", "0.5", "0.5", "1.5"),
            ("half_floor", RoundingMode::HalfFloor, "-1.5", "0.5", "0.5", "0.5", "1.5"),
            ("half_expand", RoundingMode::HalfExpand, "-1.5", "0.5", "0.5", "0.5", "1.5"),
            ("half_trunc", RoundingMode::HalfTrunc, "-1.5", "0.5", "0.5", "0.5", "1.5"),
            ("half_even", RoundingMode::HalfEven, "-1.5", "0.5", "0.5", "0.5", "1.5"),
        ]),
        ("twenty-five", RoundingIncrement::MultiplesOf25, [
            ("ceil", RoundingMode::Ceil, "-0.0", "2.5", "2.5", "2.5", "2.5"),
            ("floor", RoundingMode::Floor, "-2.5", "0.0", "0.0", "0.0", "0.0"),
            ("expand", RoundingMode::Expand, "-2.5", "2.5", "2.5", "2.5", "2.5"),
            ("trunc", RoundingMode::Trunc, "-0.0", "0.0", "0.0", "0.0", "0.0"),
            ("half_ceil", RoundingMode::HalfCeil, "-2.5", "0.0", "0.0", "0.0", "2.5"),
            ("half_floor", RoundingMode::HalfFloor, "-2.5", "0.0", "0.0", "0.0", "2.5"),
            ("half_expand", RoundingMode::HalfExpand, "-2.5", "0.0", "0.0", "0.0", "2.5"),
            ("half_trunc", RoundingMode::HalfTrunc, "-2.5", "0.0", "0.0", "0.0", "2.5"),
            ("half_even", RoundingMode::HalfEven, "-2.5", "0.0", "0.0", "0.0", "2.5"),
        ]),
    ];

    for (increment_str, increment, cases) in cases {
        for (rounding_mode_name, rounding_mode, e1, e2, e3, e4, e5) in cases {
            let mut fd1: FixedDecimal = "-1.5".parse().unwrap();
            let mut fd2: FixedDecimal = "0.4".parse().unwrap();
            let mut fd3: FixedDecimal = "0.5".parse().unwrap();
            let mut fd4: FixedDecimal = "0.6".parse().unwrap();
            let mut fd5: FixedDecimal = "1.5".parse().unwrap();
            // The original ECMA-402 table tests rounding at magnitude 0.
            // However, testing rounding at magnitude -1 gives more
            // interesting test cases for increments.
            fd1.round_with_mode_and_increment(-1, rounding_mode, increment);
            fd2.round_with_mode_and_increment(-1, rounding_mode, increment);
            fd3.round_with_mode_and_increment(-1, rounding_mode, increment);
            fd4.round_with_mode_and_increment(-1, rounding_mode, increment);
            fd5.round_with_mode_and_increment(-1, rounding_mode, increment);
            assert_eq!(
                fd1.write_to_string(),
                e1,
                "-1.5 failed for {rounding_mode_name} with increments of {increment_str}"
            );
            assert_eq!(
                fd2.write_to_string(),
                e2,
                "0.4 failed for {rounding_mode_name} with increments of {increment_str}"
            );
            assert_eq!(
                fd3.write_to_string(),
                e3,
                "0.5 failed for {rounding_mode_name} with increments of {increment_str}"
            );
            assert_eq!(
                fd4.write_to_string(),
                e4,
                "0.6 failed for {rounding_mode_name} with increments of {increment_str}"
            );
            assert_eq!(
                fd5.write_to_string(),
                e5,
                "1.5 failed for {rounding_mode_name} with increments of {increment_str}"
            );
        }
    }
}
