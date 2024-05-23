// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Configures gnrt behavior. Types match `gnrt_config.toml` fields. Currently
//! only used for std bindings.

use crate::group::Group;

use std::collections::{BTreeMap, HashMap, HashSet};

use serde::Deserialize;

#[derive(Clone, Debug, Default, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct BuildConfig {
    pub resolve: ResolveConfig,
    #[serde(rename = "gn")]
    pub gn_config: GnConfig,
    #[serde(rename = "vet")]
    pub vet_config: VetConfig,
    /// Configuration that applies to all crates
    #[serde(rename = "all-crates")]
    pub all_config: CrateConfig,
    /// Additional configuration options for specific crates. Keyed by crate
    /// name. Config is additive with `all_config`.
    #[serde(rename = "crate")]
    pub per_crate_config: BTreeMap<String, CrateConfig>,
}

impl BuildConfig {
    /// Combines the global and per-crate `CrateConfig` for a single
    /// `Vec<String>` entry.
    pub fn get_combined_set(
        &self,
        package_name: &str,
        entry_getter: impl Fn(&CrateConfig) -> &Vec<String>,
    ) -> HashSet<&str> {
        let all: Option<&Vec<String>> = Some(entry_getter(&self.all_config));
        let per: Option<&Vec<String>> = self.per_crate_config.get(package_name).map(entry_getter);
        all.into_iter().chain(per).flatten().map(String::as_str).collect()
    }
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

/// Configures Cargo Vet output for this session.
#[derive(Clone, Debug, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct VetConfig {
    /// Path to a handlebars template for writing Cargo Vet's config.toml file.
    /// The path is relative to the config file. Only used for
    /// //third_party/rust crates.
    #[serde(default)]
    pub config_template: std::path::PathBuf,
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
    /// Features that are disallowed (e.g. because the feature-gated code hasn't
    /// been audited or because the audit uncovered unsoundness)
    pub ban_features: Vec<String>,
    /// If true, the build script should not be built or used.
    #[serde(default)]
    pub remove_build_rs: bool,
    /// Include rs and input files under these paths relative to the crate's
    /// root source file as part of the crate. The roots may each be a single
    /// file or a directory.
    #[serde(default)]
    pub extra_src_roots: Vec<std::path::PathBuf>,
    /// Include input files under these paths relative to the crate's root
    /// source file as part of the crate. The roots may each be a single file or
    /// a directory.
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
    /// Names of binary targets to include.  This list is empty by default,
    /// which means that the default generated `BUILD.gn` will only cover
    /// the library target (if any) of the package.
    #[serde(default)]
    pub bin_targets: Vec<String>,

    // Third-party crate settings.
    #[serde(default)]
    pub allow_first_party_usage: bool,
    #[serde(default)]
    pub build_script_outputs: Vec<std::path::PathBuf>,
    #[serde(default)]
    pub group: Option<Group>,
    #[serde(default)]
    pub security_critical: Option<bool>,
    #[serde(default)]
    pub shipped: Option<bool>,
    #[serde(default)]
    pub license: Option<String>,
    #[serde(default)]
    pub license_files: Vec<String>,
    /// Include `.lib` files under these paths relative to the crate's root
    /// source file as part of the crate. They will be available for #[link]
    /// directives in the crate. The roots may each be a single file or a
    /// directory.
    ///
    /// These paths are the only ones searched for lib files, unlike
    /// `extra_src_roots`. Nothing is searched if this is empty.
    #[serde(default)]
    pub native_libs_roots: Vec<std::path::PathBuf>,
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_get_combined_set_with_global_and_per_crate_entry() {
        let all_config =
            CrateConfig { bin_targets: vec!["foo".to_string()], ..CrateConfig::default() };
        let crate_config =
            CrateConfig { bin_targets: vec!["bar".to_string()], ..CrateConfig::default() };
        let build_config = BuildConfig {
            all_config,
            per_crate_config: [("some_crate".to_string(), crate_config)].into_iter().collect(),
            ..BuildConfig::default()
        };
        let combined_set = build_config.get_combined_set("some_crate", |cfg| &cfg.bin_targets);
        assert_eq!(combined_set.len(), 2);
        assert!(combined_set.contains("foo"));
        assert!(combined_set.contains("bar"));
    }

    #[test]
    fn test_get_combined_set_with_only_global_entry() {
        let all_config =
            CrateConfig { bin_targets: vec!["foo".to_string()], ..CrateConfig::default() };
        let build_config = BuildConfig { all_config, ..BuildConfig::default() };
        let combined_set = build_config.get_combined_set("some_crate", |cfg| &cfg.bin_targets);
        assert_eq!(combined_set.len(), 1);
        assert!(combined_set.contains("foo"));
    }
}
