// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Chromium ICU4X data generator tool (`//tools/crates/icu4x_datagen`).
//!
//! Generates per-platform baked ICU4X data in `//third_party/rust/icu4x_data`
//! using upstream CLDR / ICU / Unicode RSCD / TZDB / segmenter LSTM tags,
//! ICU4C filter files (`//third_party/icu/filters/*.json`), standalone TOML
//! marker-pattern configuration, and unified `ICU4X-json` patches.

extern crate alloc;

mod config;
mod dump_json;
mod filters;
mod overlay;

use crate::config::DatagenTomlConfig;
use crate::dump_json::DumpJsonArgs;
use crate::filters::FilteredDataProvider;
use crate::overlay::{prepare_patched_overlay, OverlayDataProvider};
use anyhow::{Context, Result};
use clap::{Args, Parser, Subcommand, ValueEnum};
use icu_locale_fallback::LocaleFallbacker;
use icu_provider::prelude::*;
use icu_provider_export::baked_exporter::{BakedExporter, Options as BakedOptions};
use icu_provider_export::prelude::*;
use icu_provider_source::SourceDataProvider;
use std::collections::{BTreeSet, HashMap};
use std::path::{Path, PathBuf};

/// Default upstream source tags tested with the current `icu_provider_source`
/// crate.
const DEFAULT_CLDR_TAG: &str = SourceDataProvider::TESTED_CLDR_TAG;
const DEFAULT_ICUEXPORT_TAG: &str = SourceDataProvider::TESTED_ICUEXPORT_TAG;
const DEFAULT_UNICODE_TAG: &str = SourceDataProvider::TESTED_UNICODE_TAG;
const DEFAULT_TZDB_TAG: &str = SourceDataProvider::TESTED_TZDB_TAG;
const DEFAULT_SEGMENTER_LSTM_TAG: &str = SourceDataProvider::TESTED_SEGMENTER_LSTM_TAG;

/// Upstream data tag overrides shared by `generate` and `dump-json`.
#[derive(Args, Clone)]
pub(crate) struct SourceTags {
    /// Override CLDR tag (defaults to `DEFAULT_CLDR_TAG`).
    #[arg(long, default_value = DEFAULT_CLDR_TAG)]
    pub(crate) cldr_tag: String,

    /// Override icuexportdata tag (defaults to `DEFAULT_ICUEXPORT_TAG`).
    #[arg(long, default_value = DEFAULT_ICUEXPORT_TAG)]
    pub(crate) icuexport_tag: String,

    /// Override Unicode RSCD tag (defaults to `DEFAULT_UNICODE_TAG`).
    #[arg(long, default_value = DEFAULT_UNICODE_TAG)]
    pub(crate) unicode_tag: String,

    /// Override TZDB tag (defaults to `DEFAULT_TZDB_TAG`).
    #[arg(long, default_value = DEFAULT_TZDB_TAG)]
    pub(crate) tzdb_tag: String,

    /// Override segmenter LSTM tag (defaults to
    /// `DEFAULT_SEGMENTER_LSTM_TAG`).
    #[arg(long, default_value = DEFAULT_SEGMENTER_LSTM_TAG)]
    pub(crate) segmenter_lstm_tag: String,
}

impl Default for SourceTags {
    fn default() -> Self {
        Self {
            cldr_tag: DEFAULT_CLDR_TAG.to_string(),
            icuexport_tag: DEFAULT_ICUEXPORT_TAG.to_string(),
            unicode_tag: DEFAULT_UNICODE_TAG.to_string(),
            tzdb_tag: DEFAULT_TZDB_TAG.to_string(),
            segmenter_lstm_tag: DEFAULT_SEGMENTER_LSTM_TAG.to_string(),
        }
    }
}

impl SourceTags {
    pub(crate) fn source_data_provider(&self) -> SourceDataProvider {
        SourceDataProvider::new_custom()
            .with_cldr_for_tag(&self.cldr_tag)
            .with_icuexport_for_tag(&self.icuexport_tag)
            .with_unicode_rscd_for_tag(&self.unicode_tag)
            .with_tzdb_for_tag(&self.tzdb_tag)
            .with_segmenter_lstm_for_tag(&self.segmenter_lstm_tag)
    }
}

/// Target platform bundle corresponding to
/// `//third_party/icu/filters/<platform>.json`.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, ValueEnum)]
#[value(rename_all = "snake_case")]
enum Icu4xPlatform {
    Common,
    Chromeos,
    Android,
    Ios,
    Cast,
    ChromecastVideo,
    Flutter,
    FlutterDesktop,
}

impl Icu4xPlatform {
    const fn as_str(self) -> &'static str {
        match self {
            Self::Common => "common",
            Self::Chromeos => "chromeos",
            Self::Android => "android",
            Self::Ios => "ios",
            Self::Cast => "cast",
            Self::ChromecastVideo => "chromecast_video",
            Self::Flutter => "flutter",
            Self::FlutterDesktop => "flutter_desktop",
        }
    }
}

impl std::fmt::Display for Icu4xPlatform {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.as_str())
    }
}

#[derive(Parser)]
#[command(name = "icu4x_datagen")]
#[command(about = "Chromium ICU4X baked data generator")]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// List registered ICU4X markers and their mapped ICU4C filter
    /// categories.
    ListMarkers {
        /// Only list markers matching the specified marker path prefixes
        /// (defaults to `all`).
        #[arg(long, num_args = 1.., default_value = "all")]
        markers: Vec<String>,
    },
    /// Parse all ICU4C filter JSON files in `third_party/icu/filters` and print
    /// a diagnostic summary of how each ICU4C filter category (`locales_tree`,
    /// `curr_tree`, `lang_tree`, `brkitr_dictionaries`, etc.) resolves per
    /// platform. Useful for debugging filter rules and verifying parity with
    /// ICU4C's `filtration.py`.
    InspectFilters {
        /// Directory containing ICU4C filter JSON files
        /// (`third_party/icu/filters`).
        #[arg(long, default_value = "third_party/icu/filters")]
        icu_filters_dir: PathBuf,
    },
    /// Dump pristine `ICU4X-json` files for specific markers and locales to
    /// inspect or create patches.
    DumpJson(DumpJsonArgs),
    /// Generate `//third_party/rust/icu4x_data` for one or more platforms.
    Generate {
        /// Path to the existing `//third_party/rust/icu4x_data` crate.
        #[arg(long, default_value = "third_party/rust/icu4x_data")]
        icu4x_data_dir: PathBuf,

        /// Directory containing ICU4C filter JSON files
        /// (`third_party/icu/filters`).
        #[arg(long, default_value = "third_party/icu/filters")]
        icu_filters_dir: PathBuf,

        /// Path to the TOML configuration file for marker filter overrides and
        /// default patches (`patches` are still applied when `--unfiltered` is
        /// set, while filter rules and `resource_filters` are ignored).
        #[arg(long, default_value = "tools/crates/icu4x_datagen/config.toml")]
        config: PathBuf,

        /// Platforms to generate (defaults to all ICU4C filter platforms; must
        /// include `common`).
        #[arg(long, num_args = 1..)]
        platforms: Vec<Icu4xPlatform>,

        /// Only export markers matching the specified marker path prefixes
        /// (`chromium` for the default Chromium marker prefixes, `all` for all
        /// registered ICU4X markers, or specific marker path prefixes like
        /// `decimal/` or `locale/names/`).
        #[arg(long, num_args = 1.., default_value = "chromium")]
        markers: Vec<String>,

        /// Ignore ICU4C filter files and `config.toml` filter rules, exporting
        /// upstream CLDR recommended locales (`Modern`, `Moderate`, and `Basic`
        /// coverage levels) for all selected markers while still applying
        /// patches. Useful for measuring the size reduction of Chromium's
        /// locale filters or testing upstream data before updating filter
        /// rules.
        #[arg(long)]
        unfiltered: bool,

        #[command(flatten)]
        source_tags: SourceTags,

        /// Optional additional unified `.patch` files to apply to `ICU4X-json`
        /// payloads (appended after `patches` from `config.toml`).
        #[arg(long, num_args = 1..)]
        patches: Vec<PathBuf>,
    },
}

#[derive(Clone, Copy, Debug)]
struct MarkerEntry {
    name: &'static str,
    info: DataMarkerInfo,
    is_unstable: bool,
}

macro_rules! cb {
    ($($marker_ty:ty:$marker:ident,)+ #[unstable] $($emarker_ty:ty:$emarker:ident,)+) => {
        fn all_registered_markers() -> Vec<MarkerEntry> {
            vec![
                $(
                    MarkerEntry {
                        name: stringify!($marker),
                        info: <$marker_ty>::INFO,
                        is_unstable: false,
                    },
                )+
                $(
                    MarkerEntry {
                        name: stringify!($emarker),
                        info: <$emarker_ty>::INFO,
                        is_unstable: true,
                    },
                )+
            ]
        }

        icu_provider::export::make_exportable_provider!(
            FilteredDataProvider<OverlayDataProvider>,
            [
                $($marker_ty,)+
                $($emarker_ty,)+
            ]
        );
    };
}

icu_provider_registry::registry!(cb);

/// Convert a `DataMarkerId` (e.g. `DatetimeNamesWeekdayV1`) into its
/// slash-separated path string (e.g. `"datetime/names/weekday/v1"`), matching
/// `icu_provider_fs`.
fn marker_path_str(marker: icu_provider::marker::DataMarkerId) -> String {
    let mut parts = Vec::new();
    let mut last = 0;
    let name = marker.name();
    for i in 1..name.len() {
        if name.as_bytes().get(i + 1).is_none_or(|b| b.is_ascii_uppercase()) {
            parts.push(name[last..=i].to_ascii_lowercase());
            last = i + 1;
        }
    }
    parts.join("/")
}

/// Marker path prefixes exported by default for Chromium (corresponding to the
/// `icu_*_data` crates transitively depended on by
/// `//third_party/rust/chromium_crates_io`), as well as data we plan to use
/// shortly.
const CHROMIUM_MARKER_PREFIXES: &[&str] = &[
    "calendar/",
    "case/",
    "currency/",
    "datetime/",
    "decimal/",
    "list/",
    "locale/",
    "normalizer/",
    "person/",
    "plurals/",
    "property/",
    "transliterator/",
    "units/",
];

fn select_markers_by_args(marker_args: &[String]) -> Result<Vec<MarkerEntry>> {
    let all = all_registered_markers();
    let matches_arg = |arg: &str, path: &str, name: &str| match arg {
        "all" => true,
        "chromium" => CHROMIUM_MARKER_PREFIXES.iter().any(|p| path.starts_with(p)),
        prefix => path.starts_with(prefix) || name == prefix,
    };
    for arg in marker_args {
        let matched =
            all.iter().any(|m| matches_arg(arg.as_str(), &marker_path_str(m.info.id), m.name));
        anyhow::ensure!(matched, "No ICU4X markers matched `--markers {arg}`");
    }
    Ok(all
        .into_iter()
        .filter(|m| {
            let path = marker_path_str(m.info.id);
            marker_args.iter().any(|arg| matches_arg(arg.as_str(), &path, m.name))
        })
        .collect())
}

fn main() -> Result<()> {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();
    let cli = Cli::parse();

    match cli.command {
        Commands::ListMarkers { markers } => {
            let selected = select_markers_by_args(&markers)?;
            filters::print_markers_by_category(&selected);
            Ok(())
        }
        Commands::InspectFilters { icu_filters_dir } => filters::inspect_filters(&icu_filters_dir),
        Commands::DumpJson(args) => args.run(),
        Commands::Generate {
            icu4x_data_dir,
            icu_filters_dir,
            config,
            platforms,
            markers: marker_args,
            unfiltered,
            source_tags,
            patches,
        } => {
            for required in ["OWNERS", "BUILD.gn", "sources.gni"] {
                let path = icu4x_data_dir.join(required);
                anyhow::ensure!(
                    path.is_file(),
                    "Expected `{}` to exist; run `icu4x_datagen` from the Chromium `src/` root \
                     or pass a valid `--icu4x-data-dir`",
                    path.display()
                );
            }

            let platform_list = if platforms.is_empty() {
                Icu4xPlatform::value_variants().to_vec()
            } else {
                anyhow::ensure!(
                    platforms.contains(&Icu4xPlatform::Common),
                    "--platforms must include `common`"
                );
                platforms
            };

            log::info!("Loading TOML configuration from {}", config.display());
            let cfg_text = std::fs::read_to_string(&config)
                .with_context(|| format!("Failed to read {}", config.display()))?;
            let toml_cfg: DatagenTomlConfig = toml::from_str(&cfg_text)
                .with_context(|| format!("Failed to parse {}", config.display()))?;
            toml_cfg.validate(&all_registered_markers())?;

            let selected_markers = select_markers_by_args(&marker_args)?;
            log::info!("Selected {} ICU4X markers to generate", selected_markers.len());

            // Configure SourceDataProvider
            let provider = source_tags.source_data_provider();

            let fallbacker = LocaleFallbacker::try_new_unstable(&provider)
                .context("Failed to initialize LocaleFallbacker from SourceDataProvider")?;

            let src_root = chromium_src_root();
            let mut all_patches: Vec<PathBuf> = toml_cfg
                .patches
                .iter()
                .map(|p| if p.is_relative() { src_root.join(p) } else { p.clone() })
                .collect();
            all_patches.extend(patches);

            // Keep the temporary overlay directory alive for the duration of
            // the export across all target platforms.
            let patched_overlay = if !all_patches.is_empty() {
                let work_overlay =
                    prepare_patched_overlay(&provider, fallbacker.clone(), &all_patches)?;
                log::info!(
                    "Using patched ICU4X-json overlay from {}",
                    work_overlay.path().display()
                );
                Some(work_overlay)
            } else {
                None
            };

            let src_dir = icu4x_data_dir.join("src");
            let data_root = src_dir.join("data");
            std::fs::create_dir_all(&data_root)?;

            for plat in &platform_list {
                let plat_dir = data_root.join(plat.as_str());
                if plat_dir.exists() {
                    std::fs::remove_dir_all(&plat_dir)?;
                }
                let mut baked_opts = BakedOptions::default();
                baked_opts.use_separate_crates = false;
                baked_opts.overwrite = true;
                baked_opts.pretty = false;
                let baked_exporter = BakedExporter::new(plat_dir.clone(), baked_opts)
                    .with_context(|| format!("Failed to create BakedExporter for {plat}"))?;

                let overlay_provider = OverlayDataProvider::new(
                    provider.clone(),
                    patched_overlay.as_ref().map(|d| d.path().to_path_buf()),
                );

                if unfiltered {
                    let exportable = FilteredDataProvider::unfiltered(overlay_provider);
                    let rec_locales: Vec<DataLocaleFamily> = provider
                        .locales_for_coverage_levels([
                            icu_provider_source::CoverageLevel::Modern,
                            icu_provider_source::CoverageLevel::Moderate,
                            icu_provider_source::CoverageLevel::Basic,
                        ])?
                        .into_iter()
                        .map(DataLocaleFamily::with_descendants)
                        .collect();
                    let driver = ExportDriver::new(
                        rec_locales,
                        DeduplicationStrategy::Maximal.into(),
                        fallbacker.clone(),
                    )
                    .with_markers(selected_markers.iter().map(|m| m.info));
                    driver.export(&exportable, baked_exporter)?;
                } else {
                    filters::export_platform_filtered(
                        *plat,
                        &icu_filters_dir,
                        &toml_cfg,
                        overlay_provider,
                        fallbacker.clone(),
                        &selected_markers,
                        baked_exporter,
                    )?;
                }
            }

            deduplicate_cross_platform_data(&data_root, &platform_list)?;

            write_crate_files(
                &icu4x_data_dir,
                &platform_list,
                &source_tags.cldr_tag,
                &source_tags.unicode_tag,
                &source_tags.segmenter_lstm_tag,
            )?;

            Ok(())
        }
    }
}

/// Format `<plat>/mod.rs` so each `include!("...");` is on its own line
/// (matching `rustfmt`), and deduplicate `.rs.data` files in non-`common`
/// platform directories that are byte-for-byte identical to
/// `common/<file>.rs.data`, rewriting `include!("<file>.rs.data")` in
/// `<plat>/mod.rs` to `include!("../common/<file>.rs.data")`.
fn deduplicate_cross_platform_data(data_root: &Path, platforms: &[Icu4xPlatform]) -> Result<()> {
    let common_dir = data_root.join(Icu4xPlatform::Common.as_str());
    let mut common_files: HashMap<String, Vec<u8>> = HashMap::new();
    if common_dir.exists() {
        for entry in std::fs::read_dir(&common_dir)? {
            let entry = entry?;
            let path = entry.path();
            if path.extension().is_some_and(|ext| ext == "data") {
                let fname = entry.file_name().to_str().unwrap().to_string();
                common_files.insert(fname, std::fs::read(&path)?);
            }
        }
    }

    for plat in platforms {
        let plat_dir = data_root.join(plat.as_str());
        let mod_rs_path = plat_dir.join("mod.rs");
        if !mod_rs_path.exists() {
            continue;
        }
        let mut mod_rs = std::fs::read_to_string(&mod_rs_path)?;
        // Normalize `BakedExporter` output (`include ! ("...") ; `) into one
        // `include!("...");` per line
        mod_rs = mod_rs
            .replace("include ! (\"", "include!(\"")
            .replace("\") ; ", "\");\n")
            .replace("\") ;", "\");\n");
        if !mod_rs.ends_with('\n') {
            mod_rs.push('\n');
        }

        if *plat == Icu4xPlatform::Common || common_files.is_empty() {
            std::fs::write(&mod_rs_path, mod_rs)?;
            continue;
        }

        let mut deduped_count = 0usize;
        let mut kept_count = 0usize;
        let mut saved_bytes = 0usize;

        for entry in std::fs::read_dir(&plat_dir)? {
            let entry = entry?;
            let path = entry.path();
            if path.extension().is_some_and(|ext| ext == "data") {
                let fname = entry.file_name().to_str().unwrap().to_string();
                let bytes = std::fs::read(&path)?;
                if let Some(common_bytes) = common_files.get(&fname) {
                    if &bytes == common_bytes {
                        std::fs::remove_file(&path)?;
                        deduped_count += 1;
                        saved_bytes += bytes.len();
                    } else {
                        kept_count += 1;
                    }
                } else {
                    kept_count += 1;
                }
            }
        }
        for fname in common_files.keys() {
            if !plat_dir.join(fname).exists() {
                let target_compact = format!("include!(\"{fname}\")");
                let replacement_compact = format!("include!(\"../common/{fname}\")");
                mod_rs = mod_rs.replace(&target_compact, &replacement_compact);
            }
        }
        std::fs::write(&mod_rs_path, mod_rs)?;
        log::info!(
            "Platform {plat}: deduplicated {deduped_count} .rs.data files against common/ ({:.1} KB saved, {kept_count} platform-specific files kept)",
            saved_bytes as f64 / 1024.0
        );
    }

    Ok(())
}

/// Write `src/lib.rs` and `sources.gni` in `//third_party/rust/icu4x_data`.
fn write_crate_files(
    icu4x_data_dir: &Path,
    platforms: &[Icu4xPlatform],
    cldr_tag: &str,
    unicode_tag: &str,
    segmenter_lstm_tag: &str,
) -> Result<()> {
    let src_dir = icu4x_data_dir.join("src");
    std::fs::create_dir_all(&src_dir)?;

    let all_platforms = Icu4xPlatform::value_variants();

    // 1. Write `src/lib.rs`
    let mut lib_rs = String::from(
        r#"// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Pre-built ICU4X baked data for Chromium (`//third_party/rust/icu4x_data`).
//!
//! Generated by `//tools/crates/run_icu4x_datagen.py`.

#![no_std]
#![doc(html_no_source)]
#![allow(unused_imports, clippy::needless_lifetimes, clippy::upper_case_acronyms)]

#[macro_export]
macro_rules! cldr_tag {
    () => {
"#,
    );
    lib_rs.push_str(&format!("        {cldr_tag:?}\n    }};\n}}\n\n"));
    lib_rs.push_str(
        r#"#[macro_export]
macro_rules! unicode_tag {
    () => {
"#,
    );
    lib_rs.push_str(&format!("        {unicode_tag:?}\n    }};\n}}\n\n"));
    lib_rs.push_str(
        r#"#[macro_export]
macro_rules! segmenter_lstm_tag {
    () => {
"#,
    );
    lib_rs.push_str(&format!("        {segmenter_lstm_tag:?}\n    }};\n}}\n\n"));

    let mut cfg_list = Vec::new();
    for plat in all_platforms {
        let cfg_name = format!("icu4x_data_platform_{plat}");
        cfg_list.push(cfg_name.clone());
        lib_rs.push_str(&format!("#[cfg({cfg_name})]\ninclude!(\"data/{plat}/mod.rs\");\n"));
    }
    if !cfg_list.is_empty() {
        lib_rs.push_str("#[cfg(not(any(\n");
        for (i, cfg_name) in cfg_list.iter().enumerate() {
            if i + 1 < cfg_list.len() {
                lib_rs.push_str(&format!("    {cfg_name},\n"));
            } else {
                lib_rs.push_str(&format!("    {cfg_name}\n"));
            }
        }
        lib_rs.push_str(
            ")))]\ncompile_error!(\"No icu4x_data_platform_<platform> cfg flag set\");\n",
        );
    }

    std::fs::write(src_dir.join("lib.rs"), lib_rs)?;

    // 2. Write `sources.gni` listing all `.rs.data` and `mod.rs` files for GN
    //    dependency tracking
    let data_root = src_dir.join("data");
    let mut sources_gni = String::from(
        "# Copyright 2026 The Chromium Authors\n# Generated by //tools/crates/run_icu4x_datagen.py. DO NOT EDIT.\n\n",
    );
    for (i, plat) in all_platforms.iter().enumerate() {
        let plat_dir = data_root.join(plat.as_str());
        let mut files = BTreeSet::new();
        if plat_dir.exists() {
            for entry in std::fs::read_dir(&plat_dir)? {
                let entry = entry?;
                let fname = entry.file_name().to_str().unwrap().to_string();
                files.insert(format!("src/data/{plat}/{fname}"));
            }
            let mod_rs_path = plat_dir.join("mod.rs");
            if mod_rs_path.exists() {
                let mod_rs = std::fs::read_to_string(&mod_rs_path)?;
                for part in mod_rs.split("\"../common/").skip(1) {
                    if let Some((fname, _)) = part.split_once('"') {
                        files.insert(format!("src/data/common/{fname}"));
                    }
                }
            }
        }
        if files.is_empty() {
            sources_gni.push_str(&format!("icu4x_data_{plat}_inputs = []\n"));
        } else {
            sources_gni.push_str(&format!("icu4x_data_{plat}_inputs = [\n"));
            for f in files {
                sources_gni.push_str(&format!("  \"{f}\",\n"));
            }
            sources_gni.push_str("]\n");
        }
        if i + 1 < all_platforms.len() {
            sources_gni.push('\n');
        }
    }
    std::fs::write(icu4x_data_dir.join("sources.gni"), sources_gni)?;

    // 3. Format `src/lib.rs` and `src/data/*/mod.rs` with Chromium's `rustfmt`
    //    if present. Note that `mod.rs` starts with `// @generated`, which
    //    file-based `rustfmt` skips, whereas `git cl format` pipes via stdin.
    let rustfmt = Path::new("third_party/rust-toolchain/bin/rustfmt")
        .with_extension(std::env::consts::EXE_EXTENSION);
    if rustfmt.exists() {
        let mut cmd = std::process::Command::new(&rustfmt);
        if Path::new(".rustfmt.toml").exists() {
            cmd.arg("--config-path=.rustfmt.toml");
        }
        cmd.arg(src_dir.join("lib.rs"));
        let _ = cmd.status();

        for plat in platforms {
            let mod_rs = data_root.join(plat.as_str()).join("mod.rs");
            if mod_rs.exists() {
                let mut fmt_cmd = std::process::Command::new(&rustfmt);
                if Path::new(".rustfmt.toml").exists() {
                    fmt_cmd.arg("--config-path=.rustfmt.toml");
                }
                fmt_cmd
                    .arg("--emit=stdout")
                    .stdin(std::process::Stdio::piped())
                    .stdout(std::process::Stdio::piped());
                if let Ok(mut child) = fmt_cmd.spawn() {
                    if let Some(mut stdin) = child.stdin.take() {
                        use std::io::Write;
                        let content = std::fs::read(&mod_rs)?;
                        let _ = stdin.write_all(&content);
                    }
                    if let Ok(output) = child.wait_with_output() {
                        if output.status.success() && !output.stdout.is_empty() {
                            std::fs::write(&mod_rs, output.stdout)?;
                        }
                    }
                }
            }
        }
    }

    Ok(())
}

pub(crate) fn chromium_src_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .to_path_buf()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_select_markers_by_args_errors_on_unknown() {
        assert!(select_markers_by_args(&["calender".to_string()]).is_err());
        let chromium = select_markers_by_args(&["chromium".to_string()]).unwrap();
        assert!(!chromium.is_empty());
    }

    #[test]
    fn test_cross_platform_deduplication_and_generated_data() {
        let root = chromium_src_root();
        let data_dir = root.join("third_party/rust/icu4x_data/src/data");

        // 1. Verify `google3-overlay.patch` (`RSD` overridden to `digits: 0,
        //    cash_digits: None` -> `\0\xFF` in `currency_fractions_v1.rs.data`)
        let fractions =
            std::fs::read_to_string(data_dir.join("common/currency_fractions_v1.rs.data")).unwrap();
        assert!(fractions.contains("OMRPKRPYGRSDRWF"));
        assert!(
            fractions.contains(r"\x03\xFF\x02\0\0\xFF\0\xFF\0\xFF"),
            "Expected patched RSD (0 digits) fraction bytes in currency_fractions_v1.rs.data"
        );

        // 2. Verify `name_5_langs.patch` non-UI autonyms (`an` -> `"aragonés"`,
        //    `ay` -> `"Aymar"`, `dv` -> `"ދިވެހި"`, `ilo` -> `"Ilokano"`, `lus` ->
        //    `"Mizo tawng"`, `ts` -> `"Xitsonga"`, `wa` -> `"Walon"`)
        let lang_tiny = std::fs::read_to_string(
            data_dir.join("common/locale_names_language_medium_tiny_v1.rs.data"),
        )
        .unwrap();
        assert!(lang_tiny.contains(r"aragon\xC3\xA9s"));
        assert!(lang_tiny.contains("Aymar"));
        assert!(!lang_tiny.contains("Aymar aru"));
        assert!(lang_tiny.contains(r"\xDE\x8B\xDE\xA8\xDE\x88\xDE\xAC\xDE\x80\xDE\xA8"));
        assert!(lang_tiny.contains("Ilokano"));
        assert!(lang_tiny.contains("Mizo tawng"));
        assert!(lang_tiny.contains("Xitsonga"));
        assert!(lang_tiny.contains("Walon"));

        // 3. Verify `locale1.patch` Korean `yMMMd` day-interval spacing (`일 ~
        //    ` instead of `일~` in `datetime_patterns_range_date_gregorian_v1`)
        let date_range = std::fs::read_to_string(
            data_dir.join("common/datetime_patterns_range_date_gregorian_v1.rs.data"),
        )
        .unwrap();
        assert!(date_range.contains(r"\0\xC7|\0\0 \0\0~\0\0 \x80@\x01\0\xC7|"));

        // 4. Verify `TransliteratorRulesV1` attribute filtering (only the
        //    Chrome transliterators)
        let translit =
            std::fs::read_to_string(data_dir.join("common/transliterator_rules_v1.rs.data"))
                .unwrap();
        assert!(translit.contains("TRANSLITERATOR_RULES_V1_UND_DE_T_DE_D0_ASCII"));
        assert!(translit.contains("TRANSLITERATOR_RULES_V1_UND_UND_HIRA_T_UND_KANA"));
        assert!(translit.contains("TRANSLITERATOR_RULES_V1_UND_UND_KANA_T_UND_HIRA"));
        assert!(!translit.contains("TRANSLITERATOR_RULES_V1_UND_UND_ARAB_T_UND_LATN"));

        // 5. Verify cross-platform deduplication across all platforms and
        //    `sources.gni`
        let crate_dir = root.join("third_party/rust/icu4x_data");

        let chromeos_mod = std::fs::read_to_string(data_dir.join("chromeos/mod.rs")).unwrap();
        assert!(chromeos_mod.contains("include!(\"../common/currency_fractions_v1.rs.data\")"));
        assert!(!data_dir.join("chromeos/currency_fractions_v1.rs.data").exists());

        let sources_gni = std::fs::read_to_string(crate_dir.join("sources.gni")).unwrap();
        assert!(sources_gni.contains("\"src/data/common/currency_fractions_v1.rs.data\""));
        let rustc = std::env::var("RUSTC").unwrap_or_else(|_| {
            root.join("third_party/rust-toolchain/bin/rustc")
                .with_extension(std::env::consts::EXE_EXTENSION)
                .to_string_lossy()
                .to_string()
        });
        let tmp_dir = crate::overlay::TempDir::new("icu4x_data_test").unwrap();
        let tmp_rmeta = tmp_dir.path().join("libicu4x_data.rmeta");
        for plat in Icu4xPlatform::value_variants() {
            let plat_dir = data_dir.join(plat.as_str());
            let mod_rs = std::fs::read_to_string(plat_dir.join("mod.rs")).unwrap();
            assert!(
                !mod_rs.contains("include ! ("),
                "{plat}/mod.rs was not formatted with rustfmt"
            );
            let mut checked_count = 0usize;
            for part in mod_rs.split("include!(\"").skip(1) {
                if let Some((rel_path, _)) = part.split_once('"') {
                    if !rel_path.ends_with(".rs.data") {
                        continue;
                    }
                    checked_count += 1;
                    let resolved = plat_dir.join(rel_path);
                    assert!(
                        resolved.exists(),
                        "Broken include in {plat}/mod.rs: {rel_path} ({}) does not exist",
                        resolved.display()
                    );
                }
            }
            assert!(checked_count > 0, "Expected at least one `.rs.data` include in {plat}/mod.rs");
            let status = std::process::Command::new(&rustc)
                .arg("--crate-name=icu4x_data")
                .arg("--crate-type=rlib")
                .arg("--edition=2024")
                .arg(format!("--cfg=icu4x_data_platform_{plat}"))
                .arg("--emit=metadata")
                .arg("-o")
                .arg(&tmp_rmeta)
                .arg(crate_dir.join("src/lib.rs"))
                .status()
                .expect("Failed to invoke rustc");
            assert!(
                status.success(),
                "rustc failed to compile third_party/rust/icu4x_data/src/lib.rs for platform {plat}"
            );
        }
    }
}
