// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Standalone TOML configuration (`config.toml`) for overriding marker filter
//! rules and `resourceFilters` behavior.

use crate::filters::{
    resolve_default_marker_filter, Icu4cFilterCategory, ParsedIcu4cFilterFile, ResolvedLocaleFilter,
};
use crate::{marker_path_str, MarkerEntry};
use anyhow::{bail, Result};
use icu_provider::prelude::*;
use serde::Deserialize;
use std::collections::BTreeSet;

/// Standalone TOML configuration (`config.toml`) for overriding marker filter
/// rules or `resourceFilters` behavior.
#[derive(Debug, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct DatagenTomlConfig {
    #[serde(default)]
    pub(crate) resource_filters: ResourceFiltersConfig,
    #[serde(default)]
    pub(crate) rules: Vec<MarkerFilterRule>,
}

impl DatagenTomlConfig {
    /// Validate all `[[rules]]` entries against the registered ICU4X markers.
    pub(crate) fn validate(&self, all_markers: &[MarkerEntry]) -> Result<()> {
        for rule in &self.rules {
            anyhow::ensure!(!rule.pattern.is_empty(), "TOML rule `pattern` must not be empty");
            let without_trailing_star = rule.pattern.strip_suffix('*').unwrap_or(&rule.pattern);
            anyhow::ensure!(
                !without_trailing_star.contains('*'),
                "TOML rule pattern {:?} only supports `*` at the end of the string",
                rule.pattern
            );

            match (&rule.locales, &rule.icu4c_category) {
                (Some(locs), None) => {
                    ResolvedLocaleFilter::Families(locs.iter().cloned().collect())
                        .to_locale_families()?;
                }
                (None, Some(cat)) => {
                    anyhow::ensure!(
                        *cat != Icu4cFilterCategory::Singleton,
                        "TOML rule for pattern {:?} cannot set `icu4c_category = \"singleton\"` \
                         (singleton markers are detected automatically via `info.is_singleton`)",
                        rule.pattern
                    );
                }
                (Some(_), Some(_)) => {
                    bail!(
                        "TOML rule for pattern {:?} must specify either `locales` or \
                         `icu4c_category`, not both",
                        rule.pattern
                    );
                }
                (None, None) => {
                    bail!(
                        "TOML rule for pattern {:?} must specify either `locales` or \
                         `icu4c_category`",
                        rule.pattern
                    );
                }
            }

            let matched: Vec<&MarkerEntry> = all_markers
                .iter()
                .filter(|m| {
                    let path = marker_path_str(m.info.id);
                    glob_matches(&rule.pattern, &path)
                        || glob_matches(&rule.pattern, m.info.id.name())
                })
                .collect();
            anyhow::ensure!(
                !matched.is_empty(),
                "TOML rule pattern {:?} did not match any registered ICU4X marker",
                rule.pattern
            );
            if matched.iter().all(|m| m.info.is_singleton) {
                bail!(
                    "TOML rule pattern {:?} only matches singleton markers (e.g. `{}`); \
                     singleton markers have no locale dependence and cannot be locale-filtered",
                    rule.pattern,
                    matched[0].name,
                );
            }
        }
        Ok(())
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub(crate) struct ResourceFiltersConfig {
    /// Restrict `TransliteratorRulesV1` to transliterators listed in ICU4C
    /// `resourceFilters` (`translit`).
    pub(crate) filter_transliterators: bool,
    /// Restrict `CurrencyExtendedDataV1` and `CurrencySymbolsV1` to currencies
    /// listed in ICU4C `resourceFilters` (`curr_tree`).
    pub(crate) filter_currencies: bool,
    /// Restrict `LocaleNamesLanguage*V1` to target languages listed in ICU4C
    /// `resourceFilters` (`lang_tree`).
    pub(crate) filter_target_languages: bool,
    /// Restrict `LocaleNamesVariant*V1` to target variants listed in ICU4C
    /// `resourceFilters` (`lang_tree`).
    pub(crate) filter_target_variants: bool,
    /// Abridge non-UI Accept-Languages via `-/*` in `locales_tree` and keep
    /// only autonyms in `LocaleNamesLanguage*V1`.
    pub(crate) abridge_non_ui_locales: bool,
}

impl Default for ResourceFiltersConfig {
    fn default() -> Self {
        Self {
            filter_transliterators: true,
            filter_currencies: true,
            filter_target_languages: true,
            filter_target_variants: true,
            abridge_non_ui_locales: true,
        }
    }
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct MarkerFilterRule {
    /// Glob pattern matching `DataMarkerInfo::id` (e.g. `"datetime/*"`,
    /// `"plurals/*"`). Only trailing `*` wildcards are supported.
    pub(crate) pattern: String,
    /// ICU4C filter category (`"locales_tree"`, `"curr_tree"`, etc.).
    #[serde(default)]
    pub(crate) icu4c_category: Option<Icu4cFilterCategory>,
    /// Optional explicit list of `DataLocaleFamily` strings (overrides ICU4C
    /// filter).
    #[serde(default)]
    pub(crate) locales: Option<Vec<String>>,
}

fn glob_matches(pattern: &str, text: &str) -> bool {
    if pattern == "*" {
        return true;
    }
    if let Some(prefix) = pattern.strip_suffix('*') {
        return text.starts_with(prefix);
    }
    pattern == text
}

pub(crate) fn resolve_marker_filter(
    info: DataMarkerInfo,
    filter_file: &ParsedIcu4cFilterFile,
    toml_cfg: &DatagenTomlConfig,
    abridge_non_ui_locales: bool,
) -> Result<ResolvedLocaleFilter> {
    if info.is_singleton {
        return Ok(ResolvedLocaleFilter::Singleton);
    }
    let marker_path = marker_path_str(info.id);
    for rule in &toml_cfg.rules {
        if glob_matches(&rule.pattern, &marker_path) || glob_matches(&rule.pattern, info.id.name())
        {
            if let Some(ref locs) = rule.locales {
                if locs.is_empty() {
                    return Ok(ResolvedLocaleFilter::Exclude);
                }
                return Ok(ResolvedLocaleFilter::Families(
                    locs.iter().cloned().collect::<BTreeSet<_>>(),
                ));
            }
            if let Some(cat) = rule.icu4c_category {
                let autonym_only =
                    abridge_non_ui_locales && info.attributes_domain == "locale_names_language";
                return filter_file.resolve_category(cat, abridge_non_ui_locales, autonym_only);
            }
        }
    }
    resolve_default_marker_filter(info, filter_file, abridge_non_ui_locales)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{all_registered_markers, chromium_src_root, select_markers_by_args};

    #[test]
    fn test_toml_config_and_marker_resolution() {
        let root = chromium_src_root();
        let all_markers = all_registered_markers();
        let cfg_path = root.join("tools/crates/icu4x_datagen/config.toml");
        let cfg_str = std::fs::read_to_string(&cfg_path).unwrap();
        let toml_cfg: DatagenTomlConfig = toml::from_str(&cfg_str).unwrap();
        toml_cfg.validate(&all_markers).unwrap();
        assert!(toml_cfg.resource_filters.filter_transliterators);
        assert!(toml_cfg.resource_filters.filter_currencies);
        assert!(toml_cfg.resource_filters.abridge_non_ui_locales);

        let common =
            ParsedIcu4cFilterFile::load(&root.join("third_party/icu/filters/common.json")).unwrap();
        let markers = select_markers_by_args(&["chromium".to_string()]).unwrap();
        assert_eq!(markers.len(), 306);

        // `LocaleNamesLanguageMediumTinyV1` has `attributes_domain ==
        // "locale_names_language"`, so `resolve_marker_filter` converts
        // abridged non-UI Accept-Languages to `^locale` (`without_descendants`)
        // when `abridge_non_ui_locales == true`, whereas
        // `LocaleNamesScriptMediumLightV1` is abridged from 164 to the 59 UI
        // language families (54 UI languages + 4 non-default script families +
        // `und`).
        let lang_tiny = markers
            .iter()
            .find(|m| marker_path_str(m.info.id) == "locale/names/language/medium/tiny/v1")
            .unwrap();
        let script_light = markers
            .iter()
            .find(|m| marker_path_str(m.info.id) == "locale/names/script/medium/light/v1")
            .unwrap();

        match resolve_marker_filter(lang_tiny.info, &common, &toml_cfg, true).unwrap() {
            ResolvedLocaleFilter::Families(fams) => {
                assert!(fams.contains("^ak"));
                assert!(!fams.contains("ak"));
                assert!(fams.contains("en"));
            }
            other => panic!("Unexpected filter for lang_tiny: {other:?}"),
        }
        match resolve_marker_filter(lang_tiny.info, &common, &toml_cfg, false).unwrap() {
            ResolvedLocaleFilter::Families(fams) => {
                assert!(fams.contains("ak"));
                assert!(!fams.contains("^ak"));
            }
            other => panic!("Unexpected unabridged filter for lang_tiny: {other:?}"),
        }
        match resolve_marker_filter(script_light.info, &common, &toml_cfg, true).unwrap() {
            ResolvedLocaleFilter::Families(fams) => assert_eq!(fams.len(), 59),
            other => panic!("Unexpected filter for script_light: {other:?}"),
        }
        match resolve_marker_filter(script_light.info, &common, &toml_cfg, false).unwrap() {
            ResolvedLocaleFilter::Families(fams) => assert_eq!(fams.len(), 164),
            other => panic!("Unexpected unabridged filter for script_light: {other:?}"),
        }

        // Verify custom `[[rules]]`: `locales` override, `locales = []`
        // (`Exclude`), `icu4c_category` override, matching by marker name, and
        // first-matching-rule precedence.
        let custom_cfg: DatagenTomlConfig = toml::from_str(
            r#"
            [[rules]]
            pattern = "LocaleNamesScriptMediumLightV1"
            locales = ["und", "en", "fr"]

            [[rules]]
            pattern = "locale/names/script/*"
            locales = []

            [[rules]]
            pattern = "locale/names/language/*"
            icu4c_category = "region_tree"

            [[rules]]
            pattern = "currency/*"
            locales = ["und", "en"]
            "#,
        )
        .unwrap();
        custom_cfg.validate(&all_markers).unwrap();

        // Broad prefix `currency/*` matches both non-singleton markers
        // (`CurrencySymbolsV1`) and singleton markers (`CurrencyFractionsV1`):
        // `validate` succeeds, `CurrencySymbolsV1` gets `{"und", "en"}`, and
        // `CurrencyFractionsV1` remains `Singleton`.
        let curr_symbols = markers.iter().find(|m| m.name == "CurrencySymbolsV1").unwrap();
        let curr_fractions = markers.iter().find(|m| m.name == "CurrencyFractionsV1").unwrap();
        assert_eq!(
            resolve_marker_filter(curr_symbols.info, &common, &custom_cfg, true).unwrap(),
            ResolvedLocaleFilter::Families(BTreeSet::from(["en".to_string(), "und".to_string()]))
        );
        assert_eq!(
            resolve_marker_filter(curr_fractions.info, &common, &custom_cfg, true).unwrap(),
            ResolvedLocaleFilter::Singleton
        );

        // First rule matches `LocaleNamesScriptMediumLightV1` by marker name
        // and takes precedence over the second rule (`locale/names/script/*`).
        match resolve_marker_filter(script_light.info, &common, &custom_cfg, true).unwrap() {
            ResolvedLocaleFilter::Families(fams) => assert_eq!(fams.len(), 3),
            other => panic!("Unexpected custom rule filter: {other:?}"),
        }

        // Another `locale/names/script/*` marker falls through to the second
        // rule (`locales = []` -> `Exclude`).
        let script_other = markers
            .iter()
            .find(|m| {
                let p = marker_path_str(m.info.id);
                p.starts_with("locale/names/script/") && m.name != "LocaleNamesScriptMediumLightV1"
            })
            .unwrap();
        assert_eq!(
            resolve_marker_filter(script_other.info, &common, &custom_cfg, true).unwrap(),
            ResolvedLocaleFilter::Exclude
        );

        // `lang_tiny` matches the third rule (`icu4c_category =
        // "region_tree"`), which in `ios.json` has `includelist: ["en"]` ->
        // `{"^en", "und"}`.
        let ios =
            ParsedIcu4cFilterFile::load(&root.join("third_party/icu/filters/ios.json")).unwrap();
        assert_eq!(
            resolve_marker_filter(lang_tiny.info, &ios, &custom_cfg, true).unwrap(),
            ResolvedLocaleFilter::Families(BTreeSet::from(["^en".to_string(), "und".to_string()]))
        );
    }

    #[test]
    fn test_toml_config_validation_errors() {
        let all_markers = all_registered_markers();

        // 1. Neither `locales` nor `icu4c_category`
        let cfg: DatagenTomlConfig = toml::from_str(
            r#"
            [[rules]]
            pattern = "decimal/*"
            "#,
        )
        .unwrap();
        assert!(cfg.validate(&all_markers).is_err());

        // 2. Both `locales` and `icu4c_category`
        let cfg: DatagenTomlConfig = toml::from_str(
            r#"
            [[rules]]
            pattern = "decimal/*"
            locales = ["en"]
            icu4c_category = "locales_tree"
            "#,
        )
        .unwrap();
        assert!(cfg.validate(&all_markers).is_err());

        // 3. Unknown `icu4c_category` rejected at deserialization
        assert!(toml::from_str::<DatagenTomlConfig>(
            r#"
            [[rules]]
            pattern = "decimal/*"
            icu4c_category = "not_a_category"
            "#,
        )
        .is_err());

        // 4. `icu4c_category = "singleton"` rejected by `validate`
        let cfg: DatagenTomlConfig = toml::from_str(
            r#"
            [[rules]]
            pattern = "decimal/*"
            icu4c_category = "singleton"
            "#,
        )
        .unwrap();
        assert!(cfg.validate(&all_markers).is_err());

        // 5. Wildcard `*` not at the end of `pattern`
        let cfg: DatagenTomlConfig = toml::from_str(
            r#"
            [[rules]]
            pattern = "locale/*/v1"
            locales = ["en"]
            "#,
        )
        .unwrap();
        assert!(cfg.validate(&all_markers).is_err());

        // 6. Pattern matching 0 markers
        let cfg: DatagenTomlConfig = toml::from_str(
            r#"
            [[rules]]
            pattern = "nonexistent_marker/*"
            locales = ["en"]
            "#,
        )
        .unwrap();
        assert!(cfg.validate(&all_markers).is_err());

        // 7. Pattern matching a singleton marker
        let cfg: DatagenTomlConfig = toml::from_str(
            r#"
            [[rules]]
            pattern = "normalizer/*"
            locales = ["en"]
            "#,
        )
        .unwrap();
        assert!(cfg.validate(&all_markers).is_err());

        // 8. Invalid `DataLocaleFamily` in `locales`
        let cfg: DatagenTomlConfig = toml::from_str(
            r#"
            [[rules]]
            pattern = "decimal/*"
            locales = ["not_a_valid_locale_!@#"]
            "#,
        )
        .unwrap();
        assert!(cfg.validate(&all_markers).is_err());
    }
}
