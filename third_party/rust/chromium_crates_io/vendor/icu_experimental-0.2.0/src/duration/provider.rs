// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

//! Provider structs for digital data.

use alloc::borrow::Cow;
use icu_provider::prelude::*;

#[icu_provider::data_struct(DigitalDurationDataV1Marker = "duration/digital@1")]
#[derive(Debug, Clone, PartialEq)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::duration::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]

/// A struct containing digital duration data (durationUnit-type-* patterns).
pub struct DigitalDurationDataV1<'data> {
    /// The separator between the hour, minute, and second fields.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub separator: Cow<str, 'data>,

    /// The number of digits to pad hours when hour, minutes and seconds must be displayed.
    /// Calculated from the hms pattern.
    pub hms_padding: HmsPadding,

    /// The number of digits to pad hours when only hour and minutes must be displayed.
    /// Calculated from the hm pattern.
    pub hm_padding: HmPadding,

    /// The number of digits to pad minutes when only minutes and seconds must be displayed.
    /// Calculated from the ms pattern.
    pub ms_padding: MsPadding,
}

#[derive(Debug, Clone, Copy, PartialEq)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::duration::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
/// A struct containing the number of digits to pad hours, minutes, and seconds.
pub struct HmsPadding {
    /// Hour padding.
    pub h: u8,
    /// Minute padding.
    pub m: u8,
    /// Second padding.
    pub s: u8,
}

#[derive(Debug, Clone, Copy, PartialEq)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::duration::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
/// A struct containing the number of digits to pad minutes, and seconds.
pub struct MsPadding {
    /// Minute padding.
    pub m: u8,
    /// Second padding.
    pub s: u8,
}

#[derive(Debug, Clone, Copy, PartialEq)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::duration::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
/// A struct containing the number of digits to pad hours and minutes.
pub struct HmPadding {
    /// Hour padding.
    pub h: u8,
    /// Minute padding.
    pub m: u8,
}
