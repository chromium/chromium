// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! GN build file generation.

use crate::config::BuildConfig;
use crate::crates::CrateFiles;
use crate::crates::{Epoch, NormalizedName, VendoredCrate, Visibility};
use crate::deps::{self, DepOfDep};
use crate::group::Group;
use crate::paths;

use std::collections::{HashMap, HashSet};
use std::ops::Deref;

use anyhow::{anyhow, bail, Context, Result};
use itertools::Itertools;
use serde::Serialize;

/// Describes a BUILD.gn file for a single crate epoch. Each file may have
/// multiple rules, including:
/// * A :lib target for normal dependents
/// * A :test_support target for first-party testonly dependents
/// * A :cargo_tests_support target for building third-party tests
/// * A :buildrs_support target for third-party build script dependents
/// * Binary targets for crate executables
#[derive(Default, Serialize)]
pub struct BuildFile {
    pub rules: Vec<Rule>,
}

/// Identifies a package version. A package's dependency list uses this to refer
/// to other targets.
#[derive(Clone, Debug, Eq, Ord, PartialEq, PartialOrd, Serialize)]
pub struct PackageId {
    /// Package name in normalized form, as used in GN target and path names.
    pub name: String,
    /// Package epoch if relevant (i.e. when needed as part of target paths).
    pub epoch: Option<String>,
}

/// Defines what other GN targets can depend on this one.
#[derive(Debug, Default, Serialize)]
pub struct GnVisibility {
    pub testonly: bool,
    /// Controls the visibility constraint on the GN target. If this is true, no
    /// visibility constraint is generated. If false, it's defined so that only
    /// other third party Rust crates can depend on this target.
    pub public: bool,
}

/// A GN rule in a generated build file.
#[derive(Debug, Serialize)]
pub struct Rule {
    /// The GN rule name, which can be unrelated to the Cargo package name.
    pub name: String,
    pub gn_visibility: GnVisibility,
    pub detail: RuleDetail,
}

/// A concrete build rule. Refer to //build/rust/cargo_crate.gni for fields
/// undocumented here.
#[derive(Clone, Debug, Default, Serialize)]
pub struct RuleDetail {
    pub crate_name: Option<String>,
    pub epoch: Option<Epoch>,
    pub crate_type: String,
    pub crate_root: String,
    pub sources: Vec<String>,
    pub inputs: Vec<String>,
    pub edition: String,
    pub cargo_pkg_version: String,
    pub cargo_pkg_authors: Option<String>,
    pub cargo_pkg_name: String,
    pub cargo_pkg_description: Option<String>,
    pub deps: Vec<DepGroup>,
    pub build_deps: Vec<DepGroup>,
    pub aliased_deps: Vec<(String, String)>,
    pub features: Vec<String>,
    pub build_root: Option<String>,
    pub build_script_sources: Vec<String>,
    pub build_script_inputs: Vec<String>,
    pub build_script_outputs: Vec<String>,
    pub native_libs: Vec<String>,
    /// Data passed unchanged from gnrt_config.toml to the build file template.
    pub extra_kv: HashMap<String, serde_json::Value>,
    /// Whether this rule depends on the main lib target in its group (e.g. a
    /// bin target alongside a lib inside a package).
    pub dep_on_lib: bool,
}

/// Set of rule dependencies with a shared condition.
#[derive(Clone, Debug, Serialize)]
pub struct DepGroup {
    /// `if` condition for GN, or `None` for unconditional deps.
    cond: Option<String>,
    /// Packages to depend on. The build file template determines the exact name
    /// based on the identified package and context.
    packages: Vec<PackageId>,
}

/// Extra metadata influencing GN output for a particular crate.
#[derive(Clone, Debug, Default)]
pub struct PerCrateMetadata {
    /// Names of files the build.rs script may output.
    pub build_script_outputs: Vec<String>,
    /// Extra GN code pasted literally into the build rule.
    pub gn_variables: Option<String>,
    /// GN target visibility.
    pub visibility: Visibility,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum NameLibStyle {
    PackageName,
    LibLiteral,
}

pub fn build_file_from_deps<'a, 'b, Iter, GetFiles>(
    deps: Iter,
    paths: &'b paths::ChromiumPaths,
    extra_config: &'b BuildConfig,
    name_lib_style: NameLibStyle,
    get_files: GetFiles,
) -> Result<BuildFile>
where
    Iter: IntoIterator<Item = &'a deps::Package>,
    GetFiles: Fn(&VendoredCrate) -> &'b CrateFiles,
{
    let mut b = BuildFile { rules: Vec::new() };
    for dep in deps {
        let crate_id = dep.crate_id();
        b.rules.extend(build_rule_from_dep(
            dep,
            paths,
            get_files(&crate_id),
            extra_config,
            name_lib_style,
        )?)
    }
    Ok(b)
}

pub fn build_rule_from_dep(
    dep: &deps::Package,
    paths: &paths::ChromiumPaths,
    details: &CrateFiles,
    extra_config: &BuildConfig,
    name_lib_style: NameLibStyle,
) -> Result<Vec<Rule>> {
    let cargo_pkg_authors =
        if dep.authors.is_empty() { None } else { Some(dep.authors.join(", ")) };
    let per_crate_config = extra_config.per_crate_config.get(&*dep.package_name);
    let normalized_crate_name = NormalizedName::from_crate_name(&dep.package_name);
    let crate_epoch = Epoch::from_version(&dep.version);

    // Get deps to exclude from resolved deps.
    let exclude_deps: Vec<String> = per_crate_config
        .iter()
        .flat_map(|c| &c.exclude_deps_in_gn)
        .chain(&extra_config.all_config.exclude_deps_in_gn)
        .cloned()
        .collect();

    // Get the config's extra (key, value) pairs, which are passed as-is to the
    // build file template engine.
    let mut extra_kv = extra_config.all_config.extra_kv.clone();
    if let Some(per_crate) = per_crate_config {
        extra_kv.extend(per_crate.extra_kv.iter().map(|(k, v)| (k.clone(), v.clone())));
    }

    let allow_first_party_usage = match extra_kv.get("allow_first_party_usage") {
        Some(serde_json::Value::Bool(b)) => *b,
        _ => dep.is_toplevel_dep,
    };

    let mut detail_template = RuleDetail {
        edition: dep.edition.clone(),
        cargo_pkg_version: dep.version.to_string(),
        cargo_pkg_authors,
        cargo_pkg_name: dep.package_name.to_string(),
        cargo_pkg_description: dep.description.as_ref().map(|s| s.trim_end().to_string()),

        extra_kv,
        ..Default::default()
    };

    // Add only normal and build dependencies: we don't run unit tests.
    let normal_deps: Vec<&DepOfDep> = dep
        .dependencies
        .iter()
        .filter(|d| !exclude_deps.iter().any(|e| e.as_str() == &*d.package_name))
        .collect();
    let build_deps: Vec<&DepOfDep> = dep
        .build_dependencies
        .iter()
        .filter(|d| !exclude_deps.iter().any(|e| e.as_str() == &*d.package_name))
        .collect();
    let aliased_normal_deps = {
        let mut aliases = Vec::new();
        for dep in &normal_deps {
            let target_name = NormalizedName::from_crate_name(&dep.package_name).to_string();
            if target_name != dep.use_name {
                aliases.push((dep.use_name.clone(), format!(":{target_name}")));
            }
        }
        aliases.sort_unstable();
        aliases.dedup();
        aliases
    };
    // TODO(danakj): There is no support for `aliased_build_deps` in the
    // `cargo_crate` GN template as there's been no usage needed. So we don't
    // compute it here.

    // Group the dependencies by condition, where the unconditional deps come
    // first.
    detail_template.deps = group_deps(&normal_deps, |d| PackageId {
        name: NormalizedName::from_crate_name(&d.package_name).to_string(),
        epoch: match name_lib_style {
            // TODO(danakj): Separate this choice to another parameter option.
            NameLibStyle::LibLiteral => Some(Epoch::from_version(&d.version).to_string()),
            NameLibStyle::PackageName => None,
        },
    })?;
    detail_template.build_deps = group_deps(&build_deps, |d| PackageId {
        name: NormalizedName::from_crate_name(&d.package_name).to_string(),
        epoch: match name_lib_style {
            // TODO(danakj): Separate this choice to another parameter option.
            NameLibStyle::LibLiteral => Some(Epoch::from_version(&d.version).to_string()),
            NameLibStyle::PackageName => None,
        },
    })?;
    detail_template.aliased_deps = aliased_normal_deps;

    detail_template.sources =
        details.sources.iter().map(|p| format!("//{}", paths.to_gn_abs_path(p).unwrap())).collect();
    detail_template.inputs =
        details.inputs.iter().map(|p| format!("//{}", paths.to_gn_abs_path(p).unwrap())).collect();
    detail_template.native_libs = details
        .native_libs
        .iter()
        .map(|p| format!("//{}", paths.to_gn_abs_path(p).unwrap()))
        .collect();

    let requested_features_for_normal = {
        let mut features = dep
            .dependency_kinds
            .get(&deps::DependencyKind::Normal)
            .map(|per_kind_info| per_kind_info.features.clone())
            .unwrap_or_default();
        features.sort_unstable();
        features.dedup();
        features
    };

    let requested_features_for_build = {
        let mut features = dep
            .dependency_kinds
            .get(&deps::DependencyKind::Build)
            .map(|per_kind_info| per_kind_info.features.clone())
            .unwrap_or_default();
        features.sort_unstable();
        features.dedup();
        features
    };

    let unexpected_features: Vec<&str> = {
        let banned_features =
            extra_config.get_combined_set(&dep.package_name, |cfg| &cfg.ban_features);
        let mut actual_features = HashSet::new();
        actual_features.extend(requested_features_for_normal.iter().map(Deref::deref));
        actual_features.extend(requested_features_for_build.iter().map(Deref::deref));
        banned_features.intersection(&actual_features).copied().sorted_unstable().collect()
    };
    if !unexpected_features.is_empty() {
        bail!(
            "The following crate features are enabled in crate `{}` \
             despite being listed in `ban_features`: {}",
            &*dep.package_name,
            unexpected_features.iter().map(|f| format!("`{f}`")).join(", "),
        );
    }

    if !per_crate_config.map(|config| config.remove_build_rs).unwrap_or(false) {
        let build_script_from_src =
            dep.build_script.as_ref().map(|p| paths.to_gn_abs_path(p).unwrap());

        detail_template.build_root = build_script_from_src.as_ref().map(|p| format!("//{p}"));
        detail_template.build_script_sources = build_script_from_src
            .as_ref()
            .map(|p| format!("//{p}"))
            .into_iter()
            .chain(
                details
                    .build_script_sources
                    .iter()
                    .map(|p| format!("//{}", paths.to_gn_abs_path(p).unwrap())),
            )
            .collect();
        detail_template.build_script_inputs = details
            .build_script_inputs
            .iter()
            .map(|p| format!("//{}", paths.to_gn_abs_path(p).unwrap()))
            .collect();
        detail_template.build_script_outputs =
            if let Some(outs) = per_crate_config.map(|config| &config.build_script_outputs) {
                outs.iter().map(|path| path.display().to_string()).collect()
            } else {
                vec![]
            };
    }

    let mut rules: Vec<Rule> = Vec::new();

    // Generate rules for each binary the package provides.
    for bin_target in &dep.bin_targets {
        let bin_root_from_src = paths.to_gn_abs_path(&bin_target.root).unwrap();

        let mut bin_detail = detail_template.clone();
        bin_detail.crate_type = "bin".to_string();
        bin_detail.crate_root = format!("//{bin_root_from_src}");
        // Bins are not part of a build script, so they don't need build-script
        // deps, only normal deps.
        bin_detail.features = requested_features_for_normal.clone();

        if dep.lib_target.is_some() {
            bin_detail.dep_on_lib = true;
            if bin_detail.deps.is_empty() {
                bin_detail.deps.push(DepGroup { cond: None, packages: Vec::new() });
            }
        }

        rules.push(Rule {
            name: NormalizedName::from_crate_name(&bin_target.name).to_string(),
            gn_visibility: GnVisibility { testonly: dep.group == Group::Test, public: true },
            detail: bin_detail,
        });
    }

    // Generate the rule for the main library target, if it exists.
    if let Some(lib_target) = &dep.lib_target {
        use deps::DependencyKind::*;

        let lib_root_from_src = paths.to_gn_abs_path(&lib_target.root).unwrap();

        // Generate the rules for each dependency kind. We use a stable
        // order instead of the hashmap iteration order.
        for dep_kind in [Normal, Build] {
            if !dep.dependency_kinds.contains_key(&dep_kind) {
                continue;
            }

            let lib_rule_name: String = match &dep_kind {
                deps::DependencyKind::Normal => match name_lib_style {
                    NameLibStyle::PackageName => normalized_crate_name.to_string(),
                    NameLibStyle::LibLiteral => "lib".to_string(),
                },
                deps::DependencyKind::Build => "buildrs_support".to_string(),
                _ => unreachable!(),
            };
            let (crate_name, epoch) = match name_lib_style {
                NameLibStyle::PackageName => (None, None),
                NameLibStyle::LibLiteral => {
                    (Some(normalized_crate_name.to_string()), Some(crate_epoch))
                }
            };
            let crate_type = {
                // The stdlib is a "dylib" crate but we only want rlibs.
                let t = lib_target.lib_type.to_string();
                if t == "dylib" {
                    "rlib".to_string()
                } else {
                    t
                }
            };

            let mut lib_detail = detail_template.clone();
            lib_detail.crate_name = crate_name;
            lib_detail.epoch = epoch;
            lib_detail.crate_type = crate_type;
            lib_detail.crate_root = format!("//{lib_root_from_src}");
            lib_detail.features = match &dep_kind {
                Normal => requested_features_for_normal.clone(),
                Build => requested_features_for_build.clone(),
                _ => unreachable!(), // The for loop here is over [Normal, Build].
            };

            // TODO(danakj): Crates in the 'sandbox' group should have their
            // visibility restructed in some way. Possibly to an allowlist
            // specified in the crate's config, and reviewed by security folks?
            rules.push(Rule {
                name: lib_rule_name.clone(),
                gn_visibility: GnVisibility {
                    testonly: dep.group == Group::Test,
                    public: allow_first_party_usage,
                },
                detail: lib_detail,
            });
        }
    }

    Ok(rules)
}

/// Group dependencies by condition, with unconditional deps first.
///
/// If the returned list is non-empty, it will always have a group without a
/// condition, even if that group is empty. If there are no dependencies, then
/// the returned list is empty.
fn group_deps(
    deps: &[&DepOfDep],
    target_name: impl Fn(&DepOfDep) -> PackageId,
) -> Result<Vec<DepGroup>> {
    let mut groups = HashMap::<Option<String>, Vec<_>>::new();
    for dep in deps {
        let cond = dep.condition.to_handlebars_value().with_context(|| {
            format!(
                "Error processing condition of the following dependency: `{}`",
                dep.package_name
            )
        })?;
        groups.entry(cond).or_default().push(target_name(dep));
    }

    if !groups.is_empty() {
        groups.entry(None).or_default();
    }

    let mut groups: Vec<DepGroup> =
        groups.into_iter().map(|(cond, packages)| DepGroup { cond, packages }).collect();

    for group in groups.iter_mut() {
        group.packages.sort_unstable();
    }
    groups.sort_unstable_by(|l, r| l.cond.cmp(&r.cond));
    Ok(groups)
}

/// Describes a condition for some GN declaration.
#[derive(Clone, Debug, Hash, Eq, Ord, PartialEq, PartialOrd, Serialize)]
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
    pub fn or(lhs: Condition, rhs: Condition) -> Self {
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

    fn to_handlebars_value(&self) -> Result<Option<String>> {
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
}

pub fn target_platform_to_condition(spec: &cargo_platform::Platform) -> Condition {
    use cargo_platform::Platform::*;
    match spec {
        Name(triple) => triple_to_condition(triple.as_str()),
        Cfg(cfg_expr) => cfg_expr_to_condition(cfg_expr),
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
            _ => Condition::Unsupported(format!("Unknown key `{key}` in `{cfg}`")),
        },
    }
}

/// `name` should correspond to https://doc.rust-lang.org/reference/conditional-compilation.html#r-cfg.option-name
fn cfg_name_to_condition(name: &str) -> Condition {
    const FAMILY_NAMES: [&str; 2] = ["unix", "windows"];
    if FAMILY_NAMES.contains(&name) {
        return target_family_to_condition(name);
    }

    // We don't support `windows_raw_dylib` in Chromium.  See also
    // https://github.com/rust-lang/rust/issues/58713
    if ["windows_raw_dylib"].contains(&name) {
        return Condition::AlwaysFalse;
    }

    Condition::Unsupported(format!("unknown option name: `#[cfg({name})]`"))
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
    for (t, c) in &[
        // Based on `triple_to_condition` `msvc` is the only supported environment
        // on Windows.
        //
        // TODO(lukasza): Would returning `Condition::Expr("is_win")` be more correct?
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
        // TODO(lukasza): Would returning `Condition::Expr("is_linux || is_chromeos")` be more
        // correct?
        ("gnu", Condition::AlwaysFalse),
        // `sgx` is used as condition in `dlmalloc` package in `std` library.
        ("sgx", Condition::AlwaysFalse),
    ] {
        if *t == target_env {
            return c.clone();
        }
    }

    Condition::Unsupported(format!("unknown `target_env` value: `{target_env}`"))
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
        ("darwin", "is_mac"),
        ("fuchsia", "is_fuchsia"),
        ("ios", "is_ios"),
        ("linux", "is_linux || is_chromeos"),
        ("windows", "is_win"),
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
    const UNSUPPORTED_VENDORS: [&str; 2] = [
        "fortanix", // Used as condition in `dlmalloc` package used in `std` library.
        "uwp",      // Used as condition in some `windows...` crates.
    ];
    if UNSUPPORTED_VENDORS.contains(&target_vendor) {
        return Condition::AlwaysFalse;
    }

    Condition::Unsupported(format!("unknown `target_vendor` name: `{target_vendor}`"))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn condition_from_test_triple(triple: &str) -> Condition {
        let platform = cargo_platform::Platform::Name(triple.to_string());
        target_platform_to_condition(&platform)
    }

    fn condition_from_test_expr(expr: &str) -> Condition {
        let platform =
            cargo_platform::Platform::Cfg(expr.parse::<cargo_platform::CfgExpr>().unwrap());
        target_platform_to_condition(&platform)
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
    }
}
