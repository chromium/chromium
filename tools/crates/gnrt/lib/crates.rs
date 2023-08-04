// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Utilities to handle vendored third-party crates.

use crate::config::BuildConfig;
use crate::deps;
use crate::log_err;
use crate::manifest;
use crate::util::AsDebug;

use std::collections::HashMap;
use std::fmt::{self, Display};
use std::fs;
use std::hash::Hash;
use std::io;
use std::path::{Path, PathBuf};
use std::str::FromStr;

use log::{error, warn};
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
}

impl CrateFiles {
    fn new() -> Self {
        Self { sources: vec![], inputs: vec![] }
    }

    /// Sorts the CrateFiles for a deterministic output.
    fn sort(&mut self) {
        self.sources.sort_unstable();
        self.inputs.sort_unstable();
    }
}

/// Set of vendored packages in `//third_party/rust` format. Namely, foo 1.2.3
/// would be in `<root>/foo/v1/crate` and bar 0.1.2 would be in
/// `<root>/bar/v0_1/crate`. The names also must be normalized according to
/// `NormalizedName` rules. Multiple versions of a name can exist, as long as
/// their "epoch" (vX for 1.0.0+ or vO_Y for <1.0.0) does not collide. This is
/// enforced naturally by the directory layout.
pub struct ThirdPartySource {
    /// The available set of versions for each crate.
    crate_versions: HashMap<String, Vec<Version>>,
    /// Serves as a set of crate ids (the VendoredCrate keys), and holds the
    /// source and input files for each crate.
    crate_files: HashMap<VendoredCrate, CrateFiles>,
}

impl ThirdPartySource {
    /// Collects set of vendored crates on disk.
    pub fn new(crates_path: &Path) -> io::Result<Self> {
        let mut crate_versions = HashMap::<String, Vec<Version>>::new();
        let mut all_crate_files = HashMap::new();

        for crate_dir in log_err!(
            fs::read_dir(crates_path),
            "reading dir {crates_path}",
            crates_path = AsDebug(crates_path)
        )? {
            // Look at each crate directory.
            let crate_dir: fs::DirEntry = log_err!(crate_dir)?;
            if !crate_dir.file_type()?.is_dir() {
                continue;
            }

            let crate_path = crate_dir.path();

            // Ensure the path has a valid name: is UTF8, has our normalized format.
            let normalized_name = path_as_str(crate_path.file_name().unwrap())?;
            into_io_result(NormalizedName::new(normalized_name).ok_or_else(|| {
                format!("unnormalized crate name in path {}", crate_path.to_string_lossy())
            }))?;

            for epoch_dir in fs::read_dir(crate_dir.path())? {
                // Look at each epoch of the crate we have checked in.
                let epoch_dir: fs::DirEntry = epoch_dir?;
                if !epoch_dir.file_type()?.is_dir() {
                    continue;
                }

                // Skip it if it's not a valid epoch.
                if epoch_dir.file_name().to_str().and_then(|s| Epoch::from_str(s).ok()).is_none() {
                    continue;
                }

                let crate_path = epoch_dir.path().join("crate");

                let Some(crate_id) = get_vendored_crate_id(&crate_path)? else {
                    warn!(
                        "directory name parsed as valid epoch but contained no Cargo.toml: {}",
                        crate_path.to_string_lossy()
                    );
                    continue;
                };

                let mut files = CrateFiles::new();
                recurse_crate_files(&crate_path, &mut |filepath| {
                    collect_crate_file(&mut files, CollectCrateFiles::Internal, filepath)
                })?;
                files.sort();

                all_crate_files.insert(crate_id.clone(), files);

                crate_versions.entry(crate_id.name).or_default().push(crate_id.version);
            }
        }

        Ok(ThirdPartySource { crate_versions, crate_files: all_crate_files })
    }

    /// Find crate with `name` that meets version requirement. Returns `None` if
    /// there are none.
    pub fn find_match(&self, name: &str, req: &semver::VersionReq) -> Option<VendoredCrate> {
        let (key, versions) = self.crate_versions.get_key_value(name)?;
        let version = versions.iter().find(|v| req.matches(v))?.clone();
        Some(VendoredCrate { name: key.clone(), version })
    }

    pub fn crate_files(&self) -> &HashMap<VendoredCrate, CrateFiles> {
        &self.crate_files
    }

    /// Get Cargo.toml `[patch]` sections for each third-party crate.
    pub fn cargo_patches(&self) -> Vec<manifest::PatchSpecification> {
        let mut patches: Vec<_> = self
            .crate_files
            .keys()
            .map(|c| manifest::PatchSpecification {
                package_name: c.name.clone(),
                patch_name: format!(
                    "{name}_{epoch}",
                    name = c.name,
                    epoch = Epoch::from_version(&c.version)
                ),
                path: Self::crate_path(c),
            })
            .collect();
        // Give patches a stable ordering, instead of the arbitrary HashMap
        // order.
        patches.sort_unstable_by(|p1, p2| p1.patch_name.cmp(&p2.patch_name));
        patches
    }

    /// Get the root of `id`'s sources relative to the vendor dir.
    pub fn crate_path(id: &VendoredCrate) -> PathBuf {
        let mut path: PathBuf = Self::build_path(id);
        path.push("crate");
        path
    }

    /// Get the BUILD.gn file directory of `id` relative to the vendor dir.
    pub fn build_path(id: &VendoredCrate) -> PathBuf {
        let mut path: PathBuf = id.normalized_name().0.into();
        path.push(Epoch::from_version(&id.version).to_string());
        path
    }
}

/// Get the subdir name containing `id` in a `cargo vendor` directory.
pub fn std_crate_path(id: &VendoredCrate) -> PathBuf {
    format!("{}-{}", id.name, id.version).into()
}

/// Collect the source and input files (i.e. `CrateFiles`) for each library that
/// is part of the standard library build.
pub fn collect_std_crate_files<'a>(
    p: &deps::Package,
    config: &BuildConfig,
) -> io::Result<(VendoredCrate, CrateFiles)> {
    // We only look at lib targets here because these are stdlib targets, and thus
    // we only are building the libs. We're not building bins even if they existed.
    let lib_target = p.lib_target.as_ref().expect("dependency had no lib target");
    let root_dir = lib_target.root.parent().expect("lib target has no directory in its path");
    let crate_config = config.per_crate_config.get(&p.crate_id().name);

    let extra_src_roots =
        crate_config.iter().flat_map(|crate_config| &crate_config.extra_src_roots);
    let extra_input_roots =
        crate_config.iter().flat_map(|crate_config| &crate_config.extra_input_roots);

    let mut files = CrateFiles::new();
    recurse_crate_files(&root_dir, &mut |filepath| {
        collect_crate_file(&mut files, CollectCrateFiles::Internal, filepath)
    })?;
    for path in extra_src_roots {
        recurse_crate_files(&root_dir.to_owned().join(path), &mut |filepath| {
            collect_crate_file(&mut files, CollectCrateFiles::ExternalSourcesAndInputs, filepath)
        })?;
    }
    for path in extra_input_roots {
        recurse_crate_files(&root_dir.to_owned().join(path), &mut |filepath| {
            collect_crate_file(&mut files, CollectCrateFiles::ExternalInputsOnly, filepath)
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
            .rfind("-")
            .and_then(|pos| Some(std_path[..pos].to_string()))
            .or(Some(std_path.to_string()))
            .unwrap();
        if &std_path != &dir_name && &std_path_no_version != &dir_name {
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
}

// Adds a `filepath` to `CrateFiles` depending on the type of file and the
// `mode` of collection.
fn collect_crate_file(files: &mut CrateFiles, mode: CollectCrateFiles, filepath: PathBuf) {
    match filepath.extension().map(std::ffi::OsStr::to_str).flatten() {
        Some("rs") => match mode {
            CollectCrateFiles::Internal => files.sources.push(filepath),
            CollectCrateFiles::ExternalSourcesAndInputs => files.inputs.push(filepath),
            CollectCrateFiles::ExternalInputsOnly => (),
        },
        // md: Markdown files are commonly include!()'d into source code as docs.
        // h: cxxbridge_cmd include!()'s its .h file into it.
        Some("md") | Some("h") => files.inputs.push(filepath),
        _ => (),
    };
}

// Recursively visits all files under `dir` and calls `f` on each one.
fn recurse_crate_files(dir: &Path, f: &mut dyn FnMut(PathBuf)) -> io::Result<()> {
    fn recurse(dir: &Path, root: &Path, f: &mut dyn FnMut(PathBuf)) -> io::Result<()> {
        'each_dir_entry: for r in std::fs::read_dir(dir)? {
            let entry = r?;
            let path = entry.path();
            let is_dir = entry.metadata()?.is_dir();
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
                    continue 'each_dir_entry;
                }
            }
            if is_dir { recurse(&path, root, f)? } else { f(path) }
        }
        Ok(())
    }
    recurse(dir, dir, f)
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

/// Utility to read a path as a `&str` with an informative error message if it
/// had invalid UTF8.
fn path_as_str<T: AsRef<Path> + ?Sized>(path: &T) -> io::Result<&str> {
    let path = path.as_ref();
    into_io_result(
        path.to_str().ok_or_else(|| format!("invalid utf8 in path {}", path.to_string_lossy())),
    )
}

/// Utility to convert a `Result<T, E>` into `io::Result<T>` for any compatible
/// error type `E`.
fn into_io_result<T, E: Into<Box<dyn std::error::Error + Send + Sync>>>(
    result: Result<T, E>,
) -> io::Result<T> {
    result.map_err(|e| io::Error::new(io::ErrorKind::Other, e))
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
