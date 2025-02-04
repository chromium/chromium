// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Algorithms to determine where to position grouping separators.

use crate::options::GroupingStrategy;
use crate::provider::GroupingSizesV1;
use core::cmp;

/// Returns whether to display a grouping separator at the given magnitude.
///
/// `upper_magnitude` is the magnitude of the highest-power digit, used for resolving minimum
/// grouping digits.
pub fn check(
    upper_magnitude: i16,
    magnitude: i16,
    strategy: GroupingStrategy,
    sizes: &GroupingSizesV1,
) -> bool {
    let primary = if sizes.primary == 0 {
        return false;
    } else {
        sizes.primary as i16
    };
    if magnitude < primary {
        return false;
    }
    let min_grouping = {
        use GroupingStrategy::*;
        match strategy {
            Never => return false,
            // Note: Auto and Always are the same for FixedDecimalFormatter.
            // When currencies are implemented, this will change.
            Auto | Always => cmp::max(1, sizes.min_grouping) as i16,
            Min2 => cmp::max(2, sizes.min_grouping) as i16,
        }
    };
    if upper_magnitude < primary + min_grouping - 1 {
        return false;
    }
    let secondary = if sizes.secondary == 0 {
        primary
    } else {
        sizes.secondary as i16
    };
    let magnitude_prime = magnitude - primary;
    if magnitude_prime % secondary == 0 {
        return true;
    }
    false
}

#[test]
fn test_grouper() {
    use crate::options;
    use crate::provider::*;
    use crate::FixedDecimalFormatter;
    use fixed_decimal::FixedDecimal;
    use icu_provider::prelude::*;
    use icu_provider_adapters::fixed::FixedProvider;
    use icu_provider_adapters::fork::ForkByMarkerProvider;
    use writeable::assert_writeable_eq;

    let western_sizes = GroupingSizesV1 {
        min_grouping: 1,
        primary: 3,
        secondary: 3,
    };
    let indic_sizes = GroupingSizesV1 {
        min_grouping: 1,
        primary: 3,
        secondary: 2,
    };
    let western_sizes_min3 = GroupingSizesV1 {
        min_grouping: 3,
        primary: 3,
        secondary: 3,
    };

    // primary=0 implies no grouping; the other fields are ignored
    let zero_test = GroupingSizesV1 {
        min_grouping: 0,
        primary: 0,
        secondary: 0,
    };

    // secondary=0 implies that it inherits from primary
    let blank_secondary = GroupingSizesV1 {
        min_grouping: 0,
        primary: 3,
        secondary: 0,
    };

    #[derive(Debug)]
    struct TestCase {
        strategy: GroupingStrategy,
        sizes: GroupingSizesV1,
        // Expected results for numbers with magnitude 3, 4, 5, and 6
        expected: [&'static str; 4],
    }
    #[allow(clippy::redundant_clone)]
    let cases = [
        TestCase {
            strategy: GroupingStrategy::Auto,
            sizes: western_sizes,
            expected: ["1,000", "10,000", "100,000", "1,000,000"],
        },
        TestCase {
            strategy: GroupingStrategy::Min2,
            sizes: western_sizes,
            expected: ["1000", "10,000", "100,000", "1,000,000"],
        },
        TestCase {
            strategy: GroupingStrategy::Auto,
            sizes: indic_sizes,
            expected: ["1,000", "10,000", "1,00,000", "10,00,000"],
        },
        TestCase {
            strategy: GroupingStrategy::Min2,
            sizes: indic_sizes,
            expected: ["1000", "10,000", "1,00,000", "10,00,000"],
        },
        TestCase {
            strategy: GroupingStrategy::Auto,
            sizes: western_sizes_min3,
            expected: ["1000", "10000", "100,000", "1,000,000"],
        },
        TestCase {
            strategy: GroupingStrategy::Min2,
            sizes: western_sizes_min3,
            expected: ["1000", "10000", "100,000", "1,000,000"],
        },
        TestCase {
            strategy: GroupingStrategy::Auto,
            sizes: zero_test,
            expected: ["1000", "10000", "100000", "1000000"],
        },
        TestCase {
            strategy: GroupingStrategy::Min2,
            sizes: zero_test,
            expected: ["1000", "10000", "100000", "1000000"],
        },
        TestCase {
            strategy: GroupingStrategy::Auto,
            sizes: blank_secondary,
            expected: ["1,000", "10,000", "100,000", "1,000,000"],
        },
        TestCase {
            strategy: GroupingStrategy::Min2,
            sizes: blank_secondary,
            expected: ["1000", "10,000", "100,000", "1,000,000"],
        },
    ];
    for cas in &cases {
        for i in 0..4 {
            let dec = FixedDecimal::from(1).multiplied_pow10((i as i16) + 3);
            let provider_symbols = FixedProvider::<DecimalSymbolsV2Marker>::from_owned(
                crate::provider::DecimalSymbolsV2 {
                    grouping_sizes: cas.sizes,
                    ..DecimalSymbolsV2::new_en_for_testing()
                },
            );
            let provider_digits = FixedProvider::<DecimalDigitsV1Marker>::from_owned(
                crate::provider::DecimalDigitsV1 {
                    digits: ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9'],
                },
            );
            let provider = ForkByMarkerProvider::new(provider_symbols, provider_digits);
            let options = options::FixedDecimalFormatterOptions {
                grouping_strategy: cas.strategy,
                ..Default::default()
            };
            let fdf = FixedDecimalFormatter::try_new_unstable(
                &provider.as_downcasting(),
                Default::default(),
                options,
            )
            .unwrap();
            let actual = fdf.format(&dec);
            assert_writeable_eq!(actual, cas.expected[i], "{:?}", cas);
        }
    }
}
