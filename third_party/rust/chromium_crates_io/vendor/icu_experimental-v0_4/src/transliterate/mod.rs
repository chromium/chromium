// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Transliteration
//!
//! See [`Transliterator`].

// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
        clippy::exhaustive_structs,
        clippy::exhaustive_enums,
        clippy::trivially_copy_pass_by_ref,
        missing_debug_implementations,
    )
)]
#![warn(missing_docs)]

pub mod provider;

mod compile;
mod transliterator;

#[cfg(feature = "compiled_data")]
pub use transliterator::TransliteratorBuilder;
pub use transliterator::{CustomTransliterator, Transliterator};

pub use compile::RuleCollection;
pub use compile::RuleCollectionProvider;
