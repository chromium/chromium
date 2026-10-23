// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use icu_pattern::DoublePlaceholderPattern;
use icu_plurals::provider::PluralElementsPackedCow;

icu_provider::data_marker!(
    /// `CurrencyPatternsDataV1`
    CurrencyPatternsDataV1,
    CurrencyPatternsData<'static>,
);

/// Currency Extended data struct.
pub type CurrencyPatternsData<'data> = PluralElementsPackedCow<'data, DoublePlaceholderPattern>;
