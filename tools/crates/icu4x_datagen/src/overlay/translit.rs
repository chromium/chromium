// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Datagen-time compilation and dependency validation for `[transliterators]`
//! (`include` and `[[transliterators.custom]]` in `config.toml`).

use super::OverlayDataProvider;
use crate::config::{canonicalize_uts35_alias, TransliteratorsConfig};
use anyhow::{bail, Context, Result};
use icu_experimental::transliterate::provider::{RuleBasedTransliterator, TransliteratorRulesV1};
use icu_experimental::transliterate::{RuleCollection, Transliterator};
use icu_locale_core::Locale;
use icu_provider::prelude::*;
use icu_provider_source::SourceDataProvider;
use std::collections::{BTreeSet, HashSet};
use std::path::Path;

/// Standard CLDR legacy transform aliases registered on `RuleCollection` so
/// custom UTS #35 rule strings (e.g. `::Latin-ASCII;`) resolve to their
/// upstream `TransliteratorRulesV1` BCP-47-T attribute IDs.
///
/// Note: BCP-47-T orders subtags `<target>-t-<source>`, whereas UTS #35 / ICU
/// legacy IDs order `<source>-<target>` (`Hira-Kana` = `Hiragana-Katakana` =
/// `und-kana-t-und-hira`; `Kana-Hira` = `Katakana-Hiragana` =
/// `und-hira-t-und-kana`).
///
/// TODO(https://github.com/unicode-org/icu4x/issues/8739): Remove this table
/// once `icu_provider_source` / `icu_experimental` exposes an API to layer
/// custom rules on top of `SourceDataProvider`'s full CLDR `RuleCollection`
/// alias map.
pub(crate) const STANDARD_CLDR_TRANSLITERATOR_ALIASES: &[(&str, &[&str])] = &[
    ("und-t-und-latn-d0-ascii", &["Latin-ASCII"]),
    ("und-hira-t-und-kana", &["Katakana-Hiragana", "Kana-Hira"]),
    ("und-kana-t-und-hira", &["Hiragana-Katakana", "Hira-Kana"]),
];

/// Built-in hardcoded special transform IDs emitted by
/// `icu_experimental::transliterate`'s `Pass2::compile_single_id` that execute
/// via `icu_normalizer` / `icu_casemap` rather than `TransliteratorRulesV1`
/// payloads.
///
/// Must be kept in sync on ICU4X rolls when upstream adds new built-in
/// transforms (tracked in https://github.com/unicode-org/icu4x/issues/3909 and
/// https://github.com/unicode-org/icu4x/issues/3910, and
/// https://github.com/unicode-org/icu4x/issues/8739).
pub(crate) const BUILTIN_HARDCODED_TRANSLITERATOR_IDS: &[&str] = &[
    "any-nfc",
    "any-nfkc",
    "any-nfd",
    "any-nfkd",
    "any-null",
    "any-remove",
    "any-lower",
    "any-upper",
    "any-hex/unicode",
    "any-hex/rust",
    "any-hex/xml",
    "any-hex/perl",
    "any-hex/plain",
];

fn validate_rbt_dependencies(
    owner_id: &str,
    rbt: &RuleBasedTransliterator<'_>,
    upstream_ids: &BTreeSet<String>,
    visible_custom_ids: &HashSet<&str>,
    all_custom_ids: &HashSet<&str>,
    allowed_set: Option<&HashSet<&str>>,
) -> Result<()> {
    for dep in rbt.deps() {
        if BUILTIN_HARDCODED_TRANSLITERATOR_IDS.contains(&dep.as_ref()) {
            continue;
        }
        if let Some(unknown) = dep.strip_prefix("x-") {
            bail!(
                "Transliterator {owner_id:?} references unknown transliterator {unknown:?} \
                 (register an upstream alias in `STANDARD_CLDR_TRANSLITERATOR_ALIASES` or a \
                 custom alias under `[[transliterators.custom]]` `aliases`)"
            );
        }
        let dep_attr = dep.to_ascii_lowercase();
        if !visible_custom_ids.contains(dep_attr.as_str())
            && all_custom_ids.contains(dep_attr.as_str())
        {
            bail!(
                "Custom transliterator {owner_id:?} cannot reference {dep_attr:?} (self-reference \
                 or forward reference to a custom transliterator declared later in `config.toml`; \
                 cycles are not permitted)"
            );
        }
        anyhow::ensure!(
            upstream_ids.contains(&dep_attr) || visible_custom_ids.contains(dep_attr.as_str()),
            "Transliterator {owner_id:?} depends on {dep_attr:?}, which does not exist in \
             upstream or custom `TransliteratorRulesV1` data"
        );
        if let Some(allowed) = allowed_set {
            anyhow::ensure!(
                allowed.contains(dep_attr.as_str()),
                "Transliterator {owner_id:?} depends on {dep_attr:?}, which is missing from \
                 `transliterators.include`"
            );
        }
    }
    Ok(())
}

/// Validate `translit_cfg.include` against `SourceDataProvider` + overlay IDs
/// (when `check_include` is `true`), compile any `translit_cfg.custom` UTS #35
/// rule sets into `<overlay_dir>/transliterator/rules/v1/<bcp47_id>/und.json`,
/// and verify that all dependencies resolve and instantiate cleanly.
pub(crate) fn validate_and_compile_transliterators(
    provider: &SourceDataProvider,
    translit_cfg: &TransliteratorsConfig,
    src_root: &Path,
    check_include: bool,
    overlay_dir: Option<&Path>,
) -> Result<()> {
    let has_checked_include = check_include && translit_cfg.include.is_some();
    if !has_checked_include && translit_cfg.custom.is_empty() {
        return Ok(());
    }

    let overlay_provider =
        OverlayDataProvider::new(provider.clone(), overlay_dir.map(Path::to_path_buf));

    let upstream_ids: BTreeSet<String> =
        IterableDataProvider::<TransliteratorRulesV1>::iter_ids(&overlay_provider)
            .context("Failed to list upstream TransliteratorRulesV1 IDs")?
            .into_iter()
            .map(|id| id.marker_attributes.as_str().to_string())
            .collect();

    if check_include {
        if let Some(ref include) = translit_cfg.include {
            for id in include {
                anyhow::ensure!(
                    upstream_ids.contains(id),
                    "`transliterators.include` entry {id:?} does not exist in upstream \
                     `TransliteratorRulesV1` data"
                );
            }
        }
    }

    let all_custom_ids: HashSet<&str> =
        translit_cfg.custom.iter().map(|c| c.bcp47_id.as_str()).collect();
    let allowed_set: Option<HashSet<&str>> = if check_include {
        translit_cfg.include.as_ref().map(|inc| {
            inc.iter().map(String::as_str).chain(all_custom_ids.iter().copied()).collect()
        })
    } else {
        None
    };

    if !translit_cfg.custom.is_empty() {
        let overlay_dir = overlay_dir.context(
            "Internal error: overlay directory required when `transliterators.custom` is non-empty",
        )?;

        let mut collection = RuleCollection::default();
        for &(bcp47_id, aliases) in STANDARD_CLDR_TRANSLITERATOR_ALIASES {
            anyhow::ensure!(
                upstream_ids.contains(bcp47_id),
                "Expected standard CLDR transliterator {bcp47_id:?} in upstream data"
            );
            let loc: Locale = bcp47_id.parse().expect("valid standard CLDR locale");
            let canonical_aliases: Vec<String> = aliases
                .iter()
                .map(|a| {
                    canonicalize_uts35_alias(a)
                        .expect("built-in CLDR transliterator alias must be valid UTS #35")
                })
                .collect();
            collection.register_aliases(&loc, canonical_aliases.iter().map(String::as_str));
        }

        let loaded_rules: Vec<String> = translit_cfg
            .custom
            .iter()
            .map(|custom| custom.load_rules(src_root))
            .collect::<Result<Vec<_>>>()?;

        for (custom, rule_source) in translit_cfg.custom.iter().zip(loaded_rules.iter()) {
            anyhow::ensure!(
                !upstream_ids.contains(&custom.bcp47_id),
                "Custom transliterator {:?} collides with an existing upstream \
                 `TransliteratorRulesV1` ID",
                custom.bcp47_id
            );
            let loc: Locale = custom.bcp47_id.parse()?;
            let canonical_aliases: Vec<String> = custom
                .aliases
                .iter()
                .map(|alias| canonicalize_uts35_alias(alias))
                .collect::<Result<Vec<_>>>()?;
            collection.register_source(
                &loc,
                rule_source,
                canonical_aliases.iter().map(String::as_str),
                false,
                true,
            );
        }

        let rule_provider = collection
            .as_provider_unstable(&overlay_provider, &overlay_provider, &overlay_provider)
            .context("Failed to initialize RuleCollectionProvider")?;

        let marker_dir = overlay_dir.join("transliterator/rules/v1");
        let mut prior_custom_ids: HashSet<&str> = HashSet::new();
        for custom in &translit_cfg.custom {
            let attrs = DataMarkerAttributes::try_from_str(&custom.bcp47_id)
                .map_err(|e| anyhow::anyhow!("Invalid attributes {:?}: {e:?}", custom.bcp47_id))?;
            let resp = DataProvider::<TransliteratorRulesV1>::load(
                &rule_provider,
                DataRequest {
                    id: DataIdentifierBorrowed::for_marker_attributes(attrs),
                    ..Default::default()
                },
            )
            .with_context(|| {
                format!("Failed to compile custom transliterator {:?}", custom.bcp47_id)
            })?;

            validate_rbt_dependencies(
                &custom.bcp47_id,
                resp.payload.get(),
                &upstream_ids,
                &prior_custom_ids,
                &all_custom_ids,
                allowed_set.as_ref(),
            )?;
            prior_custom_ids.insert(custom.bcp47_id.as_str());

            let dst_file = marker_dir.join(&custom.bcp47_id).join("und.json");
            anyhow::ensure!(
                !dst_file.exists(),
                "Custom transliterator {:?} collides with patched overlay file {}",
                custom.bcp47_id,
                dst_file.display()
            );
            std::fs::create_dir_all(dst_file.parent().expect("und.json has parent"))?;
            let json_bytes = serde_json::to_vec_pretty(resp.payload.get())?;
            std::fs::write(&dst_file, json_bytes)?;
        }
    }

    // Verify transitive dependency closure for `transliterators.include` and
    // end-to-end runtime instantiation via `OverlayDataProvider` for all
    // `transliterators.custom` entries.
    if check_include {
        if let Some(ref include) = translit_cfg.include {
            for id in include {
                let attrs = DataMarkerAttributes::try_from_str(id)
                    .map_err(|e| anyhow::anyhow!("Invalid attributes {id:?}: {e:?}"))?;
                let resp = DataProvider::<TransliteratorRulesV1>::load(
                    &overlay_provider,
                    DataRequest {
                        id: DataIdentifierBorrowed::for_marker_attributes(attrs),
                        ..Default::default()
                    },
                )
                .with_context(|| format!("Failed to load included transliterator {id:?}"))?;
                validate_rbt_dependencies(
                    id,
                    resp.payload.get(),
                    &upstream_ids,
                    &all_custom_ids,
                    &all_custom_ids,
                    allowed_set.as_ref(),
                )?;
            }
        }
    }

    for custom in &translit_cfg.custom {
        let loc: Locale = custom.bcp47_id.parse()?;
        Transliterator::try_new_unstable(
            &overlay_provider,
            &overlay_provider,
            &overlay_provider,
            &loc,
        )
        .with_context(|| {
            format!("Failed to instantiate compiled custom transliterator {:?}", custom.bcp47_id)
        })?;
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::config::DatagenTomlConfig;
    use crate::filters::export_platform_filtered;
    use crate::overlay::TempDir;
    use crate::{all_registered_markers, select_markers_by_args, Icu4xPlatform, SourceTags};
    use icu_locale_fallback::LocaleFallbacker;
    use icu_provider_export::baked_exporter::{BakedExporter, Options as BakedOptions};

    #[test]
    fn test_compile_and_validate_custom_transliterators_and_errors() {
        let provider = SourceTags::default().source_data_provider();
        let all_markers = all_registered_markers();

        let src_dir = TempDir::new("icu4x_translit_src").unwrap();

        // 1. Unknown upstream ID in `transliterators.include` fails loudly.
        let bad_inc: DatagenTomlConfig = toml::from_str(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-missing"]
            "#,
        )
        .unwrap();
        bad_inc.validate(&all_markers).unwrap();
        let err = validate_and_compile_transliterators(
            &provider,
            &bad_inc.transliterators,
            src_dir.path(),
            true,
            None,
        )
        .unwrap_err()
        .to_string();
        assert!(err.contains("und-t-und-latn-d0-missing"), "Error was: {err}");

        // 2. Included upstream transliterator (`de-t-de-d0-ascii`) missing its
        //    transitive dependency (`und-t-und-latn-d0-ascii`) in `include`
        //    fails!
        let missing_transitive: DatagenTomlConfig = toml::from_str(
            r#"
            [transliterators]
            include = ["de-t-de-d0-ascii"]
            "#,
        )
        .unwrap();
        missing_transitive.validate(&all_markers).unwrap();
        let err = validate_and_compile_transliterators(
            &provider,
            &missing_transitive.transliterators,
            src_dir.path(),
            true,
            None,
        )
        .unwrap_err()
        .to_string();
        assert!(err.contains("und-t-und-latn-d0-ascii"), "Error was: {err}");

        // 3. Invalid UTS #35 syntax in custom rule fails at datagen time.
        let bad_syntax: DatagenTomlConfig = toml::from_str(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-badsyn"
            rules = "[unclosed > a;"
            "#,
        )
        .unwrap();
        bad_syntax.validate(&all_markers).unwrap();
        let tmp = TempDir::new("icu4x_translit_badsyn").unwrap();
        assert!(validate_and_compile_transliterators(
            &provider,
            &bad_syntax.transliterators,
            src_dir.path(),
            true,
            Some(tmp.path())
        )
        .is_err());

        // 4. Unknown transform call `::NoSuchTranslit;` (`x-...`) fails loudly.
        let bad_dep: DatagenTomlConfig = toml::from_str(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-baddep"
            rules = "::NoSuchTranslit;"
            "#,
        )
        .unwrap();
        bad_dep.validate(&all_markers).unwrap();
        let tmp = TempDir::new("icu4x_translit_baddep").unwrap();
        let err = validate_and_compile_transliterators(
            &provider,
            &bad_dep.transliterators,
            src_dir.path(),
            true,
            Some(tmp.path()),
        )
        .unwrap_err()
        .to_string();
        assert!(err.contains("nosuchtranslit"), "Error was: {err}");

        // 5. Custom rule referencing `::Latin-ASCII;` when
        //    `transliterators.include` omits `"und-t-und-latn-d0-ascii"` fails
        //    loudly!
        let missing_dep: DatagenTomlConfig = toml::from_str(
            r#"
            [transliterators]
            include = ["und-hira-t-und-kana"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-needlatn"
            rules = "::NFD; ::Latin-ASCII;"
            "#,
        )
        .unwrap();
        missing_dep.validate(&all_markers).unwrap();
        let tmp = TempDir::new("icu4x_translit_missingdep").unwrap();
        let err = validate_and_compile_transliterators(
            &provider,
            &missing_dep.transliterators,
            src_dir.path(),
            true,
            Some(tmp.path()),
        )
        .unwrap_err()
        .to_string();
        assert!(err.contains("und-t-und-latn-d0-ascii"), "Error was: {err}");

        // 6. Self-referencing (`A -> A`) and forward/cyclic (`A -> B -> A`)
        //    custom transliterator references fail loudly.
        let self_rec: DatagenTomlConfig = toml::from_str(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-selfrec"
            aliases = ["SelfRec"]
            rules = "::SelfRec;"
            "#,
        )
        .unwrap();
        self_rec.validate(&all_markers).unwrap();
        let tmp = TempDir::new("icu4x_translit_selfrec").unwrap();
        let err = validate_and_compile_transliterators(
            &provider,
            &self_rec.transliterators,
            src_dir.path(),
            true,
            Some(tmp.path()),
        )
        .unwrap_err()
        .to_string();
        assert!(err.contains("cycles are not permitted"), "Error was: {err}");

        let mutual_rec: DatagenTomlConfig = toml::from_str(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-first"
            aliases = ["FirstRule"]
            rules = "::SecondRule;"

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-second"
            aliases = ["SecondRule"]
            rules = "::FirstRule;"
            "#,
        )
        .unwrap();
        mutual_rec.validate(&all_markers).unwrap();
        let tmp = TempDir::new("icu4x_translit_mutualrec").unwrap();
        let err = validate_and_compile_transliterators(
            &provider,
            &mutual_rec.transliterators,
            src_dir.path(),
            true,
            Some(tmp.path()),
        )
        .unwrap_err()
        .to_string();
        assert!(err.contains("cycles are not permitted"), "Error was: {err}");

        // 7. Missing or comment-only `rules_file` fails loudly.
        let missing_file: DatagenTomlConfig = toml::from_str(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-nofile"
            rules_file = "nonexistent/rules.txt"
            "#,
        )
        .unwrap();
        missing_file.validate(&all_markers).unwrap();
        let tmp = TempDir::new("icu4x_translit_nofile").unwrap();
        assert!(validate_and_compile_transliterators(
            &provider,
            &missing_file.transliterators,
            src_dir.path(),
            true,
            Some(tmp.path()),
        )
        .is_err());

        std::fs::write(src_dir.path().join("comments_only.txt"), "# only comments\n  \n").unwrap();
        let comments_only: DatagenTomlConfig = toml::from_str(
            r#"
            [transliterators]
            include = ["und-t-und-latn-d0-ascii"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-cmtonly"
            rules_file = "comments_only.txt"
            "#,
        )
        .unwrap();
        comments_only.validate(&all_markers).unwrap();
        assert!(validate_and_compile_transliterators(
            &provider,
            &comments_only.transliterators,
            src_dir.path(),
            true,
            Some(tmp.path()),
        )
        .is_err());
    }

    #[test]
    fn test_decoupled_transliterator_export_when_icu4c_excludes_translit() {
        let provider = SourceTags::default().source_data_provider();
        let fallbacker = LocaleFallbacker::try_new_unstable(&provider).unwrap();
        let all_markers = all_registered_markers();

        // Simulate ICU4C filter files AFTER stripping ICU4C's `translit` data
        // (`"translit": "exclude"` and zero `resourceFilters` for `translit`)
        // on both a browser/Cast platform (`common.json` with `localeFilter`)
        // and an excluded-locale platform (`flutter.json` with `localeFilter:
        // {}`).
        let filters_dir = TempDir::new("icu4x_translit_filters").unwrap();
        std::fs::write(
            filters_dir.path().join("common.json"),
            r#"{
                "localeFilter": {
                    "filterType": "language",
                    "includelist": ["en"]
                },
                "featureFilters": {
                    "translit": "exclude"
                }
            }"#,
        )
        .unwrap();
        std::fs::write(
            filters_dir.path().join("flutter.json"),
            r#"{
                "localeFilter": {},
                "featureFilters": {
                    "translit": "exclude"
                }
            }"#,
        )
        .unwrap();

        let src_dir = TempDir::new("icu4x_translit_export_src").unwrap();
        std::fs::create_dir_all(src_dir.path().join("transliterators")).unwrap();
        std::fs::write(
            src_dir.path().join("transliterators/ger.txt"),
            "# Multi-line German address rules\n\
             [ö {o\u{0308}} Ö {O\u{0308}}] → oe;\n\
             ::CustomBase;\n",
        )
        .unwrap();

        let toml_cfg: DatagenTomlConfig = toml::from_str(
            r#"
            [[rules]]
            pattern = "transliterator/*"
            icu4c_category = "locales_tree"

            [transliterators]
            include = ["und-t-und-latn-d0-ascii", "und-hira-t-und-kana", "und-kana-t-und-hira"]

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-base"
            aliases = ["CustomBase"]
            rules = "::NFD; ::[:Nonspacing Mark:] Remove; ::Lower; ::NFC; ::Latin-ASCII;"

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-ger"
            rules_file = "transliterators/ger.txt"

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-hkcheck"
            rules = "::Hira-Kana;"

            [[transliterators.custom]]
            bcp47_id = "und-t-und-x0-khcheck"
            rules = "::Kana-Hira;"
            "#,
        )
        .unwrap();
        toml_cfg.validate(&all_markers).unwrap();

        let overlay_dir = TempDir::new("icu4x_translit_overlay").unwrap();
        validate_and_compile_transliterators(
            &provider,
            &toml_cfg.transliterators,
            src_dir.path(),
            true,
            Some(overlay_dir.path()),
        )
        .unwrap();

        // Verify runtime behavior directly on the compiled transliterators
        // (including `Hira-Kana` = Hiragana->Katakana and `Kana-Hira` =
        // Katakana->Hiragana directionality).
        let check_overlay =
            OverlayDataProvider::new(provider.clone(), Some(overlay_dir.path().to_path_buf()));
        let de_translit = Transliterator::try_new_unstable(
            &check_overlay,
            &check_overlay,
            &check_overlay,
            &"und-t-und-x0-ger".parse().unwrap(),
        )
        .unwrap();
        assert_eq!(de_translit.transliterate("Größe Äpfel Ł".to_string()), "groesse apfel l");

        let hk_translit = Transliterator::try_new_unstable(
            &check_overlay,
            &check_overlay,
            &check_overlay,
            &"und-t-und-x0-hkcheck".parse().unwrap(),
        )
        .unwrap();
        assert_eq!(hk_translit.transliterate("あめりか".to_string()), "アメリカ");

        let kh_translit = Transliterator::try_new_unstable(
            &check_overlay,
            &check_overlay,
            &check_overlay,
            &"und-t-und-x0-khcheck".parse().unwrap(),
        )
        .unwrap();
        assert_eq!(kh_translit.transliterate("アメリカ".to_string()), "あめりか");

        // Export `transliterator/` for both `common` and `flutter` and verify
        // `common` contains only the 3 included + 4 custom transliterators
        // while `flutter` excludes all transliterator payloads (`0 data
        // identifiers`).
        let selected = select_markers_by_args(&["transliterator/".to_string()]).unwrap();
        let out_dir = TempDir::new("icu4x_translit_baked_out").unwrap();

        let mut opts_common = BakedOptions::default();
        opts_common.use_separate_crates = false;
        opts_common.overwrite = true;
        let common_out = out_dir.path().join("common");
        export_platform_filtered(
            Icu4xPlatform::Common,
            filters_dir.path(),
            &toml_cfg,
            OverlayDataProvider::new(provider.clone(), Some(overlay_dir.path().to_path_buf())),
            fallbacker.clone(),
            &selected,
            BakedExporter::new(common_out.clone(), opts_common).unwrap(),
        )
        .unwrap();
        let common_baked =
            std::fs::read_to_string(common_out.join("transliterator_rules_v1.rs.data")).unwrap();
        assert!(common_baked.contains("TRANSLITERATOR_RULES_V1_UND_UND_T_UND_LATN_D0_ASCII"));
        assert!(common_baked.contains("TRANSLITERATOR_RULES_V1_UND_UND_T_UND_X0_BASE"));
        assert!(common_baked.contains("TRANSLITERATOR_RULES_V1_UND_UND_T_UND_X0_GER"));
        assert!(!common_baked.contains("TRANSLITERATOR_RULES_V1_UND_DE_T_DE_D0_ASCII"));

        let mut opts_flutter = BakedOptions::default();
        opts_flutter.use_separate_crates = false;
        opts_flutter.overwrite = true;
        let flutter_out = out_dir.path().join("flutter");
        export_platform_filtered(
            Icu4xPlatform::Flutter,
            filters_dir.path(),
            &toml_cfg,
            OverlayDataProvider::new(provider, Some(overlay_dir.path().to_path_buf())),
            fallbacker,
            &selected,
            BakedExporter::new(flutter_out.clone(), opts_flutter).unwrap(),
        )
        .unwrap();
        let flutter_baked =
            std::fs::read_to_string(flutter_out.join("transliterator_rules_v1.rs.data")).unwrap();
        assert!(!flutter_baked.contains("TRANSLITERATOR_RULES_V1_UND_"));
    }
}
