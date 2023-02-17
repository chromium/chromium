// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Parsing configuration file that customizes gnrt BUILD.gn output. Currently
//! only used for std bindings.

use std::collections::BTreeMap;

use serde::Deserialize;

/// Extra GN configuration for targets. Contains one entry for each crate with
/// custom config.
#[derive(Clone, Debug, Deserialize)]
pub struct ConfigFile {
    #[serde(flatten)]
    pub per_lib_config: BTreeMap<String, PerLibConfig>,
}

#[derive(Clone, Debug, Deserialize)]
pub struct PerLibConfig {
    /// List of `cfg(...)` options for building this crate.
    #[serde(default)]
    pub cfg: Vec<String>,
    /// List of compile-time environment variables for this crate.
    #[serde(default)]
    pub env: Vec<String>,
}
