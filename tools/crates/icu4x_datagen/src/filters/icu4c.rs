// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Parsing and locale resolution for ICU4C filter JSON files
//! (`//third_party/icu/filters/*.json`).

use anyhow::{bail, Context, Result};
use icu_locale_core::DataLocale;
use icu_provider_export::prelude::*;
use serde::Deserialize;
use std::collections::{BTreeMap, BTreeSet, HashSet};
use std::path::Path;

/// Logical ICU4C filter category for an ICU4X data marker.
#[derive(
    Clone,
    Copy,
    Debug,
    PartialEq,
    Eq,
    PartialOrd,
    Ord,
    Hash,
    Deserialize,
    strum::IntoStaticStr,
    strum::EnumIter,
)]
#[serde(rename_all = "snake_case")]
#[strum(serialize_all = "snake_case")]
pub(crate) enum Icu4cFilterCategory {
    /// Singleton marker (`is_singleton == true`), locale filter does not apply.
    Singleton,
    /// Segmenter dictionary / LSTM models (`segmenter/dictionary/*`,
    /// `segmenter/lstm/*`).
    #[serde(rename = "brkitr_dictionaries")]
    #[strum(serialize = "brkitr_dictionaries")]
    SegmenterModels,
    /// `curr_tree` (`currency/*`).
    CurrTree,
    /// `zone_tree` (`time_zone/*`).
    ZoneTree,
    /// `region_tree` (`locale/names/region/*`).
    RegionTree,
    /// `lang_tree` (`locale/names/language/*`, `locale/names/locale/*`,
    /// `locale/names/script/*`, `locale/names/variant/*`,
    /// `locale/names/essentials/*`).
    LangTree,
    /// `unit_tree` (`units/names/*`, `units/essentials/*`, `units/duration/*`).
    UnitTree,
    /// `coll_tree` (`collator/*` non-singleton).
    CollTree,
    /// `brkitr_tree` (`segmenter/break/*`).
    BrkitrTree,
    /// `translit` (`transliterator/*`).
    Translit,
    /// `plurals` (`plurals/*` — in ICU4C `misc/plurals.res`, unfiltered unless
    /// `locales_tree` is excluded).
    Plurals,
    /// `locales_tree` (`datetime/*`, `decimal/*`, `percent/*`, `list/*`,
    /// `personnames/*`, `locale/exemplar_characters/*`).
    LocalesTree,
}

impl Icu4cFilterCategory {
    pub(super) fn as_str(self) -> &'static str {
        self.into()
    }
}

/// Representation of a resolved locale filter for a category/group of markers.
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) enum ResolvedLocaleFilter {
    /// Singleton markers (`und` only).
    Singleton,
    /// Exclude all locales (0 locales exported; baked macro produces empty
    /// lookup table).
    Exclude,
    /// Full locales (`DataLocaleFamily::FULL`).
    Full,
    /// Explicit list of `DataLocaleFamily` strings (e.g., `"en"`, `"^en-US"`,
    /// `"und"`).
    Families(BTreeSet<String>),
}

impl ResolvedLocaleFilter {
    pub(crate) fn to_locale_families(&self) -> Result<Vec<DataLocaleFamily>> {
        match self {
            Self::Singleton | Self::Exclude => Ok(vec![]),
            Self::Full => Ok(vec![DataLocaleFamily::FULL]),
            Self::Families(set) => {
                let mut out = Vec::with_capacity(set.len());
                for s in set {
                    if s == "und" || s == "@und" || s == "^und" {
                        out.push(DataLocaleFamily::single(DataLocale::default()));
                        continue;
                    }
                    let fam: DataLocaleFamily = s
                        .parse()
                        .with_context(|| format!("Invalid DataLocaleFamily string: {s:?}"))?;
                    out.push(fam);
                }
                Ok(out)
            }
        }
    }
}

/// Normalize an ICU4C locale string (`en_US`, `root`, `sr_Latn_RS`,
/// `tzdbNames`) into a canonical BCP-47 string for `DataLocale`, or `None` if
/// it is a non-locale pseudo-stem like `tzdbNames` or `en_US_POSIX`.
fn normalize_icu4c_locale(stem: &str) -> Option<String> {
    if stem == "tzdbNames" || stem == "en_US_POSIX" {
        return None;
    }
    if stem == "root" {
        return Some("und".to_string());
    }
    let bcp47 = stem.replace('_', "-");
    // Validate that DataLocale/DataLocaleFamily can parse it
    let _: DataLocale =
        bcp47.parse().expect("ICU4C locale stem should parse as a valid BCP-47 DataLocale");
    Some(bcp47)
}

/// Return CLDR non-default `<lang>-<Script>` locales for a bare language subtag
/// `lang`.
///
/// In CLDR `parentLocales` (and
/// `//third_party/icu/source/data/locales/LOCALE_DEPS.json`), non-default
/// script locales such as `zh-Hant` or `sr-Latn` inherit directly from `root`
/// (`und`, or `en-IN` for `hi-Latn`) rather than from `<lang>`. Consequently,
/// `LocaleFallbacker` chains for `zh-TW -> zh-Hant -> und` or `sr-ME -> sr-Latn
/// -> und` never visit `zh` or `sr`. When expanding an ICU4C `"language"`
/// filter (which matches the language prefix before `_`) or a `"locale"` filter
/// with `includeScripts: true`, we must include these `<lang>-<Script>`
/// families alongside `<lang>`.
fn non_default_scripts_for_language(lang: &str) -> &'static [&'static str] {
    match lang {
        "az" => &["az-Cyrl"],
        "bs" => &["bs-Cyrl"],
        "ff" => &["ff-Adlm"],
        "hi" => &["hi-Latn"],
        "kk" => &["kk-Arab"],
        "kok" => &["kok-Latn"],
        "ks" => &["ks-Deva"],
        "kxv" => &["kxv-Deva", "kxv-Orya", "kxv-Telu"],
        "pa" => &["pa-Arab"],
        "sd" => &["sd-Deva"],
        "shi" => &["shi-Latn"],
        "sr" => &["sr-Latn"],
        "uz" => &["uz-Arab", "uz-Cyrl"],
        "vai" => &["vai-Latn"],
        "yue" => &["yue-Hans"],
        "zh" => &["zh-Hant"],
        _ => &[],
    }
}

/// Map an ICU4C `brkitr_dictionaries` stem to its corresponding ICU4X
/// segmenter dictionary and LSTM model names.
fn icu4x_models_for_icu4c_dict(dict_name: &str) -> Result<&'static [&'static str]> {
    match dict_name {
        "burmesedict" => Ok(&["Burmese_codepoints_exclusive_model4_heavy", "burmesedict"]),
        "cjdict" => Ok(&["cjdict"]),
        "khmerdict" => Ok(&["Khmer_codepoints_exclusive_model4_heavy", "khmerdict"]),
        "laodict" => Ok(&["Lao_codepoints_exclusive_model4_heavy", "laodict"]),
        "thaidict" => Ok(&["Thai_codepoints_exclusive_model4_heavy", "thaidict"]),
        other => bail!("Unknown ICU4C brkitr_dictionaries entry: {other:?}"),
    }
}

/// Strip `// ...` line comments from ICU4C's JSON-with-comments filter files.
fn strip_json_comments(input: &str) -> String {
    let mut out = String::with_capacity(input.len());
    for line in input.lines() {
        let trimmed = line.trim_start();
        if trimmed.starts_with("//") {
            continue;
        }
        if let Some(idx) = line.find("//") {
            // Simple check that `//` is not inside quotes
            let prefix = &line[..idx];
            if prefix.chars().filter(|&c| c == '"').count() % 2 == 0 {
                out.push_str(prefix);
                out.push('\n');
                continue;
            }
        }
        out.push_str(line);
        out.push('\n');
    }
    out
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
struct RawFilterFile {
    #[serde(default, rename = "collationUCAData")]
    _collation_uca_data: Option<String>,
    #[serde(default)]
    locale_filter: Option<RawFilterNode>,
    #[serde(default)]
    feature_filters: BTreeMap<String, RawFilterNodeOrString>,
    #[serde(default)]
    resource_filters: Vec<RawResourceFilter>,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(untagged)]
enum RawFilterNodeOrString {
    Mode(String),
    Node(RawFilterNode),
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
struct RawFilterNode {
    #[serde(default)]
    filter_type: Option<String>,
    #[serde(default)]
    include_children: Option<bool>,
    #[serde(default)]
    include_scripts: Option<bool>,
    #[serde(default, alias = "whitelist")] // nocheck
    includelist: Option<Vec<String>>,
    #[serde(default, alias = "blacklist")] // nocheck
    excludelist: Option<Vec<String>>,
    #[serde(default)]
    union_of: Option<Vec<RawFilterNode>>,
}

impl RawFilterNode {
    fn is_empty(&self) -> bool {
        self.filter_type.is_none()
            && self.include_children.is_none()
            && self.include_scripts.is_none()
            && self.includelist.is_none()
            && self.excludelist.is_none()
            && self.union_of.is_none()
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
struct RawResourceFilter {
    categories: Vec<String>,
    #[serde(default)]
    files: Option<RawFilterNode>,
    #[serde(default)]
    includelist: Option<Vec<String>>,
    rules: Vec<String>,
}

#[derive(Debug, Clone)]
pub(crate) struct ParsedIcu4cFilterFile {
    pub(super) platform: String,
    raw: RawFilterFile,
    pub(super) segmenter_models: Vec<String>,
}

impl ParsedIcu4cFilterFile {
    pub(crate) fn load(path: &Path) -> Result<Self> {
        let platform = path
            .file_stem()
            .and_then(|s| s.to_str())
            .context("Invalid filter filename")?
            .to_string();
        let content = std::fs::read_to_string(path)
            .with_context(|| format!("Failed to read {}", path.display()))?;
        let clean = strip_json_comments(&content);
        let raw: RawFilterFile = serde_json::from_str(&clean)
            .with_context(|| format!("Failed to parse JSON in {}", path.display()))?;

        // Resolve segmenter models based on
        // `featureFilters.brkitr_dictionaries`
        let default_models = vec![
            "Burmese_codepoints_exclusive_model4_heavy".to_string(),
            "burmesedict".to_string(),
            "cjdict".to_string(),
            "Khmer_codepoints_exclusive_model4_heavy".to_string(),
            "khmerdict".to_string(),
            "Lao_codepoints_exclusive_model4_heavy".to_string(),
            "laodict".to_string(),
            "Thai_codepoints_exclusive_model4_heavy".to_string(),
            "thaidict".to_string(),
        ];

        let mut segmenter_models = default_models;
        if let Some(dict_filter) = raw.feature_filters.get("brkitr_dictionaries") {
            match dict_filter {
                RawFilterNodeOrString::Mode(mode) if mode == "exclude" => {
                    segmenter_models.clear();
                }
                RawFilterNodeOrString::Mode(mode) => {
                    bail!("Unsupported brkitr_dictionaries mode: {mode:?}");
                }
                RawFilterNodeOrString::Node(node) => {
                    if node.filter_type.as_deref() == Some("exclude") {
                        segmenter_models.clear();
                    } else if let Some(inc) = &node.includelist {
                        let mut allowed_models = HashSet::new();
                        for dict_name in inc {
                            for &m in icu4x_models_for_icu4c_dict(dict_name)? {
                                allowed_models.insert(m);
                            }
                        }
                        segmenter_models.retain(|m| allowed_models.contains(m.as_str()));
                    } else if let Some(exc) = &node.excludelist {
                        let mut excluded_models = HashSet::new();
                        for dict_name in exc {
                            for &m in icu4x_models_for_icu4c_dict(dict_name)? {
                                excluded_models.insert(m);
                            }
                        }
                        segmenter_models.retain(|m| !excluded_models.contains(m.as_str()));
                    } else {
                        bail!("Unsupported brkitr_dictionaries filter node: {node:?}");
                    }
                }
            }
        }

        Ok(Self { platform, raw, segmenter_models })
    }

    /// Find the first `resourceFilters` entry without a file filter that
    /// applies to `category` and contains `marker_rule`, returning its
    /// `rules` slice.
    fn rules_for(&self, category: &str, marker_rule: &str) -> Option<&[String]> {
        for rf in &self.raw.resource_filters {
            if rf.files.is_some() || rf.includelist.is_some() {
                continue;
            }
            if rf.categories.iter().any(|c| c == category)
                && rf.rules.iter().any(|r| r == marker_rule)
            {
                return Some(&rf.rules);
            }
        }
        None
    }

    /// Extract the set of non-UI locales abridged via `-/*` in
    /// `resourceFilters` for `category` (`locales_tree` or `lang_tree`).
    pub(super) fn abridged_non_ui_locales_for(
        &self,
        category: Icu4cFilterCategory,
    ) -> HashSet<String> {
        let cat_name = category.as_str();
        let mut abridged = HashSet::new();
        for rf in &self.raw.resource_filters {
            if rf.categories.iter().any(|c| c == cat_name) && rf.rules.iter().any(|r| r == "-/*") {
                if let Some(files) = &rf.files {
                    if let Some(list) = &files.includelist {
                        for item in list {
                            if let Some(norm) = normalize_icu4c_locale(item) {
                                abridged.insert(norm);
                            }
                        }
                    }
                }
            }
        }
        abridged
    }

    /// Extract allowed target language attributes from `lang_tree`
    /// `resourceFilters` (`+/*/<lang>` or `+/Languages/<lang>`).
    pub(super) fn allowed_target_languages(&self) -> Option<HashSet<String>> {
        let rules = self.rules_for("lang_tree", "-/Languages")?;
        let mut set = HashSet::new();
        for r in rules {
            if let Some(lang) = r.strip_prefix("+/*/").or_else(|| r.strip_prefix("+/Languages/")) {
                let norm = lang.replace('_', "-");
                set.insert(norm);
                if let Some(norm_loc) = normalize_icu4c_locale(lang) {
                    set.insert(norm_loc);
                }
            }
        }
        Some(set)
    }

    /// Extract allowed target variant attributes from `lang_tree`
    /// `resourceFilters` (`+/Variants/<var>`).
    pub(super) fn allowed_target_variants(&self) -> Option<HashSet<String>> {
        let rules = self.rules_for("lang_tree", "-/Variants")?;
        let set = rules
            .iter()
            .filter_map(|r| r.strip_prefix("+/Variants/"))
            .map(|var| var.to_ascii_lowercase())
            .collect();
        Some(set)
    }

    /// Extract allowed ISO currency codes from `curr_tree` `resourceFilters`
    /// (`+/*/<CURRENCY>` or `+/Currencies/<CURRENCY>`).
    pub(super) fn allowed_currencies(&self) -> Option<HashSet<String>> {
        let rules = self.rules_for("curr_tree", "-/Currencies")?;
        let set = rules
            .iter()
            .filter_map(|r| r.strip_prefix("+/*/").or_else(|| r.strip_prefix("+/Currencies/")))
            .map(|curr| curr.to_ascii_uppercase())
            .collect();
        Some(set)
    }

    /// Extract allowed transliterator BCP-47 IDs from `translit`
    /// `resourceFilters` (`+/RuleBasedTransliteratorIDs/<id>`).
    pub(super) fn allowed_transliterators(&self) -> Option<HashSet<String>> {
        let rules = self.rules_for("translit", "-/*")?;
        let mut set = HashSet::new();
        for r in rules {
            if let Some(id) = r.strip_prefix("+/RuleBasedTransliteratorIDs/") {
                match id {
                    "Latin-ASCII" => {
                        set.insert("und-t-und-latn-d0-ascii".to_string());
                        set.insert("und-latn-t-und-latn-d0-ascii".to_string());
                        set.insert("de-t-de-d0-ascii".to_string());
                    }
                    "Katakana-Hiragana" | "Hira-Kana" => {
                        set.insert("und-hira-t-und-kana".to_string());
                    }
                    "Hiragana-Katakana" | "Kana-Hira" => {
                        set.insert("und-kana-t-und-hira".to_string());
                    }
                    "Any-Accents" => {
                        set.insert("und-t-und-x0-accents".to_string());
                    }
                    "Accents-Any" => {
                        set.insert("und-x0-accents-t-und".to_string());
                    }
                    other => {
                        set.insert(other.to_ascii_lowercase());
                    }
                }
            }
        }
        Some(set)
    }

    /// Parse a `featureFilters` entry (`"exclude"` / `"include"` or a filter
    /// object) into a `ResolvedLocaleFilter`.
    fn parse_filter_node_or_string(node: &RawFilterNodeOrString) -> Result<ResolvedLocaleFilter> {
        match node {
            RawFilterNodeOrString::Mode(mode) => match mode.as_str() {
                "exclude" => Ok(ResolvedLocaleFilter::Exclude),
                "include" => Ok(ResolvedLocaleFilter::Full),
                other => bail!("Unknown ICU4C filter mode string: {other:?}"),
            },
            RawFilterNodeOrString::Node(n) => Self::parse_filter_node(n),
        }
    }

    /// Parse a single ICU4C filter JSON object (`localeFilter` or
    /// `featureFilters.<tree>`) into a `ResolvedLocaleFilter`.
    fn parse_filter_node(node: &RawFilterNode) -> Result<ResolvedLocaleFilter> {
        match node.filter_type.as_deref() {
            Some("exclude") => Ok(ResolvedLocaleFilter::Exclude),
            Some("union") => {
                let union_of =
                    node.union_of.as_ref().context("`union` filter requires `unionOf`")?;
                let mut families = BTreeSet::new();
                for sub in union_of {
                    match Self::parse_filter_node(sub)? {
                        ResolvedLocaleFilter::Families(sub_fams) => {
                            families.extend(sub_fams);
                        }
                        ResolvedLocaleFilter::Full => {
                            return Ok(ResolvedLocaleFilter::Full);
                        }
                        ResolvedLocaleFilter::Exclude | ResolvedLocaleFilter::Singleton => {}
                    }
                }
                if families.is_empty() {
                    Ok(ResolvedLocaleFilter::Exclude)
                } else {
                    families.insert("und".to_string());
                    Ok(ResolvedLocaleFilter::Families(families))
                }
            }
            Some("language") => {
                anyhow::ensure!(
                    node.excludelist.is_none(),
                    "`language` filter with `excludelist` is not supported"
                );
                let inc = node
                    .includelist
                    .as_ref()
                    .context("`language` filter requires `includelist`")?;
                let mut families = BTreeSet::new();
                families.insert("und".to_string());
                for item in inc {
                    if let Some(norm) = normalize_icu4c_locale(item) {
                        for &script_loc in non_default_scripts_for_language(&norm) {
                            families.insert(script_loc.to_string());
                        }
                        // `DataLocaleFamily` without prefix means
                        // `with_descendants` (unless `und`).
                        families.insert(norm);
                    }
                }
                Ok(ResolvedLocaleFilter::Families(families))
            }
            Some("locale") => {
                anyhow::ensure!(
                    node.excludelist.is_none(),
                    "`locale` filter with `excludelist` is not supported"
                );
                let include_children = node.include_children.unwrap_or(true);
                let include_scripts = node.include_scripts.unwrap_or(false);
                let inc =
                    node.includelist.as_ref().context("`locale` filter requires `includelist`")?;
                let mut families = BTreeSet::new();
                families.insert("und".to_string());
                for item in inc {
                    if let Some(norm) = normalize_icu4c_locale(item) {
                        if include_scripts {
                            for &script_loc in non_default_scripts_for_language(&norm) {
                                if include_children {
                                    families.insert(script_loc.to_string());
                                } else {
                                    families.insert(format!("^{script_loc}"));
                                }
                            }
                        }
                        if norm == "und" || include_children {
                            families.insert(norm);
                        } else {
                            // `^locale` in DataLocaleFamily syntax is
                            // `DataLocaleFamily::without_descendants`
                            // (the locale and its ancestors via ICU4X
                            // fallback, matching ICU4C `_locales_required`).
                            families.insert(format!("^{norm}"));
                        }
                    }
                }
                Ok(ResolvedLocaleFilter::Families(families))
            }
            None | Some("file-stem") => {
                anyhow::ensure!(
                    node.include_children.is_none() && node.include_scripts.is_none(),
                    "`file-stem` filter does not support `includeChildren` or `includeScripts`"
                );
                if let Some(inc) = &node.includelist {
                    // In ICU4C, `file-stem` matches exact `.txt` bundle stems.
                    // In ICU4X, `ExportDriver` with
                    // `DeduplicationStrategy::Maximal` and runtime
                    // `LocaleFallbacker` require ancestor payloads up to `und`,
                    // so we map each stem to `^locale`
                    // (`DataLocaleFamily::without_descendants`, which includes
                    // `locale` and its ancestors up to `und`, without
                    // descendants).
                    let mut families = BTreeSet::new();
                    families.insert("und".to_string());
                    for item in inc {
                        if let Some(norm) = normalize_icu4c_locale(item) {
                            if norm == "und" {
                                families.insert(norm);
                            } else {
                                families.insert(format!("^{norm}"));
                            }
                        }
                    }
                    Ok(ResolvedLocaleFilter::Families(families))
                } else if let Some(exc) = &node.excludelist {
                    let real_excluded: Vec<String> =
                        exc.iter().filter_map(|item| normalize_icu4c_locale(item)).collect();
                    anyhow::ensure!(
                        real_excluded.is_empty(),
                        "Unsupported locale excludelist in file-stem filter: {real_excluded:?}"
                    );
                    Ok(ResolvedLocaleFilter::Full)
                } else {
                    bail!("`file-stem` filter requires `includelist` or `excludelist`");
                }
            }
            Some(other) => bail!("Unknown filterType in ICU4C filter file: {other:?}"),
        }
    }

    fn apply_abridgment(
        &self,
        filter: &mut ResolvedLocaleFilter,
        category: Icu4cFilterCategory,
        autonyms: bool,
    ) {
        let abridged = self.abridged_non_ui_locales_for(category);
        if abridged.is_empty() {
            return;
        }
        let ResolvedLocaleFilter::Families(fams) = filter else {
            return;
        };
        if autonyms {
            // Keep abridged non-UI locales as `^locale` (`without_descendants`)
            // so only their autonym is exported by `FilteredDataProvider`,
            // without pulling in regional children like `ps-PK` or `hr-BA`.
            let updated: BTreeSet<String> = fams
                .iter()
                .filter_map(|f| {
                    let clean = f.strip_prefix('^').unwrap_or(f);
                    if clean == "und" {
                        return Some("und".to_string());
                    }
                    let lang = clean.split('-').next().unwrap_or(clean);
                    if abridged.contains(clean) {
                        Some(format!("^{clean}"))
                    } else if abridged.contains(lang) {
                        None
                    } else {
                        Some(f.clone())
                    }
                })
                .collect();
            *fams = updated;
        } else {
            fams.retain(|f| {
                let clean = f.strip_prefix('^').unwrap_or(f);
                let lang = clean.split('-').next().unwrap_or(clean);
                clean == "und" || (!abridged.contains(clean) && !abridged.contains(lang))
            });
        }
    }

    pub(crate) fn resolve_category(
        &self,
        category: Icu4cFilterCategory,
        abridge_non_ui_locales: bool,
        autonym_only: bool,
    ) -> Result<ResolvedLocaleFilter> {
        let resolve_locale_filter = || -> Result<ResolvedLocaleFilter> {
            let Some(lf_node) = &self.raw.locale_filter else {
                return Ok(ResolvedLocaleFilter::Exclude);
            };
            if lf_node.is_empty() {
                return Ok(ResolvedLocaleFilter::Exclude);
            }
            Self::parse_filter_node(lf_node)
        };

        match category {
            Icu4cFilterCategory::Singleton => return Ok(ResolvedLocaleFilter::Singleton),
            Icu4cFilterCategory::SegmenterModels => {
                if self.segmenter_models.is_empty() {
                    return Ok(ResolvedLocaleFilter::Exclude);
                }
                return Ok(ResolvedLocaleFilter::Full);
            }
            Icu4cFilterCategory::Plurals => {
                // In ICU4C, plurals is in `misc/plurals.res`. If `locales_tree`
                // is excluded (e.g. flutter / flutter_desktop),
                // exclude plurals too; otherwise include
                // all localeFilter locales.
                if let Some(n) = self.raw.feature_filters.get("locales_tree") {
                    if Self::parse_filter_node_or_string(n)? == ResolvedLocaleFilter::Exclude {
                        return Ok(ResolvedLocaleFilter::Exclude);
                    }
                }
                return resolve_locale_filter();
            }
            _ => {}
        }

        let tree_key = category.as_str();

        let mut base = if let Some(tree_node) = self.raw.feature_filters.get(tree_key) {
            let parsed = Self::parse_filter_node_or_string(tree_node)?;
            // If a tree filter (such as `brkitr_tree`) only excludes
            // non-locale stems like `en_US_POSIX`, `parse_filter_node` returns
            // `Full`, which in ICU4C's subtractive filter model means all
            // locales permitted by `localeFilter`.
            if parsed == ResolvedLocaleFilter::Full {
                resolve_locale_filter()?
            } else {
                parsed
            }
        } else {
            // In ICU4C `filtration.py`, any `*_tree` not explicitly listed in
            // `featureFilters` inherits `localeFilter`.
            resolve_locale_filter()?
        };

        // Both `locales_tree` and `lang_tree` abridge non-UI Accept-Languages
        // via `-/*` in `resourceFilters` (including when `featureFilters`
        // explicitly lists the tree, as in `cast.json`'s `lang_tree`).
        if abridge_non_ui_locales
            && matches!(category, Icu4cFilterCategory::LocalesTree | Icu4cFilterCategory::LangTree)
        {
            self.apply_abridgment(&mut base, category, autonym_only);
        }
        Ok(base)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::chromium_src_root;
    use icu_locale_fallback::LocaleFallbacker;

    #[test]
    fn test_normalize_icu4c_locale() {
        assert_eq!(normalize_icu4c_locale("root").as_deref(), Some("und"));
        assert_eq!(normalize_icu4c_locale("en_US").as_deref(), Some("en-US"));
        assert_eq!(normalize_icu4c_locale("sr_Latn_RS").as_deref(), Some("sr-Latn-RS"));
        assert_eq!(normalize_icu4c_locale("tzdbNames"), None);
        assert_eq!(normalize_icu4c_locale("en_US_POSIX"), None);
    }

    #[test]
    fn test_und_locale_family_does_not_expand_to_full() {
        let filter = ResolvedLocaleFilter::Families(BTreeSet::from([
            "und".to_string(),
            "@und".to_string(),
            "^und".to_string(),
            "en".to_string(),
            "^en-US".to_string(),
        ]));
        let families = filter.to_locale_families().unwrap();
        let single_und = DataLocaleFamily::single(DataLocale::default());
        for fam in &families {
            assert_ne!(
                *fam,
                DataLocaleFamily::FULL,
                "Normalized 'und' must produce single('@und'), never DataLocaleFamily::FULL"
            );
        }
        assert_eq!(
            families.iter().filter(|&f| *f == single_und).count(),
            3,
            "All three und spellings should map to DataLocaleFamily::single(und)"
        );
    }

    #[test]
    fn test_non_default_script_locales_match_icu_locale_deps() {
        let deps_path =
            chromium_src_root().join("third_party/icu/source/data/locales/LOCALE_DEPS.json");
        let content = std::fs::read_to_string(&deps_path).unwrap();
        let clean = strip_json_comments(&content);
        let json: serde_json::Value = serde_json::from_str(&clean).unwrap();
        let parents = json.get("parents").and_then(|v| v.as_object()).unwrap();

        let mut expected_by_lang: BTreeMap<String, Vec<String>> = BTreeMap::new();
        for key in parents.keys() {
            let parts: Vec<&str> = key.split('_').collect();
            if parts.len() == 2
                && parts[1].len() == 4
                && parts[1].chars().next().unwrap().is_ascii_uppercase()
            {
                expected_by_lang
                    .entry(parts[0].to_string())
                    .or_default()
                    .push(format!("{}-{}", parts[0], parts[1]));
            }
        }

        for (lang, expected_scripts) in &expected_by_lang {
            let actual: Vec<String> =
                non_default_scripts_for_language(lang).iter().map(|s| (*s).to_string()).collect();
            assert_eq!(
                &actual, expected_scripts,
                "Mismatch in non_default_scripts_for_language({lang:?}) vs LOCALE_DEPS.json"
            );
        }
    }

    #[test]
    fn test_common_language_filter_includes_non_default_scripts() {
        let filters_dir = chromium_src_root().join("third_party/icu/filters");
        let common = ParsedIcu4cFilterFile::load(&filters_dir.join("common.json")).unwrap();
        let resolved =
            common.resolve_category(Icu4cFilterCategory::LocalesTree, true, false).unwrap();
        let ResolvedLocaleFilter::Families(ref fam_set) = resolved else {
            panic!("Expected Families, got {resolved:?}");
        };
        assert!(fam_set.contains("zh"));
        assert!(fam_set.contains("zh-Hant"));
        assert!(fam_set.contains("sr"));
        assert!(fam_set.contains("sr-Latn"));

        // Verify that a fallback walk from `zh-TW`, `zh-HK`, `sr-Latn-RS`, and
        // `sr-ME` visits a requested `with_descendants` family in `fam_set`.
        let fallbacker = LocaleFallbacker::new();
        for child_str in ["zh-TW", "zh-HK", "zh-Hant", "sr-Latn-RS", "sr-ME", "sr-Latn"] {
            let child_loc: DataLocale = child_str.parse().unwrap();
            let mut it = fallbacker.for_config(Default::default()).fallback_for(child_loc);
            let mut matched = false;
            loop {
                let ancestor = it.get().to_string();
                if ancestor != "und" && fam_set.contains(&ancestor) {
                    matched = true;
                    break;
                }
                if it.get().is_unknown() {
                    break;
                }
                it.step();
            }
            assert!(
                matched,
                "Expected {child_str} fallback chain to match a family in common.json LocalesTree"
            );
        }
    }

    #[test]
    fn test_parse_all_chromium_icu4c_filter_files() {
        let filters_dir = chromium_src_root().join("third_party/icu/filters");
        let platforms = [
            "common",
            "chromeos",
            "android",
            "ios",
            "cast",
            "chromecast_video",
            "flutter",
            "flutter_desktop",
        ];
        for plat in platforms {
            let path = filters_dir.join(format!("{plat}.json"));
            let parsed = ParsedIcu4cFilterFile::load(&path)
                .unwrap_or_else(|e| panic!("Failed to load {}: {e}", path.display()));
            assert_eq!(parsed.platform, plat);
        }

        // Verify `common.json` details
        let common = ParsedIcu4cFilterFile::load(&filters_dir.join("common.json")).unwrap();
        let abridged_locales = common.abridged_non_ui_locales_for(Icu4cFilterCategory::LocalesTree);
        assert_eq!(abridged_locales.len(), 98);
        assert!(abridged_locales.contains("ny"));
        assert!(!abridged_locales.contains("ak"));

        let abridged_lang = common.abridged_non_ui_locales_for(Icu4cFilterCategory::LangTree);
        assert_eq!(abridged_lang.len(), 99);
        assert!(abridged_lang.contains("ak"));
        assert!(abridged_lang.contains("ay"));
        assert!(abridged_lang.contains("wa"));
        assert!(abridged_lang.contains("zu"));
        assert!(!abridged_lang.contains("en"));
        assert!(!abridged_lang.contains("fr"));

        let translit = common.allowed_transliterators().unwrap();
        assert!(translit.contains("und-t-und-latn-d0-ascii"));
        assert!(translit.contains("und-latn-t-und-latn-d0-ascii"));
        assert!(translit.contains("de-t-de-d0-ascii"));
        assert!(translit.contains("und-hira-t-und-kana"));
        assert!(translit.contains("und-kana-t-und-hira"));
        assert_eq!(translit.len(), 5);

        let currencies = common.allowed_currencies().unwrap();
        assert!(currencies.contains("USD"));
        assert!(currencies.contains("EUR"));
        assert!(currencies.contains("PKR"));
        assert!(currencies.contains("RSD"));

        let variants = common.allowed_target_variants().unwrap();
        assert_eq!(variants, HashSet::from(["oxendict".to_string()]));

        // `LocalesTree` abridged has 59 families (54 UI languages + 3 UI
        // non-default scripts `hi-Latn`, `sr-Latn`, `zh-Hant` + `ak`, `nyn` +
        // `und` = 59), unabridged has 164 (152 languages + 11 non-default
        // scripts + `und`).
        match common.resolve_category(Icu4cFilterCategory::LocalesTree, true, false).unwrap() {
            ResolvedLocaleFilter::Families(fams) => assert_eq!(fams.len(), 59),
            other => panic!("Unexpected filter: {other:?}"),
        }
        match common.resolve_category(Icu4cFilterCategory::LocalesTree, false, false).unwrap() {
            ResolvedLocaleFilter::Families(fams) => assert_eq!(fams.len(), 164),
            other => panic!("Unexpected filter: {other:?}"),
        }

        // Verify `ios.json` and `chromecast_video.json` `+/Languages/zh` rule
        let ios = ParsedIcu4cFilterFile::load(&filters_dir.join("ios.json")).unwrap();
        assert_eq!(ios.allowed_target_languages(), Some(HashSet::from(["zh".to_string()])));
        assert_eq!(ios.allowed_target_variants(), Some(HashSet::new()));

        let ccv = ParsedIcu4cFilterFile::load(&filters_dir.join("chromecast_video.json")).unwrap();
        assert_eq!(ccv.allowed_target_languages(), Some(HashSet::from(["zh".to_string()])));
        assert!(!ccv.segmenter_models.contains(&"cjdict".to_string()));
        assert!(common.segmenter_models.contains(&"cjdict".to_string()));

        // Verify `cast.json`, which defines both `featureFilters.lang_tree`
        // (`^locale` stems) and a `lang_tree` `resourceFilters` `-/*` rule for
        // 113 non-UI locales.
        let cast = ParsedIcu4cFilterFile::load(&filters_dir.join("cast.json")).unwrap();
        assert_eq!(cast.abridged_non_ui_locales_for(Icu4cFilterCategory::LangTree).len(), 113);
        match cast.resolve_category(Icu4cFilterCategory::LangTree, false, false).unwrap() {
            ResolvedLocaleFilter::Families(fams) => {
                assert_eq!(fams.len(), 129);
                assert!(fams.contains("^af"));
                assert!(fams.contains("^en"));
            }
            other => panic!("Unexpected unabridged cast LangTree: {other:?}"),
        }
        match cast.resolve_category(Icu4cFilterCategory::LangTree, true, false).unwrap() {
            ResolvedLocaleFilter::Families(fams) => {
                assert_eq!(fams.len(), 60);
                assert!(!fams.contains("^af"));
                assert!(!fams.contains("^ak"));
                assert!(fams.contains("^en"));
                assert!(fams.contains("^en-GB"));
                assert!(fams.contains("^ja"));
            }
            other => panic!("Unexpected abridged cast LangTree: {other:?}"),
        }
        match cast.resolve_category(Icu4cFilterCategory::LangTree, true, true).unwrap() {
            ResolvedLocaleFilter::Families(fams) => {
                assert!(fams.contains("^af"));
                assert!(fams.contains("^ak"));
                assert!(fams.contains("^en"));
                assert!(fams.contains("^en-GB"));
            }
            other => panic!("Unexpected autonym cast LangTree: {other:?}"),
        }

        // Verify `flutter.json` excludes locale trees and `cjdict`
        let flutter = ParsedIcu4cFilterFile::load(&filters_dir.join("flutter.json")).unwrap();
        assert!(!flutter.segmenter_models.contains(&"cjdict".to_string()));
        assert!(flutter.segmenter_models.contains(&"thaidict".to_string()));
        assert_eq!(
            flutter.resolve_category(Icu4cFilterCategory::CurrTree, true, false).unwrap(),
            ResolvedLocaleFilter::Exclude
        );
        assert_eq!(
            flutter.resolve_category(Icu4cFilterCategory::LocalesTree, true, false).unwrap(),
            ResolvedLocaleFilter::Exclude
        );
        assert_eq!(
            flutter.resolve_category(Icu4cFilterCategory::Plurals, true, false).unwrap(),
            ResolvedLocaleFilter::Exclude
        );
    }

    #[test]
    fn test_filter_validation_errors() {
        let unknown_type: RawFilterNode =
            serde_json::from_str(r#"{"filterType": "regex", "includelist": ["en"]}"#).unwrap();
        assert!(ParsedIcu4cFilterFile::parse_filter_node(&unknown_type).is_err());

        let missing_list: RawFilterNode =
            serde_json::from_str(r#"{"filterType": "locale"}"#).unwrap();
        assert!(ParsedIcu4cFilterFile::parse_filter_node(&missing_list).is_err());

        let unsupported_exclude: RawFilterNode =
            serde_json::from_str(r#"{"excludelist": ["en"]}"#).unwrap();
        assert!(ParsedIcu4cFilterFile::parse_filter_node(&unsupported_exclude).is_err());

        let posix_exclude: RawFilterNode =
            serde_json::from_str(r#"{"excludelist": ["en_US_POSIX"]}"#).unwrap();
        assert_eq!(
            ParsedIcu4cFilterFile::parse_filter_node(&posix_exclude).unwrap(),
            ResolvedLocaleFilter::Full
        );
    }
}
