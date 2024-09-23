// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Utilities to handle vendored third-party crates.

use crate::config::BuildConfig;
use crate::deps;
use crate::manifest;

use std::fmt::{self, Display};
use std::fs;
use std::hash::Hash;
use std::io;
use std::path::{Path, PathBuf};
use std::str::FromStr;

use anyhow::Context;
use log::error;
use semver::Version;
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Visibility {
    /// The crate can be used by any build targets.
    Public,
    /// The crate can be used by only third-party crates.
    ThirdParty,
    /// The crate can be used by any test target, and in production by
    /// third-party crates.
    TestOnlyAndThirdParty,
}

/// Returns a default of `ThirdParty`, which is the most conservative option and
/// generally what we want if one isn't explicitly computed.
impl std::default::Default for Visibility {
    fn default() -> Self {
        Visibility::ThirdParty
    }
}

/// A normalized version as used in third_party/rust crate paths.
///
/// A crate version is identified by the major version, if it's >= 1, or the
/// minor version, if the major version is 0. There is a many-to-one
/// relationship between crate versions and epochs.
///
/// `Epoch` is serialized as a version string: e.g. "1" or "0.2".
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, Ord, PartialEq, PartialOrd, Serialize)]
#[serde(from = "EpochString", into = "EpochString")]
pub enum Epoch {
    /// Epoch with major version == 0. The field is the minor version. It is an
    /// error to use 0: methods may panic in this case.
    Minor(u64),
    /// Epoch with major version >= 1. It is an error to use 0: methods may
    /// panic in this case.
    Major(u64),
}

impl Epoch {
    /// Get the semver version string for this Epoch. This will only have a
    /// non-zero major component, or a zero major component and a non-zero minor
    /// component. Note this differs from Epoch's `fmt::Display` impl.
    pub fn to_version_string(&self) -> String {
        match *self {
            // These should never return Err since formatting an integer is
            // infallible.
            Epoch::Minor(minor) => {
                assert_ne!(minor, 0);
                format!("0.{minor}")
            }
            Epoch::Major(major) => {
                assert_ne!(major, 0);
                format!("{major}")
            }
        }
    }

    /// A `semver::VersionReq` that matches any version of this epoch.
    pub fn to_version_req(&self) -> semver::VersionReq {
        let (major, minor) = match self {
            Self::Minor(x) => (0, Some(*x)),
            Self::Major(x) => (*x, None),
        };
        semver::VersionReq {
            comparators: vec![semver::Comparator {
                // "^1" is the same as "1" in Cargo.toml.
                op: semver::Op::Caret,
                major,
                minor,
                patch: None,
                pre: semver::Prerelease::EMPTY,
            }],
        }
    }

    /// Compute the Epoch from a `semver::Version`. This is useful since we can
    /// parse versions from `cargo_metadata` and in Cargo.toml files using the
    /// `semver` library.
    pub fn from_version(version: &Version) -> Self {
        match version.major {
            0 => Self::Minor(version.minor),
            x => Self::Major(x),
        }
    }

    /// Get the requested epoch from a supported dependency version string.
    /// `req` should be a version request as used in Cargo.toml's [dependencies]
    /// section.
    ///
    /// `req` must use the default strategy as defined in
    /// https://doc.rust-lang.org/cargo/reference/specifying-dependencies.html#specifying-dependencies-from-cratesio
    pub fn from_version_req_str(req: &str) -> Self {
        // For convenience, leverage semver::VersionReq for parsing even
        // though we don't need the full expressiveness.
        let req = semver::VersionReq::from_str(req).unwrap();
        // We require the spec to have exactly one comparator, which must use
        // the default strategy.
        assert_eq!(req.comparators.len(), 1);
        let comp: &semver::Comparator = &req.comparators[0];
        // Caret is semver's name for the default strategy.
        assert_eq!(comp.op, semver::Op::Caret);
        match (comp.major, comp.minor) {
            (0, Some(0) | None) => panic!("invalid version req {req}"),
            (0, Some(x)) => Epoch::Minor(x),
            (x, _) => Epoch::Major(x),
        }
    }
}

// This gives us a ToString implementation for free.
impl Display for Epoch {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            // These should never return Err since formatting an integer is
            // infallible.
            Epoch::Minor(minor) => {
                assert_ne!(minor, 0);
                f.write_fmt(format_args!("v0_{minor}")).unwrap()
            }
            Epoch::Major(major) => {
                assert_ne!(major, 0);
                f.write_fmt(format_args!("v{major}")).unwrap()
            }
        }

        Ok(())
    }
}

impl FromStr for Epoch {
    type Err = EpochParseError;

    /// A valid input string is of the form:
    /// * "v{i}", where i >= 1, or
    /// * "v0_{i}", where i >= 1
    ///
    /// Any other string is invalid. If the "v" is missing, there are extra
    /// underscore-separated components, or there are two numbers but both
    /// are 0 or greater than zero are all invalid strings.
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        // Split off the "v" prefix.
        let mut iter = s.split_inclusive('v');
        if iter.next() != Some("v") {
            return Err(EpochParseError::BadFormat);
        }
        let s = iter.next().ok_or(EpochParseError::BadFormat)?;
        if iter.next().is_some() {
            return Err(EpochParseError::BadFormat);
        }

        // Split the major and minor version numbers.
        let mut parts = s.split('_');
        let major: Option<u64> =
            parts.next().map(|s| s.parse().map_err(EpochParseError::InvalidInt)).transpose()?;
        let minor: Option<u64> =
            parts.next().map(|s| s.parse().map_err(EpochParseError::InvalidInt)).transpose()?;

        // Get the final epoch, checking that the (major, minor) pair is valid.
        let result = match (major, minor) {
            (Some(0), Some(0)) => Err(EpochParseError::BadVersion),
            (Some(0), Some(minor)) => Ok(Epoch::Minor(minor)),
            (Some(major), None) => Ok(Epoch::Major(major)),
            (Some(_), Some(_)) => Err(EpochParseError::BadVersion),
            (None, None) => Err(EpochParseError::BadFormat),
            _ => unreachable!(),
        }?;

        // Ensure there's no remaining parts.
        if parts.next().is_none() { Ok(result) } else { Err(EpochParseError::BadFormat) }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum EpochParseError {
    /// An integer could not be parsed where expected.
    InvalidInt(std::num::ParseIntError),
    /// The string was not formatted correctly. It was missing the 'v' prefix,
    /// was missing the '_' separator, or had a tail after the last integer.
    BadFormat,
    /// The epoch had an invalid combination of versions: e.g. "v0_0", "v1_0",
    /// "v1_1".
    BadVersion,
}

impl Display for EpochParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use EpochParseError::*;
        match self {
            InvalidInt(parse_int_error) => parse_int_error.fmt(f),
            BadFormat => f.write_str("epoch string had incorrect format"),
            BadVersion => f.write_str("epoch string had invalid version"),
        }
    }
}

impl std::error::Error for EpochParseError {}

/// A crate name normalized to the format we use in //third_party.
#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct NormalizedName(String);

impl NormalizedName {
    /// Wrap a normalized name, checking that it is valid.
    pub fn new(normalized_name: &str) -> Option<NormalizedName> {
        let converted_name = Self::from_crate_name(normalized_name);
        if converted_name.0 == normalized_name { Some(converted_name) } else { None }
    }

    /// Normalize a crate name. `crate_name` is the name Cargo uses to refer to
    /// the crate.
    pub fn from_crate_name(crate_name: &str) -> NormalizedName {
        NormalizedName(
            crate_name
                .chars()
                .map(|c| match c {
                    '-' | '.' => '_',
                    c => c,
                })
                .collect(),
        )
    }

    /// Get the wrapped string.
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl fmt::Display for NormalizedName {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

/// Identifies a crate available in some vendored source. Each crate is uniquely
/// identified by its Cargo.toml package name and version.
#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct VendoredCrate {
    pub name: String,
    pub version: Version,
}

impl fmt::Display for VendoredCrate {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} {}", self.name, self.version)
    }
}

impl VendoredCrate {
    pub fn normalized_name(&self) -> NormalizedName {
        NormalizedName::from_crate_name(&self.name)
    }
}

pub struct CrateFiles {
    /// The list of all source files that are part of the crate and may be used
    /// by rustc when building any part of the crate, as absolute paths. These
    /// files are those found under the crate root.
    pub sources: Vec<PathBuf>,
    /// The list of all input files that are part of the crate and may be used
    /// by rustc when building any part of the crate, as absolute paths. This
    /// may contain .rs files as well that are part of other crates and which
    /// may be include()'d or used through module paths.
    pub inputs: Vec<PathBuf>,
    /// The list of all native lib files that are part of the crate and may be
    /// depended on through `#[link]` directives. These files are those found
    /// under the crate root.
    pub native_libs: Vec<PathBuf>,
    /// Like `sources` but for the crate's build script.
    pub build_script_sources: Vec<PathBuf>,
    /// Like `inputs` but for the crate's build script.
    pub build_script_inputs: Vec<PathBuf>,
}

impl CrateFiles {
    fn new() -> Self {
        Self {
            sources: vec![],
            inputs: vec![],
            native_libs: vec![],
            build_script_sources: vec![],
            build_script_inputs: vec![],
        }
    }

    /// Sorts the CrateFiles for a deterministic output.
    fn sort(&mut self) {
        self.sources.sort_unstable();
        self.inputs.sort_unstable();
        self.native_libs.sort_unstable();
        self.build_script_sources.sort_unstable();
        self.build_script_inputs.sort_unstable();
    }
}

/// Get the subdir name containing `id` in a `cargo vendor` directory.
pub fn std_crate_path(id: &VendoredCrate) -> PathBuf {
    format!("{}-{}", id.name, id.version).into()
}

#[derive(Debug, PartialEq, Eq)]
pub enum IncludeCrateTargets {
    LibOnly,
    LibAndBin,
}

/// Collect the source and input files (i.e. `CrateFiles`) for each library that
/// is part of the build.
pub fn collect_crate_files(
    p: &deps::Package,
    config: &BuildConfig,
    include_targets: IncludeCrateTargets,
) -> anyhow::Result<(VendoredCrate, CrateFiles)> {
    let crate_config = config.per_crate_config.get(&p.crate_id().name);

    let mut files = CrateFiles::new();

    struct RootDir {
        path: PathBuf,
        collect: CollectCrateFiles,
    }

    let mut root_dirs = Vec::new();
    if let Some(lib_target) = p.lib_target.as_ref() {
        let lib_root = lib_target.root.parent().expect("lib target has no directory in its path");
        root_dirs.push(RootDir { path: lib_root.to_owned(), collect: CollectCrateFiles::Internal });

        root_dirs.extend(
            crate_config
                .iter()
                .flat_map(|crate_config| &crate_config.extra_src_roots)
                .chain(&config.all_config.extra_src_roots)
                .map(|path| RootDir {
                    path: lib_root.join(path),
                    collect: CollectCrateFiles::ExternalSourcesAndInputs,
                }),
        );
        root_dirs.extend(
            crate_config
                .iter()
                .flat_map(|crate_config| &crate_config.extra_input_roots)
                .chain(&config.all_config.extra_input_roots)
                .map(|path| RootDir {
                    path: lib_root.join(path),
                    collect: CollectCrateFiles::ExternalInputsOnly,
                }),
        );
        root_dirs.extend(
            crate_config
                .iter()
                .flat_map(|crate_config| &crate_config.extra_build_script_src_roots)
                .chain(&config.all_config.extra_build_script_src_roots)
                .map(|path| RootDir {
                    path: lib_root.join(path),
                    collect: CollectCrateFiles::BuildScriptExternalSourcesAndInputs,
                }),
        );
        root_dirs.extend(
            crate_config
                .iter()
                .flat_map(|crate_config| &crate_config.extra_build_script_input_roots)
                .chain(&config.all_config.extra_build_script_input_roots)
                .map(|path| RootDir {
                    path: lib_root.join(path),
                    collect: CollectCrateFiles::BuildScriptExternalInputsOnly,
                }),
        );

        root_dirs.extend(
            crate_config
                .iter()
                .flat_map(|crate_config| &crate_config.native_libs_roots)
                .chain(&config.all_config.native_libs_roots)
                .map(|path| RootDir {
                    path: lib_root.join(path),
                    collect: CollectCrateFiles::LibsOnly,
                }),
        );
    }
    if include_targets == IncludeCrateTargets::LibAndBin {
        for bin in &p.bin_targets {
            let bin_root = bin.root.parent().expect("bin target has no directory in its path");
            root_dirs
                .push(RootDir { path: bin_root.to_owned(), collect: CollectCrateFiles::Internal });
        }
    }

    for root_dir in root_dirs {
        recurse_crate_files(&root_dir.path, &mut |filepath| {
            collect_crate_file(&mut files, root_dir.collect, filepath)
        })?;
    }
    files.sort();

    let crate_id = VendoredCrate { name: p.package_name.clone(), version: p.version.clone() };
    Ok((crate_id, files))
}

/// Traverse vendored third-party crates in the Rust source package. Each
/// `VendoredCrate` is paired with the package metadata from its manifest. The
/// returned list is in unspecified order.
pub fn collect_std_vendored_crates(vendor_path: &Path) -> io::Result<Vec<VendoredCrate>> {
    let mut crates = Vec::new();

    for vendored_crate in fs::read_dir(vendor_path)? {
        let vendored_crate: fs::DirEntry = vendored_crate?;
        if !vendored_crate.file_type()?.is_dir() {
            continue;
        }

        let Some(crate_id) = get_vendored_crate_id(&vendored_crate.path())? else {
            error!(
                "Cargo.toml not found at {}. cargo vendor would not do that to us.",
                vendored_crate.path().to_string_lossy()
            );
            panic!()
        };

        // Vendored crate directories can be named "{package_name}" or
        // "{package_name}-{version}", but for now we only use the latter for
        // std vendored deps. For simplicity, accept only that.
        let dir_name = vendored_crate.file_name().to_string_lossy().into_owned();
        let std_path = std_crate_path(&crate_id).to_str().unwrap().to_string();
        let std_path_no_version = std_path
            .rfind('-')
            .map(|pos| std_path[..pos].to_string())
            .unwrap_or(std_path.to_string());
        if std_path != dir_name && std_path_no_version != dir_name {
            return Err(io::Error::new(
                io::ErrorKind::Other,
                format!(
                    "directory name {dir_name} does not match package information for {crate_id:?}"
                ),
            ));
        }
        crates.push(crate_id);
    }

    Ok(crates)
}

#[derive(Copy, Clone, PartialEq, Eq)]
enum CollectCrateFiles {
    /// Collect .rs files and store them as `sources` and other files as
    /// `inputs`. These are part of the crate directly.
    Internal,
    /// Collect .rs files, .md files and other file types that may be
    /// include!()'d into the crate, and store them as `inputs`. These are not
    /// directly part of the crate.
    ExternalSourcesAndInputs,
    /// Like ExternalSourcesAndInputs but excludes .rs files.
    ExternalInputsOnly,
    /// Like `ExternalSourcesAndInputs` but for build scripts.
    BuildScriptExternalSourcesAndInputs,
    /// Like `ExternalInputsOnly` but for build scripts.
    BuildScriptExternalInputsOnly,
    /// Collect .lib files and store them as `native_libs`. These can be
    /// depended on by the crate through `#[link]` directives.
    LibsOnly,
}

// Adds a `filepath` to `CrateFiles` depending on the type of file and the
// `mode` of collection.
fn collect_crate_file(files: &mut CrateFiles, mode: CollectCrateFiles, filepath: &Path) {
    use CollectCrateFiles::*;
    match filepath.extension().and_then(std::ffi::OsStr::to_str) {
        Some("rs") => match mode {
            Internal => files.sources.push(filepath.to_owned()),
            ExternalSourcesAndInputs => files.inputs.push(filepath.to_owned()),
            ExternalInputsOnly => (),
            BuildScriptExternalSourcesAndInputs => {
                files.build_script_inputs.push(filepath.to_owned())
            }
            BuildScriptExternalInputsOnly => (),
            LibsOnly => (),
        },
        // md: Markdown files are commonly include!()'d into source code as docs.
        // h: cxxbridge_cmd include!()'s its .h file into it.
        // json: json files are include!()'d into source code in the wycheproof crate
        Some("md") | Some("h") | Some("json") => match mode {
            Internal | ExternalSourcesAndInputs | ExternalInputsOnly => {
                files.inputs.push(filepath.to_owned())
            }
            BuildScriptExternalSourcesAndInputs | BuildScriptExternalInputsOnly => {
                files.build_script_inputs.push(filepath.to_owned())
            }
            LibsOnly => (),
        },
        Some("lib") if mode == LibsOnly => files.native_libs.push(filepath.to_owned()),
        _ => (),
    };
}

/// Recursively visits all files under `path` and calls `f` on each one.
///
/// The `path` may be a single file or a directory.
pub fn recurse_crate_files(path: &Path, f: &mut dyn FnMut(&Path)) -> anyhow::Result<()> {
    fn recurse(path: &Path, root: &Path, f: &mut dyn FnMut(&Path)) -> anyhow::Result<()> {
        let meta = std::fs::metadata(path).with_context(|| format!("missing path {:?}", path))?;
        if !meta.is_dir() {
            // Working locally can produce files in tree that should not be considered, and
            // which are not part of the git repository.
            //
            // * `.devcontainer/` may contain .md files such as a README.md that are never
            //   part of the build.
            // * `.vscode/` may contain .md files such as a README.md generated there.
            // * `target/` may contain .rs files generated by build scripts when compiling
            //   the crate with cargo or rust-analyzer.
            //
            // Ideally we should just include files that are listed in `git ls-files`.
            const SKIP_PREFIXES: [&str; 3] = [".devcontainer", ".vscode", "target"];
            for skip in SKIP_PREFIXES {
                if path.starts_with(root.join(Path::new(skip))) {
                    return Ok(());
                }
            }
            f(path)
        } else {
            for r in std::fs::read_dir(path).with_context(|| format!("dir at {:?}", path))? {
                let entry = r?;
                let path = entry.path();
                recurse(&path, root, f)?;
            }
        }
        Ok(())
    }
    recurse(path, path, f)
}

/// Get a crate's ID and parsed manifest from its path. Returns `Ok(None)` if
/// there was no Cargo.toml, or `Err(_)` for other IO errors.
fn get_vendored_crate_id(package_path: &Path) -> io::Result<Option<VendoredCrate>> {
    let manifest_file = match fs::read_to_string(package_path.join("Cargo.toml")) {
        Ok(f) => f,
        Err(e) if e.kind() == io::ErrorKind::NotFound => return Ok(None),
        Err(e) => return Err(e),
    };

    let manifest: manifest::CargoManifest = toml::de::from_str(&manifest_file).unwrap();
    let crate_id = VendoredCrate {
        name: manifest.package.name.as_str().into(),
        version: manifest.package.version.clone(),
    };
    Ok(Some(crate_id))
}

/// Proxy for [de]serializing epochs to/from strings. This uses the "1" or "0.1"
/// format rather than the `Display` format for `Epoch`.
#[derive(Debug, Deserialize, Serialize)]
struct EpochString(String);

impl From<Epoch> for EpochString {
    fn from(epoch: Epoch) -> Self {
        Self(epoch.to_version_string())
    }
}

impl From<EpochString> for Epoch {
    fn from(epoch: EpochString) -> Self {
        Epoch::from_version_req_str(&epoch.0)
    }
}

#[cfg(test)]
mod tests {
    use super::Epoch::*;
    use super::*;

    #[test]
    fn epoch_from_str() {
        use EpochParseError::*;
        assert_eq!(Epoch::from_str("v1"), Ok(Major(1)));
        assert_eq!(Epoch::from_str("v2"), Ok(Major(2)));
        assert_eq!(Epoch::from_str("v0_3"), Ok(Minor(3)));
        assert_eq!(Epoch::from_str("0_1"), Err(BadFormat));
        assert_eq!(Epoch::from_str("v1_9"), Err(BadVersion));
        assert_eq!(Epoch::from_str("v0_0"), Err(BadVersion));
        assert_eq!(Epoch::from_str("v0_1_2"), Err(BadFormat));
        assert_eq!(Epoch::from_str("v1_0"), Err(BadVersion));
        assert!(matches!(Epoch::from_str("v1_0foo"), Err(InvalidInt(_))));
        assert!(matches!(Epoch::from_str("vx_1"), Err(InvalidInt(_))));
    }

    #[test]
    fn epoch_to_string() {
        assert_eq!(Major(1).to_string(), "v1");
        assert_eq!(Major(2).to_string(), "v2");
        assert_eq!(Minor(3).to_string(), "v0_3");
    }

    #[test]
    fn epoch_from_version() {
        use semver::Version;

        assert_eq!(Epoch::from_version(&Version::new(0, 1, 0)), Minor(1));
        assert_eq!(Epoch::from_version(&Version::new(1, 2, 0)), Major(1));
    }

    #[test]
    fn epoch_from_version_req_string() {
        assert_eq!(Epoch::from_version_req_str("0.1.0"), Minor(1));
        assert_eq!(Epoch::from_version_req_str("1.0.0"), Major(1));
        assert_eq!(Epoch::from_version_req_str("2.3.0"), Major(2));
    }
}
