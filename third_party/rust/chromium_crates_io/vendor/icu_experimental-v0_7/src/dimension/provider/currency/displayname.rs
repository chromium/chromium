// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use zerovec::VarZeroCow;

icu_provider::data_marker!(
    /// Currency display name data.
    CurrencyDisplaynameV1,
    CurrencyDisplayname<'static>,
    #[cfg(feature = "datagen")]
    attributes_domain = "currency",
);

pub type CurrencyDisplayname<'data> = VarZeroCow<'data, str>;
