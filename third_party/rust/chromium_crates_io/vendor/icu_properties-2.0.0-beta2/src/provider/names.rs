// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 🚧 \[Unstable\] Property names-related data for this component
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]

use icu_locale_core::subtags::Script;
use icu_provider::prelude::*;

use zerotrie::ZeroTrieSimpleAscii;
use zerovec::ule::NichedOption;
use zerovec::{VarZeroVec, ZeroMap, ZeroVec};

icu_provider::data_marker!(
    /// `BidiClassNameToValueV2`
    BidiClassNameToValueV2,
    PropertyValueNameToEnumMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `CanonicalCombiningClassNameToValueV2`
    CanonicalCombiningClassNameToValueV2,
    PropertyValueNameToEnumMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `EastAsianWidthNameToValueV2`
    EastAsianWidthNameToValueV2,
    PropertyValueNameToEnumMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `GeneralCategoryMaskNameToValueV2`
    GeneralCategoryMaskNameToValueV2,
    PropertyValueNameToEnumMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `GeneralCategoryNameToValueV2`
    GeneralCategoryNameToValueV2,
    PropertyValueNameToEnumMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `GraphemeClusterBreakNameToValueV2`
    GraphemeClusterBreakNameToValueV2,
    PropertyValueNameToEnumMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `HangulSyllableTypeNameToValueV2`
    HangulSyllableTypeNameToValueV2,
    PropertyValueNameToEnumMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `IndicSyllabicCategoryNameToValueV2`
    IndicSyllabicCategoryNameToValueV2,
    PropertyValueNameToEnumMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `JoiningTypeNameToValueV2`
    JoiningTypeNameToValueV2,
    PropertyValueNameToEnumMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `LineBreakNameToValueV2`
    LineBreakNameToValueV2,
    PropertyValueNameToEnumMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `ScriptNameToValueV2`
    ScriptNameToValueV2,
    PropertyValueNameToEnumMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `SentenceBreakNameToValueV2`
    SentenceBreakNameToValueV2,
    PropertyValueNameToEnumMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `WordBreakNameToValueV2`
    WordBreakNameToValueV2,
    PropertyValueNameToEnumMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `BidiClassValueToLongNameV1`
    BidiClassValueToLongNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `BidiClassValueToShortNameV1`
    BidiClassValueToShortNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `EastAsianWidthValueToLongNameV1`
    EastAsianWidthValueToLongNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `EastAsianWidthValueToShortNameV1`
    EastAsianWidthValueToShortNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `GeneralCategoryValueToLongNameV1`
    GeneralCategoryValueToLongNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `GeneralCategoryValueToShortNameV1`
    GeneralCategoryValueToShortNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `GraphemeClusterBreakValueToLongNameV1`
    GraphemeClusterBreakValueToLongNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `GraphemeClusterBreakValueToShortNameV1`
    GraphemeClusterBreakValueToShortNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `HangulSyllableTypeValueToLongNameV1`
    HangulSyllableTypeValueToLongNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `HangulSyllableTypeValueToShortNameV1`
    HangulSyllableTypeValueToShortNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `IndicSyllabicCategoryValueToLongNameV1`
    IndicSyllabicCategoryValueToLongNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `IndicSyllabicCategoryValueToShortNameV1`
    IndicSyllabicCategoryValueToShortNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `JoiningTypeValueToLongNameV1`
    JoiningTypeValueToLongNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `JoiningTypeValueToShortNameV1`
    JoiningTypeValueToShortNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `LineBreakValueToLongNameV1`
    LineBreakValueToLongNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `LineBreakValueToShortNameV1`
    LineBreakValueToShortNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `ScriptValueToLongNameV1`
    ScriptValueToLongNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `SentenceBreakValueToLongNameV1`
    SentenceBreakValueToLongNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `SentenceBreakValueToShortNameV1`
    SentenceBreakValueToShortNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `WordBreakValueToLongNameV1`
    WordBreakValueToLongNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `WordBreakValueToShortNameV1`
    WordBreakValueToShortNameV1,
    PropertyEnumToValueNameLinearMap<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `CanonicalCombiningClassValueToLongNameV1`
    CanonicalCombiningClassValueToLongNameV1,
    PropertyEnumToValueNameSparseMap<'static>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// `CanonicalCombiningClassValueToShortNameV1`
    CanonicalCombiningClassValueToShortNameV1,
    PropertyEnumToValueNameSparseMap<'static>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// `ScriptValueToShortNameV1`
    ScriptValueToShortNameV1,
    PropertyScriptToIcuScriptMap<'static>,
    is_singleton = true,
);

/// A set of characters and strings which share a particular property value.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, Clone, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_properties::provider::names))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct PropertyValueNameToEnumMap<'data> {
    /// A map from names to their value discriminant
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub map: ZeroTrieSimpleAscii<ZeroVec<'data, u8>>,
}

icu_provider::data_struct!(
    PropertyValueNameToEnumMap<'_>,
    #[cfg(feature = "datagen")]
);

/// A mapping of property values to their names. A single instance of this map will only cover
/// either long or short names, determined whilst loading data.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, Clone, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_properties::provider::names))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct PropertyEnumToValueNameSparseMap<'data> {
    /// A map from the value discriminant to the names
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub map: ZeroMap<'data, u16, str>,
}

icu_provider::data_struct!(
    PropertyEnumToValueNameSparseMap<'_>,
    #[cfg(feature = "datagen")]
);

/// A mapping of property values to their names. A single instance of this map will only cover
/// either long or short names, determined whilst loading data.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, Clone, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_properties::provider::names))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct PropertyEnumToValueNameLinearMap<'data> {
    /// A map from the value discriminant (the index) to the names, for mostly
    /// contiguous data. Empty strings count as missing.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub map: VarZeroVec<'data, str>,
}

icu_provider::data_struct!(
    PropertyEnumToValueNameLinearMap<'_>,
    #[cfg(feature = "datagen")]
);

/// A mapping of property values to their names. A single instance of this map will only cover
/// either long or short names, determined whilst loading data.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, Clone, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_properties::provider::names))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct PropertyScriptToIcuScriptMap<'data> {
    /// A map from the value discriminant (the index) to the names, for mostly
    /// contiguous data. Empty strings count as missing.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub map: ZeroVec<'data, NichedOption<Script, 4>>,
}

icu_provider::data_struct!(
    PropertyScriptToIcuScriptMap<'_>,
    #[cfg(feature = "datagen")]
);
