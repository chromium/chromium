// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::{
    DisplayNamesPreferences, LanguageIdentifierDisplayNameOptions, RegionDisplayName,
    ScriptDisplayName, VariantDisplayName, load_one,
};
use crate::provider::names::{
    LocaleNamesEssentialsV1, LocaleNamesLanguageLongHeavyV1, LocaleNamesLanguageLongLightV1,
    LocaleNamesLanguageMediumHeavyV1, LocaleNamesLanguageMediumLightV1,
    LocaleNamesLanguageMediumTinyV1, LocaleNamesLanguageMenuMediumHeavyV1,
    LocaleNamesLanguageMenuMediumLightV1, LocaleNamesLanguageShortHeavyV1,
    LocaleNamesLanguageShortLightV1, LocaleNamesRegionMediumLightV1, LocaleNamesRegionMediumTinyV1,
    LocaleNamesRegionShortLightV1, LocaleNamesRegionShortTinyV1, LocaleNamesScriptMediumHeavyV1,
    LocaleNamesScriptMediumLightV1, LocaleNamesScriptMediumTinyV1, LocaleNamesScriptShortHeavyV1,
    LocaleNamesVariantMediumHeavyV1, MenuNamePartsULE,
};
use crate::size_test_macro::size_test;
use alloc::vec::Vec;
use icu_locale_core::LanguageIdentifier;
use icu_locale_core::subtags::{Language, Region, Script, Variant};
use icu_pattern::{DoublePlaceholderPattern, PatternItem, TryWrap};
use icu_provider::DataPayloadOr;
use icu_provider::marker::ErasedMarker;
use icu_provider::prelude::*;
use tinystr::{TinyAsciiStr, tinystr};
use writeable::LengthHint;
use writeable::{PartsWrite, TryWriteable, adapters::LossyWrap};
use zerovec::VarZeroCow;

/// An error returned when a display name was not found in data and has fallen back to the raw BCP-47 subtag code.
#[derive(displaydoc::Display, Debug, Copy, Clone, PartialEq, Eq, Default)]
#[allow(clippy::exhaustive_structs)]
pub struct LanguageIdentifierNameFallbackError;

/// A data struct that is either [`MenuNameParts`] or a string
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[allow(clippy::exhaustive_enums)] // provider-unstable
enum MenuNamePartsOrString<'a> {
    /// A data struct that is [`MenuNameParts`]
    MenuNameParts(VarZeroCow<'a, MenuNamePartsULE>),
    /// A data struct that is a string
    String(VarZeroCow<'a, str>),
}

type MenuNamePartsMarker = ErasedMarker<VarZeroCow<'static, MenuNamePartsULE>>;

type StringMarker = ErasedMarker<VarZeroCow<'static, str>>;

size_test!(
    LanguageIdentifierDisplayName,
    language_identifier_display_name_owned_size,
    192
);

macro_rules! table_row {
    (try_new_tiny) => {
        "| [`try_new_tiny`](Self::try_new_tiny) | \"English (United States)\" | ❌ | ❌ |"
    };
    (try_new_light) => {
        "| [`try_new_light`](Self::try_new_light) | \"American English\" | \"Chinese\" | ❌ |"
    };
    (try_new_short_light) => {
        "| [`try_new_short_light`](Self::try_new_short_light) | \"US English\" | \"Chinese\" | ❌ |"
    };
    (try_new_long_light) => {
        "| [`try_new_long_light`](Self::try_new_long_light) | \"American English\" | \"Mandarin Chinese\" | ❌ |"
    };
    (try_new_menu_light) => {
        "| [`try_new_menu_light`](Self::try_new_menu_light) | \"English (United States)\" | \"Chinese, Mandarin\" | ❌ |"
    };
    (try_new_short_menu_light) => {
        "| [`try_new_short_menu_light`](Self::try_new_short_menu_light) | \"English (US)\" | \"Chinese, Mandarin\" | ❌ |"
    };
    (try_new_heavy) => {
        "| [`try_new_heavy`](Self::try_new_heavy) | \"American English\" | \"Chinese\" | \"Azerbaijani\" |"
    };
    (try_new_short_heavy) => {
        "| [`try_new_short_heavy`](Self::try_new_short_heavy) | \"US English\" | \"Chinese\" | \"Azeri\" |"
    };
    (try_new_long_heavy) => {
        "| [`try_new_long_heavy`](Self::try_new_long_heavy) | \"American English\" | \"Mandarin Chinese\" | \"Azerbaijani\" |"
    };
    (try_new_menu_heavy) => {
        "| [`try_new_menu_heavy`](Self::try_new_menu_heavy) | \"English (United States)\" | \"Chinese, Mandarin\" | \"Azerbaijani\" |"
    };
    (try_new_short_menu_heavy) => {
        "| [`try_new_short_menu_heavy`](Self::try_new_short_menu_heavy) | \"English (US)\" | \"Chinese, Mandarin\" | \"Azeri\" |"
    };
}

/// A localized display name for a language identifier, owned version.
///
/// The formatter falls back to the BCP-47 subtag when localized display names are missing
/// from the data provider. Fallback can be detected using [`TryWriteable`].
///
/// # Constructor Behavior
///
/// There are several constructors, each of which links different data and serve
/// different use cases. The behavior is illustrated in the table below.
///
/// | Constructor | `en-US` | `zh` | `az` |
/// | :--- | :--- | :--- | :--- |
#[doc = concat!(table_row!(try_new_tiny), "\n")]
#[doc = concat!(table_row!(try_new_light), "\n")]
#[doc = concat!(table_row!(try_new_short_light), "\n")]
#[doc = concat!(table_row!(try_new_long_light), "\n")]
#[doc = concat!(table_row!(try_new_menu_light), "\n")]
#[doc = concat!(table_row!(try_new_short_menu_light), "\n")]
#[doc = concat!(table_row!(try_new_heavy), "\n")]
#[doc = concat!(table_row!(try_new_short_heavy), "\n")]
#[doc = concat!(table_row!(try_new_long_heavy), "\n")]
#[doc = concat!(table_row!(try_new_menu_heavy), "\n")]
#[doc = concat!(table_row!(try_new_short_menu_heavy), "\n")]
///
/// > Note: :x: means that the name includes a BCP-47 subtag fallback.
///
/// # Examples
///
/// ```
/// use icu::locale::names::{
///     DisplayNamesPreferences, LanguageIdentifierDisplayName,
///     LanguageIdentifierDisplayNameOptions,
/// };
/// use icu::locale::{langid, locale};
/// use writeable::assert_try_writeable_eq;
///
/// let prefs = DisplayNamesPreferences::from(locale!("en"));
/// let options = LanguageIdentifierDisplayNameOptions::default();
/// let display_name = LanguageIdentifierDisplayName::try_new_light(
///     prefs,
///     langid!("fr-CA"),
///     options,
/// )
/// .expect("Data should load successfully");
///
/// assert_try_writeable_eq!(
///     display_name.as_borrowed(),
///     "Canadian French",
///     Ok(())
/// );
/// ```
///
/// When a subtag is unknown:
///
/// ```
/// use icu::locale::names::{
///     DisplayNamesPreferences, LanguageIdentifierDisplayName, LanguageIdentifierDisplayNameOptions,
///     LanguageIdentifierNameFallbackError,
/// };
/// use icu::locale::{locale, langid};
/// use writeable::{Part, TryWriteable, assert_try_writeable_parts_eq};
///
/// let prefs = DisplayNamesPreferences::from(locale!("en"));
/// let options = LanguageIdentifierDisplayNameOptions::default();
///
/// // "it-Qabc-150" has known language "it" ("Italian") and known region "150" ("Europe"),
/// // but unknown script "Qabc".
/// let lang_id = langid!("it-Qabc-150");
/// let display_name = LanguageIdentifierDisplayName::try_new_light(
///     prefs,
///     lang_id,
///     options,
/// )
/// .expect("Data should load successfully");
///
/// let borrowed = display_name.as_borrowed();
///
/// // The fallback string is identified with a [`Part::ERROR`](writeable::Part::Error):
/// assert_try_writeable_parts_eq!(
///     borrowed,
///     "Italian (Qabc, Europe)",
///     Err(LanguageIdentifierNameFallbackError),
///     [(9, 13, Part::ERROR)] // the span of Qabc
/// );
///
/// // To format in lossy mode (ignoring fallback errors), use [`Writeable`] or [`Display`]:
/// use writeable::assert_writeable_eq;
/// assert_writeable_eq!(borrowed, "Italian (Qabc, Europe)");
/// ```
#[doc = language_identifier_display_name_owned_size!()]
#[derive(Debug)]
pub struct LanguageIdentifierDisplayName {
    /// Either the language display name or the subtag as fallback
    language_payload: DataPayloadOr<ErasedMarker<MenuNamePartsOrString<'static>>, Language>,
    /// All other fields (shared between Standard and Menu)
    qualifiers: QualifiersOwned,
}

type VariantPayloads = DataPayloadOr<
    ErasedMarker<VarZeroCow<'static, str>>,
    Result<Vec<DataPayloadOr<ErasedMarker<VarZeroCow<'static, str>>, Variant>>, Variant>,
>;

#[derive(Debug)]
struct QualifiersOwned {
    /// Either the script display name, the subtag as fallback, or None if absent
    script_payload: DataPayloadOr<ErasedMarker<VarZeroCow<'static, str>>, Option<Script>>,
    /// Either the region display name, the subtag as fallback, or None if absent
    region_payload: DataPayloadOr<ErasedMarker<VarZeroCow<'static, str>>, Option<Region>>,
    /// Either a single variant display name, the subtag as fallback, or
    /// a vector of variant display names or subtags as fallback.
    /// The vector may be empty.
    variant_payloads: VariantPayloads,
    essentials_payload: DataPayload<LocaleNamesEssentialsV1>,
}

#[inline]
fn make_attributes_for_subtag(subtag: &Language) -> &DataMarkerAttributes {
    // Valid Language subtags conform to DataMarkerAttributes syntax.
    DataMarkerAttributes::from_str_or_panic(subtag.as_str())
}

#[inline]
fn make_attributes_for_langid(
    language: Language,
    script: Option<Script>,
    region: Option<Region>,
    buffer: &mut TinyAsciiStr<16>,
) -> &DataMarkerAttributes {
    const HYPHEN: TinyAsciiStr<1> = tinystr!(1, "-");
    let lang_str = language.to_tinystr();
    *buffer = match (script, region) {
        (Some(script), Some(region)) => {
            let script_str = script.to_tinystr();
            let region_str = region.to_tinystr();
            lang_str
                .concat::<1, 16>(HYPHEN)
                .concat::<4, 16>(script_str)
                .concat::<1, 16>(HYPHEN)
                .concat::<3, 16>(region_str)
        }
        (Some(script), None) => {
            let script_str = script.to_tinystr();
            lang_str.concat::<1, 16>(HYPHEN).concat::<4, 16>(script_str)
        }
        (None, Some(region)) => {
            let region_str = region.to_tinystr();
            lang_str.concat::<1, 16>(HYPHEN).concat::<3, 16>(region_str)
        }
        (None, None) => lang_str.resize::<16>(),
    };
    // This is infallible (will not panic) because validated `Language`, `Script`,
    // `Region`, and hyphens are guaranteed to conform to `DataMarkerAttributes` syntax.
    DataMarkerAttributes::from_str_or_panic(buffer)
}

#[inline]
fn make_locale(prefs: DisplayNamesPreferences) -> DataLocale {
    // All language markers use the same locale
    LocaleNamesLanguageMediumLightV1::make_locale(prefs.locale_preferences)
}

/// Loads the name for a language dialect, which includes script and region subtags.
///
/// We try to load names for combinations of subtags:
///
/// - Language + Script + Region (e.g., "zh-Hant-HK")
/// - Language + Script (e.g., "zh-Hant")
/// - Language + Region (e.g., "en-GB")
///
/// We then "consume"  the corresponding subtags from the input `LanguageIdentifier`
/// so they are not repeated in the qualifiers.
macro_rules! try_load_dialect_name {
    ($provider:expr, $locale:expr, $subject:expr, [ $first_marker:ident $(, $rest_marker:ident)* $(,)? ]) => {{
        let mut result = None;
        for (language, script, region) in [
            ($subject.language, Some($subject.script), Some($subject.region)),
            ($subject.language, Some($subject.script), None),
            ($subject.language, None, Some($subject.region)),
        ] {
            let script_val = match script {
                Some(Some(s)) => Some(s),
                Some(None) => continue,
                None => None,
            };
            let region_val = match region {
                Some(Some(r)) => Some(r),
                Some(None) => continue,
                None => None,
            };
            let mut buffer = TinyAsciiStr::default();
            let attrs = make_attributes_for_langid(
                language,
                script_val,
                region_val,
                &mut buffer,
            );
            let loaded = load_one::<$first_marker, StringMarker, _>($provider, $locale, attrs)?
                $(
                    .map_or_else(
                        || load_one::<$rest_marker, _, _>($provider, $locale, attrs),
                        |p| Ok::<_, DataError>(Some(p)),
                    )?
                )*;
            if let Some(payload) = loaded {
                if script_val.is_some() {
                    $subject.script = None;
                }
                if region_val.is_some() {
                    $subject.region = None;
                }
                result = Some(payload.map_project(|p, _| MenuNamePartsOrString::String(p)));
                break;
            }
        }
        Ok::<_, DataError>(result)
    }};
}

impl LanguageIdentifierDisplayName {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, subject: LanguageIdentifier, options: LanguageIdentifierDisplayNameOptions) -> result: Result<Self, DataError>,
        /// Loads a language display name in a formatting locale using compiled data.
        ///
        /// The `light` constructor links data for all common languages.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::{
        ///     DisplayNamesPreferences, LanguageIdentifierDisplayName,
        ///     LanguageIdentifierDisplayNameOptions, LanguageIdentifierNameFallbackError,
        /// };
        /// use icu::locale::{langid, locale};
        /// use writeable::assert_try_writeable_eq;
        ///
        /// let prefs = DisplayNamesPreferences::from(locale!("de"));
        /// let options = LanguageIdentifierDisplayNameOptions::default();
        ///
        /// // Light dataset formats common languages and subtags (e.g. region qualification):
        /// let display_name = LanguageIdentifierDisplayName::try_new_light(
        ///     prefs,
        ///     langid!("fr-CA"),
        ///     options,
        /// )
        /// .expect("Data should load successfully");
        /// assert_try_writeable_eq!(display_name.as_borrowed(), "Französisch (Kanada)", Ok(()));
        ///
        /// // Light data does not include rare languages like Klingon ("tlh"), returning a fallback error:
        /// let tlh = LanguageIdentifierDisplayName::try_new_light(prefs, langid!("tlh"), options)
        ///     .expect("Data should load successfully");
        /// assert_try_writeable_eq!(tlh.as_borrowed(), "tlh", Err(LanguageIdentifierNameFallbackError));
        /// ```
        functions: [
            try_new_light,
            try_new_light_with_buffer_provider,
            try_new_light_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_light)]
    pub fn try_new_light_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        mut subject: LanguageIdentifier,
        options: LanguageIdentifierDisplayNameOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesLanguageMediumLightV1>
            + DataProvider<LocaleNamesLanguageMediumTinyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        let locale = make_locale(prefs);
        let load_dialect = options.should_load_dialect();
        let mut language_payload = None;
        if load_dialect {
            language_payload = try_load_dialect_name!(
                provider,
                &locale,
                subject,
                [
                    LocaleNamesLanguageMediumLightV1,
                    LocaleNamesLanguageMediumTinyV1
                ]
            )?;
        }
        if language_payload.is_none() {
            let attrs = make_attributes_for_subtag(&subject.language);
            language_payload =
                load_one::<LocaleNamesLanguageMediumLightV1, _, _>(provider, &locale, attrs)?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumTinyV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map(|payload: DataPayload<StringMarker>| {
                        payload.map_project(|p, _| MenuNamePartsOrString::String(p))
                    });
        }
        let language_payload = match language_payload {
            Some(payload) => DataPayloadOr::from_payload(payload),
            None => DataPayloadOr::from_other(subject.language),
        };

        let qualifiers = QualifiersOwned::try_new_light_unstable(provider, prefs, subject)?;
        Ok(Self {
            language_payload,
            qualifiers,
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, subject: LanguageIdentifier, options: LanguageIdentifierDisplayNameOptions) -> result: Result<Self, DataError>,
        /// Loads a language display name in a formatting locale using compiled data.
        ///
        /// The `tiny` constructor links an extremely limited amount of data, with a focus on
        /// languages associated with the formatting locale.
        /// See the [class docs](Self) for more information on which constructor to use.
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::{
        ///     LanguageIdentifierDisplayName, LanguageIdentifierNameFallbackError,
        /// };
        /// use icu::locale::{langid, locale};
        /// use writeable::assert_try_writeable_eq;
        ///
        /// // French contains its own translation in French...
        /// let fr_fr = LanguageIdentifierDisplayName::try_new_tiny(
        ///     locale!("fr").into(),
        ///     langid!("fr"),
        ///     Default::default()
        /// )
        /// .unwrap();
        /// assert_try_writeable_eq!(
        ///     fr_fr.as_borrowed(),
        ///     "français"
        /// );
        ///
        /// // ...but not a translation for German.
        /// let fr_de = LanguageIdentifierDisplayName::try_new_tiny(
        ///     locale!("fr").into(),
        ///     langid!("de"),
        ///     Default::default()
        /// )
        /// .unwrap();
        /// assert_try_writeable_eq!(
        ///     fr_de.as_borrowed(),
        ///     "de",
        ///     Err(LanguageIdentifierNameFallbackError)
        /// );
        /// ```
        functions: [
            try_new_tiny,
            try_new_tiny_with_buffer_provider,
            try_new_tiny_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_tiny)]
    pub fn try_new_tiny_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        mut subject: LanguageIdentifier,
        options: LanguageIdentifierDisplayNameOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesLanguageMediumTinyV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        let locale = make_locale(prefs);
        // Step 1: Load language name
        let load_dialect = options.should_load_dialect();
        let mut language_payload = None;
        // Only try dialect if requested (or default)
        if load_dialect {
            language_payload = try_load_dialect_name!(
                provider,
                &locale,
                subject,
                [LocaleNamesLanguageMediumTinyV1]
            )?;
        }
        if language_payload.is_none() {
            let attrs = make_attributes_for_subtag(&subject.language);
            language_payload = load_one::<LocaleNamesLanguageMediumTinyV1, _, _>(
                provider, &locale, attrs,
            )?
            .map(|payload: DataPayload<StringMarker>| {
                payload.map_project(|p, _| MenuNamePartsOrString::String(p))
            });
        }
        let language_payload = match language_payload {
            Some(payload) => DataPayloadOr::from_payload(payload),
            None => DataPayloadOr::from_other(subject.language),
        };

        // Load the remaining data
        let qualifiers = QualifiersOwned::try_new_tiny_unstable(provider, prefs, subject)?;
        Ok(Self {
            language_payload,
            qualifiers,
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, subject: LanguageIdentifier, options: LanguageIdentifierDisplayNameOptions) -> result: Result<Self, DataError>,
        /// Loads a short language display name in a formatting locale using compiled data.
        ///
        /// Falls back to default (medium) length if a short name is not available.
        ///
        /// The `light` constructor links data for all common languages.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::{
        ///     DisplayNamesPreferences, LanguageIdentifierDisplayName,
        ///     LanguageIdentifierDisplayNameOptions, LanguageIdentifierNameFallbackError,
        /// };
        /// use icu::locale::{langid, locale};
        /// use writeable::assert_try_writeable_eq;
        ///
        /// let prefs = DisplayNamesPreferences::from(locale!("en"));
        /// let options = LanguageIdentifierDisplayNameOptions::default();
        ///
        /// // Short length format uses shorter subtag/qualifier names when available (e.g. "US English"):
        /// let display_name = LanguageIdentifierDisplayName::try_new_short_light(
        ///     prefs,
        ///     langid!("en-US"),
        ///     options,
        /// )
        /// .expect("Data should load successfully");
        /// assert_try_writeable_eq!(display_name.as_borrowed(), "US English", Ok(()));
        ///
        /// // Light data does not include rare languages like Klingon ("tlh"), returning a fallback error:
        /// let tlh = LanguageIdentifierDisplayName::try_new_short_light(prefs, langid!("tlh"), options)
        ///     .expect("Data should load successfully");
        /// assert_try_writeable_eq!(tlh.as_borrowed(), "tlh", Err(LanguageIdentifierNameFallbackError));
        /// ```
        functions: [
            try_new_short_light,
            try_new_short_light_with_buffer_provider,
            try_new_short_light_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_short_light)]
    pub fn try_new_short_light_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        mut subject: LanguageIdentifier,
        options: LanguageIdentifierDisplayNameOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesLanguageShortLightV1>
            + DataProvider<LocaleNamesLanguageMediumLightV1>
            + DataProvider<LocaleNamesLanguageMediumTinyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionShortLightV1>
            + DataProvider<LocaleNamesRegionShortTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        let locale = make_locale(prefs);
        let load_dialect = options.should_load_dialect();
        let mut language_payload = None;
        if load_dialect {
            language_payload = try_load_dialect_name!(
                provider,
                &locale,
                subject,
                [
                    LocaleNamesLanguageShortLightV1,
                    LocaleNamesLanguageMediumLightV1,
                    LocaleNamesLanguageMediumTinyV1
                ]
            )?;
        }
        if language_payload.is_none() {
            let attrs = make_attributes_for_subtag(&subject.language);
            language_payload =
                load_one::<LocaleNamesLanguageShortLightV1, _, _>(provider, &locale, attrs)?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumLightV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumTinyV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map(|payload: DataPayload<StringMarker>| {
                        payload.map_project(|p, _| MenuNamePartsOrString::String(p))
                    });
        }
        let language_payload = match language_payload {
            Some(payload) => DataPayloadOr::from_payload(payload),
            None => DataPayloadOr::from_other(subject.language),
        };

        let qualifiers = QualifiersOwned::try_new_short_light_unstable(provider, prefs, subject)?;
        Ok(Self {
            language_payload,
            qualifiers,
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, subject: LanguageIdentifier, options: LanguageIdentifierDisplayNameOptions) -> result: Result<Self, DataError>,
        /// Loads a long language display name in a formatting locale using compiled data.
        ///
        /// Falls back to default (medium) length if a long name is not available.
        ///
        /// The `light` constructor links data for all common languages.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::{
        ///     DisplayNamesPreferences, LanguageIdentifierDisplayName,
        ///     LanguageIdentifierDisplayNameOptions, LanguageIdentifierNameFallbackError,
        /// };
        /// use icu::locale::{langid, locale};
        /// use writeable::assert_try_writeable_eq;
        ///
        /// let prefs = DisplayNamesPreferences::from(locale!("en"));
        /// let options = LanguageIdentifierDisplayNameOptions::default();
        ///
        /// // Long length format uses longer subtag names when available (e.g. "Mandarin Chinese"):
        /// let display_name = LanguageIdentifierDisplayName::try_new_long_light(
        ///     prefs,
        ///     langid!("zh"),
        ///     options,
        /// )
        /// .expect("Data should load successfully");
        /// assert_try_writeable_eq!(display_name.as_borrowed(), "Mandarin Chinese", Ok(()));
        ///
        /// // Light data does not include rare languages like Klingon ("tlh"), returning a fallback error:
        /// let tlh = LanguageIdentifierDisplayName::try_new_long_light(prefs, langid!("tlh"), options)
        ///     .expect("Data should load successfully");
        /// assert_try_writeable_eq!(tlh.as_borrowed(), "tlh", Err(LanguageIdentifierNameFallbackError));
        /// ```
        functions: [
            try_new_long_light,
            try_new_long_light_with_buffer_provider,
            try_new_long_light_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_long_light)]
    pub fn try_new_long_light_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        mut subject: LanguageIdentifier,
        options: LanguageIdentifierDisplayNameOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesLanguageLongLightV1>
            + DataProvider<LocaleNamesLanguageMediumLightV1>
            + DataProvider<LocaleNamesLanguageMediumTinyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        let locale = make_locale(prefs);
        let load_dialect = options.should_load_dialect();
        let mut language_payload = None;
        if load_dialect {
            language_payload = try_load_dialect_name!(
                provider,
                &locale,
                subject,
                [
                    LocaleNamesLanguageLongLightV1,
                    LocaleNamesLanguageMediumLightV1,
                    LocaleNamesLanguageMediumTinyV1
                ]
            )?;
        }
        if language_payload.is_none() {
            let attrs = make_attributes_for_subtag(&subject.language);
            language_payload =
                load_one::<LocaleNamesLanguageLongLightV1, _, _>(provider, &locale, attrs)?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumLightV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumTinyV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map(|payload: DataPayload<StringMarker>| {
                        payload.map_project(|p, _| MenuNamePartsOrString::String(p))
                    });
        }
        let language_payload = match language_payload {
            Some(payload) => DataPayloadOr::from_payload(payload),
            None => DataPayloadOr::from_other(subject.language),
        };

        let qualifiers = QualifiersOwned::try_new_light_unstable(provider, prefs, subject)?;
        Ok(Self {
            language_payload,
            qualifiers,
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, subject: LanguageIdentifier, options: LanguageIdentifierDisplayNameOptions) -> result: Result<Self, DataError>,
        /// Loads a menu-style language display name in a formatting locale using compiled data.
        ///
        /// The `light` constructor links data for all common languages.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::{
        ///     DisplayNamesPreferences, LanguageIdentifierDisplayName,
        ///     LanguageIdentifierDisplayNameOptions, LanguageIdentifierNameFallbackError,
        /// };
        /// use icu::locale::{langid, locale};
        /// use writeable::assert_try_writeable_eq;
        ///
        /// let prefs = DisplayNamesPreferences::from(locale!("en"));
        /// let options = LanguageIdentifierDisplayNameOptions::default();
        ///
        /// // Menu-style format places primary language name first for dropdown sorting (e.g. "Chinese, Mandarin"):
        /// let display_name = LanguageIdentifierDisplayName::try_new_menu_light(
        ///     prefs,
        ///     langid!("zh"),
        ///     options,
        /// )
        /// .expect("Data should load successfully");
        /// assert_try_writeable_eq!(display_name.as_borrowed(), "Chinese, Mandarin", Ok(()));
        ///
        /// // Light data does not include rare languages like Klingon ("tlh"), returning a fallback error:
        /// let tlh = LanguageIdentifierDisplayName::try_new_menu_light(prefs, langid!("tlh"), options)
        ///     .expect("Data should load successfully");
        /// assert_try_writeable_eq!(tlh.as_borrowed(), "tlh", Err(LanguageIdentifierNameFallbackError));
        /// ```
        functions: [
            try_new_menu_light,
            try_new_menu_light_with_buffer_provider,
            try_new_menu_light_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_menu_light)]
    pub fn try_new_menu_light_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        subject: LanguageIdentifier,
        _options: LanguageIdentifierDisplayNameOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesLanguageMenuMediumLightV1>
            + DataProvider<LocaleNamesLanguageMediumLightV1>
            + DataProvider<LocaleNamesLanguageMediumTinyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        let locale = make_locale(prefs);
        let attrs = make_attributes_for_subtag(&subject.language);
        let mut language_payload = load_one::<LocaleNamesLanguageMenuMediumLightV1, _, _>(
            provider, &locale, attrs,
        )?
        .map(|payload: DataPayload<MenuNamePartsMarker>| {
            payload.map_project(|p, _| MenuNamePartsOrString::MenuNameParts(p))
        });
        if language_payload.is_none() {
            let attrs = make_attributes_for_subtag(&subject.language);
            language_payload =
                load_one::<LocaleNamesLanguageMediumLightV1, _, _>(provider, &locale, attrs)?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumTinyV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map(|payload: DataPayload<StringMarker>| {
                        payload.map_project(|p, _| MenuNamePartsOrString::String(p))
                    });
        }
        let language_payload = match language_payload {
            Some(payload) => DataPayloadOr::from_payload(payload),
            None => DataPayloadOr::from_other(subject.language),
        };

        let qualifiers = QualifiersOwned::try_new_light_unstable(provider, prefs, subject)?;
        Ok(Self {
            language_payload,
            qualifiers,
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, subject: LanguageIdentifier, options: LanguageIdentifierDisplayNameOptions) -> result: Result<Self, DataError>,
        /// Loads a short menu-style language display name in a formatting locale using compiled data.
        ///
        /// Falls back to default (medium) length if a short name is not available.
        ///
        /// The `light` constructor links data for all common languages.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::{
        ///     DisplayNamesPreferences, LanguageIdentifierDisplayName,
        ///     LanguageIdentifierDisplayNameOptions, LanguageIdentifierNameFallbackError,
        /// };
        /// use icu::locale::{langid, locale};
        /// use writeable::assert_try_writeable_eq;
        ///
        /// let prefs = DisplayNamesPreferences::from(locale!("en"));
        /// let options = LanguageIdentifierDisplayNameOptions::default();
        ///
        /// // Short menu format uses abbreviated region name in menu layout (e.g. "English (US)"):
        /// let display_name = LanguageIdentifierDisplayName::try_new_short_menu_light(
        ///     prefs,
        ///     langid!("en-US"),
        ///     options,
        /// )
        /// .expect("Data should load successfully");
        /// assert_try_writeable_eq!(display_name.as_borrowed(), "English (US)", Ok(()));
        ///
        /// // Light data does not include rare languages like Klingon ("tlh"), returning a fallback error:
        /// let tlh = LanguageIdentifierDisplayName::try_new_short_menu_light(prefs, langid!("tlh"), options)
        ///     .expect("Data should load successfully");
        /// assert_try_writeable_eq!(tlh.as_borrowed(), "tlh", Err(LanguageIdentifierNameFallbackError));
        /// ```
        functions: [
            try_new_short_menu_light,
            try_new_short_menu_light_with_buffer_provider,
            try_new_short_menu_light_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_short_menu_light)]
    pub fn try_new_short_menu_light_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        subject: LanguageIdentifier,
        _options: LanguageIdentifierDisplayNameOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesLanguageMenuMediumLightV1>
            + DataProvider<LocaleNamesLanguageShortLightV1>
            + DataProvider<LocaleNamesLanguageMediumLightV1>
            + DataProvider<LocaleNamesLanguageMediumTinyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionShortLightV1>
            + DataProvider<LocaleNamesRegionShortTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        let locale = make_locale(prefs);
        let attrs = make_attributes_for_subtag(&subject.language);
        let mut language_payload = load_one::<LocaleNamesLanguageMenuMediumLightV1, _, _>(
            provider, &locale, attrs,
        )?
        .map(|payload: DataPayload<MenuNamePartsMarker>| {
            payload.map_project(|p, _| MenuNamePartsOrString::MenuNameParts(p))
        });
        if language_payload.is_none() {
            let attrs = make_attributes_for_subtag(&subject.language);
            language_payload =
                load_one::<LocaleNamesLanguageShortLightV1, _, _>(provider, &locale, attrs)?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumLightV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumTinyV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map(|payload: DataPayload<StringMarker>| {
                        payload.map_project(|p, _| MenuNamePartsOrString::String(p))
                    });
        }
        let language_payload = match language_payload {
            Some(payload) => DataPayloadOr::from_payload(payload),
            None => DataPayloadOr::from_other(subject.language),
        };

        let qualifiers = QualifiersOwned::try_new_short_light_unstable(provider, prefs, subject)?;
        Ok(Self {
            language_payload,
            qualifiers,
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, subject: LanguageIdentifier, options: LanguageIdentifierDisplayNameOptions) -> result: Result<Self, DataError>,
        /// Loads a language display name in a formatting locale using compiled data.
        ///
        /// The `heavy` constructor includes additional data coverage for subtags that are
        /// less commonly formatted in the target locale.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::{
        ///     DisplayNamesPreferences, LanguageIdentifierDisplayName, LanguageIdentifierDisplayNameOptions,
        /// };
        /// use icu::locale::{langid, locale};
        /// use writeable::assert_try_writeable_eq;
        ///
        /// let prefs = DisplayNamesPreferences::from(locale!("nl"));
        /// let options = LanguageIdentifierDisplayNameOptions::default();
        ///
        /// // Heavy dataset includes data coverage for rare languages like Klingon ("tlh"):
        /// let display_name = LanguageIdentifierDisplayName::try_new_heavy(
        ///     prefs,
        ///     langid!("tlh"),
        ///     options,
        /// )
        /// .expect("Data should load successfully");
        /// assert_try_writeable_eq!(display_name.as_borrowed(), "Klingon", Ok(()));
        /// ```
        functions: [
            try_new_heavy,
            try_new_heavy_with_buffer_provider,
            try_new_heavy_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_heavy)]
    pub fn try_new_heavy_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        mut subject: LanguageIdentifier,
        options: LanguageIdentifierDisplayNameOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesLanguageMediumHeavyV1>
            + DataProvider<LocaleNamesLanguageMediumLightV1>
            + DataProvider<LocaleNamesLanguageMediumTinyV1>
            + DataProvider<LocaleNamesScriptMediumHeavyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        let locale = make_locale(prefs);
        // Step 1: Load language name
        let load_dialect = options.should_load_dialect();
        let mut language_payload = None;
        // Only try dialect if requested (or default)
        if load_dialect {
            language_payload = try_load_dialect_name!(
                provider,
                &locale,
                subject,
                [
                    LocaleNamesLanguageMediumHeavyV1,
                    LocaleNamesLanguageMediumLightV1,
                    LocaleNamesLanguageMediumTinyV1
                ]
            )?;
        }
        if language_payload.is_none() {
            let attrs = make_attributes_for_subtag(&subject.language);
            language_payload =
                load_one::<LocaleNamesLanguageMediumHeavyV1, _, _>(provider, &locale, attrs)?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumLightV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumTinyV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map(|payload: DataPayload<StringMarker>| {
                        payload.map_project(|p, _| MenuNamePartsOrString::String(p))
                    });
        }
        let language_payload = match language_payload {
            Some(payload) => DataPayloadOr::from_payload(payload),
            None => DataPayloadOr::from_other(subject.language),
        };

        // Load the remaining data
        let qualifiers = QualifiersOwned::try_new_heavy_unstable(provider, prefs, subject)?;
        Ok(Self {
            language_payload,
            qualifiers,
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, subject: LanguageIdentifier, options: LanguageIdentifierDisplayNameOptions) -> result: Result<Self, DataError>,
        /// Loads a short language display name in a formatting locale using compiled data.
        ///
        /// Falls back to default (medium) length if a short name is not available.
        ///
        /// The `heavy` constructor includes additional data coverage for subtags that are
        /// less commonly formatted in the target locale.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::{
        ///     DisplayNamesPreferences, LanguageIdentifierDisplayName, LanguageIdentifierDisplayNameOptions,
        /// };
        /// use icu::locale::{langid, locale};
        /// use writeable::assert_try_writeable_eq;
        ///
        /// let prefs = DisplayNamesPreferences::from(locale!("nl"));
        /// let options = LanguageIdentifierDisplayNameOptions::default();
        ///
        /// // Short heavy formats short names with full data coverage for rare subtags:
        /// let display_name = LanguageIdentifierDisplayName::try_new_short_heavy(
        ///     prefs,
        ///     langid!("tlh-001"),
        ///     options,
        /// )
        /// .expect("Data should load successfully");
        /// assert_try_writeable_eq!(display_name.as_borrowed(), "Klingon (wereld)", Ok(()));
        /// ```
        functions: [
            try_new_short_heavy,
            try_new_short_heavy_with_buffer_provider,
            try_new_short_heavy_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_short_heavy)]
    pub fn try_new_short_heavy_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        mut subject: LanguageIdentifier,
        options: LanguageIdentifierDisplayNameOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesLanguageShortHeavyV1>
            + DataProvider<LocaleNamesLanguageShortLightV1>
            + DataProvider<LocaleNamesLanguageMediumHeavyV1>
            + DataProvider<LocaleNamesLanguageMediumLightV1>
            + DataProvider<LocaleNamesLanguageMediumTinyV1>
            + DataProvider<LocaleNamesScriptShortHeavyV1>
            + DataProvider<LocaleNamesScriptMediumHeavyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionShortLightV1>
            + DataProvider<LocaleNamesRegionShortTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        let locale = make_locale(prefs);
        // Step 1: Load language name
        let load_dialect = options.should_load_dialect();
        let mut language_payload = None;
        // Only try dialect if requested (or default)
        if load_dialect {
            language_payload = try_load_dialect_name!(
                provider,
                &locale,
                subject,
                [
                    LocaleNamesLanguageShortHeavyV1,
                    LocaleNamesLanguageShortLightV1,
                    LocaleNamesLanguageMediumHeavyV1,
                    LocaleNamesLanguageMediumLightV1,
                    LocaleNamesLanguageMediumTinyV1
                ]
            )?;
        }
        if language_payload.is_none() {
            let attrs = make_attributes_for_subtag(&subject.language);
            language_payload =
                load_one::<LocaleNamesLanguageShortHeavyV1, _, _>(provider, &locale, attrs)?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageShortLightV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumHeavyV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumLightV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumTinyV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map(|payload: DataPayload<StringMarker>| {
                        payload.map_project(|p, _| MenuNamePartsOrString::String(p))
                    });
        }
        let language_payload = match language_payload {
            Some(payload) => DataPayloadOr::from_payload(payload),
            None => DataPayloadOr::from_other(subject.language),
        };

        // Load the remaining data
        let qualifiers = QualifiersOwned::try_new_short_heavy_unstable(provider, prefs, subject)?;
        Ok(Self {
            language_payload,
            qualifiers,
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, subject: LanguageIdentifier, options: LanguageIdentifierDisplayNameOptions) -> result: Result<Self, DataError>,
        /// Loads a long language display name in a formatting locale using compiled data.
        ///
        /// Falls back to default (medium) length if a long name is not available.
        ///
        /// The `heavy` constructor includes additional data coverage for subtags that are
        /// less commonly formatted in the target locale.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::{
        ///     DisplayNamesPreferences, LanguageIdentifierDisplayName, LanguageIdentifierDisplayNameOptions,
        /// };
        /// use icu::locale::{langid, locale};
        /// use writeable::assert_try_writeable_eq;
        ///
        /// let prefs = DisplayNamesPreferences::from(locale!("nl"));
        /// let options = LanguageIdentifierDisplayNameOptions::default();
        ///
        /// // Long heavy formats long names with full data coverage for rare subtags:
        /// let display_name = LanguageIdentifierDisplayName::try_new_long_heavy(
        ///     prefs,
        ///     langid!("tlh-001"),
        ///     options,
        /// )
        /// .expect("Data should load successfully");
        /// assert_try_writeable_eq!(display_name.as_borrowed(), "Klingon (wereld)", Ok(()));
        /// ```
        functions: [
            try_new_long_heavy,
            try_new_long_heavy_with_buffer_provider,
            try_new_long_heavy_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_long_heavy)]
    pub fn try_new_long_heavy_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        mut subject: LanguageIdentifier,
        options: LanguageIdentifierDisplayNameOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesLanguageLongHeavyV1>
            + DataProvider<LocaleNamesLanguageLongLightV1>
            + DataProvider<LocaleNamesLanguageMediumHeavyV1>
            + DataProvider<LocaleNamesLanguageMediumLightV1>
            + DataProvider<LocaleNamesLanguageMediumTinyV1>
            + DataProvider<LocaleNamesScriptMediumHeavyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        let locale = make_locale(prefs);
        let load_dialect = options.should_load_dialect();
        let mut language_payload = None;
        if load_dialect {
            language_payload = try_load_dialect_name!(
                provider,
                &locale,
                subject,
                [
                    LocaleNamesLanguageLongHeavyV1,
                    LocaleNamesLanguageLongLightV1,
                    LocaleNamesLanguageMediumHeavyV1,
                    LocaleNamesLanguageMediumLightV1,
                    LocaleNamesLanguageMediumTinyV1
                ]
            )?;
        }
        if language_payload.is_none() {
            let attrs = make_attributes_for_subtag(&subject.language);
            language_payload =
                load_one::<LocaleNamesLanguageLongHeavyV1, _, _>(provider, &locale, attrs)?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageLongLightV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumHeavyV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumLightV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumTinyV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map(|payload: DataPayload<StringMarker>| {
                        payload.map_project(|p, _| MenuNamePartsOrString::String(p))
                    });
        }
        let language_payload = match language_payload {
            Some(payload) => DataPayloadOr::from_payload(payload),
            None => DataPayloadOr::from_other(subject.language),
        };

        let qualifiers = QualifiersOwned::try_new_heavy_unstable(provider, prefs, subject)?;
        Ok(Self {
            language_payload,
            qualifiers,
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, subject: LanguageIdentifier, options: LanguageIdentifierDisplayNameOptions) -> result: Result<Self, DataError>,
        /// Loads a menu-style language display name in a formatting locale using compiled data.
        ///
        /// The `heavy` constructor includes additional data coverage for subtags that are
        /// less commonly formatted in the target locale.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::{
        ///     DisplayNamesPreferences, LanguageIdentifierDisplayName, LanguageIdentifierDisplayNameOptions,
        /// };
        /// use icu::locale::{langid, locale};
        /// use writeable::assert_try_writeable_eq;
        ///
        /// let prefs = DisplayNamesPreferences::from(locale!("nl"));
        /// let options = LanguageIdentifierDisplayNameOptions::default();
        ///
        /// // Menu heavy formats menu-style names with full data coverage for rare subtags:
        /// let display_name = LanguageIdentifierDisplayName::try_new_menu_heavy(
        ///     prefs,
        ///     langid!("tlh-001"),
        ///     options,
        /// )
        /// .expect("Data should load successfully");
        /// assert_try_writeable_eq!(display_name.as_borrowed(), "Klingon (wereld)", Ok(()));
        /// ```
        functions: [
            try_new_menu_heavy,
            try_new_menu_heavy_with_buffer_provider,
            try_new_menu_heavy_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_menu_heavy)]
    pub fn try_new_menu_heavy_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        subject: LanguageIdentifier,
        _options: LanguageIdentifierDisplayNameOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesLanguageMenuMediumHeavyV1>
            + DataProvider<LocaleNamesLanguageMenuMediumLightV1>
            + DataProvider<LocaleNamesLanguageMediumHeavyV1>
            + DataProvider<LocaleNamesLanguageMediumLightV1>
            + DataProvider<LocaleNamesLanguageMediumTinyV1>
            + DataProvider<LocaleNamesScriptMediumHeavyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        let locale = make_locale(prefs);
        let attrs = make_attributes_for_subtag(&subject.language);
        let mut language_payload =
            load_one::<LocaleNamesLanguageMenuMediumHeavyV1, _, _>(provider, &locale, attrs)?
                .map_or_else(
                    || {
                        load_one::<LocaleNamesLanguageMenuMediumLightV1, _, _>(
                            provider, &locale, attrs,
                        )
                    },
                    |p| Ok(Some(p)),
                )?
                .map(|payload: DataPayload<MenuNamePartsMarker>| {
                    payload.map_project(|p, _| MenuNamePartsOrString::MenuNameParts(p))
                });
        if language_payload.is_none() {
            let attrs = make_attributes_for_subtag(&subject.language);
            language_payload =
                load_one::<LocaleNamesLanguageMediumHeavyV1, _, _>(provider, &locale, attrs)?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumLightV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumTinyV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map(|payload: DataPayload<StringMarker>| {
                        payload.map_project(|p, _| MenuNamePartsOrString::String(p))
                    });
        }
        let language_payload = match language_payload {
            Some(payload) => DataPayloadOr::from_payload(payload),
            None => DataPayloadOr::from_other(subject.language),
        };

        let qualifiers = QualifiersOwned::try_new_heavy_unstable(provider, prefs, subject)?;
        Ok(Self {
            language_payload,
            qualifiers,
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, subject: LanguageIdentifier, options: LanguageIdentifierDisplayNameOptions) -> result: Result<Self, DataError>,
        /// Loads a short menu-style language display name in a formatting locale using compiled data.
        ///
        /// Falls back to default (medium) length if a short name is not available.
        ///
        /// The `heavy` constructor includes additional data coverage for subtags that are
        /// less commonly formatted in the target locale.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::{
        ///     DisplayNamesPreferences, LanguageIdentifierDisplayName, LanguageIdentifierDisplayNameOptions,
        /// };
        /// use icu::locale::{langid, locale};
        /// use writeable::assert_try_writeable_eq;
        ///
        /// let prefs = DisplayNamesPreferences::from(locale!("nl"));
        /// let options = LanguageIdentifierDisplayNameOptions::default();
        ///
        /// // Short menu heavy formats short menu-style names with full data coverage for rare subtags:
        /// let display_name = LanguageIdentifierDisplayName::try_new_short_menu_heavy(
        ///     prefs,
        ///     langid!("tlh-001"),
        ///     options,
        /// )
        /// .expect("Data should load successfully");
        /// assert_try_writeable_eq!(display_name.as_borrowed(), "Klingon (wereld)", Ok(()));
        /// ```
        functions: [
            try_new_short_menu_heavy,
            try_new_short_menu_heavy_with_buffer_provider,
            try_new_short_menu_heavy_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_short_menu_heavy)]
    pub fn try_new_short_menu_heavy_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        subject: LanguageIdentifier,
        _options: LanguageIdentifierDisplayNameOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesLanguageMenuMediumHeavyV1>
            + DataProvider<LocaleNamesLanguageMenuMediumLightV1>
            + DataProvider<LocaleNamesLanguageShortHeavyV1>
            + DataProvider<LocaleNamesLanguageShortLightV1>
            + DataProvider<LocaleNamesLanguageMediumHeavyV1>
            + DataProvider<LocaleNamesLanguageMediumLightV1>
            + DataProvider<LocaleNamesLanguageMediumTinyV1>
            + DataProvider<LocaleNamesScriptShortHeavyV1>
            + DataProvider<LocaleNamesScriptMediumHeavyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionShortLightV1>
            + DataProvider<LocaleNamesRegionShortTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        let locale = make_locale(prefs);
        let attrs = make_attributes_for_subtag(&subject.language);
        let mut language_payload =
            load_one::<LocaleNamesLanguageMenuMediumHeavyV1, _, _>(provider, &locale, attrs)?
                .map_or_else(
                    || {
                        load_one::<LocaleNamesLanguageMenuMediumLightV1, _, _>(
                            provider, &locale, attrs,
                        )
                    },
                    |p| Ok(Some(p)),
                )?
                .map(|payload: DataPayload<MenuNamePartsMarker>| {
                    payload.map_project(|p, _| MenuNamePartsOrString::MenuNameParts(p))
                });
        if language_payload.is_none() {
            let attrs = make_attributes_for_subtag(&subject.language);
            language_payload =
                load_one::<LocaleNamesLanguageShortHeavyV1, _, _>(provider, &locale, attrs)?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageShortLightV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumHeavyV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumLightV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map_or_else(
                        || {
                            load_one::<LocaleNamesLanguageMediumTinyV1, _, _>(
                                provider, &locale, attrs,
                            )
                        },
                        |p| Ok(Some(p)),
                    )?
                    .map(|payload: DataPayload<StringMarker>| {
                        payload.map_project(|p, _| MenuNamePartsOrString::String(p))
                    });
        }
        let language_payload = match language_payload {
            Some(payload) => DataPayloadOr::from_payload(payload),
            None => DataPayloadOr::from_other(subject.language),
        };

        let qualifiers = QualifiersOwned::try_new_short_heavy_unstable(provider, prefs, subject)?;
        Ok(Self {
            language_payload,
            qualifiers,
        })
    }
}

impl QualifiersOwned {
    fn try_new_internal_unstable<D, FS, FR, FV>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        subject: LanguageIdentifier,
        load_script: FS,
        load_region: FR,
        load_variant: FV,
    ) -> Result<Self, DataError>
    where
        D: ?Sized + DataProvider<LocaleNamesEssentialsV1>,
        FS: Fn(&D, DisplayNamesPreferences, Script) -> Result<ScriptDisplayName, DataError>,
        FR: Fn(&D, DisplayNamesPreferences, Region) -> Result<RegionDisplayName, DataError>,
        FV: Fn(&D, DisplayNamesPreferences, Variant) -> Result<VariantDisplayName, DataError>,
    {
        // Step 2: Load script name (if present in subject)
        let script_payload = if let Some(script) = subject.script {
            match load_script(provider, prefs, script).allow_identifier_not_found()? {
                Some(obj) => obj.payload.cast().map_other(Some),
                None => DataPayloadOr::from_other(Some(script)),
            }
        } else {
            DataPayloadOr::from_other(None)
        };

        // Step 3: Load region name (if present in subject)
        let region_payload = if let Some(region) = subject.region {
            match load_region(provider, prefs, region).allow_identifier_not_found()? {
                Some(obj) => obj.payload.cast().map_other(Some),
                None => DataPayloadOr::from_other(Some(region)),
            }
        } else {
            DataPayloadOr::from_other(None)
        };

        // Step 4: Load variant names (if present in subject)
        let load_variant_helper = |variant: Variant| -> Result<
            DataPayloadOr<ErasedMarker<VarZeroCow<'static, str>>, Variant>,
            DataError,
        > {
            match load_variant(provider, prefs, variant).allow_identifier_not_found()? {
                Some(obj) => Ok(obj.payload.cast()),
                None => Ok(DataPayloadOr::from_other(variant)),
            }
        };

        let mut variant_results = subject
            .variants
            .iter()
            .map(|variant| load_variant_helper(*variant));

        let variant_payloads = if let Some(first) = variant_results.next() {
            if let Some(second) = variant_results.next() {
                // 2 or more variants
                let payload_vec = [first, second]
                    .into_iter()
                    .chain(variant_results)
                    .collect::<Result<Vec<_>, _>>()?;
                DataPayloadOr::from_other(Ok(payload_vec))
            } else {
                // 1 variant
                match first?.into_inner() {
                    Ok(payload) => DataPayloadOr::from_payload(payload),
                    Err(fallback_code) => DataPayloadOr::from_other(Err(fallback_code)),
                }
            }
        } else {
            // 0 variants
            DataPayloadOr::from_other(Ok(Vec::new()))
        };

        // Step 5: Load essentials
        let essentials_payload = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&LocaleNamesEssentialsV1::make_locale(
                    prefs.locale_preferences,
                )),
                ..Default::default()
            })?
            .payload;

        Ok(Self {
            script_payload,
            region_payload,
            variant_payloads,
            essentials_payload,
        })
    }

    fn try_new_light_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        subject: LanguageIdentifier,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        Self::try_new_internal_unstable(
            provider,
            prefs,
            subject,
            ScriptDisplayName::try_new_light_unstable,
            RegionDisplayName::try_new_light_unstable,
            VariantDisplayName::try_new_heavy_unstable,
        )
    }

    fn try_new_tiny_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        subject: LanguageIdentifier,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        Self::try_new_internal_unstable(
            provider,
            prefs,
            subject,
            ScriptDisplayName::try_new_tiny_unstable,
            RegionDisplayName::try_new_tiny_unstable,
            VariantDisplayName::try_new_heavy_unstable,
        )
    }

    fn try_new_short_light_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        subject: LanguageIdentifier,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionShortLightV1>
            + DataProvider<LocaleNamesRegionShortTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        Self::try_new_internal_unstable(
            provider,
            prefs,
            subject,
            ScriptDisplayName::try_new_light_unstable,
            RegionDisplayName::try_new_short_light_unstable,
            VariantDisplayName::try_new_heavy_unstable,
        )
    }

    fn try_new_heavy_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        subject: LanguageIdentifier,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesScriptMediumHeavyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        Self::try_new_internal_unstable(
            provider,
            prefs,
            subject,
            ScriptDisplayName::try_new_heavy_unstable,
            RegionDisplayName::try_new_light_unstable,
            VariantDisplayName::try_new_heavy_unstable,
        )
    }

    fn try_new_short_heavy_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        subject: LanguageIdentifier,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesScriptShortHeavyV1>
            + DataProvider<LocaleNamesScriptMediumHeavyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>
            + DataProvider<LocaleNamesRegionShortLightV1>
            + DataProvider<LocaleNamesRegionShortTinyV1>
            + DataProvider<LocaleNamesRegionMediumLightV1>
            + DataProvider<LocaleNamesRegionMediumTinyV1>
            + DataProvider<LocaleNamesVariantMediumHeavyV1>
            + DataProvider<LocaleNamesEssentialsV1>,
    {
        Self::try_new_internal_unstable(
            provider,
            prefs,
            subject,
            ScriptDisplayName::try_new_short_heavy_unstable,
            RegionDisplayName::try_new_short_light_unstable,
            VariantDisplayName::try_new_heavy_unstable,
        )
    }
}

impl LanguageIdentifierDisplayName {
    /// Returns a borrowed version of this display name
    /// suitable for writing out to a string.
    pub fn as_borrowed(&self) -> LanguageIdentifierDisplayNameBorrowed<'_> {
        let mut qualifiers = self.qualifiers.as_borrowed();
        let base_name = match self.language_payload.get() {
            Ok(MenuNamePartsOrString::String(string)) => NameOrFallback(Ok(string.as_ref())),
            Ok(MenuNamePartsOrString::MenuNameParts(parts)) => {
                if !parts.extension().is_empty() {
                    qualifiers.menu_extension = Some(parts.extension());
                }
                NameOrFallback(Ok(parts.core()))
            }
            Err(lang) => NameOrFallback(Err(lang.as_str())),
        };

        LanguageIdentifierDisplayNameBorrowed(LossyWrap(LanguageIdentifierDisplayNameInner {
            base_name,
            qualifiers,
        }))
    }
}

impl QualifiersOwned {
    pub fn as_borrowed(&self) -> QualifiersBorrowed<'_> {
        let script = match self.script_payload.get() {
            Ok(p) => Some(NameOrFallback(Ok(p.as_ref()))),
            Err(Some(script)) => Some(NameOrFallback(Err(script.as_str()))),
            Err(None) => None,
        };

        let region = match self.region_payload.get() {
            Ok(p) => Some(NameOrFallback(Ok(p.as_ref()))),
            Err(Some(region)) => Some(NameOrFallback(Err(region.as_str()))),
            Err(None) => None,
        };

        let variants = match self.variant_payloads.get() {
            Ok(variant_name) => BorrowedVariants::One(NameOrFallback(Ok(variant_name))),
            Err(Ok(vec)) => BorrowedVariants::Slice(vec.as_slice()),
            Err(Err(variant)) => BorrowedVariants::One(NameOrFallback(Err(variant.as_str()))),
        };

        QualifiersBorrowed {
            menu_extension: None,
            script,
            region,
            variants,
            glue: &self.essentials_payload.get().locale_pattern,
            separator: &self.essentials_payload.get().locale_separator,
        }
    }
}

/// Borrowed variants representation to avoid heap allocation.
///
/// Note: if a compiled-data-only constructor is added in the future,
/// this will need a new variant for a vec of borrowed variant names.
#[derive(Debug, Clone, Copy)]
enum BorrowedVariants<'a> {
    One(NameOrFallback<'a>),
    Slice(&'a [DataPayloadOr<ErasedMarker<VarZeroCow<'static, str>>, Variant>]),
}

impl BorrowedVariants<'_> {
    #[inline]
    fn is_empty(&self) -> bool {
        matches!(self, Self::Slice([]))
    }
}

/// A localized display name for a language identifier.
///
/// See [`LanguageIdentifierDisplayName`].
#[derive(Debug, Clone, Copy)]
pub struct LanguageIdentifierDisplayNameBorrowed<'a>(
    pub(crate) LossyWrap<LanguageIdentifierDisplayNameInner<'a>>,
);

/// A struct implementing [`TryWriteable`] that returns a [`LanguageIdentifierNameFallbackError`]
#[derive(Debug, Clone, Copy)]
struct NameOrFallback<'a>(Result<&'a str, &'a str>);

writeable::impl_try_writeable_delegate!(
    NameOrFallback<'a>,
    |&self| &self.0,
    Error = LanguageIdentifierNameFallbackError,
    |_fallback_str| LanguageIdentifierNameFallbackError,
    where 'a
);

#[derive(Debug, Clone, Copy)]
pub(crate) struct LanguageIdentifierDisplayNameInner<'a> {
    base_name: NameOrFallback<'a>,
    qualifiers: QualifiersBorrowed<'a>,
}

writeable::impl_try_writeable_delegate!(
    LanguageIdentifierDisplayNameBorrowed<'_>,
    |&self| &self.0.0,
    Error = LanguageIdentifierNameFallbackError
);

writeable::impl_writeable_delegate!(LanguageIdentifierDisplayNameBorrowed<'_>, |&self| &self.0);

writeable::impl_display_with_writeable!(LanguageIdentifierDisplayNameBorrowed<'_>);

#[derive(Debug, Copy, Clone)]
struct QualifiersBorrowed<'a> {
    menu_extension: Option<&'a str>,
    script: Option<NameOrFallback<'a>>,
    region: Option<NameOrFallback<'a>>,
    variants: BorrowedVariants<'a>,
    glue: &'a DoublePlaceholderPattern,
    separator: &'a DoublePlaceholderPattern,
}

impl<'a> QualifiersBorrowed<'a> {
    fn separator_str(&self) -> &'a str {
        let mut separator_str = ", ";
        for item in self.separator.iter() {
            if let PatternItem::Literal(s) = item {
                separator_str = s;
                break;
            }
        }
        separator_str
    }

    fn is_empty(&self) -> bool {
        self.menu_extension.is_none()
            && self.script.is_none()
            && self.region.is_none()
            && self.variants.is_empty()
    }
}

impl<'a> TryWriteable for QualifiersBorrowed<'a> {
    type Error = LanguageIdentifierNameFallbackError;

    fn try_write_to_parts<S: PartsWrite + ?Sized>(
        &self,
        sink: &mut S,
    ) -> Result<Result<(), Self::Error>, core::fmt::Error> {
        // TODO: See whether we can share this code with the list component.
        let mut first = true;
        let separator_str = self.separator_str();

        if let Some(menu_extension) = self.menu_extension {
            sink.write_str(menu_extension)?;
            first = false;
        }

        let mut write_item = |sink: &mut S,
                              res: NameOrFallback|
         -> Result<
            Result<(), LanguageIdentifierNameFallbackError>,
            core::fmt::Error,
        > {
            if !first {
                sink.write_str(separator_str)?;
            }
            first = false;
            res.try_write_to_parts(sink)
        };

        let mut result = Ok(());
        if let Some(script) = self.script {
            result = result.and(write_item(sink, script)?);
        }
        if let Some(region) = self.region {
            result = result.and(write_item(sink, region)?);
        }
        match self.variants {
            BorrowedVariants::One(variant) => {
                result = result.and(write_item(sink, variant)?);
            }
            BorrowedVariants::Slice(slice) => {
                for item in slice.iter() {
                    let res = match item.get() {
                        Ok(p) => NameOrFallback(Ok(p.as_ref())),
                        Err(var) => NameOrFallback(Err(var.as_str())),
                    };
                    result = result.and(write_item(sink, res)?);
                }
            }
        }

        Ok(result)
    }

    fn writeable_length_hint(&self) -> LengthHint {
        let mut length_hint = LengthHint::exact(0);
        let mut num_items = 0;
        if let Some(menu_extension) = self.menu_extension {
            length_hint += writeable::Writeable::writeable_length_hint(&menu_extension);
            num_items += 1;
        }
        if let Some(script) = self.script {
            length_hint += script.writeable_length_hint();
            num_items += 1;
        }
        if let Some(region) = self.region {
            length_hint += region.writeable_length_hint();
            num_items += 1;
        }
        match self.variants {
            BorrowedVariants::One(variant) => {
                length_hint += variant.writeable_length_hint();
                num_items += 1;
            }
            BorrowedVariants::Slice(slice) => {
                for item in slice.iter() {
                    length_hint += match item.get() {
                        Ok(p) => writeable::Writeable::writeable_length_hint(&**p),
                        Err(var) => writeable::Writeable::writeable_length_hint(var.as_str()),
                    };
                    num_items += 1;
                }
            }
        }
        length_hint += LengthHint::exact(self.separator_str().len() * (num_items - 1));
        length_hint
    }
}

impl<'a> TryWriteable for LanguageIdentifierDisplayNameInner<'a> {
    type Error = LanguageIdentifierNameFallbackError;

    fn try_write_to_parts<S: PartsWrite + ?Sized>(
        &self,
        sink: &mut S,
    ) -> Result<Result<(), Self::Error>, core::fmt::Error> {
        if self.qualifiers.is_empty() {
            self.base_name.try_write_to_parts(sink)
        } else {
            let result = self
                .qualifiers
                .glue
                .try_interpolate(TryWrap((self.base_name, self.qualifiers)))
                .try_write_to_parts(sink)?;
            Ok(result.map_err(either::Either::into_inner))
        }
    }

    fn writeable_length_hint(&self) -> LengthHint {
        if self.qualifiers.is_empty() {
            self.base_name.writeable_length_hint()
        } else {
            self.qualifiers
                .glue
                .try_interpolate(TryWrap((self.base_name, self.qualifiers)))
                .writeable_length_hint()
        }
    }

    fn try_writeable_borrow(&self) -> Option<Result<&str, (Self::Error, &str)>> {
        if self.qualifiers.is_empty() {
            self.base_name.try_writeable_borrow()
        } else {
            None
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use icu_locale_core::{langid, locale};

    #[test]
    fn test_language_identifier_display_name_owned_table() {
        let prefs_en = DisplayNamesPreferences::from(locale!("en"));
        let options = LanguageIdentifierDisplayNameOptions::default();
        let inputs = [langid!("en-US"), langid!("zh"), langid!("az")];

        macro_rules! check_row {
            ($constructor:ident) => {
                let items = inputs.iter().map(|id| {
                    LanguageIdentifierDisplayName::$constructor(prefs_en, id.clone(), options).map(
                        |display_name| {
                            display_name
                                .as_borrowed()
                                .try_write_to_string()
                                .map(|s| s.into_owned())
                                .map_err(|e| e.0)
                        },
                    )
                });
                assert_eq!(
                    super::super::format_table_row(stringify!($constructor), items),
                    table_row!($constructor)
                );
            };
        }

        check_row!(try_new_tiny);
        check_row!(try_new_light);
        check_row!(try_new_short_light);
        check_row!(try_new_long_light);
        check_row!(try_new_menu_light);
        check_row!(try_new_short_menu_light);
        check_row!(try_new_heavy);
        check_row!(try_new_short_heavy);
        check_row!(try_new_long_heavy);
        check_row!(try_new_menu_heavy);
        check_row!(try_new_short_menu_heavy);
    }
}
