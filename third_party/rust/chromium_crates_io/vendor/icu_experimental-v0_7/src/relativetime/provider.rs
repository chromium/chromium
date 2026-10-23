// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

#[cfg(feature = "datagen")]
use core::fmt::Debug;
use icu_pattern::SinglePlaceholderPattern;
use icu_plurals::provider::PluralElementsPackedCow;
use icu_provider::prelude::*;
use zerovec::VarZeroVec;

icu_provider::data_marker!(
    /// `DatetimeRelativeSecondLongV1`
    DatetimeRelativeSecondLongV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeSecondShortV1`
    DatetimeRelativeSecondShortV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeSecondNarrowV1`
    DatetimeRelativeSecondNarrowV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeMinuteLongV1`
    DatetimeRelativeMinuteLongV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeMinuteShortV1`
    DatetimeRelativeMinuteShortV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeMinuteNarrowV1`
    DatetimeRelativeMinuteNarrowV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeHourLongV1`
    DatetimeRelativeHourLongV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeHourShortV1`
    DatetimeRelativeHourShortV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeHourNarrowV1`
    DatetimeRelativeHourNarrowV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeDayLongV1`
    DatetimeRelativeDayLongV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeDayShortV1`
    DatetimeRelativeDayShortV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeDayNarrowV1`
    DatetimeRelativeDayNarrowV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeWeekLongV1`
    DatetimeRelativeWeekLongV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeWeekShortV1`
    DatetimeRelativeWeekShortV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeWeekNarrowV1`
    DatetimeRelativeWeekNarrowV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeMonthLongV1`
    DatetimeRelativeMonthLongV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeMonthShortV1`
    DatetimeRelativeMonthShortV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeMonthNarrowV1`
    DatetimeRelativeMonthNarrowV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeQuarterLongV1`
    DatetimeRelativeQuarterLongV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeQuarterShortV1`
    DatetimeRelativeQuarterShortV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeQuarterNarrowV1`
    DatetimeRelativeQuarterNarrowV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeYearLongV1`
    DatetimeRelativeYearLongV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeYearShortV1`
    DatetimeRelativeYearShortV1,
    RelativeTimePatternData<'static>,
);
icu_provider::data_marker!(
    /// `DatetimeRelativeYearNarrowV1`
    DatetimeRelativeYearNarrowV1,
    RelativeTimePatternData<'static>,
);

/// Relative time format  data struct.
#[derive(Debug, Clone, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::relativetime::provider))]
#[yoke(prove_covariance_manually)]
pub struct RelativeTimePatternData<'data> {
    /// Index of offset 0 within `relatives` (so offset `i` is stored at `i + zero_index`).
    pub zero_index: u8,
    /// Mapping for relative times with unique names (e.g. `["yesterday", "today", "tomorrow"]`).
    /// Empty strings indicate missing entries in the contiguous range.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub relatives: VarZeroVec<'data, str>,
    /// How to display times in the past.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub past: PluralElementsPackedCow<'data, SinglePlaceholderPattern>,
    /// How to display times in the future.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub future: PluralElementsPackedCow<'data, SinglePlaceholderPattern>,
}

impl<'data> RelativeTimePatternData<'data> {
    /// Returns the relative time string for `offset` (e.g. `-1` for `"yesterday"`), if present.
    pub fn get_relative(&self, offset: i8) -> Option<&str> {
        let idx = usize::from(self.zero_index).wrapping_add_signed(isize::from(offset));
        self.relatives.as_slice().get(idx).filter(|s| !s.is_empty())
    }
}

icu_provider::data_struct!(RelativeTimePatternData<'_>, #[cfg(feature = "datagen")]);
