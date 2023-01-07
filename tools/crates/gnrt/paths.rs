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
    /// The Rust source tree, containing the standard library and vendored
    /// dependencies.
    pub rust_src: &'static Path,
    pub rust_src_vendor: &'static Path,
    pub rust_std: &'static Path,
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
            rust_src: check_path(&cur_dir, RUST_SRC_DIR)?,
            rust_src_vendor: check_path(&cur_dir, RUST_SRC_VENDOR_DIR)?,
            rust_std: check_path(&cur_dir, RUST_STD_DIR)?,
        })
    }
}

fn check_path<'a>(root: &Path, p_str: &'a str) -> io::Result<&'a Path> {
    let p = Path::new(p_str);
    if !root.join(p).is_dir() {
        return Err(io::Error::new(
            io::ErrorKind::Other,
            format!(
                "could not find {} (invoked from Chromium checkout root? is use_rust enabled in .gclient?)",
                p.display()
            ),
        ));
    }

    Ok(p)
}

static RUST_THIRD_PARTY_DIR: &str = "third_party/rust";
static RUST_SRC_DIR: &str = "third_party/rust_src/src";
static RUST_SRC_VENDOR_DIR: &str = "third_party/rust_src/src/vendor";
static RUST_STD_DIR: &str = "third_party/rust_src/src/library/std";
