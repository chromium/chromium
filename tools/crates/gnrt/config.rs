// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Parsing configuration file that customizes gnrt BUILD.gn output. Currently
//! only used for std bindings.

use std::collections::BTreeMap;

use serde::Deserialize;

/// Customizes GN output for a session.
#[derive(Clone, Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct BuildConfig {
    /// Configuration that applies to all crates
    #[serde(default, rename = "all")]
    pub all_config: CrateConfig,
    /// Additional configuration options for specific crates. Keyed by crate
    /// name. Config is additive with `all_config`.
    #[serde(rename = "crate")]
    pub per_crate_config: BTreeMap<String, CrateConfig>,
}

#[derive(Clone, Debug, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct CrateConfig {
    /// `cfg(...)` options for building this crate.
    #[serde(default)]
    pub cfg: Vec<String>,
    /// Compile-time environment variables for this crate.
    #[serde(default)]
    pub env: Vec<String>,
    /// Extra rustc flags.
    #[serde(default)]
    pub rustflags: Vec<String>,
    /// Sets GN output_dir variable.
    #[serde(default)]
    pub output_dir: Option<String>,
    /// GN deps to add to the generated target.
    #[serde(default)]
    pub extra_gn_deps: Vec<String>,
    /// Deps on generated targets to exclude from this target's `deps` list.
    /// These do not affect dependency resolution, so it will not change any
    /// other generated targets.
    #[serde(default)]
    pub exclude_deps_in_gn: Vec<String>,
}
