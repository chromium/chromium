// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![allow(clippy::exhaustive_structs)] // part of data struct and internal API

use super::super::{reference, PatternError, PatternItem, TimeGranularity};
use alloc::vec::Vec;
use core::str::FromStr;
use icu_plurals::provider::FourBitMetadata;
use icu_provider::prelude::*;
use zerovec::{ZeroSlice, ZeroVec};

/// A raw, low-level pattern for datetime formatting.
///
/// It consists of an owned-or-borrowed list of [`PatternItem`]s corresponding
/// to either fields or literal characters.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Eq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::pattern::runtime))]
#[zerovec::make_varule(PatternULE)]
#[zerovec::derive(Debug)]
#[zerovec::skip_derive(Ord)]
#[cfg_attr(feature = "serde", zerovec::derive(Deserialize))]
#[cfg_attr(feature = "datagen", zerovec::derive(Serialize))]
pub struct Pattern<'data> {
    /// The list of [`PatternItem`]s.
    pub items: ZeroVec<'data, PatternItem>,
    /// Pre-computed metadata about the pattern.
    ///
    /// This field should contain the smallest time unit from the `items` vec.
    /// If it doesn't, unexpected results for day periods may be encountered.
    pub metadata: PatternMetadata,
}

/// Fully borrowed version of [`Pattern`].
#[derive(Debug, Copy, Clone)]
pub(crate) struct PatternBorrowed<'data> {
    pub(crate) items: &'data ZeroSlice<PatternItem>,
    pub(crate) metadata: PatternMetadata,
}

/// Metadata associated with a [`Pattern`].
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[zerovec::make_ule(PatternMetadataULE)]
#[zerovec::skip_derive(Ord)]
pub struct PatternMetadata(u8);

impl PatternMetadata {
    pub(crate) const DEFAULT: PatternMetadata = Self::from_time_granularity(TimeGranularity::None);

    #[inline]
    pub(crate) fn time_granularity(self) -> TimeGranularity {
        TimeGranularity::from_ordinal(self.0)
    }

    pub(crate) fn from_items(items: &[PatternItem]) -> Self {
        Self::from_iter_items(items.iter().copied())
    }

    pub(crate) fn from_iter_items(iter_items: impl Iterator<Item = PatternItem>) -> Self {
        let time_granularity: TimeGranularity =
            iter_items.map(Into::into).max().unwrap_or_default();
        Self::from_time_granularity(time_granularity)
    }

    /// Merges the metadata from a date pattern and a time pattern into one.
    #[inline]
    pub(crate) fn merge_date_and_time_metadata(
        _date: PatternMetadata,
        time: PatternMetadata,
    ) -> PatternMetadata {
        // Currently we only have time granularity so we ignore the date metadata.
        time
    }

    /// Creates a [`PatternMetadata`] from the [`TimeGranularity`] enum.
    #[inline]
    pub const fn from_time_granularity(time_granularity: TimeGranularity) -> Self {
        Self(time_granularity.ordinal())
    }

    #[cfg(feature = "datagen")]
    #[inline]
    pub(crate) fn set_time_granularity(&mut self, time_granularity: TimeGranularity) {
        self.0 = time_granularity.ordinal();
    }

    pub(crate) fn to_four_bit_metadata(self) -> FourBitMetadata {
        #[allow(clippy::unwrap_used)] // valid values for self.0 are 0, 1, 2, 3, or 4
        FourBitMetadata::try_from_byte(self.0).unwrap()
    }

    pub(crate) fn from_u8(other: u8) -> Self {
        Self(TimeGranularity::from_ordinal(other).ordinal())
    }
}

impl Default for PatternMetadata {
    #[inline]
    fn default() -> Self {
        Self::DEFAULT
    }
}

impl Pattern<'_> {
    #[cfg(feature = "datagen")]
    pub(crate) fn into_owned(self) -> Pattern<'static> {
        Pattern {
            items: self.items.into_owned(),
            metadata: self.metadata,
        }
    }

    pub(crate) fn as_borrowed(&self) -> PatternBorrowed {
        PatternBorrowed {
            items: &self.items,
            metadata: self.metadata,
        }
    }

    /// Borrows a [`Pattern`] from another [`Pattern`].
    pub fn as_ref(&self) -> Pattern<'_> {
        self.as_borrowed().as_pattern()
    }
}

impl<'data> PatternBorrowed<'data> {
    pub(crate) const DEFAULT: PatternBorrowed<'static> = PatternBorrowed {
        items: ZeroSlice::new_empty(),
        metadata: PatternMetadata::DEFAULT,
    };

    pub(crate) fn as_pattern(&self) -> Pattern<'data> {
        Pattern {
            items: self.items.as_zerovec(),
            metadata: self.metadata,
        }
    }
}

impl From<Vec<PatternItem>> for Pattern<'_> {
    fn from(items: Vec<PatternItem>) -> Self {
        Self {
            metadata: PatternMetadata::from_items(&items),
            items: ZeroVec::alloc_from_slice(&items),
        }
    }
}

impl FromIterator<PatternItem> for Pattern<'_> {
    fn from_iter<T: IntoIterator<Item = PatternItem>>(iter: T) -> Self {
        let items = iter.into_iter().collect::<ZeroVec<PatternItem>>();
        Self {
            metadata: PatternMetadata::from_iter_items(items.iter()),
            items,
        }
    }
}

impl From<&reference::Pattern> for Pattern<'_> {
    fn from(input: &reference::Pattern) -> Self {
        Self {
            items: ZeroVec::alloc_from_slice(&input.items),
            metadata: PatternMetadata::from_time_granularity(input.time_granularity),
        }
    }
}

impl From<&Pattern<'_>> for reference::Pattern {
    fn from(input: &Pattern<'_>) -> Self {
        Self {
            items: input.items.to_vec(),
            time_granularity: input.metadata.time_granularity(),
        }
    }
}

impl FromStr for Pattern<'_> {
    type Err = PatternError;

    fn from_str(input: &str) -> Result<Self, Self::Err> {
        let reference = reference::Pattern::from_str(input)?;
        Ok(Self::from(&reference))
    }
}

impl Default for Pattern<'_> {
    fn default() -> Self {
        Self {
            items: ZeroVec::new(),
            metadata: PatternMetadata::default(),
        }
    }
}

#[cfg(feature = "datagen")]
impl databake::Bake for PatternMetadata {
    fn bake(&self, ctx: &databake::CrateEnv) -> databake::TokenStream {
        ctx.insert("icu_datetime");
        let time_granularity = databake::Bake::bake(&self.time_granularity(), ctx);
        databake::quote! {
            icu_datetime::provider::pattern::runtime::PatternMetadata::from_time_granularity(#time_granularity)
        }
    }
}

#[cfg(feature = "datagen")]
impl databake::BakeSize for PatternMetadata {
    fn borrows_size(&self) -> usize {
        0
    }
}

#[test]
#[cfg(feature = "datagen")]
fn databake() {
    databake::test_bake!(
        PatternMetadata,
        const,
        crate::provider::pattern::runtime::PatternMetadata::from_time_granularity(
            crate::provider::pattern::TimeGranularity::Hours
        ),
        icu_datetime,
    );
}
