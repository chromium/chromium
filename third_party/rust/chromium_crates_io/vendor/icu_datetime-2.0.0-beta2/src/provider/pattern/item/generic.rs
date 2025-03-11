// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

/// An element of a [`GenericPattern`](super::super::runtime::GenericPattern).
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::pattern))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub enum GenericPatternItem {
    /// A single digit, 0..=9
    Placeholder(u8),
    /// A literal code point
    Literal(char),
}

impl From<u8> for GenericPatternItem {
    fn from(input: u8) -> Self {
        Self::Placeholder(input)
    }
}

impl From<char> for GenericPatternItem {
    fn from(input: char) -> Self {
        Self::Literal(input)
    }
}
