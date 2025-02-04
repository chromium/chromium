// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_decimal::options::{FixedDecimalFormatterOptions, GroupingStrategy};

/// A bag of options defining how numbers will be formatted by
/// [`CompactDecimalFormatter`](super::CompactDecimalFormatter).
#[derive(Debug, Eq, PartialEq, Clone)]
#[non_exhaustive]
pub struct CompactDecimalFormatterOptions {
    /// Options to configure the inner [`icu_decimal::FixedDecimalFormatter`].
    pub fixed_decimal_formatter_options: FixedDecimalFormatterOptions,
}

impl Default for CompactDecimalFormatterOptions {
    fn default() -> Self {
        Self {
            fixed_decimal_formatter_options: GroupingStrategy::Min2.into(),
        }
    }
}

impl From<FixedDecimalFormatterOptions> for CompactDecimalFormatterOptions {
    fn from(fixed_decimal_formatter_options: FixedDecimalFormatterOptions) -> Self {
        Self {
            fixed_decimal_formatter_options,
        }
    }
}

impl From<GroupingStrategy> for CompactDecimalFormatterOptions {
    fn from(grouping_strategy: GroupingStrategy) -> Self {
        Self {
            fixed_decimal_formatter_options: grouping_strategy.into(),
        }
    }
}
