// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Configures gnrt behavior. Types match `gnrt_config.toml` fields. Currently
//! only used for std bindings.

use std::collections::BTreeMap;

use serde::Deserialize;

#[derive(Clone, Debug, Default, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct BuildConfig {
    pub resolve: ResolveConfig,
    /// Configuration that applies to all crates
    #[serde(rename = "all-crates")]
    pub all_config: CrateConfig,
    /// Additional configuration options for specific crates. Keyed by crate
    /// name. Config is additive with `all_config`.
    #[serde(rename = "crate")]
    pub per_crate_config: BTreeMap<String, CrateConfig>,
}

/// Influences dependency resolution for a session.
#[derive(Clone, Debug, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ResolveConfig {
    /// The Cargo package to use as the root of the dependency graph.
    pub root: String,
}

/// Customizes GN output for a crate.
#[derive(Clone, Debug, Default, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct CrateConfig {
    /// `cfg(...)` options for building this crate.
    pub cfg: Vec<String>,
    /// Compile-time environment variables for this crate.
    pub env: Vec<String>,
    /// Apply rustc metadata to this target.
    pub rustc_metadata: Option<String>,
    /// Extra rustc flags.
    pub rustflags: Vec<String>,
    /// Sets GN output_dir variable.
    pub output_dir: Option<String>,
    /// Adds the specified default library configs in the target.
    #[serde(default)]
    pub add_library_configs: Vec<String>,
    /// Removes the specified default library configs in the target.
    #[serde(default)]
    pub remove_library_configs: Vec<String>,
    /// GN deps to add to the generated target.
    pub extra_gn_deps: Vec<String>,
    /// Remove GN deps added by the overall config.
    ///
    /// TODO(crbug.com/1245714): find a way to express these sorts of
    /// dependencies.
    #[serde(default)]
    pub extra_gn_deps_to_ignore: Vec<String>,
    /// Deps on generated targets to exclude from this target's `deps` list.
    /// These do not affect dependency resolution, so it will not change any
    /// other generated targets.
    pub exclude_deps_in_gn: Vec<String>,
}
