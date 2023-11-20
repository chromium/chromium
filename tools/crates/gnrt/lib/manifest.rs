// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Utilities for parsing and generating Cargo.toml files.

use std::collections::BTreeMap;

use semver::Version;
use serde::{Deserialize, Serialize};

/// Set of dependencies for a particular usage: final artifacts, tests, or
/// build scripts.
pub type DependencySet<Type> = BTreeMap<String, Type>;

/// A version constraint in a dependency spec. We don't use `semver::VersionReq`
/// since we only pass it through opaquely from third_party.toml to Cargo.toml.
/// Parsing it is unnecessary.
#[derive(Clone, Debug, Deserialize, Eq, PartialEq, Serialize)]
// From serde's perspective we serialize and deserialize this as a plain string.
#[serde(transparent)]
pub struct VersionConstraint(pub String);

#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields, rename_all = "kebab-case")]
pub struct WorkspaceSpec {
    pub members: Vec<String>,
}

/// A single crate dependency. Cargo.toml and third_party.toml have different
/// version formats and some different fields. This is generic to share the same
/// type between them.
#[derive(Clone, Debug, Deserialize, Eq, PartialEq, Serialize)]
#[serde(untagged)]
pub enum Dependency<VersionType, DepType> {
    /// A dependency of the form `foo = "1.0.11"`: just the package name as key
    /// and the version as value. The sole field is the crate version.
    Short(VersionType),
    /// A dependency that specifies other fields in the form of `foo = { ... }`
    /// or `[dependencies.foo] ... `.
    Full(DepType),
}

/// A single Cargo.toml dependency.
pub type CargoDependency = Dependency<String, CargoFullDependency>;
pub type CargoDependencySet = DependencySet<CargoDependency>;

#[derive(Clone, Debug, Deserialize, Eq, PartialEq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub struct CargoFullDependency {
    /// Include the package's default features. Influences Cargo behavior.
    #[serde(default = "get_true", skip_serializing_if = "is_true")]
    pub default_features: bool,
    /// Version constraint on dependency.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub version: Option<VersionConstraint>,
    /// Required features.
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub features: Vec<String>,
}

impl Default for CargoFullDependency {
    fn default() -> Self {
        Self { default_features: true, version: None, features: vec![] }
    }
}

/// Representation of a Cargo.toml file.
#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(rename_all = "kebab-case")]
pub struct CargoManifest {
    pub package: CargoPackage,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub workspace: Option<WorkspaceSpec>,
    /// Regular dependencies built into production code.
    #[serde(
        default,
        skip_serializing_if = "DependencySet::is_empty",
        serialize_with = "toml::ser::tables_last"
    )]
    pub dependencies: CargoDependencySet,
}

#[derive(Clone, Debug, Deserialize, Eq, PartialEq, Serialize)]
#[serde(rename_all = "kebab-case")]
pub struct CargoPackage {
    pub name: String,
    pub version: Version,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub authors: Vec<String>,
    #[serde(default)]
    pub edition: Edition,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub description: Option<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub include: Vec<String>,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub license: String,
}

#[derive(Clone, Debug, Deserialize, Eq, PartialEq, Serialize)]
#[serde(transparent)]
pub struct Edition(pub String);

impl Default for Edition {
    fn default() -> Self {
        Edition("2015".to_string())
    }
}

impl std::fmt::Display for Edition {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.0)
    }
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(rename_all = "kebab-case")]
pub struct CargoPatch {
    pub path: String,
    pub package: String,
}

// Used to set the serde default of a field to true.
fn get_true() -> bool {
    true
}

fn is_true(b: &bool) -> bool {
    *b
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn package_manifest() {
        let manifest: CargoManifest = toml::de::from_str(concat!(
            "[package]
name = \"foo\"
version = \"1.2.3\"
authors = [\"alice@foo.com\", \"bob@foo.com\"]
edition = \"2021\"
description = \"A library to foo the bars\"
license = \"funtimes\"
"
        ))
        .unwrap();

        assert_eq!(
            manifest.package,
            CargoPackage {
                name: "foo".to_string(),
                version: Version::new(1, 2, 3),
                authors: vec!["alice@foo.com".to_string(), "bob@foo.com".to_string()],
                edition: Edition("2021".to_string()),
                description: Some("A library to foo the bars".to_string()),
                license: "funtimes".to_string(),
                include: Vec::new(),
            }
        )
    }
}
