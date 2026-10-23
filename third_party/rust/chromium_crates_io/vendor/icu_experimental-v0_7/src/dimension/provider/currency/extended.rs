// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use icu_plurals::provider::PluralElementsPackedCow;

icu_provider::data_marker!(
    /// Extended currency data needed for currency formatting. For example, currency display names.
    CurrencyExtendedDataV1,
    CurrencyExtendedData<'static>,
    #[cfg(feature = "datagen")]
    attributes_domain = "currency",
);

/// Currency extended data struct.
pub type CurrencyExtendedData<'data> = PluralElementsPackedCow<'data, str>;
