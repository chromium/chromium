// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
#![cfg_attr(not(any(test, doc)), no_std)]
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

//! TODO

#[cfg(feature = "alloc")]
extern crate alloc;

pub mod provider;
pub mod scaffold;

#[cfg(feature = "ixdtf")]
mod ixdtf;
#[cfg(feature = "ixdtf")]
pub use ixdtf::ParseError;

pub mod zone;
#[doc(no_inline)]
pub use zone::{TimeZone, TimeZoneInfo};

mod types;
pub use types::{DateTime, Hour, Minute, Nanosecond, Second, Time, ZonedDateTime};
