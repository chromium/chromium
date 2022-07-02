// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Maps Rust targets to Chromium targets.

use std::collections::HashSet;
use std::iter::Iterator;

use cargo_platform::Cfg;
use once_cell::sync::OnceCell;

pub use cargo_platform::Platform;

/// A set of platforms: either the set of all platforms, or a finite set of
/// platform configurations.
#[derive(Clone, Debug)]
pub enum PlatformSet {
    /// Matches any platform configuration.
    All,
    /// Matches a finite set of configurations.
    Platforms(HashSet<Platform>),
}

impl PlatformSet {
    /// A `PlatformSet` that matches no platforms. Useful as a starting point
    /// when iteratively adding platforms with `add`.
    pub fn empty() -> Self {
        Self::Platforms(HashSet::new())
    }

    /// Add a single platform filter to `self`. The resulting set is superset of
    /// the original. If `filter` is `None`, `self` becomes `PlatformSet::All`.
    pub fn add(&mut self, filter: Option<Platform>) {
        let set = match self {
            // If the set is already all platforms, no need to add `filter`.
            Self::All => return,
            Self::Platforms(set) => set,
        };

        match filter {
            None => *self = Self::All,
            Some(platform) => {
                set.insert(platform);
            }
        }
    }
}

/// Whether `platform`, either an explicit rustc target triple or a `cfg(...)`
/// expression, is supported in Chromium.
pub fn is_supported(platform: &Platform) -> bool {
    match platform {
        Platform::Name(name) => SUPPORTED_NAMED_PLATFORMS.iter().any(|p| *p == name),
        Platform::Cfg(cfg_expr) => {
            supported_os_cfgs().iter().any(|c| cfg_expr.matches(std::slice::from_ref(c)))
        }
    }
}

pub fn supported_os_cfgs_for_testing() -> &'static [Cfg] {
    supported_os_cfgs()
}

pub fn supported_named_platforms_for_testing() -> &'static [&'static str] {
    SUPPORTED_NAMED_PLATFORMS
}

fn supported_os_cfgs() -> &'static [Cfg] {
    static CFG_SET: OnceCell<Vec<Cfg>> = OnceCell::new();
    CFG_SET.get_or_init(|| {
        let mut cfg_set: Vec<Cfg> = [
            // Set of supported OSes for `cfg(target_os = ...)`.
            "android", "darwin", "fuchsia", "ios", "linux", "windows",
        ]
        .into_iter()
        .map(|os| Cfg::KeyPair("target_os".to_string(), os.to_string()))
        .collect();

        cfg_set.extend(
            // Alternative syntax `cfg(unix)` or `cfg(windows)`.
            ["unix", "windows"].into_iter().map(|os| Cfg::Name(os.to_string())),
        );
        cfg_set
    })
}

static SUPPORTED_NAMED_PLATFORMS: &'static [&'static str] = &[
    "i686-linux-android",
    "x86_64-linux-android",
    "armv7-linux-android",
    "aarch64-linux-android",
    "aarch64-fuchsia",
    "x86_64-fuchsia",
    "aarch64-apple-ios",
    "armv7-apple-ios",
    "x86_64-apple-ios",
    "i386-apple-ios",
    "i686-pc-windows-msvc",
    "x86_64-pc-windows-msvc",
    "i686-unknown-linux-gnu",
    "x86_64-unknown-linux-gnu",
    "x86_64-apple-darwin",
    "aarch64-apple-darwin",
];
