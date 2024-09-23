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
use crate::platforms;

use std::collections::{HashMap, HashSet};
use std::ops::Deref;

use anyhow::{bail, Result};
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
    cond: Option<Condition>,
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
    });
    detail_template.build_deps = group_deps(&build_deps, |d| PackageId {
        name: NormalizedName::from_crate_name(&d.package_name).to_string(),
        epoch: match name_lib_style {
            // TODO(danakj): Separate this choice to another parameter option.
            NameLibStyle::LibLiteral => Some(Epoch::from_version(&d.version).to_string()),
            NameLibStyle::PackageName => None,
        },
    });
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
            extra_config.get_combined_set(&*dep.package_name, |cfg| &cfg.ban_features);
        let mut actual_features = HashSet::new();
        actual_features.extend(requested_features_for_normal.iter().map(Deref::deref));
        actual_features.extend(requested_features_for_build.iter().map(Deref::deref));
        banned_features.intersection(&actual_features).map(|s| *s).sorted_unstable().collect()
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
            if dep.dependency_kinds.get(&dep_kind).is_none() {
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
                if t == "dylib" { "rlib".to_string() } else { t }
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
fn group_deps<F: Fn(&DepOfDep) -> PackageId>(deps: &[&DepOfDep], target_name: F) -> Vec<DepGroup>
where
    F: Fn(&DepOfDep) -> PackageId,
{
    let mut groups = HashMap::<Option<Condition>, Vec<_>>::new();
    for dep in deps {
        let cond = dep.platform.as_ref().map(platform_to_condition);

        groups.entry(cond).or_default().push(target_name(dep));
    }

    if !groups.is_empty() {
        groups.entry(None).or_default();
    }

    let mut groups: Vec<DepGroup> =
        groups.into_iter().map(|(cond, rules)| DepGroup { cond, packages: rules }).collect();

    for group in groups.iter_mut() {
        group.packages.sort_unstable();
    }
    groups.sort_unstable_by(|l, r| l.cond.cmp(&r.cond));
    groups
}

/// Describes a condition for some GN declaration.
#[derive(Clone, Debug, Hash, Eq, Ord, PartialEq, PartialOrd, Serialize)]
pub struct Condition(pub String);

impl Condition {
    pub fn from_platform_set(platforms: platforms::PlatformSet) -> Option<Self> {
        let platforms = match platforms {
            platforms::PlatformSet::All => return None,
            platforms::PlatformSet::Platforms(platforms) => platforms,
        };

        Some(Condition(
            platforms
                .iter()
                .map(|p| format!("({})", platform_to_condition(p).0))
                .collect::<Vec<_>>()
                .join(" || "),
        ))
    }
}

/// Map a cargo `Platform` constraint to a GN conditional expression.
pub fn platform_to_condition(platform: &platforms::Platform) -> Condition {
    Condition(match platform {
        platforms::Platform::Name(triple) => triple_to_condition(triple).to_string(),
        platforms::Platform::Cfg(cfg_expr) => cfg_expr_to_condition(cfg_expr),
    })
}

pub fn cfg_expr_to_condition(cfg_expr: &cargo_platform::CfgExpr) -> String {
    match cfg_expr {
        cargo_platform::CfgExpr::Not(expr) => {
            format!("!({})", cfg_expr_to_condition(expr))
        }
        cargo_platform::CfgExpr::All(exprs) => {
            let mut conds = exprs
                .iter()
                .map(|expr| format!("({})", cfg_expr_to_condition(expr)))
                .collect::<Vec<String>>();
            conds.sort();
            conds.dedup();
            conds.join(" && ")
        }
        cargo_platform::CfgExpr::Any(exprs) => {
            let mut conds = exprs
                .iter()
                .map(|expr| format!("({})", cfg_expr_to_condition(expr)))
                .collect::<Vec<String>>();
            conds.sort();
            conds.dedup();
            conds.join(" || ")
        }
        cargo_platform::CfgExpr::Value(cfg) => cfg_to_condition(cfg),
    }
}

pub fn cfg_to_condition(cfg: &cargo_platform::Cfg) -> String {
    match cfg {
        cargo_platform::Cfg::Name(name) => match name.as_str() {
            // Note that while Fuchsia is not a unix, rustc sets the unix cfg
            // anyway. We must be consistent with rustc. This may change with
            // https://github.com/rust-lang/rust/issues/58590
            "unix" => "!is_win",
            "windows" => "is_win",
            _ => unreachable!(),
        },
        cargo_platform::Cfg::KeyPair(key, value) => match key.as_ref() {
            "target_os" => target_os_to_condition(value),
            "target_arch" => target_arch_to_condition(value),
            _ => unreachable!("unknown key in cargo_platform::Cfg"),
        },
    }
    .to_string()
}

fn triple_to_condition(triple: &str) -> &'static str {
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
            return c;
        }
    }

    panic!("target triple {triple} not found")
}

fn target_os_to_condition(target_os: &str) -> &'static str {
    for (t, c) in &[
        ("android", "is_android"),
        ("darwin", "is_mac"),
        ("fuchsia", "is_fuchsia"),
        ("ios", "is_ios"),
        ("linux", "is_linux || is_chromeos"),
        ("windows", "is_win"),
    ] {
        if *t == target_os {
            return c;
        }
    }

    panic!("target os {target_os} not found")
}

fn target_arch_to_condition(target_arch: &str) -> &'static str {
    for (t, c) in &[
        ("aarch64", "current_cpu == \"arm64\""),
        ("arm", "current_cpu == \"arm\""),
        ("x86", "current_cpu == \"x86\""),
        ("x86_64", "current_cpu == \"x64\""),
    ] {
        if *t == target_arch {
            return c;
        }
    }

    panic!("target arch {target_arch} not found")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn platform_to_condition() {
        use crate::platforms::{Platform, PlatformSet};
        use cargo_platform::CfgExpr;
        use std::str::FromStr;

        // Try an unconditional filter.
        assert_eq!(Condition::from_platform_set(PlatformSet::one(None)), None);

        // Try a target triple.
        assert_eq!(
            Condition::from_platform_set(PlatformSet::one(Some(Platform::Name(
                "x86_64-pc-windows-msvc".to_string()
            ))))
            .unwrap()
            .0,
            "(is_win && current_cpu == \"x64\")"
        );

        // Try a cfg expression.
        assert_eq!(
            Condition::from_platform_set(PlatformSet::one(Some(Platform::Cfg(
                CfgExpr::from_str("any(windows, target_os = \"android\")").unwrap()
            ))))
            .unwrap()
            .0,
            "((is_android) || (is_win))"
        );

        // Redundant cfg expression.
        assert_eq!(
            Condition::from_platform_set(PlatformSet::one(Some(Platform::Cfg(
                CfgExpr::from_str("any(windows, windows)").unwrap()
            ))))
            .unwrap()
            .0,
            "((is_win))"
        );

        // Try a PlatformSet with multiple filters.
        let mut platform_set = PlatformSet::empty();
        platform_set.add(Some(Platform::Name("armv7-linux-android".to_string())));
        platform_set.add(Some(Platform::Cfg(CfgExpr::from_str("windows").unwrap())));
        assert_eq!(
            Condition::from_platform_set(platform_set).unwrap().0,
            "(is_android && current_cpu == \"arm\") || (is_win)"
        );

        // A cfg expression on arch only.
        assert_eq!(
            Condition::from_platform_set(PlatformSet::one(Some(Platform::Cfg(
                CfgExpr::from_str("target_arch = \"aarch64\"").unwrap()
            ))))
            .unwrap()
            .0,
            "(current_cpu == \"arm64\")"
        );

        // A cfg expression on arch and OS (but not via the target triple string).
        assert_eq!(
            Condition::from_platform_set(PlatformSet::one(Some(Platform::Cfg(
                CfgExpr::from_str("all(target_arch = \"aarch64\", unix)").unwrap()
            ))))
            .unwrap()
            .0,
            "((!is_win) && (current_cpu == \"arm64\"))"
        );
    }
}
