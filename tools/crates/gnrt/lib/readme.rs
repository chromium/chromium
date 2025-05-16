// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::config::BuildConfig;
use crate::crates;
use crate::group::Group;
use crate::paths::{self, get_vendor_dir_for_package};
use anyhow::{bail, format_err, Result};
use guppy::graph::PackageMetadata;
use guppy::PackageId;
use itertools::Itertools;
use semver::Version;
use serde::Deserialize;
use serde::Serialize;
use std::collections::HashMap;
use std::fmt::Display;
use std::path::{Path, PathBuf};
use std::sync::LazyLock;

#[derive(Clone, Debug, Serialize)]
pub struct ReadmeFile {
    name: String,
    url: String,
    description: String,
    version: Version,
    security_critical: &'static str,
    shipped: &'static str,
    license: String,
    license_files: Vec<String>,
    revision: Option<String>,
}

/// Returns a map keyed by the directory where the README file should be
/// written. The value is the contents of the README file, which can be
/// consumed by a handlebars template.
pub fn readme_files_from_packages<'a>(
    deps: impl IntoIterator<Item = PackageMetadata<'a>>,
    paths: &paths::ChromiumPaths,
    extra_config: &BuildConfig,
    mut find_group: impl FnMut(&'a PackageId) -> Group,
    mut find_security_critical: impl FnMut(&'a PackageId) -> Option<bool>,
    mut find_shipped: impl FnMut(&'a PackageId) -> Option<bool>,
) -> Result<HashMap<PathBuf, ReadmeFile>> {
    let mut map = HashMap::new();

    for package in deps {
        let (dir, readme) = readme_file_from_package(
            package,
            paths,
            extra_config,
            &mut find_group,
            &mut find_security_critical,
            &mut find_shipped,
        )?;
        map.insert(dir, readme);
    }

    Ok(map)
}

pub fn readme_file_from_package<'a>(
    package: PackageMetadata<'a>,
    paths: &paths::ChromiumPaths,
    extra_config: &BuildConfig,
    find_group: &mut dyn FnMut(&'a PackageId) -> Group,
    find_security_critical: &mut dyn FnMut(&'a PackageId) -> Option<bool>,
    find_shipped: &mut dyn FnMut(&'a PackageId) -> Option<bool>,
) -> Result<(PathBuf, ReadmeFile)> {
    let epoch = crates::Epoch::from_version(package.version());
    let dir = paths
        .third_party
        .join(crates::NormalizedName::from_crate_name(package.name()).to_string())
        .join(epoch.to_string());

    let crate_config = extra_config.per_crate_config.get(package.name());
    let crate_dir = paths
        .third_party_cargo_root
        .join("vendor")
        .join(get_vendor_dir_for_package(package.name(), package.version()));
    let group = find_group(package.id());

    let security_critical = find_security_critical(package.id()).unwrap_or(match group {
        Group::Safe | Group::Sandbox => true,
        Group::Test => false,
    });

    let shipped = find_shipped(package.id()).unwrap_or(match group {
        Group::Safe | Group::Sandbox => true,
        Group::Test => false,
    });

    let license = {
        if let Some(config_license) = crate_config.and_then(|config| config.license.clone()) {
            config_license
        } else if let Some(pkg_license) = package.license() {
            let license_kinds = parse_license_string(pkg_license)?;
            license_kinds_to_string(&license_kinds)
        } else {
            return Err(format_err!(
                "No license field found in Cargo.toml for {} crate",
                package.name()
            ));
        }
    };

    let license_files = if let Some(config_license_files) = crate_config.and_then(|config| {
        if config.license_files.is_empty() {
            None
        } else {
            Some(config.license_files.iter().map(Path::new))
        }
    }) {
        config_license_files
            .map(|p| format!("//{}", paths::normalize_unix_path_separator(&crate_dir.join(p))))
            .collect()
    } else if let Some(pkg_license) = package.license() {
        let license_kinds = parse_license_string(pkg_license)?;
        find_license_files_for_kinds(&license_kinds, &crate_dir)?
    } else {
        Vec::new()
    };

    if license_files.is_empty() {
        bail!(
            "License file not found for crate {name}.\n
             \n
             You can specify the `license_files` in `crate.{name}]` \
             section of the `gnrt_config.toml` to manually point out \
             a license file relative to the crate's root. \
             (Alternatively you can tweak `gnrt`'s source code to improve \
             its ability to recognize license files based on their name).",
            name = package.name()
        );
    }

    let revision = {
        if let Ok(file) = std::fs::File::open(
            paths
                .third_party_cargo_root
                .join("vendor")
                .join(get_vendor_dir_for_package(package.name(), package.version()))
                .join(".cargo_vcs_info.json"),
        ) {
            #[derive(Deserialize)]
            struct VcsInfo {
                git: GitInfo,
            }
            #[derive(Deserialize)]
            struct GitInfo {
                sha1: String,
            }

            let json: VcsInfo = serde_json::from_reader(file)?;
            Some(json.git.sha1)
        } else {
            None
        }
    };

    let readme = ReadmeFile {
        name: package.name().to_string(),
        url: format!("https://crates.io/crates/{}", package.name()),
        description: package.description().unwrap_or_default().to_string(),
        version: package.version().clone(),
        security_critical: if security_critical { "yes" } else { "no" },
        shipped: if shipped { "yes" } else { "no" },
        license,
        license_files,
        revision,
    };

    Ok((dir, readme))
}

/// REVIEW REQUIREMENT: When adding a new `LicenseKind`, please consult
/// `readme.rs-third-party-license-review.md`.
#[allow(clippy::upper_case_acronyms)]
#[derive(Debug, Eq, Hash, PartialEq, Clone, Copy)]
enum LicenseKind {
    /// https://spdx.org/licenses/Apache-2.0.html
    Apache2,

    /// https://spdx.org/licenses/BSD-3-Clause.html
    BSD3,

    /// https://spdx.org/licenses/MIT.html
    MIT,

    /// https://spdx.org/licenses/ISC.html
    ISC,

    /// https://spdx.org/licenses/Zlib.html
    Zlib,

    /// https://spdx.org/licenses/Unicode-3.0.html
    Unicode3,
}

impl Display for LicenseKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        // NOTE: The strings used below should match the SPDX "Short identifier"
        // which can be found on the https://spdx.org website.  (e.g. see
        // https://spdx.org/licenses/Apache-2.0.html).
        match self {
            LicenseKind::Apache2 => write!(f, "Apache-2.0"),
            LicenseKind::BSD3 => write!(f, "BSD-3-Clause"),
            LicenseKind::MIT => write!(f, "MIT"),
            LicenseKind::ISC => write!(f, "ISC"),
            LicenseKind::Zlib => write!(f, "Zlib"),
            LicenseKind::Unicode3 => write!(f, "Unicode-3.0"),
        }
    }
}

/// LICENSE_STRING_TO_LICENSE_KIND, converts licenses from the format they are
/// specified in Cargo.toml files from crates.io, to the LicenseKind that will
/// be written to README.chromium.
/// Each entry looks like the following:
/// h.insert(
///   "Cargo.toml string",
///   vec![LicenseKind::<License for README.chromium>]
/// );
static LICENSE_STRING_TO_LICENSE_KIND: LazyLock<HashMap<&'static str, Vec<LicenseKind>>> =
    LazyLock::new(|| {
        let mut h = HashMap::new();
        h.insert("Apache-2.0", vec![LicenseKind::Apache2]);
        h.insert("MIT OR Apache-2.0", vec![LicenseKind::Apache2]);
        h.insert("MIT/Apache-2.0", vec![LicenseKind::Apache2]);
        h.insert("MIT / Apache-2.0", vec![LicenseKind::Apache2]);
        h.insert("Apache-2.0 / MIT", vec![LicenseKind::Apache2]);
        h.insert("Apache-2.0 OR MIT", vec![LicenseKind::Apache2]);
        h.insert("Apache-2.0/MIT", vec![LicenseKind::Apache2]);
        h.insert(
            "(Apache-2.0 OR MIT) AND BSD-3-Clause",
            vec![LicenseKind::Apache2, LicenseKind::BSD3],
        );
        h.insert("MIT OR Apache-2.0 OR Zlib", vec![LicenseKind::Apache2]);
        h.insert("MIT", vec![LicenseKind::MIT]);
        h.insert("Unlicense OR MIT", vec![LicenseKind::MIT]);
        h.insert("Unlicense/MIT", vec![LicenseKind::MIT]);
        h.insert("Apache-2.0 OR BSL-1.0", vec![LicenseKind::Apache2]);
        h.insert("BSD-3-Clause", vec![LicenseKind::BSD3]);
        h.insert("ISC", vec![LicenseKind::ISC]);
        h.insert("MIT OR Zlib OR Apache-2.0", vec![LicenseKind::Apache2]);
        h.insert("Zlib OR Apache-2.0 OR MIT", vec![LicenseKind::Apache2]);
        h.insert("0BSD OR MIT OR Apache-2.0", vec![LicenseKind::Apache2]);
        h.insert(
            "(MIT OR Apache-2.0) AND Unicode-3.0",
            vec![LicenseKind::Apache2, LicenseKind::Unicode3],
        );
        h.insert("MIT AND (MIT OR Apache-2.0)", vec![LicenseKind::Apache2]);
        h.insert("Apache-2.0 WITH LLVM-exception OR Apache-2.0 OR MIT", vec![LicenseKind::Apache2]);
        h.insert("BSD-2-Clause OR Apache-2.0 OR MIT", vec![LicenseKind::Apache2]);
        h.insert("Unicode-3.0", vec![LicenseKind::Unicode3]);
        h.insert("Zlib", vec![LicenseKind::Zlib]);
        h
    });

static LICENSE_KIND_TO_LICENSE_FILES: LazyLock<HashMap<LicenseKind, Vec<&'static str>>> =
    LazyLock::new(|| {
        let mut h = HashMap::new();
        h.insert(
            LicenseKind::Apache2,
            vec![
                "LICENSE-APACHE",
                "LICENSE-APACHE.md",
                "LICENSE-APACHE.txt",
                "license-apache-2.0",
                "LICENSE.md",
                "LICENSE",
            ],
        );
        h.insert(
            LicenseKind::MIT,
            vec!["LICENSE-MIT", "LICENSE-MIT.txt", "LICENSE-MIT.md", "LICENSE.md", "LICENSE"],
        );
        h.insert(
            LicenseKind::BSD3,
            vec!["LICENSE-BSD", "LICENSE-BSD.txt", "LICENSE-BSD.md", "LICENSE.md", "LICENSE"],
        );
        h.insert(LicenseKind::ISC, vec!["LICENSE-ISC", "LICENSE.md", "LICENSE"]);
        h.insert(LicenseKind::Zlib, vec!["LICENSE-ZLIB", "LICENSE.md", "LICENSE"]);
        h.insert(LicenseKind::Unicode3, vec!["LICENSE-UNICODE", "LICENSE.md", "LICENSE"]);
        h
    });

/// Converts a license string from Cargo.toml into a Vec of LicenseKinds.
fn parse_license_string(pkg_license: &str) -> Result<Vec<LicenseKind>> {
    LICENSE_STRING_TO_LICENSE_KIND.get(pkg_license).cloned().ok_or_else(|| {
        format_err!("License '{}' not in LICENSE_STRING_TO_LICENSE_KIND", pkg_license)
    })
}

/// Converts a slice of LicenseKinds into a comma-separated string.
fn license_kinds_to_string(license_kinds: &[LicenseKind]) -> String {
    license_kinds.iter().join(", ")
}

/// Finds license files for the given license kinds in the crate directory.
fn find_license_files_for_kinds(
    license_kinds: &[LicenseKind],
    crate_dir: &Path,
) -> Result<Vec<String>> {
    let mut found_files = Vec::new();

    for kind in license_kinds {
        // Safe to unwrap because if a LicenseKind isn't in
        // LICENSE_KIND_TO_LICENSE_FILES, it's a bug in gnrt's implementation.
        let possible_files = LICENSE_KIND_TO_LICENSE_FILES.get(kind).unwrap_or_else(|| {
            panic!("Bug in gnrt: License kind {kind:?} not in LICENSE_KIND_TO_LICENSE_FILES")
        });

        // Try each possible file in priority order.
        for file in possible_files {
            let path = crate_dir.join(file);
            if path.try_exists()? {
                let normalized_path = format!("//{}", paths::normalize_unix_path_separator(&path));
                found_files.push(normalized_path);
                break; // Found highest priority file for this license kind.
            }
        }
    }

    // Check for duplicates using itertools.
    if found_files.iter().duplicates().count() > 0 {
        bail!("Duplicate license files found: {:?}", found_files);
    }

    Ok(found_files)
}
