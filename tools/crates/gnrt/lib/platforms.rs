// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Maps Rust targets to Chromium targets.

use std::collections::BTreeSet;

use cargo_platform::{Cfg, CfgExpr};
use once_cell::sync::OnceCell;

pub use cargo_platform::Platform;

/// A set of platforms: either the set of all platforms, or a finite set of
/// platform configurations.
#[derive(Clone, Debug)]
pub enum PlatformSet {
    /// Matches any platform configuration.
    All,
    /// Matches a finite set of configurations.
    // Note we use a `BTreeSet` because stable iteration order is desired when
    // generating build files.
    Platforms(BTreeSet<Platform>),
}

impl PlatformSet {
    /// A `PlatformSet` that matches no platforms. Useful as a starting point
    /// when iteratively adding platforms with `add`.
    pub fn empty() -> Self {
        Self::Platforms(BTreeSet::new())
    }

    /// A `PlatformSet` that matches one platform filter.
    #[cfg(test)]
    pub fn one(filter: Option<Platform>) -> Self {
        let mut ps = Self::empty();
        ps.add(filter);
        ps
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

// Whether a CfgExpr matches any build target supported by Chromium.
fn supported_cfg_expr(e: &CfgExpr) -> bool {
    fn validity_can_be_true(v: ExprValidity) -> bool {
        match v {
            ExprValidity::Valid => true,
            ExprValidity::AlwaysTrue => true,
            ExprValidity::AlwaysFalse => false,
        }
    }
    fn recurse(e: &CfgExpr) -> ExprValidity {
        match e {
            CfgExpr::All(x) => {
                if x.iter().all(|e| validity_can_be_true(recurse(e))) {
                    // TODO(danakj): We don't combine to anything fancy.
                    // Technically, if they are all AlwaysTrue it should combine
                    // as such, and then it could be inverted to AlwaysFalse.
                    ExprValidity::Valid
                } else {
                    ExprValidity::AlwaysFalse
                }
            }
            CfgExpr::Any(x) => {
                if x.iter().any(|e| validity_can_be_true(recurse(e))) {
                    // TODO(danakj): We don't combine to anything fancy.
                    // Technically, if anything is AlwaysTrue it should combine
                    // as such, and then it could be inverted to AlwaysFalse.
                    ExprValidity::Valid
                } else {
                    ExprValidity::AlwaysFalse
                }
            }
            CfgExpr::Not(x) => match recurse(x) {
                ExprValidity::AlwaysFalse => ExprValidity::AlwaysTrue,
                ExprValidity::Valid => ExprValidity::Valid,
                ExprValidity::AlwaysTrue => ExprValidity::AlwaysFalse,
            },
            CfgExpr::Value(v) => supported_cfg_value(v),
        }
    }
    validity_can_be_true(recurse(e))
}

// If a Cfg option is always true/false in Chromium, or needs to be conditional
// in the build file's rules.
fn supported_cfg_value(cfg: &Cfg) -> ExprValidity {
    if supported_os_cfgs().iter().any(|c| c == cfg) {
        ExprValidity::Valid // OS is always conditional, as we support more than one.
    } else if supported_arch_cfgs().iter().any(|c| c == cfg) {
        ExprValidity::Valid // Arch is always conditional, as we support more than one.
    } else {
        // Other configs may resolve to AlwaysTrue or AlwaysFalse. If it's
        // unknown, we treat it as AlwaysFalse since we don't know how to
        // convert it to a build file condition.
        supported_other_cfgs()
            .iter()
            .find(|(c, _)| c == cfg)
            .map(|(_, validity)| *validity)
            .unwrap_or(ExprValidity::AlwaysFalse)
    }
}

/// Whether `platform`, either an explicit rustc target triple or a `cfg(...)`
/// expression, matches any build target supported by Chromium.
pub fn matches_supported_target(platform: &Platform) -> bool {
    match platform {
        Platform::Name(name) => SUPPORTED_NAMED_PLATFORMS.iter().any(|p| *p == name),
        Platform::Cfg(expr) => supported_cfg_expr(expr),
    }
}

/// Remove terms containing unsupported platforms from `platform`, assuming
/// `matches_supported_target(&platform)` is true.
///
/// `platform` may contain a cfg(...) expression referencing platforms we don't
/// support: for example, `cfg(any(unix, target_os = "wasi"))`. However, such an
/// expression may still be true on configurations we do support.
///
/// `filter_unsupported_platform_terms` returns a new platform filter without
/// unsupported terms that is logically equivalent for the set of platforms we
/// do support, or `None` if the new filter would be true for all supported
/// platforms. This is useful when generating conditional expressions in build
/// files from such a cfg(...) expression.
///
/// Assumes `matches_supported_target(&platform)` is true. If not, the function
/// may return an invalid result or panic.
pub fn filter_unsupported_platform_terms(platform: Platform) -> Option<Platform> {
    use ExprValidity::*;
    match platform {
        // If it's a target name, do nothing since `is_supported` is true.
        x @ Platform::Name(_) => Some(x),
        // Rewrite `cfg_expr` to be valid.
        Platform::Cfg(mut cfg_expr) => match cfg_expr_filter_visitor(&mut cfg_expr) {
            Valid => Some(Platform::Cfg(cfg_expr)),
            AlwaysTrue => None,
            AlwaysFalse => unreachable!("cfg would be false on all supported platforms"),
        },
    }
}

// The validity of a cfg expr for our set of supported platforms.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum ExprValidity {
    // Contains only terms for supported platforms.
    Valid,
    // Contains terms for unsupported platforms, and would evaluate to true on
    // all supported platforms.
    AlwaysTrue,
    // Contains terms for unsupported platforms, and would evaluate to false on
    // all supported platforms.
    AlwaysFalse,
}

// Rewrites `cfg_expr` to exclude unsupported terms. `ExprValidity::Valid` if
// the rewritten expr is valid: it contains no unsupported terms. Otherwise
// returns `AlwaysTrue` or `AlwaysFalse`.
fn cfg_expr_filter_visitor(cfg_expr: &mut CfgExpr) -> ExprValidity {
    use ExprValidity::*;
    // Any logical operation on a set of valid expressions also yields a valid
    // expression. If any of the set is invalid, we must apply special handling
    // to remove the invalid term or decide the expression is always true or
    // false.
    match cfg_expr {
        // A not(...) expr inverts the truth value of an invalid expr.
        CfgExpr::Not(sub_expr) => match cfg_expr_filter_visitor(sub_expr) {
            Valid => Valid,
            AlwaysTrue => AlwaysFalse,
            AlwaysFalse => AlwaysTrue,
        },
        // An all(...) expr is always false if any term is always false. If any
        // term is always true, it can be removed.
        CfgExpr::All(sub_exprs) => {
            let mut validity = Valid;
            sub_exprs.retain_mut(|e| match cfg_expr_filter_visitor(e) {
                // Keep valid terms.
                Valid => true,
                // Remove always-true terms.
                AlwaysTrue => false,
                // If a term is always false, it doesn't matter; we will discard
                // this expr.
                AlwaysFalse => {
                    validity = AlwaysFalse;
                    true
                }
            });
            if validity == AlwaysFalse {
                AlwaysFalse
            } else if sub_exprs.is_empty() {
                // We only reach this if all the terms we removed were always
                // true, in which case the expression is always true.
                AlwaysTrue
            } else if sub_exprs.len() == 1 {
                // If only one term remains, we can simplify by replacing
                // all(<term>) with <term>.
                let new_expr = sub_exprs.drain(..).next().unwrap();
                *cfg_expr = new_expr;
                Valid
            } else {
                Valid
            }
        }
        // An any(...) expr is always true if any term is always true. If any
        // term is always false, it can be removed.
        CfgExpr::Any(sub_exprs) => {
            let mut validity = Valid;
            sub_exprs.retain_mut(|e| match cfg_expr_filter_visitor(e) {
                // Keep valid terms.
                Valid => true,
                // If a term is always true, it doesn't matter; we will discard
                // this expr.
                AlwaysTrue => {
                    validity = AlwaysTrue;
                    true
                }
                // Remove always-false terms.
                AlwaysFalse => false,
            });
            if validity == AlwaysTrue {
                AlwaysTrue
            } else if sub_exprs.is_empty() {
                // We only reach this if all the terms we removed were always
                // false, in which case the expression is always false.
                AlwaysFalse
            } else if sub_exprs.len() == 1 {
                // If only one term remains, we can simplify by replacing
                // any(<term>) with <term>.
                let new_expr = sub_exprs.drain(..).next().unwrap();
                *cfg_expr = new_expr;
                Valid
            } else {
                Valid
            }
        }
        CfgExpr::Value(cfg) => supported_cfg_value(cfg),
    }
}

fn supported_os_cfgs() -> &'static [Cfg] {
    static CFG_SET: OnceCell<Vec<Cfg>> = OnceCell::new();
    CFG_SET.get_or_init(|| {
        [
            // Set of supported OSes for `cfg(target_os = ...)`.
            "android", "darwin", "fuchsia", "ios", "linux", "windows",
        ]
        .into_iter()
        .map(|os| Cfg::KeyPair("target_os".to_string(), os.to_string()))
        .chain(
            // Alternative syntax `cfg(unix)` or `cfg(windows)`.
            ["unix", "windows"].into_iter().map(|os| Cfg::Name(os.to_string())),
        )
        .collect()
    })
}

fn supported_arch_cfgs() -> &'static [Cfg] {
    static CFG_SET: OnceCell<Vec<Cfg>> = OnceCell::new();
    CFG_SET.get_or_init(|| {
        [
            // Set of supported arches for `cfg(target_arch = ...)`.
            "aarch64", "arm", "x86", "x86_64",
        ]
        .into_iter()
        .map(|a| Cfg::KeyPair("target_arch".to_string(), a.to_string()))
        .collect()
    })
}

fn supported_other_cfgs() -> &'static [(Cfg, ExprValidity)] {
    static CFG_SET: OnceCell<Vec<(Cfg, ExprValidity)>> = OnceCell::new();
    CFG_SET.get_or_init(|| {
        use ExprValidity::*;
        vec![
            // target_env = "msvc" is always true for us, so it can be dropped from expressions.
            (Cfg::KeyPair("target_env".to_string(), "msvc".to_string()), AlwaysTrue),
        ]
    })
}

static SUPPORTED_NAMED_PLATFORMS: &[&str] = &[
    "i686-linux-android",
    "x86_64-linux-android",
    "armv7-linux-android",
    "aarch64-linux-android",
    "aarch64-fuchsia",
    "x86_64-fuchsia",
    "aarch64-apple-ios",
    "aarch64-apple-ios-macabi",
    "armv7-apple-ios",
    "x86_64-apple-ios",
    "x86_64-apple-ios-macabi",
    "i386-apple-ios",
    "i686-pc-windows-msvc",
    "x86_64-pc-windows-msvc",
    "i686-unknown-linux-gnu",
    "x86_64-unknown-linux-gnu",
    "x86_64-apple-darwin",
    "aarch64-apple-darwin",
];

#[cfg(test)]
mod tests {
    use super::*;
    use cargo_platform::{CfgExpr, Platform};
    use std::str::FromStr;

    #[test]
    fn platform_is_supported() {
        for named_platform in SUPPORTED_NAMED_PLATFORMS {
            assert!(matches_supported_target(&Platform::Name(named_platform.to_string())));
        }

        assert!(!matches_supported_target(&Platform::Name("x86_64-unknown-redox".to_string())));
        assert!(!matches_supported_target(&Platform::Name("wasm32-wasi".to_string())));

        for os in supported_os_cfgs() {
            assert!(matches_supported_target(&Platform::Cfg(CfgExpr::Value(os.clone()))));
        }

        assert!(!matches_supported_target(&Platform::Cfg(
            CfgExpr::from_str("target_os = \"redox\"").unwrap()
        )));
        assert!(!matches_supported_target(&Platform::Cfg(
            CfgExpr::from_str("target_os = \"haiku\"").unwrap()
        )));

        assert!(matches_supported_target(&Platform::Cfg(
            CfgExpr::from_str("any(unix, target_os = \"wasi\")").unwrap()
        )));

        assert!(!matches_supported_target(&Platform::Cfg(
            CfgExpr::from_str("all(unix, target_os = \"wasi\")").unwrap()
        )));

        for arch in supported_arch_cfgs() {
            assert!(matches_supported_target(&Platform::Cfg(CfgExpr::Value(arch.clone()))));
        }

        assert!(!matches_supported_target(&Platform::Cfg(
            CfgExpr::from_str("target_arch = \"sparc\"").unwrap()
        )));

        assert!(matches_supported_target(&Platform::Cfg(
            CfgExpr::from_str("not(windows)").unwrap()
        )));
    }

    #[test]
    fn filter_unsupported() {
        assert_eq!(
            filter_unsupported_platform_terms(Platform::Cfg(
                CfgExpr::from_str("any(unix, target_os = \"wasi\")").unwrap()
            )),
            Some(Platform::Cfg(CfgExpr::from_str("unix").unwrap()))
        );

        assert_eq!(
            filter_unsupported_platform_terms(Platform::Cfg(
                CfgExpr::from_str("all(not(unix), not(target_os = \"wasi\"))").unwrap()
            )),
            Some(Platform::Cfg(CfgExpr::from_str("not(unix)").unwrap()))
        );

        assert_eq!(
            filter_unsupported_platform_terms(Platform::Cfg(
                CfgExpr::from_str("not(target_os = \"wasi\")").unwrap()
            )),
            None
        );

        assert_eq!(
            filter_unsupported_platform_terms(Platform::Cfg(
                CfgExpr::from_str("not(all(windows, target_vendor = \"uwp\"))").unwrap()
            )),
            None
        );

        assert_eq!(
            filter_unsupported_platform_terms(Platform::Cfg(
                CfgExpr::from_str("not(all(windows, target_env = \"msvc\"))").unwrap()
            )),
            Some(Platform::Cfg(CfgExpr::from_str("not(windows)").unwrap()))
        );

        assert_eq!(
            filter_unsupported_platform_terms(Platform::Cfg(
                CfgExpr::from_str(
                    "not(all(windows, target_env = \"msvc\", not(target_vendor = \"uwp\")))"
                )
                .unwrap()
            )),
            Some(Platform::Cfg(CfgExpr::from_str("not(windows)").unwrap()))
        );
    }

    #[test]
    // From windows-targets crate.
    fn windows_target_cfgs() {
        // Accepted. `windows_raw_dylib` is not a known cfg so considered AlwaysFalse.
        let cfg = "all(target_arch = \"aarch64\", target_env = \"msvc\", not(windows_raw_dylib))";
        assert_eq!(
            filter_unsupported_platform_terms(Platform::Cfg(CfgExpr::from_str(cfg).unwrap())),
            Some(Platform::Cfg(CfgExpr::from_str("target_arch = \"aarch64\"").unwrap()))
        );
        assert!(matches_supported_target(&Platform::Cfg(CfgExpr::from_str(cfg).unwrap())));

        // Accepted. `windows_raw_dylib` is not a known cfg so considered AlwaysFalse.
        let cfg = "all(any(target_arch = \"x86_64\", target_arch = \"arm64ec\"), \
                   target_env = \"msvc\", not(windows_raw_dylib))";
        assert_eq!(
            filter_unsupported_platform_terms(Platform::Cfg(CfgExpr::from_str(cfg).unwrap())),
            Some(Platform::Cfg(CfgExpr::from_str("target_arch = \"x86_64\"").unwrap()))
        );

        // Accepted. `windows_raw_dylib` is not a known cfg so considered AlwaysFalse.
        let cfg = "all(target_arch = \"x86\", target_env = \"msvc\", not(windows_raw_dylib))";
        assert_eq!(
            filter_unsupported_platform_terms(Platform::Cfg(CfgExpr::from_str(cfg).unwrap())),
            Some(Platform::Cfg(CfgExpr::from_str("target_arch = \"x86\"").unwrap()))
        );

        // Rejected for gnu env.
        let cfg = "all(target_arch = \"x86\", target_env = \"gnu\", not(target_abi = \"llvm\"), \
                   not(windows_raw_dylib))";
        assert!(!matches_supported_target(&Platform::Cfg(CfgExpr::from_str(cfg).unwrap())));
    }
}
