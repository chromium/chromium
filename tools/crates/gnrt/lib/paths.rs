// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Paths and helpers for running within a Chromium checkout.

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
            rust_src_vendor_subdir: Path::new(RUST_SRC_VENDOR_SUBDIR),
            // We tolerate the toolchain package dir being missing, as it's not checked out
            // on the bots that generate Clang/Rust rolls.
            rust_src_installed: Path::new(RUST_SRC_INSTALLED_DIR),
            std_config_file: check_path(&cur_dir, STD_CONFIG_FILE)?,
            std_build: check_path(&cur_dir, STD_BUILD_DIR)?,
            std_fake_root: check_path(&cur_dir, STD_FAKE_ROOT)?,
            std_fake_root_config_template: check_path(&cur_dir, STD_FAKE_ROOT_CONFIG_TEMPLATE)?,
            std_fake_root_cargo_template: check_path(&cur_dir, STD_FAKE_ROOT_CARGO_TEMPLATE)?,
        })
    }

    /// Given an absolute path to a file in the checkout, get an absolute GN
    /// path suitable for use in GN rules.
    pub fn to_gn_abs_path<'a>(
        &self,
        path: &'a Path,
    ) -> Result<std::borrow::Cow<'a, str>, std::path::StripPrefixError> {
        Ok(path.strip_prefix(&self.root)?.to_string_lossy())
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
        return Err(io::Error::new(
            io::ErrorKind::Other,
            format!("could not find {} (invoked from Chromium checkout root?)", p.display()),
        ));
    }

    Ok(p)
}

static RUST_THIRD_PARTY_DIR: &str = "third_party/rust";
static RUST_SRC_VENDOR_SUBDIR: &str = "vendor";
static RUST_SRC_INSTALLED_DIR: &str = "third_party/rust-toolchain/lib/rustlib/src/rust";
static STD_CONFIG_FILE: &str = "build/rust/std/gnrt_config.toml";
static STD_BUILD_DIR: &str = "build/rust/std/rules";
static STD_FAKE_ROOT: &str = "build/rust/std/fake_root";
static STD_FAKE_ROOT_CONFIG_TEMPLATE: &str = "build/rust/std/fake_root/.cargo/config.toml.template";
static STD_FAKE_ROOT_CARGO_TEMPLATE: &str = "build/rust/std/fake_root/Cargo.toml.template";
