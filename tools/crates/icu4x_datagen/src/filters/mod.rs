// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! ICU4C filter file (`//third_party/icu/filters/*.json`) parsing, default
//! ICU4X-marker-to-ICU4C category mapping, and filtered data provider/exporter
//! wrappers.

mod icu4c;
mod provider;

pub(crate) use icu4c::{Icu4cFilterCategory, ParsedIcu4cFilterFile, ResolvedLocaleFilter};
pub(crate) use provider::FilteredDataProvider;
use provider::NonClosingExporter;

use crate::config::{resolve_marker_filter, DatagenTomlConfig};
use crate::{marker_path_str, Icu4xPlatform, MarkerEntry};
use anyhow::Result;
use icu_locale_fallback::LocaleFallbacker;
use icu_provider::export::ExportableProvider;
use icu_provider::prelude::*;
use icu_provider_export::baked_exporter::BakedExporter;
use icu_provider_export::prelude::*;
use std::collections::{BTreeMap, HashSet};
use std::path::{Path, PathBuf};

/// Map an ICU4X `DataMarkerInfo` to its corresponding ICU4C `featureFilters` /
/// `resourceFilters` category in `//third_party/icu/filters/*.json`.
///
/// We do not need to add branches here for ICU4C `featureFilters` that
/// correspond to singleton ICU4X markers (markers with no locale dependence).
/// Those markers have `info.is_singleton == true` and return
/// `Icu4cFilterCategory::Singleton` immediately: `BakedExporter` emits
/// singleton markers as zero-cost `macro_rules! impl_<marker>` definitions that
/// are only compiled into the binary if a Rust caller actually instantiates
/// that marker's API, so Rust unused-macro / dead-code elimination handles them
/// automatically without per-platform filter exclusion.
fn default_category_for_marker(info: DataMarkerInfo) -> Icu4cFilterCategory {
    if info.is_singleton {
        return Icu4cFilterCategory::Singleton;
    }
    let id = marker_path_str(info.id);
    if id.starts_with("segmenter/dictionary/") || id.starts_with("segmenter/lstm/") {
        Icu4cFilterCategory::SegmenterModels
    } else if id.starts_with("segmenter/break/") {
        Icu4cFilterCategory::BrkitrTree
    } else if id.starts_with("currency/") {
        Icu4cFilterCategory::CurrTree
    } else if id.starts_with("timezone/") || id.starts_with("time_zone/") {
        Icu4cFilterCategory::ZoneTree
    } else if id.starts_with("locale/names/region/") {
        Icu4cFilterCategory::RegionTree
    } else if id.starts_with("locale/names/") {
        Icu4cFilterCategory::LangTree
    } else if id.starts_with("units/") {
        Icu4cFilterCategory::UnitTree
    } else if id.starts_with("collation/") || id.starts_with("collator/") {
        Icu4cFilterCategory::CollTree
    } else if id.starts_with("transliterator/") {
        Icu4cFilterCategory::Translit
    } else if id.starts_with("plurals/") {
        Icu4cFilterCategory::Plurals
    } else {
        Icu4cFilterCategory::LocalesTree
    }
}

/// Resolve the default ICU4C filter category for `info` without TOML rule
/// overrides.
pub(crate) fn resolve_default_marker_filter(
    info: DataMarkerInfo,
    filter_file: &ParsedIcu4cFilterFile,
    abridge_non_ui_locales: bool,
) -> Result<ResolvedLocaleFilter> {
    if info.is_singleton {
        return Ok(ResolvedLocaleFilter::Singleton);
    }
    let cat = default_category_for_marker(info);
    // For `LocaleNamesLanguage*V1` (`attributes_domain ==
    // "locale_names_language"`), keep the non-UI Accept-Languages as `^locale`
    // (`without_descendants`) in the locale list because
    // `FilteredDataProvider::iter_ids` restricts non-UI Accept-Languages to
    // their single autonym (`id.marker_attributes == id.locale`), matching
    // `{"categories":["lang_tree"],"files":{"includelist":["ak"]},"rules":["-/*
    // ","+/Languages/ak"]}` in `common.json`.
    let autonym_only = abridge_non_ui_locales && info.attributes_domain == "locale_names_language";
    filter_file.resolve_category(cat, abridge_non_ui_locales, autonym_only)
}

/// Print `selected` markers grouped by their default ICU4C filter category.
pub(crate) fn print_markers_by_category(selected: &[MarkerEntry]) {
    let mut by_cat: BTreeMap<&'static str, Vec<MarkerEntry>> = BTreeMap::new();
    for &m in selected {
        let cat = default_category_for_marker(m.info).as_str();
        by_cat.entry(cat).or_default().push(m);
    }
    for (cat, list) in &by_cat {
        println!("=== Category: {cat} ({} markers) ===", list.len());
        for m in list {
            println!(
                "  {:42} -> {:45} (singleton={}, unstable={})",
                m.name,
                marker_path_str(m.info.id),
                m.info.is_singleton,
                m.is_unstable
            );
        }
    }
}

/// Parse all `*.json` filter files in `icu_filters_dir` and print a summary of
/// how each ICU4C filter category resolves per platform.
pub(crate) fn inspect_filters(icu_filters_dir: &Path) -> Result<()> {
    let mut paths: Vec<PathBuf> = std::fs::read_dir(icu_filters_dir)?
        .filter_map(|e| e.ok().map(|e| e.path()))
        .filter(|p| p.extension().is_some_and(|ext| ext == "json"))
        .collect();
    paths.sort();

    for path in paths {
        let parsed = ParsedIcu4cFilterFile::load(&path)?;
        println!("=== Platform: {} ===", parsed.platform);
        println!("  segmenter_models: {:?}", parsed.segmenter_models);
        for cat in <Icu4cFilterCategory as strum::IntoEnumIterator>::iter()
            .filter(|&c| c != Icu4cFilterCategory::Singleton)
        {
            let res = parsed.resolve_category(cat, true, false)?;
            let summary = match &res {
                ResolvedLocaleFilter::Singleton => "singleton".to_string(),
                ResolvedLocaleFilter::Exclude => "exclude (0 locales)".to_string(),
                ResolvedLocaleFilter::Full => "full".to_string(),
                ResolvedLocaleFilter::Families(fams) => {
                    format!("{} locale families", fams.len())
                }
            };
            println!("  {:22} -> {}", cat.as_str(), summary);
        }
    }
    Ok(())
}

/// Export `selected_markers` for `plat` into `baked_exporter` using the
/// platform's ICU4C filter file and `toml_cfg` overrides.
pub(crate) fn export_platform_filtered<P>(
    plat: Icu4xPlatform,
    icu_filters_dir: &Path,
    toml_cfg: &DatagenTomlConfig,
    provider: P,
    fallbacker: LocaleFallbacker,
    selected_markers: &[MarkerEntry],
    mut baked_exporter: BakedExporter,
) -> Result<()>
where
    FilteredDataProvider<P>: ExportableProvider,
{
    let filter_path = icu_filters_dir.join(format!("{plat}.json"));
    let filter_file = ParsedIcu4cFilterFile::load(&filter_path)?;
    let effective_abridge_non_ui = toml_cfg.resource_filters.abridge_non_ui_locales;

    let exportable = FilteredDataProvider {
        inner: provider,
        allowed_transliterators: if toml_cfg.resource_filters.filter_transliterators {
            filter_file.allowed_transliterators()
        } else {
            None
        },
        allowed_currencies: if toml_cfg.resource_filters.filter_currencies {
            filter_file.allowed_currencies()
        } else {
            None
        },
        allowed_target_languages: if toml_cfg.resource_filters.filter_target_languages {
            filter_file.allowed_target_languages()
        } else {
            None
        },
        allowed_target_variants: if toml_cfg.resource_filters.filter_target_variants {
            filter_file.allowed_target_variants()
        } else {
            None
        },
        abridged_non_ui_locales: if effective_abridge_non_ui {
            filter_file.abridged_non_ui_locales_for(Icu4cFilterCategory::LangTree)
        } else {
            HashSet::new()
        },
    };

    // Group selected markers by their resolved locale filter
    let mut groups: BTreeMap<ResolvedLocaleFilter, Vec<DataMarkerInfo>> = BTreeMap::new();
    for m in selected_markers {
        let resolved =
            resolve_marker_filter(m.info, &filter_file, toml_cfg, effective_abridge_non_ui)?;
        groups.entry(resolved).or_default().push(m.info);
    }

    log::info!(
        "Platform {plat}: exporting {} markers across {} locale-filter groups",
        selected_markers.len(),
        groups.len()
    );

    for (res_filter, group_markers) in groups {
        let families = res_filter.to_locale_families()?;
        let driver =
            ExportDriver::new(families, DeduplicationStrategy::Maximal.into(), fallbacker.clone())
                .with_markers(group_markers)
                .with_segmenter_models(filter_file.segmenter_models.clone());

        driver.export(&exportable, NonClosingExporter(&baked_exporter))?;
    }

    baked_exporter.close()?;
    Ok(())
}
