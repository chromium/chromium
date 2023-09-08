// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Utilities for parsing and generating Cargo.toml and related manifest files.

use crate::crates::Epoch;

use std::collections::BTreeMap;
use std::path::PathBuf;

use serde::{Deserialize, Serialize};

/// Set of dependencies for a particular usage: final artifacts, tests, or
/// build scripts.
pub type DependencySet<Type> = BTreeMap<String, Type>;
/// Set of patches to replace upstream dependencies with local crates. Maps
/// arbitrary patch names to `CargoPatch` which includes the actual package name
/// and the local path.
pub type CargoPatchSet = BTreeMap<String, CargoPatch>;

/// A specific crate version.
pub use semver::Version;

/// A version constraint in a dependency spec. We don't use `semver::VersionReq`
/// since we only pass it through opaquely from third_party.toml to Cargo.toml.
/// Parsing it is unnecessary.
#[derive(Clone, Debug, Deserialize, Eq, PartialEq, Serialize)]
// From serde's perspective we serialize and deserialize this as a plain string.
#[serde(transparent)]
pub struct VersionConstraint(pub String);

/// Parsed third_party.toml. This is a limited variant of Cargo.toml.
#[derive(Clone, Debug, Default, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "kebab-case")]
pub struct ThirdPartyManifest {
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub workspace: Option<WorkspaceSpec>,
    /// Regular dependencies built into production code.
    #[serde(
        default,
        skip_serializing_if = "DependencySet::is_empty",
        serialize_with = "toml::ser::tables_last"
    )]
    pub dependencies: ThirdPartyDependencySet,
    /// Dependencies to allow only in testonly code. These still participate in
    /// the same dependency resolution.
    #[serde(
        default,
        skip_serializing_if = "DependencySet::is_empty",
        serialize_with = "toml::ser::tables_last"
    )]
    pub testonly_dependencies: ThirdPartyDependencySet,
}

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

/// A single third_party.toml dependency.
pub type ThirdPartyDependency = Dependency<Epoch, ThirdPartyFullDependency>;
pub type ThirdPartyDependencySet = DependencySet<ThirdPartyDependency>;

/// A single Cargo.toml dependency.
pub type CargoDependency = Dependency<String, CargoFullDependency>;
pub type CargoDependencySet = DependencySet<CargoDependency>;

impl ThirdPartyDependency {
    /// Expand the short form spec, filling other fields in with their defaults.
    pub fn into_full(self) -> ThirdPartyFullDependency {
        match self {
            Self::Short(version) => ThirdPartyFullDependency {
                default_features: true,
                version,
                features: vec![],
                allow_first_party_usage: true,
                build_script_outputs: vec![],
                gn_variables_lib: None,
            },
            Self::Full(full) => full,
        }
    }

    /// Generate a Cargo.toml dependency entry with the custom fields stripped
    /// away.
    pub fn into_cargo(self) -> CargoDependency {
        match self {
            Self::Short(version) => CargoDependency::Short(version.to_version_string()),
            Self::Full(full) => CargoDependency::Full(CargoFullDependency {
                default_features: full.default_features,
                version: Some(VersionConstraint(full.version.to_version_string())),
                features: full.features,
            }),
        }
    }
}

/// A single crate dependency with some extra fields from third_party.toml.
/// Unlike `CargoFullDependency` this will reject unknown fields on
/// deserialization. This is desirable since we control the third_party.toml
/// format.
#[derive(Clone, Debug, Deserialize, Eq, PartialEq, Serialize)]
#[serde(deny_unknown_fields, rename_all = "kebab-case")]
pub struct ThirdPartyFullDependency {
    /// Include the package's default features. Influences Cargo behavior.
    #[serde(default = "get_true", skip_serializing_if = "is_true")]
    pub default_features: bool,
    /// Version constraint on dependency.
    pub version: Epoch,
    /// Required features.
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub features: Vec<String>,
    /// Whether this can be used directly from Chromium code, or only from other
    /// third-party crates.
    #[serde(default = "get_true", skip_serializing_if = "is_true")]
    pub allow_first_party_usage: bool,
    /// List of files generated by build.rs script.
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub build_script_outputs: Vec<String>,
    /// Extra variables to add to the lib GN rule. The text will be added
    /// verbatim.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub gn_variables_lib: Option<String>,
}

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
    #[serde(default, rename = "patch")]
    pub patches: BTreeMap<String, CargoPatchSet>,
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

#[derive(Debug)]
pub struct PatchSpecification {
    pub package_name: String,
    pub patch_name: String,
    pub path: PathBuf,
}

pub fn generate_fake_cargo_toml<Iter: IntoIterator<Item = PatchSpecification>>(
    third_party_manifest: ThirdPartyManifest,
    patches: Iter,
) -> CargoManifest {
    let ThirdPartyManifest { workspace, mut dependencies, mut testonly_dependencies, .. } =
        third_party_manifest;

    // The regular and testonly third_party.toml dependencies are treated the
    // same for Cargo.
    dependencies.append(&mut testonly_dependencies);
    drop(testonly_dependencies);

    // Hack: set all `allow_first_party_usage` fields to true so they are
    // suppressed in the Cargo.toml.
    for dep in dependencies.values_mut() {
        if let Dependency::Full(ref mut dep) = dep {
            dep.allow_first_party_usage = true;
        }
    }

    let dependencies: BTreeMap<_, _> =
        dependencies.into_iter().map(|(pkg, dep)| (pkg, dep.into_cargo())).collect();

    let mut patch_sections = CargoPatchSet::new();
    // Generate patch section.
    for PatchSpecification { package_name, patch_name, path } in patches {
        patch_sections.insert(
            patch_name,
            CargoPatch { path: path.to_str().unwrap().to_string(), package: package_name },
        );
    }

    let package = CargoPackage {
        name: "chromium".to_string(),
        version: Version::new(0, 1, 0),
        authors: Vec::new(),
        edition: Edition("2021".to_string()),
        description: None,
        include: Vec::new(),
        license: "".to_string(),
    };

    CargoManifest {
        package,
        workspace,
        dependencies,
        patches: std::iter::once(("crates-io".to_string(), patch_sections)).collect(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::crates::Epoch;

    #[test]
    fn parse_single_full_dependency() {
        assert_eq!(
            toml::de::from_str(concat!(
                "version = \"1\"\n",
                "features = [\"foo\", \"bar\"]\n",
                "allow-first-party-usage = false\n",
                "build-script-outputs = [\"stuff.rs\"]\n",
                "gn-variables-lib = \"\"\"
                deps = []
                configs = []
                \"\"\""
            )),
            Ok(ThirdPartyFullDependency {
                default_features: true,
                version: Epoch::Major(1),
                features: vec!["foo".to_string(), "bar".to_string()],
                allow_first_party_usage: false,
                build_script_outputs: vec!["stuff.rs".to_string()],
                gn_variables_lib: Some(
                    concat!(
                        "                deps = []\n",
                        "                configs = []\n",
                        "                "
                    )
                    .to_string()
                )
            })
        );

        assert_eq!(
            toml::de::from_str(concat!(
                "version = \"3\"\n",
                "build-script-outputs = [\"generated.rs\"]\n",
            )),
            Ok(ThirdPartyFullDependency {
                default_features: true,
                version: Epoch::Major(3),
                features: vec![],
                allow_first_party_usage: true,
                build_script_outputs: vec!["generated.rs".to_string()],
                gn_variables_lib: None,
            })
        );
    }

    #[test]
    fn no_default_features() {
        assert_eq!(
            toml::de::from_str(concat!(
                "default-features = false\n",
                "version = \"1\"\n",
                "features = [\"foo\", \"bar\"]\n",
                "allow-first-party-usage = false\n",
                "build-script-outputs = [\"stuff.rs\"]\n",
                "gn-variables-lib = \"\"\"
                deps = []
                configs = []
                \"\"\""
            )),
            Ok(ThirdPartyFullDependency {
                default_features: false,
                version: Epoch::Major(1),
                features: vec!["foo".to_string(), "bar".to_string()],
                allow_first_party_usage: false,
                build_script_outputs: vec!["stuff.rs".to_string()],
                gn_variables_lib: Some(
                    concat!(
                        "                deps = []\n",
                        "                configs = []\n",
                        "                "
                    )
                    .to_string()
                )
            })
        );
    }

    #[test]
    fn parse_manifest() {
        let manifest: ThirdPartyManifest = toml::de::from_str(concat!(
            "[dependencies]\n",
            "cxx = \"1\"\n",
            "serde = \"1\"\n",
            "rustversion = {version = \"1\", build-script-outputs = [\"version.rs\"]}",
            "\n",
            "[dependencies.unicode-linebreak]\n",
            "version = \"0.1\"\n",
            "allow-first-party-usage = false\n",
            "build-script-outputs = [ \"table.rs\" ]\n",
            "\n",
            "[dependencies.special-stuff]\n",
            "version = \"0.1\"\n",
            "gn-variables-lib = \"hello = \\\"world\\\"\"\n",
            "\n",
            "[testonly-dependencies]\n",
            "syn = {version = \"1\", features = [\"full\"]}\n",
        ))
        .unwrap();

        assert_eq!(
            manifest.dependencies.get("cxx"),
            Some(&ThirdPartyDependency::Short(Epoch::Major(1)))
        );
        assert_eq!(
            manifest.dependencies.get("serde"),
            Some(&ThirdPartyDependency::Short(Epoch::Major(1)))
        );

        assert_eq!(
            manifest.dependencies.get("rustversion"),
            Some(&Dependency::Full(ThirdPartyFullDependency {
                default_features: true,
                version: Epoch::Major(1),
                features: vec![],
                allow_first_party_usage: true,
                build_script_outputs: vec!["version.rs".to_string()],
                gn_variables_lib: None,
            }))
        );

        assert_eq!(
            manifest.dependencies.get("unicode-linebreak"),
            Some(&Dependency::Full(ThirdPartyFullDependency {
                default_features: true,
                version: Epoch::Minor(1),
                features: vec![],
                allow_first_party_usage: false,
                build_script_outputs: vec!["table.rs".to_string()],
                gn_variables_lib: None,
            }))
        );

        assert_eq!(
            manifest.dependencies.get("special-stuff"),
            Some(&Dependency::Full(ThirdPartyFullDependency {
                default_features: true,
                version: Epoch::Minor(1),
                features: vec![],
                allow_first_party_usage: true,
                build_script_outputs: vec![],
                gn_variables_lib: Some("hello = \"world\"".to_string()),
            }))
        );

        assert_eq!(
            manifest.testonly_dependencies.get("syn"),
            Some(&Dependency::Full(ThirdPartyFullDependency {
                default_features: true,
                version: Epoch::Major(1),
                features: vec!["full".to_string()],
                allow_first_party_usage: true,
                build_script_outputs: vec![],
                gn_variables_lib: None,
            }))
        );
    }

    #[test]
    fn serialize_manifest_with_patches() {
        let manifest = CargoManifest {
            package: CargoPackage {
                name: "chromium".to_string(),
                version: Version::new(0, 1, 0),
                authors: Vec::new(),
                edition: Edition("2021".to_string()),
                description: None,
                license: "funtimes".to_string(),
                include: Vec::new(),
            },
            workspace: None,
            dependencies: CargoDependencySet::new(),
            patches: vec![(
                "crates-io".to_string(),
                vec![(
                    "foo_v1".to_string(),
                    CargoPatch {
                        path: "third_party/rust/foo/v1/crate".to_string(),
                        package: "foo".to_string(),
                    },
                )]
                .into_iter()
                .collect(),
            )]
            .into_iter()
            .collect(),
        };

        assert_eq!(
            toml::to_string(&manifest).unwrap(),
            "[package]
name = \"chromium\"
version = \"0.1.0\"
edition = \"2021\"
license = \"funtimes\"
[patch.crates-io.foo_v1]
path = \"third_party/rust/foo/v1/crate\"
package = \"foo\"
"
        )
    }

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
