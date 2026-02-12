// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Configures gnrt behavior. Types match `gnrt_config.toml` fields. Currently
//! only used for std bindings.

use crate::crates::Epoch;
use crate::group::Group;

use anyhow::{bail, Context, Result};
use semver::Version;
use serde::Deserialize;
use std::{
    collections::{BTreeMap, HashMap, HashSet},
    hash::Hash,
    ops::Deref,
    path::Path,
};

fn make_epoch_key(package_name: &str, epoch: Epoch) -> String {
    format!("{package_name}@{epoch}")
}

fn parse_crate_key(key: &str) -> Result<(&str, Option<Epoch>)> {
    match key.find('@') {
        Some(pos) => {
            let crate_name = &key[..pos];
            let epoch_str = &key[pos + 1..];
            let epoch = epoch_str
                .parse::<Epoch>()
                .with_context(|| format!("Invalid epoch '{epoch_str}' in config key '{key}'"))?;
            Ok((crate_name, Some(epoch)))
        }
        None => Ok((key, None)),
    }
}

#[derive(Clone, Debug, Default, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct BuildConfig {
    pub resolve: ResolveConfig,
    #[serde(rename = "gn")]
    pub gn_config: GnConfig,
    /// Configuration that applies to all crates
    #[serde(rename = "all-crates")]
    all_config: CrateConfig,
    /// Additional configuration options for specific crates. Keyed by crate
    /// name. Config is additive with `all_config`.
    #[serde(rename = "crate")]
    per_crate_config: BTreeMap<String, CrateConfig>,
}

impl BuildConfig {
    /// Combines the global and per-crate `CrateConfig` for a single entry.
    /// Combined `Vec<String>` entries will be returned as `HashSet<&str>`.
    /// Combined `Vec<PathBuf>` entries will be returned as `HashSet<&Path>`.
    pub fn get_combined_set<'a, T>(
        &'a self,
        package_name: &str,
        version: &Version,
        entry_getter: impl Fn(&CrateConfig) -> &Vec<T>,
    ) -> HashSet<&'a <T as Deref>::Target>
    where
        T: Deref + 'a,
        T::Target: Hash + Eq + PartialEq,
    {
        let all_crates_vec: &Vec<T> = entry_getter(&self.all_config);
        let maybe_per_crate_vec: Option<&Vec<T>> =
            self.get_crate_config(package_name, version).map(&entry_getter);
        let combined_vecs = std::iter::once(all_crates_vec).chain(maybe_per_crate_vec);
        combined_vecs.flatten().map(Deref::deref).collect()
    }

    /// Combines HashMap entries from global and per-crate config levels.
    /// Per-crate config overrides all-crates config.
    pub fn get_combined_map_cloned<V: Clone>(
        &self,
        package_name: &str,
        version: &Version,
        entry_getter: impl Fn(&CrateConfig) -> &HashMap<String, V>,
    ) -> HashMap<String, V> {
        let mut result = entry_getter(&self.all_config).clone();
        if let Some(crate_config) = self.get_crate_config(package_name, version) {
            result.extend(entry_getter(crate_config).iter().map(|(k, v)| (k.clone(), v.clone())));
        }
        result
    }

    pub fn get_crate_config(&self, package_name: &str, version: &Version) -> Option<&CrateConfig> {
        let epoch = Epoch::from_version(version);
        let epoch_key = make_epoch_key(package_name, epoch);
        self.per_crate_config.get(&epoch_key).or_else(|| self.per_crate_config.get(package_name))
    }

    pub fn from_path(path: &Path) -> Result<Self> {
        let context = || format!("Error reading `gnrt_config.toml` from `{}`", path.display());
        let file_content = std::fs::read_to_string(path).with_context(context)?;
        let config: Self = toml::de::from_str(&file_content).with_context(context)?;
        config.validate()?;
        Ok(config)
    }

    /// Each crate must have either:
    /// - One unversioned config (`foo`)
    /// - One or more versioned configs (`foo@v1`, `foo@v2`)
    ///
    /// Mixing unversioned and versioned configs for the same crate is an error
    /// to make the mental model simpler.
    fn validate(&self) -> Result<()> {
        let mut unversioned_crates: HashSet<&str> = HashSet::new();
        let mut versioned_crates: HashSet<&str> = HashSet::new();

        for key in self.per_crate_config.keys() {
            let (crate_name, epoch) = parse_crate_key(key)?;
            if epoch.is_some() {
                versioned_crates.insert(crate_name);
            } else {
                unversioned_crates.insert(crate_name);
            }
        }

        for crate_name in &unversioned_crates {
            if versioned_crates.contains(crate_name) {
                bail!(
                    "Crate '{crate_name}' has both unversioned ([crate.{crate_name}]) \
                     and versioned ([crate.\"{crate_name}@vX\"]) config entries. Use \
                     one or the other, not both.",
                );
            }
        }

        Ok(())
    }

    #[cfg(test)]
    pub fn insert_crate_config_for_testing(&mut self, key: &str, config: CrateConfig) {
        self.per_crate_config.insert(key.to_string(), config);
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
    pub remove_crates: HashSet<String>,
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
    /// Files or directories to remove from the vendored crate.
    #[serde(default)]
    pub remove_vendored_files: Vec<std::path::PathBuf>,
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
    /// Whether the crate is permitted to have no license files.
    #[serde(default)]
    pub no_license_file_tracked_in_crbug_369075726: bool,
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
        let version = Version::new(1, 0, 0);
        let combined_set =
            build_config.get_combined_set("some_crate", &version, |cfg| &cfg.bin_targets);
        assert_eq!(combined_set.len(), 2);
        assert!(combined_set.contains("foo"));
        assert!(combined_set.contains("bar"));
    }

    #[test]
    fn test_get_combined_set_for_remove_vendored_files() {
        let all_config = CrateConfig {
            remove_vendored_files: vec![std::path::PathBuf::from("global_file")],
            ..CrateConfig::default()
        };
        let crate_config = CrateConfig {
            remove_vendored_files: vec![std::path::PathBuf::from("local_file")],
            ..CrateConfig::default()
        };
        let build_config = BuildConfig {
            all_config,
            per_crate_config: [("some_crate".to_string(), crate_config)].into_iter().collect(),
            ..BuildConfig::default()
        };
        let version = Version::new(1, 0, 0);
        let combined_set =
            build_config.get_combined_set("some_crate", &version, |cfg| &cfg.remove_vendored_files);
        assert_eq!(combined_set.len(), 2);
        assert!(combined_set.contains(std::path::Path::new("global_file")));
        assert!(combined_set.contains(std::path::Path::new("local_file")));
    }

    #[test]
    fn test_get_combined_set_with_only_global_entry() {
        let all_config =
            CrateConfig { bin_targets: vec!["foo".to_string()], ..CrateConfig::default() };
        let build_config = BuildConfig { all_config, ..BuildConfig::default() };
        let version = Version::new(1, 0, 0);
        let combined_set =
            build_config.get_combined_set("some_crate", &version, |cfg| &cfg.bin_targets);
        assert_eq!(combined_set.len(), 1);
        assert!(combined_set.contains("foo"));
    }

    #[test]
    fn test_epoch_specific_config_takes_precedence() {
        let mut build_config = BuildConfig::default();
        build_config.insert_crate_config_for_testing(
            "foo@v1",
            CrateConfig { bin_targets: vec!["v1_target".to_string()], ..CrateConfig::default() },
        );
        build_config.insert_crate_config_for_testing(
            "foo@v2",
            CrateConfig { bin_targets: vec!["v2_target".to_string()], ..CrateConfig::default() },
        );

        let v1 = Version::new(1, 5, 0);
        let v2 = Version::new(2, 0, 0);
        assert_eq!(
            build_config.get_crate_config("foo", &v1).unwrap().bin_targets,
            vec!["v1_target"]
        );
        assert_eq!(
            build_config.get_crate_config("foo", &v2).unwrap().bin_targets,
            vec!["v2_target"]
        );
    }

    #[test]
    fn test_unversioned_config_fallback() {
        let mut build_config = BuildConfig::default();
        build_config.insert_crate_config_for_testing(
            "foo",
            CrateConfig { bin_targets: vec!["unversioned".to_string()], ..CrateConfig::default() },
        );

        // All versions fall back to unversioned config.
        let v1 = Version::new(1, 0, 0);
        let v2 = Version::new(2, 0, 0);
        assert_eq!(
            build_config.get_crate_config("foo", &v1).unwrap().bin_targets,
            vec!["unversioned"]
        );
        assert_eq!(
            build_config.get_crate_config("foo", &v2).unwrap().bin_targets,
            vec!["unversioned"]
        );
    }

    #[test]
    fn test_validation_rejects_mixed_versioned_and_unversioned() {
        let mut build_config = BuildConfig::default();
        build_config.per_crate_config.insert("foo".to_string(), CrateConfig::default());
        build_config.per_crate_config.insert("foo@v1".to_string(), CrateConfig::default());

        let result = build_config.validate();
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("both unversioned"));
    }
}
