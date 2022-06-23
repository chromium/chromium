// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Paths and helpers for running within a Chromium checkout.

use std::env;
use std::io;
use std::path::{Path, PathBuf};

pub struct ChromiumPaths {
    /// The chromium/src checkout root, as an absolute path.
    pub root: PathBuf,
    /// The third_party/rust directory, relative to `root`.
    pub third_party: PathBuf,
}

impl ChromiumPaths {
    /// Create the `ChromiumPaths` resolver. Accesses the filesystem to get the
    /// checkout root.
    pub fn new() -> io::Result<ChromiumPaths> {
        // We should be invoked from the repository root.
        let cur_dir = env::current_dir()?;

        let third_party = Path::new(RUST_THIRD_PARTY_DIR).to_path_buf();
        if !cur_dir.join(&third_party).is_dir() {
            return Err(io::Error::new(
                io::ErrorKind::Other,
                format!(
                    "could not find {} (was this invoked from the Chromium checkout root?)",
                    third_party.display()
                ),
            ));
        }

        Ok(ChromiumPaths { root: cur_dir, third_party: third_party })
    }
}

static RUST_THIRD_PARTY_DIR: &str = "third_party/rust";
