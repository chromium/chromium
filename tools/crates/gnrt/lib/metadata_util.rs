// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Utilities for working with `cargo_metadata::Metadata`.

use crate::crates;
use anyhow::{format_err, Result};
use cargo_metadata::{Metadata, Node, Package, PackageId};
use std::collections::{HashMap, HashSet};

/// Collects the set of third-party crate Packages into a map keyed by their
/// ids.
///
/// https://docs.rs/cargo_metadata/latest/cargo_metadata/struct.Package.html
///
/// The Package contains fixed information about the crate like its name and
/// version.
pub fn metadata_packages(metadata: &Metadata) -> Result<HashMap<&PackageId, &Package>> {
    let packages: HashMap<_, _> = metadata
        .packages
        .iter()
        .filter(|package| {
            // Remove the root package (our "chromium" crate).
            //
            // We have to keep packages in the `remove_crates` config
            // because they must be downloaded for `cargo metadata` to work.
            metadata.root_package().unwrap().id != package.id
        })
        // Key off the package id.
        .map(|p| (&p.id, p))
        .collect();

    // If there are multiple crates with the same epoch, this is unexpected.
    // Bail out.
    {
        let mut found = HashSet::new();
        for p in packages.values() {
            let epoch = crates::Epoch::from_version(&p.version);
            if !found.insert((&p.name, epoch)) {
                return Err(format_err!(
                    "Two '{}' crates found with the same {} epoch",
                    p.name,
                    epoch
                ));
            }
        }
    }

    Ok(packages)
}

/// Collects the set of third-party crate Nodes into a map keyed by their ids.
///
/// https://docs.rs/cargo_metadata/latest/cargo_metadata/struct.Node.html
///
/// The Node contains resolved information about the crate like its
/// dependencies, which depend on enabled feature sets.
pub fn metadata_nodes(metadata: &Metadata) -> HashMap<&PackageId, &Node> {
    metadata.resolve.as_ref().unwrap().nodes.iter().map(|node| (&node.id, node)).collect()
}
