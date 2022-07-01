// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Utilities to handle vendored third-party crates.

use crate::manifest;

use std::fmt::{self, Display};
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::str::FromStr;

/// A normalized version as used in third_party/rust crate paths.
///
/// A crate version is identified by the major version, if it's >= 1, or the
/// minor version, if the major version is 0. There is a many-to-one
/// relationship between crate versions and epochs.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum Epoch {
    /// Epoch with major version == 0. The field is the minor version. It is an
    /// error to use 0: methods may panic in this case.
    Minor(u32),
    /// Epoch with major version >= 1. It is an error to use 0: methods may
    /// panic in this case.
    Major(u32),
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
        let major: Option<u32> =
            parts.next().map(|s| s.parse().map_err(EpochParseError::InvalidInt)).transpose()?;
        let minor: Option<u32> =
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

/// Identifies a vendored third-party crate.
#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct ThirdPartyCrate {
    /// The upstream name as specified in its Cargo.toml.
    pub name: String,
    /// The crate's epoch, as defined by `Epoch`.
    pub epoch: Epoch,
}

impl ThirdPartyCrate {
    /// The location of this crate relative to the third-party Rust crate
    /// directory. Crates are laid out according to their name and epoch.
    pub fn crate_path(&self) -> PathBuf {
        let mut path = PathBuf::new();
        path.push(NormalizedName::from_crate_name(&self.name).0);
        path.push(self.epoch.to_string());
        path.push("crate");
        path
    }

    /// Unique but arbitrary name for this (crate, epoch) pair. Suitable for use
    /// in Cargo.toml [patch] sections.
    pub fn patch_name(&self) -> String {
        format!("{}_{}", self.name, self.epoch)
    }
}

impl fmt::Display for ThirdPartyCrate {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_fmt(format_args!("{} {}", self.name, self.epoch))
    }
}

/// Traverse vendored third-party crates and collect the set of names and
/// epochs. Each `ThirdPartyCrate` entry is paired with the package metadata
/// from its manifest. The returned list is in unspecified order.
pub fn collect_third_party_crates<P: AsRef<Path>>(
    crates_path: P,
) -> io::Result<Vec<(ThirdPartyCrate, manifest::CargoPackage)>> {
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

            crates.push((ThirdPartyCrate { name: package_name, epoch }, manifest.package));
        }
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
