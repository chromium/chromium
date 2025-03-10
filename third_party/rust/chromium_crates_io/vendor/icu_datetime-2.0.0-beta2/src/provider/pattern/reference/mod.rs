// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Reference `Pattern` implementation intended to follow Unicode
//! specification, and become publicly available for tooling to use
//! for parsing/inspecting/modifying and serialization.
//!
//! The runtime `Pattern` uses parsing/serialization from this module.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>

mod generic;
mod parser;
pub(crate) mod pattern;

pub use generic::GenericPattern;
pub(crate) use parser::Parser;
pub use pattern::Pattern;
