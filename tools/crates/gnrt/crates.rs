// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Utilities to handle vendored third-party crates.

use crate::manifest;

use std::fmt::{self, Display};
use std::fs;
use std::hash::Hash;
use std::io;
use std::path::{Path, PathBuf};
use std::str::FromStr;

use semver::Version;

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Visibility {
    /// The crate can be used by any build targets.
    Public,
    /// The crate can be used by only third-party crates.
    ThirdParty,
    /// The crate can be used by any test target, and in production by
    /// third-party crates.
    TestOnlyAndThirdParty,
}

/// A normalized version as used in third_party/rust crate paths.
///
/// A crate version is identified by the major version, if it's >= 1, or the
/// minor version, if the major version is 0. There is a many-to-one
/// relationship between crate versions and epochs.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
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

    /// Compute the Epoch from a `semver::Version`. This is useful since we can
    /// parse versions from `cargo_metadata` and in Cargo.toml files using the
    /// `semver` library.
    pub fn from_version(version: &Version) -> Self {
        match version.major {
            0 => Self::Minor(version.minor.try_into().unwrap()),
            x => Self::Major(x.try_into().unwrap()),
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
        if iter.next() != None {
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
        if parts.next() == None { Ok(result) } else { Err(EpochParseError::BadFormat) }
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
}

impl fmt::Display for NormalizedName {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

/// Identifies a third-party crate vendored in the format described by
/// third_party/rust/README.md
#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct ChromiumVendoredCrate {
    /// The upstream name as specified in its Cargo.toml.
    pub name: String,
    /// The crate's epoch, as defined by `Epoch`.
    pub epoch: Epoch,
}

impl ChromiumVendoredCrate {
    /// The normalized name we use in our vendoring structure.
    pub fn normalized_name(&self) -> NormalizedName {
        NormalizedName::from_crate_name(&self.name)
    }

    /// The location of this crate's directory, including its source subdir and
    /// build files, relative to the third-party Rust crate directory. Crates
    /// are laid out according to their name and epoch.
    pub fn build_path(&self) -> PathBuf {
        let mut path = PathBuf::new();
        path.push(&self.normalized_name().0);
        path.push(self.epoch.to_string());
        path
    }

    /// The location of this crate's source relative to the third-party Rust
    /// crate directory.
    pub fn crate_path(&self) -> PathBuf {
        let mut path = self.build_path();
        path.push("crate");
        path
    }

    /// Unique but arbitrary name for this (crate, epoch) pair. Suitable for use
    /// in Cargo.toml [patch] sections.
    pub fn patch_name(&self) -> String {
        format!("{}_{}", self.name, self.epoch)
    }
}

impl fmt::Display for ChromiumVendoredCrate {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_fmt(format_args!("{} {}", self.name, self.epoch))
    }
}

/// Traverse vendored third-party crates and collect the set of names and
/// epochs. Each `ChromiumVendoredCrate` entry is paired with the package
/// metadata from its manifest. The returned list is in unspecified order.
pub fn collect_third_party_crates<P: AsRef<Path>>(
    crates_path: P,
) -> io::Result<Vec<(ChromiumVendoredCrate, manifest::CargoPackage)>> {
    let mut crates = Vec::new();

    for crate_dir in fs::read_dir(crates_path)? {
        // Look at each crate directory.
        let crate_dir: fs::DirEntry = crate_dir?;
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
            if !crate_dir.file_type()?.is_dir() {
                continue;
            }

            let epoch_path = epoch_dir.path();
            let epoch_name = path_as_str(epoch_path.file_name().unwrap())?;
            let epoch = match Epoch::from_str(epoch_name) {
                Ok(epoch) => epoch,
                // Skip if we can't parse the directory as a valid epoch.
                Err(_) => continue,
            };

            // Try to read the Cargo.toml, since we need the package name. The
            // directory name on disk is normalized but we need to refer to the
            // package the way Cargo expects.
            let manifest_path = epoch_path.join("crate/Cargo.toml");
            let manifest_contents = match fs::read_to_string(&manifest_path) {
                Ok(s) => s,
                Err(e) => match e.kind() {
                    // Skip this directory and log a message if a Cargo.toml
                    // doesn't exist.
                    io::ErrorKind::NotFound => {
                        println!(
                            "Warning: directory name parsed as valid epoch but \
                            contained no Cargo.toml: {}",
                            manifest_path.to_string_lossy()
                        );
                        continue;
                    }
                    _ => return Err(e),
                },
            };
            let manifest: manifest::CargoManifest = toml::de::from_str(&manifest_contents).unwrap();
            let package_name = manifest.package.name.clone();

            crates.push((ChromiumVendoredCrate { name: package_name, epoch }, manifest.package));
        }
    }

    Ok(crates)
}

/// Identifies a third-party crate vendored in `cargo vendor` format. This is
/// used in the Rust source tree.
#[derive(Clone, Debug, Eq)]
pub struct StdVendoredCrate {
    /// The upstream name as specified in its Cargo.toml.
    pub name: String,
    /// The crate's version.
    pub version: Version,
    /// Whether this is the latest version in the vendored set. If this is
    /// false, there is a version suffix in the path. Otherwise, it is just the
    /// package name.
    ///
    /// This is not factored into comparisons or hashing.
    pub is_latest: bool,
}

impl Hash for StdVendoredCrate {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.name.hash(state);
        self.version.hash(state);
    }
}

impl PartialEq for StdVendoredCrate {
    fn eq(&self, other: &Self) -> bool {
        self.name == other.name && self.version == other.version
    }
}

impl Ord for StdVendoredCrate {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.name.cmp(&other.name).then(self.version.cmp(&other.version))
    }
}

impl PartialOrd for StdVendoredCrate {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl StdVendoredCrate {
    /// The crate's path relative to the `vendor/` dir, taking into account
    /// whether it's the latest version (and so whether the name is abridged).
    pub fn crate_path(&self) -> PathBuf {
        PathBuf::from(if self.is_latest { self.abridged_dir_name() } else { self.full_dir_name() })
    }

    /// The subdirectory name including the full version.
    fn full_dir_name(&self) -> String {
        format!("{}-{}", self.name, self.version)
    }

    /// The subdirectory name without the version.
    fn abridged_dir_name(&self) -> String {
        self.name.clone()
    }
}

impl fmt::Display for StdVendoredCrate {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} {}", self.name, self.version)
    }
}

/// Traverse vendored third-party crates in the Rust source package. Each
/// `StdVendoredCrate` is paired with the package metadata from its manifest.
/// The returned list is in unspecified order.
pub fn collect_std_vendored_crates<P: AsRef<Path>>(
    vendor_path: P,
) -> io::Result<Vec<(StdVendoredCrate, manifest::CargoPackage)>> {
    let mut crates = Vec::new();

    for vendored_crate in fs::read_dir(vendor_path)? {
        let vendored_crate: fs::DirEntry = vendored_crate?;
        if !vendored_crate.file_type()?.is_dir() {
            continue;
        }

        let crate_path = vendored_crate.path();
        let manifest_path = crate_path.join("Cargo.toml");
        let manifest: manifest::CargoManifest =
            toml::de::from_str(&fs::read_to_string(&manifest_path)?).unwrap();

        // Vendored crate directories are named as either "{package_name}" or
        // "{package_name}-{version}". The latest version of the vendored
        // package is named as the former, and older ones as the latter.
        //
        // We must determine which format is used. A simple way is to compute
        // both names and compare it to the directory.
        let dir_name = crate_path.file_name().unwrap().to_str().unwrap();

        let mut crate_id = StdVendoredCrate {
            name: manifest.package.name.clone(),
            version: manifest.package.version.clone(),
            // Placeholder value.
            is_latest: false,
        };

        crate_id.is_latest = if crate_id.full_dir_name() == dir_name {
            false
        } else if crate_id.abridged_dir_name() == dir_name {
            true
        } else {
            return Err(io::Error::new(
                io::ErrorKind::Other,
                format!(
                    "directory name {dir_name} does not match package information for {crate_id:?}"
                ),
            ));
        };

        // Check that we correctly determined `is_latest` and our computed
        // subdirectory name is correct.
        assert_eq!(crate_path.file_name().unwrap(), std::ffi::OsStr::new(&crate_id.crate_path()));

        crates.push((crate_id, manifest.package));
    }

    Ok(crates)
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
