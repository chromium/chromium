// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Paths and helpers for running within a Chromium checkout.

use crate::crates::{Epoch, NormalizedName};
use itertools::Itertools;
use std::env;
use std::io;
use std::path::{Path, PathBuf};

/// Chromium source tree paths. All members other than `root` are relative to
/// `root`.
pub struct ChromiumPaths {
    /// The chromium/src checkout root, as an absolute path.
    pub root: PathBuf,
    /// The third_party/rust directory.
    pub third_party: &'static Path,
    /// The library directory relative to the root of the Rust source tree.
    pub rust_src_library_subdir: &'static Path,
    /// The vendor directory relative to the root of the Rust source tree.
    pub rust_src_vendor_subdir: &'static Path,
    /// The root of the Rust source tree that is installed in //third_party and
    /// used in the Chromium GN build.
    pub rust_src_installed: &'static Path,

    pub std_config_file: &'static Path,
    pub std_build: &'static Path,
    pub std_fake_root: &'static Path,
    pub std_fake_root_config_template: &'static Path,
    pub std_fake_root_cargo_template: &'static Path,

    pub third_party_cargo_root: &'static Path,
    pub third_party_config_file: &'static Path,
}

impl ChromiumPaths {
    /// Create the `ChromiumPaths` resolver. Accesses the filesystem to get the
    /// checkout root.
    pub fn new() -> io::Result<ChromiumPaths> {
        // We should be invoked from the repository root.
        let cur_dir = env::current_dir()?;

        Ok(ChromiumPaths {
            root: cur_dir.clone(),
            third_party: check_path(&cur_dir, RUST_THIRD_PARTY_DIR)?,
            // We tolerate the Rust sources being missing, as they are only used to generate
            // rules for the stdlib during Clang/Rust rolls, and they are not checked out for
            // most machines.
            rust_src_library_subdir: Path::new(RUST_SRC_LIBRARY_SUBDIR),
            // We tolerate the Rust sources being missing, as they are only used to generate
            // rules for the stdlib during Clang/Rust rolls, and they are not checked out for
            // most machines.
            rust_src_vendor_subdir: Path::new(RUST_SRC_VENDOR_SUBDIR),
            // We tolerate the toolchain package dir being missing, as it's not checked out
            // on the bots that generate Clang/Rust rolls.
            rust_src_installed: Path::new(RUST_SRC_INSTALLED_DIR),
            std_config_file: check_path(&cur_dir, STD_CONFIG_FILE)?,
            std_build: check_path(&cur_dir, STD_BUILD_DIR)?,
            std_fake_root: check_path(&cur_dir, STD_FAKE_ROOT)?,
            std_fake_root_config_template: check_path(&cur_dir, STD_FAKE_ROOT_CONFIG_TEMPLATE)?,
            std_fake_root_cargo_template: check_path(&cur_dir, STD_FAKE_ROOT_CARGO_TEMPLATE)?,

            third_party_cargo_root: check_path(&cur_dir, THIRD_PARTY_CARGO_ROOT)?,
            third_party_config_file: check_path(&cur_dir, THIRD_PARTY_CONFIG_FILE)?,
        })
    }

    /// Given an absolute path to a file in the checkout, get an absolute GN
    /// path suitable for use in GN rules.
    pub fn to_gn_abs_path(&self, path: &Path) -> Result<String, std::path::StripPrefixError> {
        Ok(normalize_unix_path_separator(path.strip_prefix(&self.root)?))
    }

    /// Modifies the file name in a path from `foo.bar.template` to `foo.bar`.
    pub fn strip_template(&self, path: &Path) -> Option<std::path::PathBuf> {
        if path.extension()? != "template" {
            None
        } else {
            let mut buf = path.to_owned();
            buf.set_file_name(path.file_stem()?);
            Some(buf)
        }
    }
}

fn check_path<'a>(root: &Path, p_str: &'a str) -> io::Result<&'a Path> {
    let p = Path::new(p_str);
    if !root.join(p).exists() {
        return Err(io::Error::other(format!(
            "could not find {} (invoked from Chromium checkout root?)",
            p.display()
        )));
    }

    Ok(p)
}

/// Replace all path separators with `/` and return it as a String. The
/// resulting path is suitable for use in GN files.
pub fn normalize_unix_path_separator(path: impl AsRef<Path>) -> String {
    // `Path`s on windows use `\` separators and we need to use `/` in GN strings.
    let path = path.as_ref();
    path.iter()
        .map(|comp| comp.to_str().unwrap_or_else(|| panic!("non-UTF-8 in path {path:?}")))
        .join("/")
}

/// Returns the name of a directory where we want to vendor a third-party crate
/// with the given `name` and `version`.
///
/// Note that this can be quite an arbitrary name, because `cargo` will
/// discover available crates by reading all the nested `Cargo.toml`
/// files (rather than based on directory names).  See also
/// https://crbug.com/396397336#comment7 and adjacent comments.
pub fn get_vendor_dir_for_package(
    paths: &ChromiumPaths,
    name: &str,
    version: &semver::Version,
) -> PathBuf {
    let epoch = Epoch::from_version(version);
    paths.third_party_cargo_root.join("vendor").join(Path::new(&format!("{name}-{epoch}")))
}

/// Returns the name of a directory where we generate `BUILD.gn` (and also
/// `README.chromium`).
pub fn get_build_dir_for_package(
    paths: &ChromiumPaths,
    name: &str,
    version: &semver::Version,
) -> PathBuf {
    paths
        .third_party
        .join(NormalizedName::from_crate_name(name).to_string())
        .join(Epoch::from_version(version).to_string())
}

static RUST_THIRD_PARTY_DIR: &str = "third_party/rust";
static RUST_SRC_LIBRARY_SUBDIR: &str = "library";
static RUST_SRC_VENDOR_SUBDIR: &str = "library/vendor";
static RUST_SRC_INSTALLED_DIR: &str = "third_party/rust-toolchain/lib/rustlib/src/rust";

static STD_CONFIG_FILE: &str = "build/rust/std/gnrt_config.toml";
static STD_BUILD_DIR: &str = "build/rust/std/rules";
static STD_FAKE_ROOT: &str = "build/rust/std/fake_root";
static STD_FAKE_ROOT_CONFIG_TEMPLATE: &str = "build/rust/std/fake_root/.cargo/config.toml.template";
static STD_FAKE_ROOT_CARGO_TEMPLATE: &str = "build/rust/std/fake_root/Cargo.toml.template";

static THIRD_PARTY_CARGO_ROOT: &str = "third_party/rust/chromium_crates_io";
static THIRD_PARTY_CONFIG_FILE: &str = "third_party/rust/chromium_crates_io/gnrt_config.toml";

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_normalize() {
        assert_eq!(normalize_unix_path_separator(Path::new("rel")), "rel");
        assert_eq!(normalize_unix_path_separator(&Path::new("a").join("b")), "a/b");
    }
}
