// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Tooling for the baked provider.

#![cfg_attr(not(feature = "export"), no_std)]

extern crate alloc;

#[cfg(feature = "export")]
pub mod export;

pub use icu_provider::prelude::*;

pub mod binary_search;
pub mod zerotrie;

pub trait DataStore<M: DataMarker> {
    fn get(
        &self,
        req: DataIdentifierBorrowed,
        attributes_prefix_match: bool,
    ) -> Option<&'static M::DataStruct>;

    type IterReturn: Iterator<Item = DataIdentifierCow<'static>>;
    fn iter(&'static self) -> Self::IterReturn;
}
