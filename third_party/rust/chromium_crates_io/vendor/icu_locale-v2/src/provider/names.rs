// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_pattern::DoublePlaceholderPattern;
use icu_provider::prelude::*;
use zerovec::VarZeroCow;

/// Display name parts for use in menus.
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_locale::provider::names))]
#[zerovec::make_varule(MenuNamePartsULE)]
#[zerovec::derive(Debug)]
#[zerovec::skip_derive(Ord)]
#[cfg_attr(feature = "serde", zerovec::derive(Deserialize))]
#[cfg_attr(feature = "datagen", zerovec::derive(Serialize))]
pub struct MenuNameParts<'data> {
    /// The "core" part of a language menu display name.
    ///
    /// For example, "Kurdish" in "Kurdish (Kurmanji)".
    pub core: VarZeroCow<'data, str>,
    /// The "extension" part of a language menu display name.
    ///
    /// For example, "Kurmanji" in "Kurdish (Kurmanji)".
    ///
    /// Note: this is the empty string for language menu names that do not have an extension.
    /// For example, in CLDR 48, "Chinese, Mandarin" is the core and there is no extension.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub extension: VarZeroCow<'data, str>,
}

/// [`LocaleNamesEssentials`] provides the formatting patterns used to combine subtags.
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_locale::provider::names))]
pub struct LocaleNamesEssentials<'data> {
    /// The pattern used to combine the base language name with qualifiers (e.g., `"{0} ({1})"`).
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub locale_pattern: VarZeroCow<'data, DoublePlaceholderPattern>,
    /// The separator used to join multiple qualifiers (e.g., `"{0}, {1}"`).
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub locale_separator: VarZeroCow<'data, DoublePlaceholderPattern>,
}

icu_provider::data_struct!(LocaleNamesEssentials<'_>, #[cfg(feature = "datagen")]);

icu_provider::data_marker!(
    /// Data marker for tiny region display names.
    LocaleNamesRegionMediumTinyV1,
    "locale/names/region/medium/tiny/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_region",
);

icu_provider::data_marker!(
    /// Data marker for light region display names.
    LocaleNamesRegionMediumLightV1,
    "locale/names/region/medium/light/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_region",
);

icu_provider::data_marker!(
    /// Data marker for tiny short region display names.
    LocaleNamesRegionShortTinyV1,
    "locale/names/region/short/tiny/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_region",
);

icu_provider::data_marker!(
    /// Data marker for light short region display names.
    LocaleNamesRegionShortLightV1,
    "locale/names/region/short/light/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_region",
);

icu_provider::data_marker!(
    /// Data marker for tiny language display names.
    LocaleNamesLanguageMediumTinyV1,
    "locale/names/language/medium/tiny/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_language",
);

icu_provider::data_marker!(
    /// Data marker for light language display names.
    LocaleNamesLanguageMediumLightV1,
    "locale/names/language/medium/light/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_language",
);

icu_provider::data_marker!(
    /// Data marker for heavy language display names.
    LocaleNamesLanguageMediumHeavyV1,
    "locale/names/language/medium/heavy/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_language",
);

icu_provider::data_marker!(
    /// Data marker for light short language display names.
    LocaleNamesLanguageShortLightV1,
    "locale/names/language/short/light/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_language",
);

icu_provider::data_marker!(
    /// Data marker for heavy short language display names.
    LocaleNamesLanguageShortHeavyV1,
    "locale/names/language/short/heavy/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_language",
);

icu_provider::data_marker!(
    /// Data marker for light long language display names.
    LocaleNamesLanguageLongLightV1,
    "locale/names/language/long/light/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_language",
);

icu_provider::data_marker!(
    /// Data marker for heavy long language display names.
    LocaleNamesLanguageLongHeavyV1,
    "locale/names/language/long/heavy/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_language",
);

icu_provider::data_marker!(
    /// Data marker for light menu-medium language display names.
    LocaleNamesLanguageMenuMediumLightV1,
    "locale/names/language/menu/medium/light/v1",
    VarZeroCow<'static, MenuNamePartsULE>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_language",
);

icu_provider::data_marker!(
    /// Data marker for heavy menu-medium language display names.
    LocaleNamesLanguageMenuMediumHeavyV1,
    "locale/names/language/menu/medium/heavy/v1",
    VarZeroCow<'static, MenuNamePartsULE>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_language",
);

icu_provider::data_marker!(
    /// Data marker for tiny script display names.
    LocaleNamesScriptMediumTinyV1,
    "locale/names/script/medium/tiny/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_script",
);

icu_provider::data_marker!(
    /// Data marker for light script display names.
    LocaleNamesScriptMediumLightV1,
    "locale/names/script/medium/light/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_script",
);

icu_provider::data_marker!(
    /// Data marker for heavy script display names.
    LocaleNamesScriptMediumHeavyV1,
    "locale/names/script/medium/heavy/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_script",
);

icu_provider::data_marker!(
    /// Data marker for heavy short script display names.
    LocaleNamesScriptShortHeavyV1,
    "locale/names/script/short/heavy/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_script",
);

icu_provider::data_marker!(
    /// Data marker for heavy variant display names.
    LocaleNamesVariantMediumHeavyV1,
    "locale/names/variant/medium/heavy/v1",
    VarZeroCow<'static, str>,
    #[cfg(feature = "datagen")]
    attributes_domain = "locale_names_variant",
);

icu_provider::data_marker!(
    /// Data marker for locale names essentials (patterns).
    LocaleNamesEssentialsV1,
    "locale/names/essentials/v1",
    LocaleNamesEssentials<'static>
);
