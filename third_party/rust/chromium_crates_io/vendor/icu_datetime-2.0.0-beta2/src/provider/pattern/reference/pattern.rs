// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[cfg(any(feature = "serde", test))]
use super::super::runtime;
use super::{
    super::{PatternError, PatternItem, TimeGranularity},
    Parser,
};
use alloc::vec::Vec;
use core::str::FromStr;

/// A fully-owned, non-zero-copy type corresponding to [`Pattern`](super::super::runtime::Pattern).
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, Default, Clone, PartialEq)]
pub struct Pattern {
    pub(crate) items: Vec<PatternItem>,
    pub(crate) time_granularity: TimeGranularity,
}

impl Pattern {
    /// Convert a [`Pattern`] to a vector of pattern items.
    ///
    /// The [`Pattern`] can be restored via the `From` impl.
    pub fn into_items(self) -> Vec<PatternItem> {
        self.items
    }

    #[cfg(feature = "datagen")]
    pub(crate) fn items(&self) -> &[PatternItem] {
        &self.items
    }

    #[cfg(feature = "datagen")]
    pub(crate) fn items_mut(&mut self) -> &mut [PatternItem] {
        &mut self.items
    }

    #[cfg(any(feature = "serde", test))]
    pub(crate) fn to_runtime_pattern(&self) -> runtime::Pattern<'static> {
        runtime::Pattern::from(self)
    }
}

impl From<Vec<PatternItem>> for Pattern {
    fn from(items: Vec<PatternItem>) -> Self {
        Self {
            time_granularity: items
                .iter()
                .copied()
                .map(Into::into)
                .max()
                .unwrap_or_default(),
            items,
        }
    }
}

impl From<&str> for Pattern {
    fn from(items: &str) -> Self {
        Self {
            time_granularity: TimeGranularity::default(),
            items: items.chars().map(|ch| ch.into()).collect(),
        }
    }
}

impl FromStr for Pattern {
    type Err = PatternError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Parser::new(s).parse().map(Self::from)
    }
}
