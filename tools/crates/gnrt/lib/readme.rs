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
use std::path::{Path, PathBuf};
use std::sync::LazyLock;
use strum::IntoEnumIterator;
use strum_macros::{AsRefStr, Display, EnumIter, EnumString};

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
    deps: impl IntoIterator<Item = &'a PackageMetadata<'a>>,
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
    package: &'a PackageMetadata<'a>,
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
///
/// Note that the order of variants is significant - `Apache2` appears first
/// and so is preferred in license expressions like `Apache-2.0 OR MIT`.
#[allow(clippy::upper_case_acronyms)]
#[derive(Debug, Eq, Hash, PartialEq, Clone, Copy, AsRefStr, Display, EnumIter, EnumString)]
enum LicenseKind {
    /// https://spdx.org/licenses/Apache-2.0.html
    #[strum(serialize = "Apache-2.0")]
    Apache2,

    /// https://spdx.org/licenses/BSD-3-Clause.html
    #[strum(serialize = "BSD-3-Clause")]
    BSD3,

    /// https://spdx.org/licenses/MIT.html
    #[strum(serialize = "MIT")]
    MIT,

    /// https://spdx.org/licenses/MPL-2.0.html
    #[strum(serialize = "MPL-2.0")]
    MPL2,

    /// https://spdx.org/licenses/ISC.html
    #[strum(serialize = "ISC")]
    ISC,

    /// https://spdx.org/licenses/Zlib.html
    #[strum(serialize = "Zlib")]
    Zlib,

    /// https://spdx.org/licenses/Unicode-3.0.html
    #[strum(serialize = "Unicode-3.0")]
    Unicode3,

    /// https://spdx.org/licenses/NCSA.html
    #[strum(serialize = "NCSA")]
    NCSA,

    /// https://spdx.org/licenses/BSL-1.0.html
    #[strum(serialize = "BSL-1.0")]
    BSL,
}

impl<'a> TryFrom<&'a spdx::LicenseReq> for LicenseKind {
    type Error = anyhow::Error;

    fn try_from(req: &'a spdx::LicenseReq) -> Result<LicenseKind> {
        ensure!(
            req.addition.is_none(),
            "`gnrt` cannot yet handle SPDX additions (e.g. WITH clauses)",
        );
        Ok(match req.license {
            spdx::LicenseItem::Spdx { id, or_later: _ } => id.name.parse()?,
            spdx::LicenseItem::Other { .. } => bail!("`gnrt` cannot handle SPDX references"),
        })
    }
}

static LICENSE_KIND_TO_LICENSE_FILES: LazyLock<HashMap<LicenseKind, Vec<String>>> =
    LazyLock::new(|| {
        const PREFIXES: [&str; 2] = [
            // Most common spelling.
            "LICENSE",
            // British English spelling (e.g. found in the `pad-0.1.6` crate).
            "LICENCE",
        ];
        const EXTENSIONS: [&str; 3] = ["", ".md", ".txt"];

        // This block generates a map with the most common license file types, in order
        // of priority.
        let mut map = HashMap::new();
        for kind in LicenseKind::iter() {
            let license_name_suffix = {
                // `license_kind_name` is taken from the Display
                // implementation. E.g. "Apache-2.0" becomes "-APACHE"
                let license_kind_name = kind.as_ref().split("-").next().unwrap().to_uppercase();
                // Prepend `-` so this can be used as an optional suffix.
                format!("-{license_kind_name}")
            };
            // More specific `license_name_suffix` is prioritized and listed first.
            let suffixes: [&str; 2] = [&license_name_suffix, ""];

            let mut license_files = vec![];
            for suffix in suffixes {
                for prefix in PREFIXES {
                    for extension in EXTENSIONS {
                        license_files.push(format!("{prefix}{suffix}{extension}"));
                    }
                }
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
    fn parse(pkg_license: &str) -> Result<Vec<LicenseKind>> {
        let expr = spdx::expression::Expression::parse_mode(
            pkg_license,
            spdx::lexer::ParseMode {
                allow_imprecise_license_names: false,
                allow_postfix_plus_on_gpl: false,
                allow_deprecated: false,

                // https://spdx.dev/wp-content/uploads/sites/31/2024/12/SPDX-3.0.1-1.pdf
                // technically requires that `License1 / License2` should be spelled as
                // `License1 OR License2`, but in practice many `Cargo.toml` files use
                // the former syntax.  See also
                // https://github.com/rust-lang/cargo/issues/2039
                allow_slash_as_or_operator: true,
            },
        )?;

        static ALLOWED_LICENSES_IN_PRIORITY_ORDER: LazyLock<Vec<spdx::Licensee>> =
            LazyLock::new(|| {
                LicenseKind::iter()
                    .map(|license_kind| spdx::Licensee::parse(license_kind.as_ref()).unwrap())
                    .collect_vec()
            });

        expr.minimized_requirements(ALLOWED_LICENSES_IN_PRIORITY_ORDER.iter())?
            .iter()
            .map(LicenseKind::try_from)
            .collect::<Result<Vec<_>>>()
    }

    parse(pkg_license).with_context(|| {
        format!(
            "License '{pkg_license}' could not be parsed by `gnrt`. \
             To teach `gnrt` about a new licence kind, please consider \
             adding a new variant to the `LicenseKind` enum in \
             //tools/crates/gnrt/lib/readme.rs`.  Note that this will require \
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

#[cfg(test)]
mod test {
    use crate::readme::LicenseKind;

    use super::{license_kinds_to_string, parse_license_string, LICENSE_KIND_TO_LICENSE_FILES};

    #[test]
    fn test_convert_license_field_from_cargo_to_chromium_format() {
        let testcases = [
            ("Apache-2.0", "Apache-2.0"),
            ("MIT OR Apache-2.0", "Apache-2.0"),
            ("MIT/Apache-2.0", "Apache-2.0"),
            ("MIT / Apache-2.0", "Apache-2.0"),
            ("Apache-2.0 / MIT", "Apache-2.0"),
            ("Apache-2.0 OR MIT", "Apache-2.0"),
            ("Apache-2.0/MIT", "Apache-2.0"),
            ("(Apache-2.0 OR MIT) AND BSD-3-Clause", "Apache-2.0, BSD-3-Clause"),
            ("MIT OR Apache-2.0 OR Zlib", "Apache-2.0"),
            ("(MIT OR Apache-2.0) AND NCSA", "Apache-2.0, NCSA"),
            ("MIT", "MIT"),
            ("MPL-2.0", "MPL-2.0"),
            ("Unlicense OR MIT", "MIT"),
            ("Unlicense/MIT", "MIT"),
            ("Apache-2.0 OR BSL-1.0", "Apache-2.0"),
            ("BSD-3-Clause", "BSD-3-Clause"),
            ("ISC", "ISC"),
            ("MIT OR Zlib OR Apache-2.0", "Apache-2.0"),
            ("Zlib OR Apache-2.0 OR MIT", "Apache-2.0"),
            ("0BSD OR MIT OR Apache-2.0", "Apache-2.0"),
            ("(MIT OR Apache-2.0) AND Unicode-3.0", "Apache-2.0, Unicode-3.0"),
            ("MIT AND (MIT OR Apache-2.0)", "MIT"),
            ("Apache-2.0 WITH LLVM-exception OR Apache-2.0 OR MIT", "Apache-2.0"),
            ("BSD-2-Clause OR Apache-2.0 OR MIT", "Apache-2.0"),
            ("BSD-2-Clause OR Apache-2.0 OR MIT", "Apache-2.0"),
            ("BSD-2-Clause OR MIT OR Apache-2.0", "Apache-2.0"),
            ("BSD-3-Clause OR MIT OR Apache-2.0", "Apache-2.0"),
            ("Unicode-3.0", "Unicode-3.0"),
            ("Zlib", "Zlib"),
            ("BSL-1.0", "BSL-1.0"),
        ];

        for (input, expected_output) in testcases {
            eprintln!("Testing input = {input:?}");
            let licenses = parse_license_string(input).unwrap();
            let actual_output = license_kinds_to_string(&licenses);
            assert_eq!(&actual_output, expected_output);
        }
    }

    #[test]
    fn test_license_kind_to_possible_license_files() {
        let testcases: &[(LicenseKind, &[&'static str])] = &[
            (
                LicenseKind::Apache2,
                &[
                    "LICENSE-Apache",
                    "license-apache-2.0",
                    "LICENSE-APACHE",
                    "LICENSE-APACHE.md",
                    "LICENSE-APACHE.txt",
                    "LICENCE-APACHE",
                    "LICENCE-APACHE.md",
                    "LICENCE-APACHE.txt",
                    "LICENSE",
                    "LICENSE.md",
                    "LICENSE.txt",
                    "LICENCE",
                    "LICENCE.md",
                    "LICENCE.txt",
                ],
            ),
            (
                LicenseKind::MIT,
                &[
                    "LICENSE-MIT",
                    "LICENSE-MIT.md",
                    "LICENSE-MIT.txt",
                    "LICENCE-MIT",
                    "LICENCE-MIT.md",
                    "LICENCE-MIT.txt",
                    "LICENSE",
                    "LICENSE.md",
                    "LICENSE.txt",
                    "LICENCE",
                    "LICENCE.md",
                    "LICENCE.txt",
                ],
            ),
        ];
        for (license, expected_filenames) in testcases {
            let actual_filenames = &LICENSE_KIND_TO_LICENSE_FILES[license];
            assert_eq!(actual_filenames, expected_filenames);
        }
    }
}
