// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Implementation of the `dump-json` subcommand and shared `ICU4X-json`
//! exporter helper for dumping pristine payloads from `SourceDataProvider` to
//! inspect or apply unified `.patch` files.

use crate::filters::{all_segmenter_models, ResolvedLocaleFilter};
use crate::{select_markers_by_args, SourceTags};
use anyhow::Result;
use clap::Args;
use icu_locale_fallback::LocaleFallbacker;
use icu_provider::prelude::*;
use icu_provider_export::fs_exporter::serializers::Json as JsonSerializer;
use icu_provider_export::fs_exporter::{FilesystemExporter, Options as FsOptions, OverwriteOption};
use icu_provider_export::prelude::*;
use icu_provider_source::SourceDataProvider;
use std::collections::BTreeSet;
use std::path::{Path, PathBuf};

/// Export `ICU4X-json` payloads for `markers` and `families` from `provider`
/// into `out_dir` (removing and replacing `out_dir` if it already exists).
pub(crate) fn export_icu4x_json(
    provider: &SourceDataProvider,
    fallbacker: LocaleFallbacker,
    out_dir: &Path,
    markers: impl IntoIterator<Item = DataMarkerInfo>,
    families: impl IntoIterator<Item = DataLocaleFamily>,
) -> Result<()> {
    let mut dump_opts = FsOptions::default();
    dump_opts.root = out_dir.to_path_buf();
    dump_opts.overwrite = OverwriteOption::RemoveAndReplace;
    let exporter = FilesystemExporter::try_new(Box::new(JsonSerializer::pretty()), dump_opts)?;
    ExportDriver::new(families, DeduplicationStrategy::None.into(), fallbacker)
        .with_markers(markers)
        .with_segmenter_models(all_segmenter_models())
        .export(provider, exporter)?;
    Ok(())
}

/// CLI arguments for the `dump-json` subcommand.
#[derive(Args)]
pub(crate) struct DumpJsonArgs {
    /// Output directory where `.json` files will be written.
    #[arg(long)]
    json_dir: PathBuf,

    /// Markers to dump (e.g. `locale/names/language/medium/tiny/v1`,
    /// `chromium`, or `all`).
    #[arg(long, num_args = 1..)]
    markers: Vec<String>,

    /// Locales to dump (e.g. `en`, `^ko`, `und`, or `full`).
    #[arg(long, num_args = 1.., default_value = "full")]
    locales: Vec<String>,

    #[command(flatten)]
    source_tags: SourceTags,
}

impl DumpJsonArgs {
    /// Export pristine `ICU4X-json` files for the requested markers and locales
    /// into `self.json_dir`.
    pub(crate) fn run(self) -> Result<()> {
        let selected = select_markers_by_args(&self.markers)?;
        let provider = self.source_tags.source_data_provider();
        let fallbacker = LocaleFallbacker::try_new_unstable(&provider)?;

        let families: Vec<DataLocaleFamily> = if self.locales.as_slice() == ["full"] {
            vec![DataLocaleFamily::FULL]
        } else {
            let set: BTreeSet<String> = self.locales.into_iter().collect();
            ResolvedLocaleFilter::Families(set).to_locale_families()?
        };

        export_icu4x_json(
            &provider,
            fallbacker,
            &self.json_dir,
            selected.into_iter().map(|m| m.info),
            families,
        )?;
        log::info!("Dumped ICU4X-json files to {}", self.json_dir.display());
        Ok(())
    }
}
