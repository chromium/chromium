// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Configures gnrt behavior. Types match `gnrt_config.toml` fields. Currently
//! only used for std bindings.

use std::collections::{BTreeMap, HashMap};

use serde::Deserialize;

#[derive(Clone, Debug, Default, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct BuildConfig {
    pub resolve: ResolveConfig,
    #[serde(rename = "gn")]
    pub gn_config: GnConfig,
    /// Configuration that applies to all crates
    #[serde(rename = "all-crates")]
    pub all_config: CrateConfig,
    /// Additional configuration options for specific crates. Keyed by crate
    /// name. Config is additive with `all_config`.
    #[serde(rename = "crate")]
    pub per_crate_config: BTreeMap<String, CrateConfig>,
}

/// Configures GN output for this session.
#[derive(Clone, Debug, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct GnConfig {
    /// Path to a handlebars template for an output GN build file. The path is
    /// relative to the config file.
    pub build_file_template: std::path::PathBuf,
    /// Path to a handlebars template for writing README.chromium files. The
    /// path is relative to the config file. Only used for
    /// //third_party/rust crates.
    #[serde(default)]
    pub readme_file_template: std::path::PathBuf,
    /// Path to a handlebars template for writing placeholder crates that we
    /// don't want to vendor. This is the Cargo.toml file.
    #[serde(default)]
    pub removed_cargo_template: std::path::PathBuf,
    /// Path to a handlebars template for writing placeholder crates that we
    /// don't want to vendor. This is the src/lib.rs file.
    #[serde(default)]
    pub removed_librs_template: std::path::PathBuf,
}

/// Influences dependency resolution for a session.
#[derive(Clone, Debug, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ResolveConfig {
    /// The Cargo package to use as the root of the dependency graph.
    pub root: String,
    /// Remove crates from the set of resolved dependencies. Should be used
    /// sparingly; it does not affect Cargo's dependency resolution, so the
    /// output can easily be incorrect. This is primarily intended to work
    /// around bugs in `cargo metadata` output.
    pub remove_crates: Vec<String>,
}

/// Customizes GN output for a crate.
#[derive(Clone, Debug, Default, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct CrateConfig {
    /// Dependencies to remove from this crate. Note that this happens after
    /// dependency and feature resolution, so if an optional dep's feature is
    /// enabled but the dep is removed, the crate will still attempt to
    /// reference that dependency.
    pub remove_deps: Vec<String>,
    /// Deps on generated targets to exclude from this target's `deps` list.
    /// These do not affect dependency resolution, so it will not change any
    /// other generated targets.
    pub exclude_deps_in_gn: Vec<String>,
    /// If true, the build script should not be built or used.
    #[serde(default)]
    pub remove_build_rs: bool,
    /// Include rs and input files under these relative paths as part of the
    /// crate. The roots may each be a single file or a directory.
    #[serde(default)]
    pub extra_src_roots: Vec<std::path::PathBuf>,
    /// Include input files under these relative paths as part of the crate.
    /// The roots may each be a single file or a directory.
    #[serde(default)]
    pub extra_input_roots: Vec<std::path::PathBuf>,
    /// Include rs and input files under these relative paths as part of the
    /// crate's build script. The roots may each be a single file or a
    /// directory.
    #[serde(default)]
    pub extra_build_script_src_roots: Vec<std::path::PathBuf>,
    /// Include input files under these relative paths as part of the crate's
    /// build script. The roots may each be a single file or a directory.
    #[serde(default)]
    pub extra_build_script_input_roots: Vec<std::path::PathBuf>,
    #[serde(default)]
    pub extra_kv: HashMap<String, serde_json::Value>,

    // Third-party crate settings.
    #[serde(default)]
    pub allow_first_party_usage: bool,
    #[serde(default)]
    pub build_script_outputs: Vec<std::path::PathBuf>,
    #[serde(default)]
    pub group: Option<String>,
    #[serde(default)]
    pub security_critical: Option<bool>,
    #[serde(default)]
    pub shipped: Option<bool>,
    #[serde(default)]
    pub license: Option<String>,
    #[serde(default)]
    pub license_files: Vec<String>,
}
