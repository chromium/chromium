// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Parsing configuration file that customizes gnrt BUILD.gn output. Currently
//! only used for std bindings.

use std::collections::BTreeMap;

use serde::Deserialize;

/// Customizes GN output for a session.
#[derive(Clone, Debug, Deserialize)]
pub struct BuildConfig {
    /// Configuration that applies to all crates
    #[serde(default, rename = "all")]
    pub all_config: CrateConfig,
    /// Additional configuration options for specific crates. Keyed by crate
    /// name. Config is additive with `all_config`.
    #[serde(rename = "crate")]
    pub per_crate_config: BTreeMap<String, CrateConfig>,
}

#[derive(Clone, Debug, Default, Deserialize)]
pub struct CrateConfig {
    /// `cfg(...)` options for building this crate.
    #[serde(default)]
    pub cfg: Vec<String>,
    /// Compile-time environment variables for this crate.
    #[serde(default)]
    pub env: Vec<String>,
    /// Extra rustc flags.
    #[serde(default)]
    pub rustflags: Vec<String>,
}
