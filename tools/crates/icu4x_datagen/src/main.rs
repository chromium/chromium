// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Chromium ICU4X data generator tool (`//tools/crates/icu4x_datagen`).
//!
//! Generates per-platform baked ICU4X data in `//third_party/rust/icu4x_data`
//! using upstream CLDR / ICU / Unicode RSCD / TZDB / segmenter LSTM tags.

extern crate alloc;

use anyhow::{Context, Result};
use clap::{Parser, Subcommand, ValueEnum};
use icu_locale_fallback::LocaleFallbacker;
use icu_provider::prelude::*;
use icu_provider_export::baked_exporter::{BakedExporter, Options as BakedOptions};
use icu_provider_export::prelude::*;
use icu_provider_source::SourceDataProvider;
use std::collections::{BTreeSet, HashMap};
use std::path::{Path, PathBuf};

/// Default upstream source tags tested with the current `icu_provider_source`
/// crate.
pub const DEFAULT_CLDR_TAG: &str = SourceDataProvider::TESTED_CLDR_TAG;
pub const DEFAULT_ICUEXPORT_TAG: &str = SourceDataProvider::TESTED_ICUEXPORT_TAG;
pub const DEFAULT_UNICODE_TAG: &str = SourceDataProvider::TESTED_UNICODE_TAG;
pub const DEFAULT_TZDB_TAG: &str = SourceDataProvider::TESTED_TZDB_TAG;
pub const DEFAULT_SEGMENTER_LSTM_TAG: &str = SourceDataProvider::TESTED_SEGMENTER_LSTM_TAG;

/// Target platform bundle corresponding to
/// `//third_party/icu/filters/<platform>.json`.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, ValueEnum)]
#[value(rename_all = "snake_case")]
pub enum Icu4xPlatform {
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
    pub const fn as_str(self) -> &'static str {
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
    /// List registered ICU4X markers.
    ListMarkers {
        /// Only list markers matching the specified marker path prefixes
        /// (defaults to `all`).
        #[arg(long, num_args = 1.., default_value = "all")]
        markers: Vec<String>,
    },
    /// Generate `//third_party/rust/icu4x_data` for one or more platforms.
    Generate {
        /// Path to the existing `//third_party/rust/icu4x_data` crate.
        #[arg(long, default_value = "third_party/rust/icu4x_data")]
        icu4x_data_dir: PathBuf,

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

        /// Override CLDR tag (defaults to `DEFAULT_CLDR_TAG`).
        #[arg(long, default_value = DEFAULT_CLDR_TAG)]
        cldr_tag: String,

        /// Override icuexportdata tag (defaults to `DEFAULT_ICUEXPORT_TAG`).
        #[arg(long, default_value = DEFAULT_ICUEXPORT_TAG)]
        icuexport_tag: String,

        /// Override Unicode RSCD tag (defaults to `DEFAULT_UNICODE_TAG`).
        #[arg(long, default_value = DEFAULT_UNICODE_TAG)]
        unicode_tag: String,

        /// Override TZDB tag (defaults to `DEFAULT_TZDB_TAG`).
        #[arg(long, default_value = DEFAULT_TZDB_TAG)]
        tzdb_tag: String,

        /// Override segmenter LSTM tag (defaults to
        /// `DEFAULT_SEGMENTER_LSTM_TAG`).
        #[arg(long, default_value = DEFAULT_SEGMENTER_LSTM_TAG)]
        segmenter_lstm_tag: String,
    },
}

#[derive(Clone, Copy, Debug)]
pub struct MarkerEntry {
    pub name: &'static str,
    pub info: DataMarkerInfo,
    pub is_unstable: bool,
}

macro_rules! cb {
    ($($marker_ty:ty:$marker:ident,)+ #[unstable] $($emarker_ty:ty:$emarker:ident,)+) => {
        pub fn all_registered_markers() -> Vec<MarkerEntry> {
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
    };
}

icu_provider_registry::registry!(cb);

/// Convert a `DataMarkerId` (e.g. `DatetimeNamesWeekdayV1`) into its
/// slash-separated path string (e.g. `"datetime/names/weekday/v1"`), matching
/// `icu_provider_fs`.
pub fn marker_path_str(marker: icu_provider::marker::DataMarkerId) -> String {
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
pub const CHROMIUM_MARKER_PREFIXES: &[&str] = &[
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

pub fn select_markers_by_args(marker_args: &[String]) -> Result<Vec<MarkerEntry>> {
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
            for m in select_markers_by_args(&markers)? {
                println!(
                    "  {:42} -> {:45} (singleton={}, unstable={})",
                    m.name,
                    marker_path_str(m.info.id),
                    m.info.is_singleton,
                    m.is_unstable
                );
            }
            Ok(())
        }
        Commands::Generate {
            icu4x_data_dir,
            platforms,
            markers: marker_args,
            cldr_tag,
            icuexport_tag,
            unicode_tag,
            tzdb_tag,
            segmenter_lstm_tag,
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

            let selected_markers = select_markers_by_args(&marker_args)?;
            log::info!("Selected {} ICU4X markers to generate", selected_markers.len());

            let provider = SourceDataProvider::new_custom()
                .with_cldr_for_tag(&cldr_tag)
                .with_icuexport_for_tag(&icuexport_tag)
                .with_unicode_rscd_for_tag(&unicode_tag)
                .with_tzdb_for_tag(&tzdb_tag)
                .with_segmenter_lstm_for_tag(&segmenter_lstm_tag);

            let fallbacker = LocaleFallbacker::try_new_unstable(&provider)
                .context("Failed to initialize LocaleFallbacker from SourceDataProvider")?;

            let src_dir = icu4x_data_dir.join("src");
            let data_root = src_dir.join("data");
            std::fs::create_dir_all(&data_root)?;

            // TODO(crbug.com/571707298): Apply per-platform ICU4C locale and
            // feature filters from `//third_party/icu/filters/*.json` (wired up
            // in https://crrev.com/c/8640572).
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
                driver.export(&provider, baked_exporter)?;
            }

            deduplicate_cross_platform_data(&data_root, &platform_list)?;

            write_crate_files(
                &icu4x_data_dir,
                &platform_list,
                &cldr_tag,
                &unicode_tag,
                &segmenter_lstm_tag,
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_select_markers_by_args_errors_on_unknown() {
        assert!(select_markers_by_args(&["calender".to_string()]).is_err());
        let chromium = select_markers_by_args(&["chromium".to_string()]).unwrap();
        assert!(!chromium.is_empty());
    }
}
