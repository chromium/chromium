// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::config::BuildConfig;
use crate::group::Group;
use crate::paths::{self, get_build_dir_for_package, get_vendor_dir_for_package};
use anyhow::{bail, ensure, format_err, Context, Result};
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
use strum::IntoEnumIterator;
use strum_macros::EnumIter;

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
    update_mechanism: String,
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
        )
        .with_context(|| {
            format!(
                "Can't generate a `README.chromium` file for `{name}-{version}`.",
                name = package.name(),
                version = package.version(),
            )
        })?;
        map.insert(dir, readme);
    }

    Ok(map)
}

fn readme_file_from_package<'a>(
    package: PackageMetadata<'a>,
    paths: &paths::ChromiumPaths,
    extra_config: &BuildConfig,
    find_group: &mut dyn FnMut(&'a PackageId) -> Group,
    find_security_critical: &mut dyn FnMut(&'a PackageId) -> Option<bool>,
    find_shipped: &mut dyn FnMut(&'a PackageId) -> Option<bool>,
) -> Result<(PathBuf, ReadmeFile)> {
    let crate_build_dir = get_build_dir_for_package(paths, package.name(), package.version());
    let crate_vendor_dir = get_vendor_dir_for_package(paths, package.name(), package.version());

    let crate_config = extra_config.per_crate_config.get(package.name());
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

    let config_license_files = crate_config.and_then(|config| {
        let config_license_files = config
            .license_files
            .iter()
            .map(Path::new)
            .map(|p| crate_vendor_dir.join(p))
            .collect::<Vec<_>>();
        if config_license_files.is_empty() {
            None
        } else {
            Some(config_license_files)
        }
    });
    let license_files = if let Some(config_license_files) = config_license_files {
        for path in config_license_files.iter() {
            ensure!(
                does_license_file_exist(path)?,
                "`gnrt_config.toml` for `{crate_name}` crate listed \
                 a license file that doesn't actually exist: {path}",
                crate_name = package.name(),
                path = path.display(),
            );
        }
        config_license_files
            .into_iter()
            .map(|p| format!("//{}", paths::normalize_unix_path_separator(&p)))
            .collect()
    } else if let Some(pkg_license) = package.license() {
        let license_kinds = parse_license_string(pkg_license)?;
        find_license_files_for_kinds(&license_kinds, &crate_vendor_dir)?
    } else {
        Vec::new()
    };

    if license_files.is_empty() {
        bail!(
            r#"License file not found for crate `{name}-{version}`.

* If a license file exists but `gnrt` can't find it under
  `{crate_vendor_dir}` then,
    * Specify `license_files` in `[crate.{name}]`
      section of `chromium_crates_io/gnrt_config.toml`
    * Or tweak the `LICENSE_KIND_TO_LICENSE_FILES` map in
      `tools/crates/gnrt/lib/readme.rs` to teach `gnrt`
      about alternative filenames
* If the crate didn't publish the license, then this may need
  to be fixed upstream before the crate can be imported.
  See also:
    - https://crbug.com/369075726
    - https://github.com/brendanzab/codespan/pull/355
    - https://github.com/rust-lang/rustc-demangle/issues/72
    - https://github.com/udoprog/relative-path/pull/60"#,
            name = package.name(),
            version = package.version(),
            crate_vendor_dir = crate_vendor_dir.display(),
        );
    }

    let revision = {
        if let Ok(file) = std::fs::File::open(
            get_vendor_dir_for_package(paths, package.name(), package.version())
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
        // All Rust dependencies are granted a shared temporary exemption for
        // autorolling, because while they are manually rolled, they are
        // well managed.
        update_mechanism: "Manual (https://crbug.com/449898466)".to_string(),
    };

    Ok((crate_build_dir, readme))
}

/// REVIEW REQUIREMENT: When adding a new `LicenseKind`, please consult
/// `readme.rs-third-party-license-review.md`.
#[allow(clippy::upper_case_acronyms)]
#[derive(Debug, Eq, Hash, PartialEq, Clone, Copy, EnumIter)]
enum LicenseKind {
    /// https://spdx.org/licenses/Apache-2.0.html
    Apache2,

    /// https://spdx.org/licenses/BSD-3-Clause.html
    BSD3,

    /// https://spdx.org/licenses/MIT.html
    MIT,

    /// https://spdx.org/licenses/MPL-2.0.html
    MPL2,

    /// https://spdx.org/licenses/ISC.html
    ISC,

    /// https://spdx.org/licenses/Zlib.html
    Zlib,

    /// https://spdx.org/licenses/Unicode-3.0.html
    Unicode3,

    /// https://spdx.org/licenses/NCSA.html
    NCSA,

    /// https://spdx.org/licenses/BSL-1.0.html
    BSL,
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
            LicenseKind::MPL2 => write!(f, "MPL-2.0"),
            LicenseKind::ISC => write!(f, "ISC"),
            LicenseKind::NCSA => write!(f, "NCSA"),
            LicenseKind::Zlib => write!(f, "Zlib"),
            LicenseKind::Unicode3 => write!(f, "Unicode-3.0"),
            LicenseKind::BSL => write!(f, "BSL-1.0"),
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
        HashMap::from([
            ("Apache-2.0", vec![LicenseKind::Apache2]),
            ("MIT OR Apache-2.0", vec![LicenseKind::Apache2]),
            ("MIT/Apache-2.0", vec![LicenseKind::Apache2]),
            ("MIT / Apache-2.0", vec![LicenseKind::Apache2]),
            ("Apache-2.0 / MIT", vec![LicenseKind::Apache2]),
            ("Apache-2.0 OR MIT", vec![LicenseKind::Apache2]),
            ("Apache-2.0/MIT", vec![LicenseKind::Apache2]),
            ("(Apache-2.0 OR MIT) AND BSD-3-Clause", vec![LicenseKind::Apache2, LicenseKind::BSD3]),
            ("MIT OR Apache-2.0 OR Zlib", vec![LicenseKind::Apache2]),
            ("(MIT OR Apache-2.0) AND NCSA", vec![LicenseKind::Apache2, LicenseKind::NCSA]),
            ("MIT", vec![LicenseKind::MIT]),
            ("MPL-2.0", vec![LicenseKind::MPL2]),
            ("Unlicense OR MIT", vec![LicenseKind::MIT]),
            ("Unlicense/MIT", vec![LicenseKind::MIT]),
            ("Apache-2.0 OR BSL-1.0", vec![LicenseKind::Apache2]),
            ("BSD-3-Clause", vec![LicenseKind::BSD3]),
            ("ISC", vec![LicenseKind::ISC]),
            ("MIT OR Zlib OR Apache-2.0", vec![LicenseKind::Apache2]),
            ("Zlib OR Apache-2.0 OR MIT", vec![LicenseKind::Apache2]),
            ("0BSD OR MIT OR Apache-2.0", vec![LicenseKind::Apache2]),
            (
                "(MIT OR Apache-2.0) AND Unicode-3.0",
                vec![LicenseKind::Apache2, LicenseKind::Unicode3],
            ),
            ("MIT AND (MIT OR Apache-2.0)", vec![LicenseKind::Apache2]),
            ("Apache-2.0 WITH LLVM-exception OR Apache-2.0 OR MIT", vec![LicenseKind::Apache2]),
            ("BSD-2-Clause OR Apache-2.0 OR MIT", vec![LicenseKind::Apache2]),
            ("BSD-2-Clause OR Apache-2.0 OR MIT", vec![LicenseKind::Apache2]),
            ("BSD-2-Clause OR MIT OR Apache-2.0", vec![LicenseKind::Apache2]),
            ("BSD-3-Clause OR MIT OR Apache-2.0", vec![LicenseKind::Apache2]),
            ("Unicode-3.0", vec![LicenseKind::Unicode3]),
            ("Zlib", vec![LicenseKind::Zlib]),
            ("BSL-1.0", vec![LicenseKind::BSL]),
        ])
    });

static LICENSE_KIND_TO_LICENSE_FILES: LazyLock<HashMap<LicenseKind, Vec<String>>> =
    LazyLock::new(|| {
        const PREFIX: &str = "LICENSE";
        const EXTENSIONS: [&str; 3] = ["", ".md", ".txt"];

        // This block generates a map with the most common license file types, in order
        // of priority.
        let mut map = HashMap::new();
        for kind in LicenseKind::iter() {
            // The suffix for the license file name is taken from the Display
            // implementation. E.g. "Apache-2.0" becomes "APACHE"
            let license_suffix = kind.to_string().split("-").next().unwrap().to_uppercase();

            let mut license_files = vec![];
            // License types with the license-specific suffix are higher priority.
            for ext in EXTENSIONS {
                license_files.push(format!("{PREFIX}-{license_suffix}{ext}"));
            }
            // License types that are common to all licenses are lower priority.
            for ext in EXTENSIONS {
                license_files.push(format!("{PREFIX}{ext}"));
            }
            map.insert(kind, license_files);
        }

        // Special cases for specific license types. If your license has a case
        // that is not covered already in the map generated above, add it here.
        let apache = map.get_mut(&LicenseKind::Apache2).unwrap();
        apache.insert(0, "license-apache-2.0".to_string());
        apache.insert(0, "LICENSE-Apache".to_string());
        map
    });

/// Converts a license string from Cargo.toml into a Vec of LicenseKinds.
fn parse_license_string(pkg_license: &str) -> Result<Vec<LicenseKind>> {
    LICENSE_STRING_TO_LICENSE_KIND.get(pkg_license).cloned().ok_or_else(|| {
        format_err!(
            "License '{pkg_license}' not found in the `LICENSE_STRING_TO_LICENSE_KIND` \
             map.  Please consider teaching `gnrt` about this license kind \
             by editing //tools/crates/gnrt/lib/readme.rs` and adding the \
             license to `enum LicenseKinds`, `LICENSE_STRING_TO_LICENSE_KIND`, \
             and `LICENSE_KIND_TO_LICENSE_FILES`.  Note that this will require \
             an additional review whether the new license kind can be used in \
             Chromium - for more details see \
             `//tools/crates/gnrt/lib/readme.rs-third-party-license-review.md`."
        )
    })
}

/// Converts a slice of LicenseKinds into a comma-separated string.
fn license_kinds_to_string(license_kinds: &[LicenseKind]) -> String {
    license_kinds.iter().join(", ")
}

/// Finds license files for the given license kinds in the crate directory.
fn find_license_files_for_kinds(
    license_kinds: &[LicenseKind],
    crate_vendor_dir: &Path,
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
            let path = crate_vendor_dir.join(file);
            if does_license_file_exist(&path)? {
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

fn does_license_file_exist(path: &Path) -> Result<bool> {
    path.try_exists()
        .with_context(|| format!("Failed to check if a license file exists at {}", path.display()))
}
