// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use anyhow::Result;
use gnrt_lib::paths;
use gnrt_lib::toml_edit_utils::{self, FormatOptions, GNRT_CONFIG_FORMAT_OPTIONS};
use std::fs::{self, File};
use std::io::Write as _;
use std::path::Path;
use toml_edit::DocumentMut;

/// Implementation of `gnrt fmt ...` command.
pub fn format(paths: &paths::ChromiumPaths) -> Result<()> {
    format_file(paths.third_party_config_file, &GNRT_CONFIG_FORMAT_OPTIONS)?;

    let cargo_toml_options =
        FormatOptions { toplevel_table_order: &["package", "workspace", "dependencies", "patch"] };
    let cargo_toml_path = paths.third_party_cargo_root.join("Cargo.toml");
    format_file(&cargo_toml_path, &cargo_toml_options)?;

    Ok(())
}

fn format_file(path: &Path, options: &FormatOptions) -> Result<()> {
    let input = fs::read_to_string(path)?;
    let mut doc = input.parse::<DocumentMut>()?;
    toml_edit_utils::format(&mut doc, options);
    write!(File::create(path)?, "{doc}")?;
    Ok(())
}
