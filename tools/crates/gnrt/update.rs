// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::paths;
use crate::util::{remove_checksums_from_lock, run_cargo_command, without_cargo_config_toml};
use crate::UpdateCommandArgs;
use anyhow::{Context, Result};
use std::collections::HashMap;

pub fn update(args: UpdateCommandArgs, paths: &paths::ChromiumPaths) -> Result<()> {
    // Update needs to work with real crates.io, not with our locally vendored
    // crates.
    without_cargo_config_toml(paths, || update_impl(args, paths))?;
    println!("Update successful: run gnrt vendor to download new crate versions.");
    Ok(())
}

fn update_impl(args: UpdateCommandArgs, paths: &paths::ChromiumPaths) -> Result<()> {
    println!("Updating crates from {}", paths.third_party_cargo_root.display());

    run_cargo_command(
        paths.third_party_cargo_root.into(),
        "update",
        args.passthrough,
        HashMap::new(),
    )
    .context("run_cargo_command")?;

    // Running cargo commands against actual crates.io will put checksum into
    // the Cargo.lock file, but we don't generate checksums when we download
    // the crates. This mismatch causes everything else to fail when cargo is
    // using our vendor/ directory. So we remove all the checksums from the
    // lock file.
    remove_checksums_from_lock(paths.third_party_cargo_root)?;

    Ok(())
}
