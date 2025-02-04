// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Runtime `Pattern` implementation which is optimized for zero-allocation
//! deserialization and high-performance runtime use in `DateTimeFormatter`.
//!
//! This module is meant to remain an implementation detail and can evolve to
//! utilize all runtime performance optimizations `ICU4X` needs.
//!
//! For all spec compliant behaviors see `reference::Pattern` equivalent.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>

mod display;
mod generic;
pub(crate) mod helpers;
mod pattern;

pub use generic::GenericPattern;
pub(crate) use generic::ZERO_ONE_TWO_SLICE;
pub(crate) use pattern::PatternBorrowed;
pub use pattern::{Pattern, PatternMetadata, PatternULE};
