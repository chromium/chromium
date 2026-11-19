// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Standalone TOML configuration (`config.toml`) for overriding marker filter
//! rules, `resourceFilters` behavior, and default `ICU4X-json` patches.

use crate::filters::{
    resolve_default_marker_filter, Icu4cFilterCategory, ParsedIcu4cFilterFile, ResolvedLocaleFilter,
};
use crate::overlay::{BUILTIN_HARDCODED_TRANSLITERATOR_IDS, STANDARD_CLDR_TRANSLITERATOR_ALIASES};
use crate::{marker_path_str, MarkerEntry};
use anyhow::{bail, Context, Result};
use icu_locale_core::extensions::transform::key;
use icu_locale_core::{LanguageIdentifier, Locale};
use icu_provider::prelude::*;
use serde::Deserialize;
use std::collections::{BTreeSet, HashSet};
use std::path::{Path, PathBuf};

/// Standalone TOML configuration (`config.toml`) for overriding marker filter
/// rules, `resourceFilters` behavior, custom transliterators, or default
/// `ICU4X-json` patches.
#[derive(Debug, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct DatagenTomlConfig {
    #[serde(default)]
    pub(crate) resource_filters: ResourceFiltersConfig,
    #[serde(default)]
    pub(crate) transliterators: TransliteratorsConfig,
    #[serde(default)]
    pub(crate) patches: Vec<PathBuf>,
    #[serde(default)]
    pub(crate) rules: Vec<MarkerFilterRule>,
}

fn validate_canonical_transliterator_id(id: &str, field_desc: &str) -> Result<Locale> {
    let loc: Locale = id
        .parse()
        .map_err(|e| anyhow::anyhow!("Invalid BCP-47 locale in {field_desc} {id:?}: {e}"))?;
    let canonical_lower = loc.to_string().to_ascii_lowercase();
    anyhow::ensure!(
        id == canonical_lower,
        "Non-canonical or non-lowercase BCP-47 ID in {field_desc}: got {id:?}, expected \
         {canonical_lower:?}"
    );
    let attrs = DataMarkerAttributes::try_from_str(id).map_err(|e| {
        anyhow::anyhow!("Invalid DataMarkerAttributes in {field_desc} {id:?}: {e:?}")
    })?;
    anyhow::ensure!(
        !attrs.is_empty() && attrs.as_str() == id,
        "Invalid or empty DataMarkerAttributes in {field_desc}: {id:?}"
    );
    Ok(loc)
}

fn validate_custom_bcp47_id(id: &str) -> Result<()> {
    let loc = validate_canonical_transliterator_id(id, "`transliterators.custom.bcp47_id`")?;
    let x0_key = key!("x0");
    let Some(x0_val) = loc.extensions.transform.fields.get(&x0_key).cloned() else {
        bail!(
            "Custom transliterator `bcp47_id` {id:?} must have a private-use `-t-und-x0-...` \
             transform extension"
        );
    };
    let mut expected = Locale::UNKNOWN;
    expected.extensions.transform.lang = Some(LanguageIdentifier::UNKNOWN);
    expected.extensions.transform.fields = [(x0_key, x0_val)].into_iter().collect();
    anyhow::ensure!(
        loc == expected,
        "Custom transliterator `bcp47_id` {id:?} must be of the form `und-t-und-x0-<subtags>` \
         with no other language, transform fields, or extensions"
    );
    Ok(())
}

fn is_uts35_identifier(tok: &str) -> bool {
    let mut chars = tok.chars();
    chars.next().is_some_and(|c| c.is_ascii_alphabetic())
        && chars.all(|c| c.is_ascii_alphanumeric() || c == '_')
}

/// Validate and canonicalize a UTS #35 basic transform ID alias
/// (`<source>-<target>[/<variant>]`, or single-part `<target>[/<variant>]`
/// which defaults `<source>` to `"any"`).
pub(crate) fn canonicalize_uts35_alias(alias: &str) -> Result<String> {
    anyhow::ensure!(
        !alias.is_empty() && alias.trim() == alias,
        "UTS #35 alias {alias:?} must not be empty or whitespace-padded"
    );
    let (base, variant) = match alias.split_once('/') {
        Some((b, v)) => {
            anyhow::ensure!(
                is_uts35_identifier(v),
                "UTS #35 alias {alias:?} has invalid variant {v:?} (expected ASCII identifier \
                 with at most one `/`, and no `-` after `/`)"
            );
            (b, Some(v.to_ascii_lowercase()))
        }
        None => (alias, None),
    };
    let (source, target) = match base.split_once('-') {
        Some((s, t)) => {
            anyhow::ensure!(
                is_uts35_identifier(s) && is_uts35_identifier(t),
                "UTS #35 alias {alias:?} must have valid `<source>-<target>` identifiers \
                 (at most one `-`)"
            );
            (s.to_ascii_lowercase(), t.to_ascii_lowercase())
        }
        None => {
            anyhow::ensure!(
                is_uts35_identifier(base),
                "UTS #35 single-part alias {alias:?} must be a valid ASCII identifier"
            );
            ("any".to_string(), base.to_ascii_lowercase())
        }
    };
    Ok(match variant {
        Some(v) => format!("{source}-{target}/{v}"),
        None => format!("{source}-{target}"),
    })
}

impl DatagenTomlConfig {
    /// Validate `resource_filters`, `transliterators`, and all `[[rules]]`
    /// entries against the registered ICU4X markers.
    pub(crate) fn validate(&self, all_markers: &[MarkerEntry]) -> Result<()> {
        if self.resource_filters.filter_transliterators {
            anyhow::ensure!(
                self.transliterators.include.is_some(),
                "TOML `transliterators.include` must be set when \
                 `resource_filters.filter_transliterators = true`"
            );
        } else {
            anyhow::ensure!(
                self.transliterators.include.is_none(),
                "TOML `transliterators.include` cannot be set when \
                 `resource_filters.filter_transliterators = false`"
            );
        }

        let mut seen_include_ids = HashSet::new();
        if let Some(ref include) = self.transliterators.include {
            for id in include {
                validate_canonical_transliterator_id(id, "`transliterators.include`")?;
                anyhow::ensure!(
                    seen_include_ids.insert(id.as_str()),
                    "Duplicate BCP-47 ID {id:?} in `transliterators.include`"
                );
            }
        }

        let mut seen_custom_ids = HashSet::new();
        let mut seen_aliases: HashSet<String> = BUILTIN_HARDCODED_TRANSLITERATOR_IDS
            .iter()
            .map(|s| s.to_ascii_lowercase())
            .chain(STANDARD_CLDR_TRANSLITERATOR_ALIASES.iter().flat_map(|&(_, aliases)| {
                aliases.iter().map(|s| {
                    canonicalize_uts35_alias(s)
                        .expect("built-in CLDR transliterator alias must be valid UTS #35")
                })
            }))
            .collect();
        for custom in &self.transliterators.custom {
            validate_custom_bcp47_id(&custom.bcp47_id)?;
            anyhow::ensure!(
                !seen_include_ids.contains(custom.bcp47_id.as_str()),
                "Custom transliterator `bcp47_id` {:?} duplicates an entry in \
                 `transliterators.include`",
                custom.bcp47_id
            );
            anyhow::ensure!(
                seen_custom_ids.insert(custom.bcp47_id.as_str()),
                "Duplicate custom transliterator `bcp47_id` {:?}",
                custom.bcp47_id
            );
            custom.rule_source()?;
            for alias in &custom.aliases {
                let canonical = canonicalize_uts35_alias(alias).with_context(|| {
                    format!("Invalid alias in custom transliterator {:?}", custom.bcp47_id)
                })?;
                anyhow::ensure!(
                    seen_aliases.insert(canonical),
                    "Custom transliterator {:?} alias {alias:?} is reserved or duplicated",
                    custom.bcp47_id
                );
            }
        }

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

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub(crate) struct TransliteratorsConfig {
    /// Explicit allowlist of upstream CLDR transliterator BCP-47-T attribute
    /// IDs (e.g. `"und-t-und-latn-d0-ascii"`) to export in
    /// `TransliteratorRulesV1` when `resource_filters.filter_transliterators =
    /// true`.
    pub(crate) include: Option<Vec<String>>,
    /// Custom static UTS #35 transliterator rule sets compiled at datagen time
    /// into `TransliteratorRulesV1` payloads under private-use
    /// `und-t-und-x0-<name>` BCP-47-T IDs.
    pub(crate) custom: Vec<CustomTransliteratorConfig>,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct CustomTransliteratorConfig {
    /// Canonical lowercase private-use BCP-47-T ID (`"und-t-und-x0-"` followed
    /// by one or more subtags of 3–8 ASCII alphanumeric characters each per RFC
    /// 6497, with no other language or extension subtags), used as the
    /// `DataMarkerAttributes` key in `TransliteratorRulesV1`.
    pub(crate) bcp47_id: String,
    /// Optional UTS #35 aliases so custom transliterator entries declared later
    /// in `config.toml` can reference this rule set.
    #[serde(default)]
    pub(crate) aliases: Vec<String>,
    /// Inline UTS #35 transform rule string compiled at datagen time via
    /// `icu_experimental::transliterate::RuleCollection`. Mutually exclusive
    /// with `rules_file`.
    #[serde(default)]
    rules: Option<String>,
    /// Path to a UTF-8 `.txt` file containing UTS #35 transform rules
    /// (resolved relative to the Chromium `src/` root when relative). Mutually
    /// exclusive with `rules`.
    #[serde(default)]
    rules_file: Option<PathBuf>,
}

#[derive(Debug, Clone, Copy)]
pub(crate) enum RuleSource<'a> {
    Inline(&'a str),
    File(&'a Path),
}

fn has_non_comment_rule_content(text: &str) -> bool {
    text.lines().any(|line| {
        let trimmed = line.trim();
        !trimmed.is_empty() && !trimmed.starts_with('#')
    })
}

impl CustomTransliteratorConfig {
    /// Validate and return the mutually exclusive inline `rules` or
    /// `rules_file` source for this custom transliterator.
    pub(crate) fn rule_source(&self) -> Result<RuleSource<'_>> {
        match (&self.rules, &self.rules_file) {
            (Some(rules), None) => {
                anyhow::ensure!(
                    has_non_comment_rule_content(rules),
                    "Custom transliterator {:?} `rules` must not be empty",
                    self.bcp47_id
                );
                Ok(RuleSource::Inline(rules))
            }
            (None, Some(path)) => {
                anyhow::ensure!(
                    !path.as_os_str().is_empty(),
                    "Custom transliterator {:?} `rules_file` path must not be empty",
                    self.bcp47_id
                );
                Ok(RuleSource::File(path))
            }
            (Some(_), Some(_)) => bail!(
                "Custom transliterator {:?} must specify either `rules` or `rules_file`, not both",
                self.bcp47_id
            ),
            (None, None) => bail!(
                "Custom transliterator {:?} must specify either `rules` or `rules_file`",
                self.bcp47_id
            ),
        }
    }

    /// Return the inline `rules` string or read `rules_file` (resolved relative
    /// to `src_root` when relative), ensuring the UTS #35 source contains at
    /// least one non-comment line.
    pub(crate) fn load_rules(&self, src_root: &Path) -> Result<String> {
        match self.rule_source()? {
            RuleSource::Inline(rules) => Ok(rules.to_owned()),
            RuleSource::File(path) => {
                let full_path =
                    if path.is_relative() { src_root.join(path) } else { path.to_path_buf() };
                let content = std::fs::read_to_string(&full_path).with_context(|| {
                    format!(
                        "Failed to read `rules_file` {} for custom transliterator {:?}",
                        full_path.display(),
                        self.bcp47_id
                    )
                })?;
                anyhow::ensure!(
                    has_non_comment_rule_content(&content),
                    "Custom transliterator {:?} `rules_file` {} has no UTS #35 rules",
                    self.bcp47_id,
                    full_path.display()
                );
                Ok(content)
            }
        }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub(crate) struct ResourceFiltersConfig {
    /// Restrict `TransliteratorRulesV1` to `transliterators.include` plus any
    /// `transliterators.custom` entries.
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
        assert!(!toml_cfg.patches.is_empty());
        for patch_rel in &toml_cfg.patches {
            assert!(
                root.join(patch_rel).is_file(),
                "Expected config.toml patch `{}` to exist relative to chromium_src_root()",
                patch_rel.display()
            );
        }

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
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

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
        let assert_validate_err = |toml_body: &str, expected_substr: &str| {
            let cfg: DatagenTomlConfig = toml::from_str(toml_body).unwrap();
            let err = cfg.validate(&all_markers).unwrap_err().to_string();
            assert!(
                err.contains(expected_substr),
                "Expected error containing {expected_substr:?}, got: {err}"
            );
        };
        let assert_rule_err = |rule_body: &str, expected_substr: &str| {
            let full =
                format!("[transliterators]\ninclude = [\"und-t-und-latn-d0-ascii\"]\n{rule_body}");
            assert_validate_err(&full, expected_substr);
        };

        // 1. Neither `locales` nor `icu4c_category`
        assert_rule_err(
            r#"
            [[rules]]
            pattern = "decimal/*"
            "#,
            "must specify either `locales` or `icu4c_category`",
        );

        // 2. Both `locales` and `icu4c_category`
        assert_rule_err(
            r#"
            [[rules]]
            pattern = "decimal/*"
            locales = ["en"]
            icu4c_category = "locales_tree"
            "#,
            "must specify either `locales` or `icu4c_category`, not both",
        );

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
        assert_rule_err(
            r#"
            [[rules]]
            pattern = "decimal/*"
            icu4c_category = "singleton"
            "#,
            "cannot set `icu4c_category = \"singleton\"`",
        );

        // 5. Wildcard `*` not at the end of `pattern`
        assert_rule_err(
            r#"
            [[rules]]
            pattern = "locale/*/v1"
            locales = ["en"]
            "#,
            "only supports `*` at the end of the string",
        );

        // 6. Pattern matching 0 markers
        assert_rule_err(
            r#"
            [[rules]]
            pattern = "nonexistent_marker/*"
            locales = ["en"]
            "#,
            "did not match any registered ICU4X marker",
        );

        // 7. Pattern matching a singleton marker
        assert_rule_err(
            r#"
            [[rules]]
            pattern = "normalizer/*"
            locales = ["en"]
            "#,
            "only matches singleton markers",
        );

        // 8. Invalid `DataLocaleFamily` in `locales`
        assert_rule_err(
            r#"
            [[rules]]
            pattern = "decimal/*"
            locales = ["not_a_valid_locale_!@#"]
            "#,
            "Invalid DataLocaleFamily string",
        );

        // 9. Missing `transliterators.include` when `filter_transliterators =
        //    true`, and contradictory `filter_transliterators = false` +
        //    `include`
        assert_validate_err("", "`transliterators.include` must be set");
        assert_validate_err(
            r#"
            [resource_filters]
            filter_transliterators = false

            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]
            "#,
            "`transliterators.include` cannot be set",
        );

        // 10. Non-lowercase / non-canonical ID in `transliterators.include`
        assert_validate_err(
            r#"
            [transliterators]
            include = ["und-Hira-t-und-kana"]
            "#,
            "Non-canonical or non-lowercase BCP-47 ID",
        );

        // 11. Duplicate ID in `transliterators.include`
        assert_validate_err(
            r#"
            [transliterators]
            include = ["und-hira-t-und-kana", "und-hira-t-und-kana"]
            "#,
            "Duplicate BCP-47 ID",
        );

        // 12. Custom transliterator `bcp47_id` without `-t-und-x0-`, or with
        //     extra extensions / transform fields, or colliding / duplicated
        assert_validate_err(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "de-t-de-d0-ascii"
            rules = "a > b;"
            "#,
            "must have a private-use `-t-und-x0-...` transform extension",
        );
        for extra_id in [
            "und-latn-t-und-x0-afill",
            "und-t-en-x0-afill",
            "und-t-x0-afill",
            "und-t-und-d0-ascii-x0-afill",
            "und-t-und-x0-afill-u-ca-gregory",
            "und-t-und-x0-afill-x-foo",
        ] {
            assert_validate_err(
                &format!(
                    r#"
                    [transliterators]
                    include = ["und-t-und-latn-d0-ascii"]

                    [[transliterators.custom]]
                    bcp47_id = {extra_id:?}
                    rules = "a > b;"
                    "#
                ),
                "must be of the form `und-t-und-x0-<subtags>`",
            );
        }
        assert_validate_err(
            r#"
            [transliterators]
            include = ["und-t-und-x0-afill"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-afill"
            rules = "a > b;"
            "#,
            "duplicates an entry in `transliterators.include`",
        );
        assert_validate_err(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-dup"
            rules = "a > b;"

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-dup"
            rules = "c > d;"
            "#,
            "Duplicate custom transliterator `bcp47_id`",
        );

        // 13. Custom transliterator with subtag >8 chars or <3 chars (invalid
        //     RFC 6497 `tvalue`)
        assert_validate_err(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-subtagtoolong"
            rules = "a > b;"
            "#,
            "Invalid BCP-47 locale",
        );
        assert_validate_err(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-de"
            rules = "a > b;"
            "#,
            "Invalid BCP-47 locale",
        );

        // 14. Custom transliterator with empty/missing/both `rules` &
        //     `rules_file`, or reserved/duplicate alias
        assert_validate_err(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-empty"
            rules = "   "
            "#,
            "`rules` must not be empty",
        );
        assert_validate_err(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-neither"
            "#,
            "must specify either `rules` or `rules_file`",
        );
        assert_validate_err(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-bothsrc"
            rules = "a > b;"
            rules_file = "third_party/rust/icu4x_data/transliterators/foo.txt"
            "#,
            "must specify either `rules` or `rules_file`, not both",
        );
        assert_validate_err(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-emptyf"
            rules_file = ""
            "#,
            "`rules_file` path must not be empty",
        );

        for bad_alias in [
            "Latin-ASCII",
            "Lower",
            "Too-Many-Hyphens",
            "My Alias",
            "-EmptySource",
            "EmptyTarget-",
            "123Invalid",
            "Foo/Bar-Baz",
            "Foo/Bar/Baz",
        ] {
            assert_validate_err(
                &format!(
                    r#"
                    [transliterators]
                    include = ["und-t-und-latn-d0-ascii"]

                    [[transliterators.custom]]
                    bcp47_id = "und-t-und-x0-badal"
                    aliases = [{bad_alias:?}]
                    rules = "a > b;"
                    "#
                ),
                "alias",
            );
        }

        assert_validate_err(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-first"
            aliases = ["MyCustomAlias"]
            rules = "a > b;"

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-second"
            aliases = ["Any-mycustomalias"]
            rules = "c > d;"
            "#,
            "alias \"Any-mycustomalias\" is reserved or duplicated",
        );
    }
}
