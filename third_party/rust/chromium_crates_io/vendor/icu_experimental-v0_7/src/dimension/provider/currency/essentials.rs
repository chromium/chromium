// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use icu_provider::prelude::*;
use zerovec::VarZeroVec;

use icu_pattern::{DoublePlaceholderKey, DoublePlaceholderPattern, PatternItem};

icu_provider::data_marker!(
    /// Essential currency data needed for currency formatting. For example, currency patterns.
    CurrencyEssentialsV1,
    CurrencyEssentials<'static>
);

#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::currency::essentials))]
// TODO: Pack these 8 distinct index fields into a single 32-bit integer bitfield (3 bits per index) to minimize stack size.
pub struct PatternIndices {
    pub standard: u8,
    pub standard_negative: Option<u8>,
    pub standard_alpha_next_to_number: u8,
    pub standard_alpha_next_to_number_negative: Option<u8>,
    pub accounting_positive: u8,
    pub accounting_negative: Option<u8>,
    pub accounting_alpha_next_to_number_positive: u8,
    pub accounting_alpha_next_to_number_negative: Option<u8>,
}

/// This type contains all of the essential data for currency formatting.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Clone, PartialEq, Debug, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::currency::essentials))]
#[yoke(prove_covariance_manually)]
pub struct CurrencyEssentials<'data> {
    /// A packed list of distinct currency patterns referenced by [`PatternIndices`].
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub patterns: VarZeroVec<'data, DoublePlaceholderPattern>,

    /// Indices into `patterns` for each formatting variant.
    pub indices: PatternIndices,
}

icu_provider::data_struct!(CurrencyEssentials<'_>, #[cfg(feature = "datagen")]);

impl<'a> CurrencyEssentials<'a> {
    pub fn get_positive(
        &'a self,
        symbol_starts_with_letter: bool,
        symbol_ends_with_letter: bool,
    ) -> &'a DoublePlaceholderPattern {
        let standard = self
            .patterns
            .get(self.indices.standard as usize)
            .unwrap_or_else(|| {
                debug_assert!(false, "Standard pattern index is out of bounds");
                <&DoublePlaceholderPattern>::default()
            });

        if self.indices.standard_alpha_next_to_number != self.indices.standard
            && is_alpha_next_to_number(standard, symbol_starts_with_letter, symbol_ends_with_letter)
        {
            self.patterns
                .get(self.indices.standard_alpha_next_to_number as usize)
                .unwrap_or_else(|| {
                    debug_assert!(false, "Standard alpha pattern index is out of bounds");
                    standard
                })
        } else {
            standard
        }
    }

    /// Returns the explicit standard negative pattern, if it exists.
    ///
    /// If the category has no explicit negative pattern, `None` is returned and the caller
    /// falls back to the positive pattern of the same category, applying the sign
    /// itself per UTS #35 (LDML Part 3: Numbers, Section 3.2.1).
    pub fn get_negative(
        &'a self,
        symbol_starts_with_letter: bool,
        symbol_ends_with_letter: bool,
    ) -> Option<&'a DoublePlaceholderPattern> {
        let Some(standard) = self.patterns.get(self.indices.standard as usize) else {
            debug_assert!(false, "Standard pattern index is out of bounds");
            return None;
        };

        let negative_idx = if self.indices.standard_alpha_next_to_number_negative
            != self.indices.standard_negative
            && is_alpha_next_to_number(standard, symbol_starts_with_letter, symbol_ends_with_letter)
        {
            self.indices.standard_alpha_next_to_number_negative
        } else {
            self.indices.standard_negative
        }?;

        let Some(pattern) = self.patterns.get(negative_idx as usize) else {
            debug_assert!(false, "Standard negative pattern index is out of bounds");
            return None;
        };
        Some(pattern)
    }

    pub fn get_positive_accounting(
        &'a self,
        symbol_starts_with_letter: bool,
        symbol_ends_with_letter: bool,
    ) -> &'a DoublePlaceholderPattern {
        let standard = self
            .patterns
            .get(self.indices.accounting_positive as usize)
            .unwrap_or_else(|| {
                debug_assert!(false, "Accounting standard pattern index is out of bounds");
                <&DoublePlaceholderPattern>::default()
            });

        if self.indices.accounting_alpha_next_to_number_positive != self.indices.accounting_positive
            && is_alpha_next_to_number(standard, symbol_starts_with_letter, symbol_ends_with_letter)
        {
            self.patterns
                .get(self.indices.accounting_alpha_next_to_number_positive as usize)
                .unwrap_or_else(|| {
                    debug_assert!(false, "Accounting alpha pattern index is out of bounds");
                    standard
                })
        } else {
            standard
        }
    }

    /// Returns the explicit accounting negative pattern, if it exists.
    ///
    /// If the selected category has no explicit negative pattern, `None` is returned and the
    /// caller falls back to the positive pattern of the same category, applying the
    /// sign itself per UTS #35 (LDML Part 3: Numbers, Section 3.2.1).
    pub fn get_negative_accounting(
        &'a self,
        symbol_starts_with_letter: bool,
        symbol_ends_with_letter: bool,
    ) -> Option<&'a DoublePlaceholderPattern> {
        let Some(accounting) = self.patterns.get(self.indices.accounting_positive as usize) else {
            debug_assert!(false, "Accounting pattern index is out of bounds");
            return None;
        };

        let negative_idx = if self.indices.accounting_alpha_next_to_number_negative
            != self.indices.accounting_negative
            && is_alpha_next_to_number(
                accounting,
                symbol_starts_with_letter,
                symbol_ends_with_letter,
            ) {
            self.indices.accounting_alpha_next_to_number_negative
        } else {
            self.indices.accounting_negative
        }?;

        let Some(pattern) = self.patterns.get(negative_idx as usize) else {
            debug_assert!(false, "Accounting negative pattern index is out of bounds");
            return None;
        };
        Some(pattern)
    }
}

fn is_alpha_next_to_number(
    pattern: &DoublePlaceholderPattern,
    symbol_starts_with_letter: bool,
    symbol_ends_with_letter: bool,
) -> bool {
    let number_placeholder_index = pattern
        .iter()
        .position(|x| x == PatternItem::Placeholder(DoublePlaceholderKey::Place0))
        .unwrap_or(usize::MAX);

    let currency_placeholder_index = pattern
        .iter()
        .position(|x| x == PatternItem::Placeholder(DoublePlaceholderKey::Place1))
        .unwrap_or(usize::MAX);

    if number_placeholder_index + 1 == currency_placeholder_index {
        symbol_starts_with_letter
    } else if currency_placeholder_index + 1 == number_placeholder_index {
        symbol_ends_with_letter
    } else {
        false
    }
}
