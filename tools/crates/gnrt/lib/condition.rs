// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use anyhow::{anyhow, Result};

/// Representation of a `Condition` associated with a conditional/optional
/// dependency.
#[derive(Clone, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
pub enum Condition {
    /// The condition is always false.  In other words, supported Chromium
    /// builds never meet this condition.
    ///
    /// Example: `#[cfg(target_arch = "powerpc")]`.
    AlwaysFalse,
    /// The condition is always true.
    ///
    /// Example: `#[cfg(not(target_arch = "powerpc"))]`.
    AlwaysTrue,
    /// Ignored terms.  For example we ignore `target_abi` and assume that
    /// `target_env` is sufficient for picking the right dependencies.
    Ignored,
    /// The condition requires evaluating the nested GN expression.
    /// The `String` payload is the condition expressed in GN syntax (e.g.
    /// `is_win`).
    ///
    /// For example `#[cfg(target_os = "windows")]` translates into
    /// `Condition::Expr("is_win".to_string())`.
    Expr(String),
    ///
    /// Some of the [conditional
    /// compilation](https://doc.rust-lang.org/reference/conditional-compilation.html) directives
    /// weren't recognized by `gnrt`.
    ///
    /// The `String` is an error message.
    ///
    /// In some cases such terms will "disappear" - e.g. `unknown_cfg &&
    /// always_false` is the same as `always_false`.  When these terms do
    /// not disappear, then it may mean that supporting a new crate would
    /// require teaching `gnrt` about the new kinds of configuration.
    Unsupported(String),
}

impl Condition {
    pub fn is_always_false(&self) -> bool {
        *self == Condition::AlwaysFalse
    }

    pub fn or(lhs: Condition, rhs: Condition) -> Self {
        // Avoiding unnecessarily constructing `is_win || is_win`.
        // This is mostly needed for `or`, because this is where
        // different dependency edge kinds (e.g. `Build` vs `Normal`)
        // result in `or`-ing of conditions as driven by `deps.rs`.
        if lhs == rhs {
            return lhs;
        }

        match (lhs, rhs) {
            (Condition::AlwaysFalse, other) | (other, Condition::AlwaysFalse) => other.clone(),
            (Condition::AlwaysTrue, _) | (_, Condition::AlwaysTrue) => Condition::AlwaysTrue,
            (Condition::Ignored, other) | (other, Condition::Ignored) => other.clone(),
            (Condition::Expr(lhs), Condition::Expr(rhs)) => {
                Condition::Expr(format!("({lhs}) || ({rhs})"))
            }
            (err @ Condition::Unsupported(_), _) | (_, err @ Condition::Unsupported(_)) => {
                err.clone()
            }
        }
    }

    fn and(lhs: Condition, rhs: Condition) -> Self {
        match (lhs, rhs) {
            (Condition::AlwaysFalse, _) | (_, Condition::AlwaysFalse) => Condition::AlwaysFalse,
            (Condition::AlwaysTrue, other) | (other, Condition::AlwaysTrue) => other,
            (Condition::Ignored, other) | (other, Condition::Ignored) => other,
            (Condition::Expr(lhs), Condition::Expr(rhs)) => {
                Condition::Expr(format!("({lhs}) && ({rhs})"))
            }
            (err @ Condition::Unsupported(_), _) | (_, err @ Condition::Unsupported(_)) => err,
        }
    }

    fn not(other: Condition) -> Self {
        match other {
            Condition::AlwaysFalse => Condition::AlwaysTrue,
            Condition::AlwaysTrue => Condition::AlwaysFalse,
            Condition::Ignored => Condition::Ignored,
            Condition::Expr(expr) => Condition::Expr(format!("!({expr})")),
            err @ Condition::Unsupported(_) => err,
        }
    }

    pub fn to_handlebars_value(&self) -> Result<Option<String>> {
        match self {
            Condition::AlwaysTrue | Condition::Ignored => Ok(None),
            Condition::Expr(expr) => Ok(Some(expr.clone())),
            Condition::AlwaysFalse => unreachable!(
                "AlwaysFalse dependencies should be filtered out \
                              by `fn collect_dependencies` from `deps.rs`"
            ),
            Condition::Unsupported(err) => {
                Err(anyhow!("{err}")
                    .context("Failed to translate `#[cfg(...)]` into a GN condition"))
            }
        }
    }

    pub fn from_target_spec(spec: &target_spec::TargetSpec) -> Self {
        use target_spec::TargetSpec::*;
        match spec {
            PlainString(triple) => triple_to_condition(triple.as_str()),
            Expression(expr) => {
                let cfg_expr = expr.expression_str().parse().unwrap();
                cfg_expr_to_condition(&cfg_expr)
            }
        }
    }
}

fn cfg_expr_to_condition(cfg_expr: &cargo_platform::CfgExpr) -> Condition {
    match cfg_expr {
        cargo_platform::CfgExpr::Not(expr) => Condition::not(cfg_expr_to_condition(expr)),
        cargo_platform::CfgExpr::All(exprs) => {
            let mut conds = exprs.iter().map(cfg_expr_to_condition).collect::<Vec<_>>();
            conds.sort();
            conds.dedup();

            // https://doc.rust-lang.org/reference/conditional-compilation.html#r-cfg.predicate.all
            // says that "It is true if "all of the given predicates are true, or if the
            // list is empty."
            conds.into_iter().fold(Condition::AlwaysTrue, |accumulated, condition| {
                Condition::and(accumulated, condition)
            })
        }
        cargo_platform::CfgExpr::Any(exprs) => {
            let mut conds = exprs.iter().map(cfg_expr_to_condition).collect::<Vec<_>>();
            conds.sort();
            conds.dedup();

            // https://doc.rust-lang.org/reference/conditional-compilation.html#r-cfg.predicate.any
            // says that "It is true if at least one of the given predicates is true. If
            // there are no predicates, it is false.".
            conds.into_iter().fold(Condition::AlwaysFalse, |accumulated, condition| {
                Condition::or(accumulated, condition)
            })
        }
        cargo_platform::CfgExpr::Value(cfg) => cfg_to_condition(cfg),
    }
}

fn cfg_to_condition(cfg: &cargo_platform::Cfg) -> Condition {
    match cfg {
        cargo_platform::Cfg::Name(name) => cfg_name_to_condition(name),
        cargo_platform::Cfg::KeyPair(key, value) => match key.as_ref() {
            "target_abi" => Condition::Ignored,
            "target_arch" => target_arch_to_condition(value),
            "target_env" => target_env_to_condition(value),
            "target_family" => target_family_to_condition(value),
            "target_os" => target_os_to_condition(value),
            "target_vendor" => target_vendor_to_condition(value),
            "panic" => panic_cfg_to_condition(value),
            _ => {
                // Keys that start with `target_` are the only remaining ones that are 1) not
                // handled above, but 2) listed as set by `rustc` by the documentation at
                // https://doc.rust-lang.org/reference/conditional-compilation.html
                if key.starts_with("target_") {
                    // TODO(https://crbug.com/402096443): Add support for more keys set by `rustc`.
                    Condition::Unsupported(format!("Not yet supported key `{key}` in `{cfg}`"))
                } else {
                    // `key` is not set by `rustc` (i.e. it is not documented in
                    // https://doc.rust-lang.org/reference/conditional-compilation.html and not
                    // handled above).  Therefore we assume that Chromium will never ask GN/ninja
                    // to pass `--cfg 'this_unrecognized_key="something"'` to `rustc`.  And
                    // therefore we treat this as `AlwaysFalse`.  See also
                    // https://crbug.com/404598090#comment4.
                    log::warn!(
                        "Treating unrecogized `#[cfg({key} = \"{value}\")]` as `AlwaysFalse"
                    );
                    Condition::AlwaysFalse
                }
            }
        },
    }
}

/// `name` should correspond to https://doc.rust-lang.org/reference/conditional-compilation.html#r-cfg.option-name
fn cfg_name_to_condition(name: &str) -> Condition {
    // See https://doc.rust-lang.org/reference/conditional-compilation.html#unix-and-windows
    const FAMILY_NAMES: [&str; 2] = ["unix", "windows"];
    if FAMILY_NAMES.contains(&name) {
        return target_family_to_condition(name);
    }

    // We don't support `windows_raw_dylib` in Chromium.  See also
    // https://github.com/rust-lang/rust/issues/58713
    if ["windows_raw_dylib"].contains(&name) {
        return Condition::AlwaysFalse;
    }

    // See https://doc.rust-lang.org/reference/conditional-compilation.html#debug_assertions
    if name == "debug_assertions" {
        return Condition::Expr("is_debug".to_string());
    }

    // See https://doc.rust-lang.org/reference/conditional-compilation.html#test
    //
    // TODO(https://crbug.com/402096443): Add support for `#[cfg(test)]`.
    if name == "test" {
        return Condition::Unsupported("`#[cfg(test)]` is not yet supported by `gnrt`".to_string());
    }

    // `name` is not something that is documented in
    // https://doc.rust-lang.org/reference/conditional-compilation.html.  We assume that Chromium
    // will never ask GN/ninja to pass `--cfg this_unrecognized_name` to `rustc`.
    // And therefore we treat this as `AlwaysFalse`.  See also https://crbug.com/404598090#comment4.
    log::warn!("Treating unrecogized `#[cfg({name})]` as `AlwaysFalse");
    Condition::AlwaysFalse
}

/// `value` should correspond to https://doc.rust-lang.org/reference/conditional-compilation.html#r-cfg.panic.values
fn panic_cfg_to_condition(value: &str) -> Condition {
    // `//build/config/compiler/BUILD.gn` always hardcodes `-Cpanic=abort` into
    // `rustflags`.
    match value {
        "abort" => Condition::AlwaysTrue,
        "unwind" => Condition::AlwaysFalse,
        _ => Condition::Unsupported(format!(
            "Unrecognized panic configuration: `#[cfg(panic = \"{value}\")]`"
        )),
    }
}

fn triple_to_condition(triple: &str) -> Condition {
    for (t, c) in &[
        ("i686-linux-android", "is_android && current_cpu == \"x86\""),
        ("x86_64-linux-android", "is_android && current_cpu == \"x64\""),
        ("armv7-linux-android", "is_android && current_cpu == \"arm\""),
        ("aarch64-linux-android", "is_android && current_cpu == \"arm64\""),
        ("aarch64-fuchsia", "is_fuchsia && current_cpu == \"arm64\""),
        ("x86_64-fuchsia", "is_fuchsia && current_cpu == \"x64\""),
        ("aarch64-apple-ios", "is_ios && current_cpu == \"arm64\""),
        ("armv7-apple-ios", "is_ios && current_cpu == \"arm\""),
        ("x86_64-apple-ios", "is_ios && current_cpu == \"x64\""),
        ("i386-apple-ios", "is_ios && current_cpu == \"x86\""),
        ("i686-pc-windows-msvc", "is_win && current_cpu == \"x86\""),
        ("x86_64-pc-windows-msvc", "is_win && current_cpu == \"x64\""),
        ("i686-unknown-linux-gnu", "(is_linux || is_chromeos) && current_cpu == \"x86\""),
        ("x86_64-unknown-linux-gnu", "(is_linux || is_chromeos) && current_cpu == \"x64\""),
        ("x86_64-apple-darwin", "is_mac && current_cpu == \"x64\""),
        ("aarch64-apple-darwin", "is_mac && current_cpu == \"arm64\""),
    ] {
        if *t == triple {
            return Condition::Expr(c.to_string());
        }
    }

    // Other target triples are never used in Chromium builds.
    Condition::AlwaysFalse
}

/// `target_arch` should correspond to https://doc.rust-lang.org/reference/conditional-compilation.html#target_arch
fn target_arch_to_condition(target_arch: &str) -> Condition {
    for (t, c) in &[
        ("aarch64", "current_cpu == \"arm64\""),
        ("arm", "current_cpu == \"arm\""),
        ("x86", "current_cpu == \"x86\""),
        ("x86_64", "current_cpu == \"x64\""),
        // `riscv64gc-unknown-linux-gnu` from `build/config/rust.gni` resolves to
        // `target_os = "riscv64"`.  And `gn help target_cpu` says that this has
        // the same spelling as GN's `current_cpu`.
        ("riscv64", "current_cpu == \"riscv64\""),
    ] {
        if *t == target_arch {
            return Condition::Expr(c.to_string());
        }
    }

    // Other `target_arch` values are never used in Chromium builds.
    // Examples: "mipc", "powerpc".
    Condition::AlwaysFalse
}

/// `target_env` should correspond to https://doc.rust-lang.org/reference/conditional-compilation.html#target_env
fn target_env_to_condition(target_env: &str) -> Condition {
    // Based on https://crbug.com/402096443#comment6 target triples supported by Chromium
    // only use `target_env` set to either `msvc`, `gnu`, or an empty string.
    for (t, c) in &[
        // `msvc` is the only supported environment on Windows.
        //
        // TODO(https://crbug.com/402096443): Would returning `Condition::Expr("is_win")` be more
        // correct?
        ("msvc", Condition::AlwaysTrue),
        // Treating `gnu` as `AlwaysFalse`, because:
        //
        // * This is how `gnrt` worked in the past
        // * This helps to filter out packages like `windows_i686_gnu` (this is desirable, because
        //   Chromium only supports `msvc` environment on Windows.
        //
        // OTOH, maybe this is not quite right, because Chromium also supports triples like
        // "i686-unknown-linux-gnu".
        //
        // TODO(https://crbug.com/402096443): Would returning
        // `Condition::Expr(CONDITION_FOR_TARGET_OS_LINUX.to_string())` be more correct?
        // OTOH `AlwaysFalse` will trim a dependency, but a more complicated
        // expression that may be equivalent to `AlwaysFalse` will not trim...
        ("gnu", Condition::AlwaysFalse),
        // Based on https://crbug.com/402096443#comment6, an empty `target_env` is only
        // used in the following cases:
        ("", Condition::Expr("is_android || is_apple || is_fuchsia".to_string())),
    ] {
        if *t == target_env {
            return c.clone();
        }
    }

    // Other `target_env` values are never used by target triples supported by
    // Chromium. For example, `sgx` is used as condition in `dlmalloc` package
    // in `std` library, but this condition will never be true in Chromium
    // builds.
    Condition::AlwaysFalse
}

/// `target_family` should correspond to https://doc.rust-lang.org/reference/conditional-compilation.html#target_family
fn target_family_to_condition(target_family: &str) -> Condition {
    for (t, c) in &[
        // Note that while Fuchsia is not a unix, rustc sets the unix cfg
        // anyway. We must be consistent with rustc. This may change with
        // https://github.com/rust-lang/rust/issues/58590
        ("unix", "!is_win"),
        ("windows", "is_win"),
    ] {
        if *t == target_family {
            return Condition::Expr(c.to_string());
        }
    }

    // Other `target_family` values are never used in Chromium builds.
    // Example: "wasm".
    Condition::AlwaysFalse
}

/// `target_os` should correspond to https://doc.rust-lang.org/reference/conditional-compilation.html#target_os
fn target_os_to_condition(target_os: &str) -> Condition {
    for (t, c) in &[
        ("android", "is_android"),
        // `rustc --print=cfg --target=aarch64-apple-darwin` prints `macos`, not `darwin`.
        ("macos", "is_mac"),
        ("fuchsia", "is_fuchsia"),
        ("ios", "is_ios"),
        ("linux", CONDITION_FOR_TARGET_OS_LINUX),
        ("windows", "is_win"),
        // TODO(https://crbug.com/402096443): Consider also mapping "tvos"
        // (since `aarch64-apple-tvos` is listed in `build/config/rust.gni`)
    ] {
        if *t == target_os {
            return Condition::Expr(c.to_string());
        }
    }

    // Other `target_os` values are never used in Chromium builds.
    // Examples: "freebsd", "macos" (not sure why "darwin" is preferred...).
    Condition::AlwaysFalse
}

/// `target_vendor` should correspond to https://doc.rust-lang.org/reference/conditional-compilation.html#target_vendor
fn target_vendor_to_condition(target_vendor: &str) -> Condition {
    for (t, c) in &[("apple", "is_apple"), ("pc", "is_win")] {
        if *t == target_vendor {
            return Condition::Expr(c.to_string());
        }
    }

    const UNSUPPORTED_VENDORS: [&str; 2] = [
        "fortanix", // Used as condition in `dlmalloc` package used in `std` library.
        "uwp",      // Used as condition in some `windows...` crates.
    ];
    if UNSUPPORTED_VENDORS.contains(&target_vendor) {
        return Condition::AlwaysFalse;
    }

    Condition::Unsupported(format!("unknown `target_vendor` name: `{target_vendor}`"))
}

/// GN condition corresponding to `target_os` being set to `linux` in `rustc`.
///
/// `//build/config/BUILDCONFIG.gn` treats `is_linux` and `is_chromeos` as
/// mutually exclusive, but at `rustc`-level they are both `target_os = "linux"`
/// (and `target_env = "gnu"`).  Both of these `rustc`-level values can be taken
/// directly from the target triple, but we have also directly verified via
/// `rustc --print=cfg` - see https://crbug.com/402096443#comment6.
const CONDITION_FOR_TARGET_OS_LINUX: &str = "is_linux || is_chromeos";

#[cfg(test)]
mod tests {
    use super::Condition;

    fn condition_from_test_triple(triple: &str) -> Condition {
        let spec = target_spec::TargetSpec::PlainString(
            target_spec::TargetSpecPlainString::new(triple.to_string()).unwrap(),
        );
        Condition::from_target_spec(&spec)
    }

    fn condition_from_test_expr(expr: &str) -> Condition {
        let spec = target_spec::TargetSpec::Expression(
            target_spec::TargetSpecExpression::new(expr).unwrap(),
        );
        Condition::from_target_spec(&spec)
    }

    #[test]
    fn test_target_spec_to_condition() {
        // Try a target triple.
        assert_eq!(
            condition_from_test_triple("x86_64-pc-windows-msvc"),
            Condition::Expr("is_win && current_cpu == \"x64\"".to_string()),
        );

        // Try a cfg expression.
        assert_eq!(
            condition_from_test_expr("any(windows, target_os = \"android\")"),
            Condition::Expr("(is_android) || (is_win)".to_string()),
        );

        // Redundant cfg expression.
        assert_eq!(
            condition_from_test_expr("any(windows, windows)"),
            Condition::Expr("is_win".to_string()),
        );

        // Try a PlatformSet with multiple filters.
        let filter1 = condition_from_test_triple("armv7-linux-android");
        let filter2 = condition_from_test_expr("windows");
        assert_eq!(
            Condition::or(filter1, filter2),
            Condition::Expr("(is_android && current_cpu == \"arm\") || (is_win)".to_string()),
        );

        // A cfg expression on arch only.
        assert_eq!(
            condition_from_test_expr("target_arch = \"aarch64\""),
            Condition::Expr("current_cpu == \"arm64\"".to_string()),
        );

        // A cfg expression on arch and OS (but not via the target triple string).
        assert_eq!(
            condition_from_test_expr("all(target_arch = \"aarch64\", unix)"),
            Condition::Expr("(!is_win) && (current_cpu == \"arm64\")".to_string()),
        );

        // A cfg expression taken from `windows_aarch64_msvc` package.
        assert_eq!(
            condition_from_test_expr(
                "all(any(target_arch = \"x86_64\", target_arch = \"arm64ec\"), \
                     target_env = \"msvc\", \
                     not(windows_raw_dylib))"
            ),
            Condition::Expr("current_cpu == \"x64\"".to_string()),
        );

        // A cfg expression taken from `windows-targets` => `windows_i686_gnu`
        // dependency.
        assert_eq!(
            condition_from_test_expr(
                "all(target_arch = \"x86\", \
                     target_env = \"gnu\", \
                     not(target_abi = \"llvm\"), \
                     not(windows_raw_dylib))"
            ),
            Condition::AlwaysFalse,
        );

        // Cfg expressions taken from `getrandom-0.3` => `libc` dependency.
        assert_eq!(
            condition_from_test_expr(
                "any(                         \
                    target_os = \"ios\",      \
                    target_os = \"visionos\", \
                    target_os = \"watchos\",  \
                    target_os = \"tvos\")",
            ),
            Condition::Expr("is_ios".to_string()),
        );
        assert_eq!(
            condition_from_test_expr(
                "any(                        \
                    target_os = \"macos\",   \
                    target_os = \"openbsd\", \
                    target_os = \"vita\",    \
                    target_os = \"emscripten\")",
            ),
            Condition::Expr("is_mac".to_string()),
        );
        assert_eq!(
            condition_from_test_expr(
                // Simplification of one of the real expressions below.
                "all(target_os = \"linux\", target_env = \"\")",
            ),
            // TODO(lukasza): Ideally `gnrt` would understand that the condition below is kind of
            // equivalent to `AlwaysFalse`... :-/
            Condition::Expr(
                "(is_android || is_apple || is_fuchsia) && \
                 (is_linux || is_chromeos)"
                    .to_string()
            ),
        );
        assert_eq!(
            condition_from_test_expr(
                "all(                                                      \
                    any(target_os = \"linux\", target_os = \"android\"),   \
                    not(any(                                               \
                            all(target_os = \"linux\", target_env = \"\"), \
                            getrandom_backend = \"custom\",                \
                            getrandom_backend = \"linux_raw\",             \
                            getrandom_backend = \"rdrand\",                \
                            getrandom_backend = \"rndr\")))",
            ),
            Condition::Expr(
                "(!((is_android || is_apple || is_fuchsia) && (is_linux || is_chromeos))) && \
                 ((is_android) || (is_linux || is_chromeos))"
                    .to_string()
            ),
        );
    }
}
