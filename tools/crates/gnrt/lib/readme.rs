// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::config::BuildConfig;
use crate::crates;
use crate::group::Group;
use crate::paths;
use anyhow::{format_err, Result};
use semver::Version;
use serde::Deserialize;
use serde::Serialize;
use std::collections::HashMap;
use std::path::{Path, PathBuf};

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
    deps: impl IntoIterator<Item = &'a cargo_metadata::Package>,
    paths: &paths::ChromiumPaths,
    extra_config: &BuildConfig,
    mut find_group: impl FnMut(&'a cargo_metadata::PackageId) -> Group,
    mut find_security_critical: impl FnMut(&'a cargo_metadata::PackageId) -> Option<bool>,
    mut find_shipped: impl FnMut(&'a cargo_metadata::PackageId) -> Option<bool>,
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
    package: &'a cargo_metadata::Package,
    paths: &paths::ChromiumPaths,
    extra_config: &BuildConfig,
    find_group: &mut dyn FnMut(&'a cargo_metadata::PackageId) -> Group,
    find_security_critical: &mut dyn FnMut(&'a cargo_metadata::PackageId) -> Option<bool>,
    find_shipped: &mut dyn FnMut(&'a cargo_metadata::PackageId) -> Option<bool>,
) -> Result<(PathBuf, ReadmeFile)> {
    let epoch = crates::Epoch::from_version(&package.version);
    let dir = paths
        .third_party
        .join(crates::NormalizedName::from_crate_name(&package.name).to_string())
        .join(epoch.to_string());

    let crate_config = extra_config.per_crate_config.get(&package.name);
    let crate_dir = paths
        .third_party_cargo_root
        .join("vendor")
        .join(format!("{}-{}", package.name, package.version));
    let group = find_group(&package.id);

    let security_critical = find_security_critical(&package.id).unwrap_or(match group {
        Group::Safe | Group::Sandbox => true,
        Group::Test => false,
    });

    let shipped = find_shipped(&package.id).unwrap_or(match group {
        Group::Safe | Group::Sandbox => true,
        Group::Test => false,
    });

    let license = {
        if let Some(config_license) = crate_config.and_then(|config| config.license.clone()) {
            config_license
        } else if let Some(pkg_license) = &package.license {
            // Map to something in ALLOWED_LICENSES.
            if let Some(mapped_license) = ALLOWED_LICENSES
                .iter()
                .find(|(allowed_license, _)| pkg_license == *allowed_license)
                .map(|(_, mapped_license)| *mapped_license)
            {
                mapped_license.to_owned()
            } else {
                return Err(format_err!(
                    "License '{}' in Cargo.toml for {} crate is not in ALLOWED_LICENSES",
                    pkg_license,
                    package.name,
                ));
            }
        } else {
            return Err(format_err!(
                "No license field found in Cargo.toml for {} crate",
                package.name
            ));
        }
    };

    let path_if_exists = |path: &'a Path| -> Result<Option<&'a Path>> {
        if crate_dir.join(path).try_exists()? { Ok(Some(path)) } else { Ok(None) }
    };
    let to_crate_dir_string = |path: &Path| -> String {
        format!("//{}", paths::normalize_unix_path_separator(&crate_dir.join(path)))
    };

    let license_files: Vec<String> = {
        if let Some(config_license_files) = crate_config.and_then(|config| {
            if config.license_files.is_empty() {
                None
            } else {
                Some(config.license_files.iter().map(Path::new))
            }
        }) {
            config_license_files.map(to_crate_dir_string).collect()
        } else if let Some(file) = &package.license_file {
            path_if_exists(file.as_std_path())?.into_iter().map(to_crate_dir_string).collect()
        } else {
            EXPECTED_LICENSE_FILE
                .iter()
                .filter_map(|(l, path)| {
                    if license == **l {
                        path_if_exists(Path::new(path)).unwrap_or(None).map(to_crate_dir_string)
                    } else {
                        None
                    }
                })
                .collect()
        }
    };
    if license_files.is_empty() && shipped {
        log::warn!(
            "License file not found for crate {name}.\n  Crates that are \
            marked `shipped` must specify a License File.\n  You can specify \
            the `license_files` in [crate.{name}] relative to the crate's root \
            directory.",
            name = package.name
        );
    }

    let revision = {
        if let Ok(file) = std::fs::File::open(
            paths
                .third_party_cargo_root
                .join("vendor")
                .join(format!("{}-{}", package.name, package.version))
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
        name: package.name.clone(),
        url: format!("https://crates.io/crates/{}", package.name),
        description: package.description.clone().unwrap_or_default(),
        version: package.version.clone(),
        security_critical: if security_critical { "yes" } else { "no" },
        shipped: if shipped { "yes" } else { "no" },
        license,
        license_files,
        revision,
    };

    Ok((dir, readme))
}

// Allowed licenses, in the format they are specified in Cargo.toml files from
// crates.io, and the format to write to README.chromium.
static ALLOWED_LICENSES: [(&str, &str); 21] = [
    // ("Cargo.toml string", "License for README.chromium")
    ("Apache-2.0", "Apache 2.0"),
    ("MIT OR Apache-2.0", "Apache 2.0"),
    ("MIT/Apache-2.0", "Apache 2.0"),
    ("MIT / Apache-2.0", "Apache 2.0"),
    ("Apache-2.0 / MIT", "Apache 2.0"),
    ("Apache-2.0 OR MIT", "Apache 2.0"),
    ("Apache-2.0/MIT", "Apache 2.0"),
    ("(Apache-2.0 OR MIT) AND BSD-3-Clause", "Apache 2.0 | BSD 3-Clause"),
    ("MIT OR Apache-2.0 OR Zlib", "Apache 2.0"),
    ("MIT", "MIT"),
    ("Unlicense OR MIT", "MIT"),
    ("Unlicense/MIT", "MIT"),
    ("Apache-2.0 OR BSL-1.0", "Apache 2.0"),
    ("BSD-3-Clause", "BSD 3-Clause"),
    ("ISC", "ISC"),
    ("MIT OR Zlib OR Apache-2.0", "Apache 2.0"),
    ("Zlib OR Apache-2.0 OR MIT", "Apache 2.0"),
    ("0BSD OR MIT OR Apache-2.0", "Apache 2.0"),
    (
        "(MIT OR Apache-2.0) AND Unicode-DFS-2016",
        "Apache 2.0 AND Unicode License Agreement - Data Files and Software (2016)",
    ),
    ("Apache-2.0 WITH LLVM-exception OR Apache-2.0 OR MIT", "Apache 2.0"),
    ("BSD-2-Clause OR Apache-2.0 OR MIT", "Apache 2.0"),
];

static EXPECTED_LICENSE_FILE: [(&str, &str); 20] = [
    ("Apache 2.0", "LICENSE"),
    ("Apache 2.0", "LICENSE.md"),
    ("Apache 2.0", "LICENSE-APACHE"),
    ("Apache 2.0", "LICENSE-APACHE.txt"),
    ("Apache 2.0", "LICENSE-APACHE.md"),
    ("MIT", "LICENSE"),
    ("MIT", "LICENSE.md"),
    ("MIT", "LICENSE-MIT"),
    ("MIT", "LICENSE-MIT.txt"),
    ("MIT", "LICENSE-MIT.md"),
    ("BSD 3-Clause", "LICENSE"),
    ("BSD 3-Clause", "LICENSE.md"),
    ("BSD 3-Clause", "LICENSE-BSD"),
    ("BSD 3-Clause", "LICENSE-BSD.txt"),
    ("BSD 3-Clause", "LICENSE-BSD.md"),
    ("ISC", "LICENSE"),
    ("ISC", "LICENSE.md"),
    ("ISC", "LICENSE-ISC"),
    ("Apache 2.0 | BSD 3-Clause", "LICENSE"),
    ("Apache 2.0 | BSD 3-Clause", "LICENSE.md"),
];
