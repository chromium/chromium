// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Utilities to process `cargo metadata` dependency graph.

use crate::{
    condition::Condition, config::BuildConfig, crates, group::Group,
    inherit::find_inherited_privilege_group,
};

use anyhow::{bail, Context, Result};
use guppy::{
    graph::cargo::{CargoOptions, CargoSet},
    graph::feature::{FeatureSet, StandardFeatures},
    graph::{
        BuildTargetId, BuildTargetKind, DependencyDirection, PackageGraph, PackageLink,
        PackageMetadata, PackageQuery, PackageSet,
    },
    platform::PlatformStatus,
};
use itertools::Itertools;
pub use semver::Version;
use std::{
    collections::{HashMap, HashSet},
    fmt::{self, Display},
    path::PathBuf,
};

/// A single transitive dependency of a root crate. Includes information needed
/// for generating build files later.
#[derive(Clone, Debug)]
pub struct Package {
    /// The package name as used by cargo.
    pub package_name: String,
    /// The package version as used by cargo.
    pub version: Version,
    pub description: Option<String>,
    pub authors: Vec<String>,
    pub edition: String,
    pub repository: Option<String>,
    /// This package's dependencies. Each element cross-references another
    /// `Package` by name and version.
    pub dependencies: Vec<DepOfDep>,
    /// Same as the above, but for build script deps.
    pub build_dependencies: Vec<DepOfDep>,
    /// A package can be depended upon in different ways: as a normal
    /// dependency, just for build scripts, or just for tests. `kinds` contains
    /// an entry for each way this package is depended on.
    pub dependency_kinds: HashMap<DependencyKind, PerKindInfo>,
    /// The package's lib target, or `None` if it doesn't have one.
    pub lib_target: Option<LibTarget>,
    /// List of binaries provided by the package.
    pub bin_targets: Vec<BinTarget>,
    /// The build script's absolute path, or `None` if the package does not use
    /// one.
    pub build_script: Option<PathBuf>,
    /// What privilege group the crate is a part of.
    pub group: Group,
    /// Whether the source is a local path. Is `false` if cargo resolved this
    /// dependency from a registry (e.g. crates.io) or git. If `false` the
    /// package may still be locally vendored through cargo configuration (see
    /// https://doc.rust-lang.org/cargo/reference/source-replacement.html)
    pub is_local: bool,
    /// Whether this package is depended on directly by the root Cargo.toml or
    /// it is a transitive dependency.
    pub is_toplevel_dep: bool,
}

impl Package {
    pub fn crate_id(&self) -> crates::VendoredCrate {
        crates::VendoredCrate { name: self.package_name.clone(), version: self.version.clone() }
    }
}

impl Display for Package {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "`{}-{}`", &self.package_name, &self.version)
    }
}

#[derive(Debug, Eq, Hash, Ord, PartialOrd, PartialEq)]
pub struct PackageId {
    name: String,
    version: Version,
}

impl PackageId {
    pub fn new(name: String, version: Version) -> Self {
        Self { name, version }
    }

    pub fn name(&self) -> &str {
        self.name.as_str()
    }

    pub fn version(&self) -> &Version {
        &self.version
    }
}

impl From<&Package> for PackageId {
    fn from(p: &Package) -> Self {
        Self { name: p.package_name.clone(), version: p.version.clone() }
    }
}

impl<'g> From<&PackageMetadata<'g>> for PackageId {
    fn from(p: &PackageMetadata<'g>) -> Self {
        Self { name: p.name().to_string(), version: p.version().clone() }
    }
}

/// How a package is depended on.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum DependencyKind {
    /// A normal (i.e. production) dependency.
    Normal,
    /// A build-type dependency: proc macro or build.rs dep.
    Build,
}

impl From<DependencyKind> for guppy::DependencyKind {
    fn from(value: DependencyKind) -> guppy::DependencyKind {
        match value {
            DependencyKind::Build => guppy::DependencyKind::Build,
            DependencyKind::Normal => guppy::DependencyKind::Normal,
        }
    }
}

/// A dependency of a `Package`. Cross-references another `Package` entry in the
/// resolved list.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DepOfDep {
    /// This dependency's package name as used by cargo.
    pub package_name: String,
    /// The name of the lib crate as `use`d by the dependent. This may be the
    /// same or different than `package_name`.
    pub use_name: String,
    /// The resolved version of this dependency.
    pub version: Version,
    /// A condition for using this dependency.
    pub condition: Condition,
}

impl DepOfDep {
    pub fn crate_id(&self) -> crates::VendoredCrate {
        crates::VendoredCrate { name: self.package_name.clone(), version: self.version.clone() }
    }
}

/// Information specific to the dependency kind: for normal, build script, or
/// test dependencies.
#[derive(Clone, Debug)]
pub struct PerKindInfo {
    /// Condition that enables the dependency.
    pub condition: Condition,
    /// The resolved feature set for this kind.
    pub features: Vec<String>,
}

/// Description of a package's lib target.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct LibTarget {
    /// The absolute path of the lib target's `lib.rs`.
    pub root: PathBuf,
    /// The type of the lib target. This is "rlib" for normal dependencies and
    /// "proc-macro" for proc macros.
    pub lib_type: LibType,
}

/// A binary provided by a package.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BinTarget {
    /// The absolute path of the binary's root source file (e.g. `main.rs`).
    pub root: PathBuf,
    /// The binary name.
    pub name: String,
}

/// The type of lib target. Only includes types supported by this tool.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum LibType {
    /// A normal Rust rlib library.
    Rlib,
    /// A procedural macro.
    ProcMacro,
}

impl LibType {
    fn from_crate_types(crate_types: &[String]) -> Result<Self> {
        const RLIB_CRATE_TYPES: [&str; 3] = [
            // Naturally "rlib" maps to `LibType::Rlib`.
            "rlib",
            // https://doc.rust-lang.org/nightly/reference/linkage.html#r-link.lib
            // says:
            //
            //     > The purpose of this generic lib option is to generate
            //     > the “compiler recommended” style of library.
            //
            // For Chromium this means `Rlib`.
            "lib",
            // Crates are rarely cdylibs. The example encountered so far aims to expose a C API to
            // other code. In a Chromium context, we don't want to build that as a dylib for a
            // couple of reasons:
            // * rust_shared_library does not work on Mac. rustc does not know how to export the
            //   __llvm_profile_raw_version symbol.
            // * even if it did work, this might require us to distribute extra binaries
            //   (.so/.dylib etc.)
            // For the only case we've had so far, it makes more sense to build the code as a
            // static^H^H^Hrlib library which we can then link into downstream binaries.
            "cdylib",
        ];
        for rlib_crate_type in RLIB_CRATE_TYPES.iter() {
            if crate_types.iter().any(|x| *x == *rlib_crate_type) {
                return Ok(LibType::Rlib);
            }
        }

        // All the other
        // [crate types](https://doc.rust-lang.org/nightly/cargo/reference/cargo-targets.html#the-crate-type-field)
        // should be either handled earlier by `get_build_targets` or unsupported:
        // - `bin` - handled via `BuildTargetKind::Binary` and
        //   `BuildTargetId::Binary(_)`
        // - `dylib` - currently not supported by `cargo_crate.gni`
        // - `staticlib` - currently not supported by `cargo_crate.gni`.
        // - `proc-macro` - handled via `BuildTargetKind::ProcMacro`
        //
        // TODO(lukasza): Should we proactively return `Rlib` for `staticlib`?
        // Or do we want to wait until this is actually needed (to double-check
        // that this will do the right thing)?
        bail!("Unknown or unexpected crate types: {}", crate_types.join(", "));
    }
}

impl std::fmt::Display for LibType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match *self {
            Self::Rlib => f.write_str("rlib"),
            Self::ProcMacro => f.write_str("proc-macro"),
        }
    }
}

/// Process the dependency graph in `metadata` to a flat list of transitive
/// dependencies. Each element in the result corresponds to a cargo package. A
/// package may have multiple crates, each of which corresponds to a single
/// rustc invocation: e.g. a package may have a lib crate as well as multiple
/// binary crates.
///
/// `root_package_name` specifies from which package to start traversing the
/// dependency graph (likely the root package to generate build files for).
pub fn collect_dependencies(
    graph: &PackageGraph,
    root_package_name: &str,
    extra_config: &BuildConfig,
) -> Result<Vec<Package>> {
    // Ask `guppy` to run Cargo feature/dependency resolution.
    let mut memoization_tables = MemoizationTables::new();
    let cargo_set = {
        let cargo_options = CargoOptions::new();
        let initials = resolve_root_package_set(graph, root_package_name)?
            .to_feature_set(StandardFeatures::Default);
        let no_extra_features = graph.resolve_none().to_feature_set(StandardFeatures::Default);
        let resolver =
            PackageResolver { extra_config, memoization_tables: &mut memoization_tables };
        CargoSet::with_package_resolver(initials, no_extra_features, resolver, &cargo_options)?
    };
    let cargo_set_links = cargo_set
        .build_dep_links()
        .chain(cargo_set.proc_macro_links())
        .chain(cargo_set.target_links())
        .chain(cargo_set.host_links())
        .map(|link| get_link_key(&link))
        .collect::<HashSet<_>>();
    let feature_set = cargo_set.target_features().union(cargo_set.host_features());
    let package_set = feature_set.to_package_set();
    let root_id = cargo_set
        .initials()
        .to_package_set()
        .root_ids(DependencyDirection::Forward)
        .exactly_one()
        .map_err(|_| ())
        .expect("`resolve_root_package_set` should have verified `exactly_one` root");

    // Translate the packages into an internal `gnrt` representation.
    let is_toplevel_dep = |package: &PackageMetadata| -> bool {
        package.reverse_direct_links().any(|link| link.from().name() == root_package_name)
    };
    let mut get_dependency_condition =
        |link: &PackageLink, dep_kind: DependencyKind| -> Condition {
            let key = get_link_key(link);
            if !cargo_set_links.contains(&key) {
                return Condition::always_false();
            }
            let dep_kind = match dep_kind {
                DependencyKind::Normal => guppy::DependencyKind::Normal,
                DependencyKind::Build => guppy::DependencyKind::Build,
            };
            memoization_tables.get_link_condition(link, dep_kind)
        };
    let mut packages = package_set
        .packages(DependencyDirection::Forward)
        .filter(|package| package.name() != root_package_name)
        .map(|package| {
            let err_context = || format!("Error processing `{}`", package.name());
            let dependencies = get_package_dependencies(&package, |link| {
                get_dependency_condition(link, DependencyKind::Normal)
            });
            let build_dependencies = get_package_dependencies(&package, |link| {
                get_dependency_condition(link, DependencyKind::Build)
            });
            let dependency_kinds =
                get_reverse_dependency_kinds(&package, &cargo_set, &mut get_dependency_condition);

            let BuildTargets { lib_target, bin_targets, build_script } =
                get_build_targets(&package, extra_config).with_context(err_context)?;
            let group = find_inherited_privilege_group(package.id(), root_id, graph, extra_config);

            Ok(Package {
                package_name: package.name().to_string(),
                version: package.version().clone(),
                description: package.description().map(|s| s.to_string()),
                authors: package.authors().to_vec(),
                edition: package.edition().to_string(),
                repository: package.repository().map(|s| s.to_string()),
                dependencies,
                build_dependencies,
                dependency_kinds,
                lib_target,
                bin_targets,
                build_script,
                group,
                is_local: !package.source().is_external(),
                is_toplevel_dep: is_toplevel_dep(&package),
            })
        })
        .collect::<Result<Vec<_>>>()?;

    // Return a flat list of dependencies.
    packages.sort_unstable_by(|a, b| {
        a.package_name.cmp(&b.package_name).then(a.version.cmp(&b.version))
    });
    Ok(packages)
}

fn resolve_root_package_set<'g>(
    graph: &'g PackageGraph,
    root_package_name: &str,
) -> Result<PackageSet<'g>> {
    let package_set = graph.resolve_package_name(root_package_name);
    match package_set.len() {
        0 => bail!(
            "Couldn't find the root package: `{root_package_name}`. \
             No package with this name."
        ),
        2.. => bail!(
            "Couldn't find the root package: `{root_package_name}`. \
             More than one package with this name: {}",
            package_set.package_ids(DependencyDirection::Forward).join(", ")
        ),
        1 => Ok(package_set),
    }
}

/// Graph traversal resolver that rejects dependency links that would have been
/// `Condition::is_always_false` on Chromium platforms.
struct PackageResolver<'a> {
    extra_config: &'a BuildConfig,
    memoization_tables: &'a mut MemoizationTables,
}

/// Gets the key to use in `cargo_set_links` `HashSet`.
fn get_link_key(link: &PackageLink) -> (PackageId, PackageId) {
    let from = &link.from();
    let to = &link.to();
    (from.into(), to.into())
}

struct MemoizationTables {
    package_conditions: HashMap<PackageId, Condition>,
}

impl MemoizationTables {
    fn new() -> Self {
        Self { package_conditions: HashMap::new() }
    }

    fn get_package_condition_unmemoized(&mut self, package: &PackageMetadata) -> Condition {
        package
            .reverse_direct_links()
            .flat_map(|link| {
                [DependencyKind::Normal, DependencyKind::Build]
                    .into_iter()
                    .map(|kind| {
                        let condition = self.get_link_condition(&link, kind.into());
                        adjust_reverse_condition_if_crossing_target_to_host(condition, &link, kind)
                    })
                    .collect_vec()
            })
            .reduce(Condition::or)
            .unwrap_or_else(Condition::always_true)
    }

    fn get_package_condition(&mut self, package: &PackageMetadata) -> Condition {
        if let Some(condition) = self.package_conditions.get(&package.into()) {
            return condition.clone();
        }

        let condition = self.get_package_condition_unmemoized(package);
        self.package_conditions.insert(package.into(), condition.clone());
        condition
    }

    fn get_link_condition(
        &mut self,
        link: &PackageLink,
        dep_kind: guppy::DependencyKind,
    ) -> Condition {
        let req = link.req_for_kind(dep_kind);
        if !req.is_present() {
            Condition::always_false()
        } else {
            let baseline_condition = self.get_package_condition(&link.from());
            Condition::and(
                baseline_condition,
                Condition::or(
                    get_condition(req.status().required_status()),
                    get_condition(req.status().optional_status()),
                ),
            )
        }
    }
}

impl<'g> guppy::graph::PackageResolver<'g> for PackageResolver<'_> {
    fn accept(&mut self, _query: &PackageQuery<'g>, link: PackageLink<'g>) -> bool {
        // Remove dependency links rejected by `gnrt_config.toml`.
        if self.extra_config.resolve.remove_crates.contains(link.to().name()) {
            return false;
        }
        let remove_deps =
            self.extra_config.get_combined_set(link.from().name(), |cfg| &cfg.remove_deps);
        if remove_deps.contains(link.to().name()) {
            return false;
        }

        // Check if the dependency is conditional, and reject the dependency if
        // the condition is never met on Chromium platforms.
        let mut get_condition = |kind| self.memoization_tables.get_link_condition(&link, kind);
        let normal_condition = get_condition(guppy::DependencyKind::Normal);
        let build_condition = get_condition(guppy::DependencyKind::Build);
        if normal_condition.is_always_false() && build_condition.is_always_false() {
            return false;
        }

        // Otherwise accept the dependency.
        true
    }
}

/// Link conditions are evaluated from the perspective of `link.from()`.
/// Therefore if the link crosses from target to host, then the condition should
/// not follow.
fn adjust_reverse_condition_if_crossing_target_to_host(
    mut condition: Condition,
    link: &PackageLink,
    kind: DependencyKind,
) -> Condition {
    let link_is_present = link.req_for_kind(kind.into()).is_present();
    let link_crosses_from_target_to_host = match kind {
        DependencyKind::Build => true,
        DependencyKind::Normal => link.to().is_proc_macro(),
    };
    let link_applies_to_chromium = !condition.is_always_false();
    if link_applies_to_chromium && link_is_present && link_crosses_from_target_to_host {
        // Reverse condition for `link.to()` needs to support it on all host platforms.
        condition = Condition::always_true();
    }

    condition
}

fn get_reverse_dependency_kinds(
    package: &PackageMetadata,
    cargo_set: &CargoSet,
    mut condition_getter: impl for<'a> FnMut(&PackageLink<'a>, DependencyKind) -> Condition,
) -> HashMap<DependencyKind, PerKindInfo> {
    let get_features = |feature_set: &FeatureSet| -> Vec<String> {
        feature_set
            .features_for(package.id())
            .unwrap()
            .map(|feature_list| feature_list.named_features().map(|s| s.to_string()).collect_vec())
            .unwrap_or_default()
    };
    let mut result = HashMap::new();
    let mut insert_if_present = |link: PackageLink, kind: DependencyKind| {
        let condition = condition_getter(&link, kind);
        let condition = adjust_reverse_condition_if_crossing_target_to_host(condition, &link, kind);
        if !condition.is_always_false() {
            let features = match kind {
                // ... => `build.rs` deps only care about host-side features.
                DependencyKind::Build => get_features(cargo_set.host_features()),
                // Other deps care about host-side and target-side features
                // (e.g. `quote` or `syn` can be "normal" dependencies of
                // `foo_derive` crates, or "normal" dependencies of regular,
                // non-proc-macro crates).
                DependencyKind::Normal => get_features(cargo_set.host_features())
                    .into_iter()
                    .chain(get_features(cargo_set.target_features()))
                    .sorted()
                    .dedup()
                    .collect_vec(),
            };
            let info: &mut PerKindInfo = result
                .entry(kind)
                .or_insert_with(|| PerKindInfo { condition: condition.clone(), features });
            info.condition = Condition::or(info.condition.clone(), condition);
        }
    };

    for link in package.reverse_direct_links() {
        insert_if_present(link, DependencyKind::Build);
        insert_if_present(link, DependencyKind::Normal);
    }

    result
}

fn get_condition(platform_status: PlatformStatus) -> Condition {
    use PlatformStatus::*;
    match platform_status {
        Never => Condition::always_false(),
        Always => Condition::always_true(),
        PlatformDependent { eval } => eval
            .target_specs()
            .iter()
            .map(Condition::from_target_spec)
            .fold(Condition::always_false(), Condition::or),
    }
}

fn get_package_dependencies(
    package: &PackageMetadata,
    mut condition_getter: impl for<'a> FnMut(&PackageLink<'a>) -> Condition,
) -> Vec<DepOfDep> {
    package
        .direct_links()
        .map(|link| (link, condition_getter(&link)))
        .filter(|&(_link, ref condition)| !condition.is_always_false())
        .map(|(link, condition)| DepOfDep {
            package_name: link.to().name().to_string(),
            use_name: link.resolved_name().to_string(),
            version: link.to().version().clone(),
            condition,
        })
        .sorted_by(|lhs, rhs| Ord::cmp(&lhs.package_name, &rhs.package_name))
        .collect()
}

struct BuildTargets {
    lib_target: Option<LibTarget>,
    bin_targets: Vec<BinTarget>,
    build_script: Option<PathBuf>,
}
fn get_build_targets(
    package: &PackageMetadata,
    extra_config: &BuildConfig,
) -> Result<BuildTargets> {
    let mut lib_target = None;
    let mut bin_targets = vec![];
    let mut build_script = None;

    let allowed_bin_targets =
        extra_config.get_combined_set(package.name(), |crate_cfg| &crate_cfg.bin_targets);
    for target in package.build_targets() {
        let root = target.path().as_std_path().into();
        let target_type = match target.id() {
            BuildTargetId::Library => {
                let lib_type = match target.kind() {
                    BuildTargetKind::ProcMacro => LibType::ProcMacro,
                    BuildTargetKind::LibraryOrExample(crate_types) => {
                        LibType::from_crate_types(crate_types)?
                    }
                    // Matching `BuildTargetId::Library` means that `BuildTargetKind::Binary`
                    // should be impossible.
                    BuildTargetKind::Binary => unreachable!(),
                    // `BuildTargetKind` is non-exhaustive.
                    other => unimplemented!("Unrecognized `BuildTargetKind`: {other:?}"),
                };
                Some(TargetType::Lib(lib_type))
            }
            BuildTargetId::BuildScript => Some(TargetType::BuildScript),
            BuildTargetId::Binary(_) => Some(TargetType::Bin),
            BuildTargetId::Example(_)
            | BuildTargetId::Test(_)
            | BuildTargetId::Benchmark(_)
            | _ => None,
        };
        match target_type {
            None => (),
            Some(TargetType::Bin) => {
                if allowed_bin_targets.contains(target.name()) {
                    bin_targets.push(BinTarget { root, name: target.name().to_string() });
                }
            }
            Some(TargetType::BuildScript) => {
                assert_eq!(
                    build_script,
                    None,
                    "found duplicate build script `{}` in package `{}`",
                    target.name(),
                    package.name(),
                );
                build_script = Some(root);
            }
            Some(TargetType::Lib(lib_type)) => {
                assert!(
                    lib_target.is_none(),
                    "found duplicate lib target `{}` in package `{}`",
                    target.name(),
                    package.name(),
                );
                lib_target = Some(LibTarget { root, lib_type });
            }
        }
    }
    Ok(BuildTargets { lib_target, bin_targets, build_script })
}

/// A crate target type we support.
#[derive(Clone, Copy, Debug)]
enum TargetType {
    Lib(LibType),
    Bin,
    BuildScript,
}

impl std::fmt::Display for TargetType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match *self {
            Self::Lib(typ) => typ.fmt(f),
            Self::Bin => f.write_str("bin"),
            Self::BuildScript => f.write_str("custom-build"),
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::config::CrateConfig;

    use super::*;

    #[test]
    fn collect_dependencies_on_sample_output() {
        use crate::config::CrateConfig;
        let foo_config = CrateConfig { group: Some(Group::Test), ..CrateConfig::default() };
        let build_config = BuildConfig {
            per_crate_config: [("foo".to_string(), foo_config)].into_iter().collect(),
            ..BuildConfig::default()
        };

        let metadata = PackageGraph::from_json(SAMPLE_CARGO_METADATA).unwrap();
        let dependencies =
            collect_dependencies(&metadata, "sample_package", &build_config).unwrap();

        let empty_str_slice: &'static [&'static str] = &[];

        let mut i = 0;

        assert_eq!(dependencies[i].package_name, "autocfg");
        assert_eq!(dependencies[i].version, Version::new(1, 1, 0));
        assert!(!dependencies[i].is_local);
        assert!(!dependencies[i].is_toplevel_dep);
        assert_eq!(dependencies[i].group, Group::Safe);
        assert!(dependencies[i].bin_targets.is_empty());
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Build).unwrap().features,
            empty_str_slice
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "bar");
        assert_eq!(dependencies[i].version, Version::new(0, 1, 0));
        assert!(dependencies[i].is_local);
        assert!(dependencies[i].is_toplevel_dep);
        assert_eq!(dependencies[i].group, Group::Safe);
        assert!(dependencies[i].bin_targets.is_empty());
        assert!(dependencies[i].lib_target.as_ref().is_some_and(|lib_target| {
            assert!(lib_target.root.ends_with("tools/crates/gnrt/sample_package/bar/src/lib.rs"));
            assert_eq!(lib_target.lib_type, LibType::Rlib);
            true
        }));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            empty_str_slice
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "foo");
        assert_eq!(dependencies[i].version, Version::new(0, 1, 0));
        assert!(dependencies[i].is_toplevel_dep);
        assert!(dependencies[i].is_local);
        assert_eq!(dependencies[i].group, Group::Test);
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            empty_str_slice
        );
        assert_eq!(dependencies[i].dependencies.len(), 2);
        assert_eq!(
            dependencies[i].dependencies[0],
            DepOfDep {
                package_name: "bar".to_string(),
                use_name: "baz".to_string(),
                version: Version::new(0, 1, 0),
                condition: Condition::always_true(),
            }
        );
        assert_eq!(
            dependencies[i].dependencies[1],
            DepOfDep {
                package_name: "time".to_string(),
                use_name: "time".to_string(),
                version: Version::new(0, 3, 14),
                condition: Condition::always_true(),
            }
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "libc");
        assert_eq!(dependencies[i].version, Version::new(0, 2, 133));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["default", "std"],
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "num-traits");
        assert_eq!(dependencies[i].version, Version::new(0, 2, 15));
        assert!(dependencies[i].is_toplevel_dep);
        assert_eq!(dependencies[i].group, Group::Safe);
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["default", "std"]
        );
        assert_eq!(dependencies[i].build_dependencies.len(), 1);
        assert_eq!(
            dependencies[i].build_dependencies[0],
            DepOfDep {
                package_name: "autocfg".to_string(),
                use_name: "autocfg".to_string(),
                version: Version::new(1, 1, 0),
                condition: Condition::always_true(),
            }
        );
        assert!(dependencies[i].build_script.as_ref().is_some_and(|path| {
            assert!(path.ends_with("num-traits-0.2.15/build.rs"));
            true
        }));

        i += 1;

        assert_eq!(dependencies[i].package_name, "num_threads");
        assert_eq!(dependencies[i].version, Version::new(0, 1, 6));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            empty_str_slice
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "once_cell");
        assert_eq!(dependencies[i].version, Version::new(1, 13, 0));
        assert!(dependencies[i].is_toplevel_dep);
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["alloc", "default", "race", "std"]
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "proc-macro2");
        assert_eq!(dependencies[i].version, Version::new(1, 0, 40));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["default", "proc-macro"]
        );
        assert!(dependencies[i].build_script.as_ref().is_some_and(|path| {
            assert!(path.ends_with("proc-macro2-1.0.40/build.rs"));
            true
        }));

        i += 1;

        assert_eq!(dependencies[i].package_name, "quote");
        assert_eq!(dependencies[i].version, Version::new(1, 0, 20));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["default", "proc-macro"]
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "serde");
        assert_eq!(dependencies[i].version, Version::new(1, 0, 139));
        assert!(dependencies[i].is_toplevel_dep);
        assert_eq!(dependencies[i].group, Group::Safe);
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["default", "derive", "serde_derive", "std"]
        );
        assert_eq!(dependencies[i].dependencies.len(), 1);
        assert_eq!(dependencies[i].build_dependencies.len(), 0);
        assert_eq!(
            dependencies[i].dependencies[0],
            DepOfDep {
                package_name: "serde_derive".to_string(),
                use_name: "serde_derive".to_string(),
                version: Version::new(1, 0, 139),
                condition: Condition::always_true(),
            }
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "serde_derive");
        assert_eq!(dependencies[i].version, Version::new(1, 0, 139));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["default"],
        );
        assert!(!dependencies[i].is_toplevel_dep);
        assert_eq!(dependencies[i].group, Group::Safe);
        assert_eq!(dependencies[i].dependencies.len(), 3);
        assert_eq!(dependencies[i].build_dependencies.len(), 0);
        assert_eq!(
            dependencies[i].dependencies[0],
            DepOfDep {
                package_name: "proc-macro2".to_string(),
                use_name: "proc_macro2".to_string(),
                version: Version::new(1, 0, 40),
                condition: Condition::always_true(),
            }
        );
        assert_eq!(
            dependencies[i].dependencies[1],
            DepOfDep {
                package_name: "quote".to_string(),
                use_name: "quote".to_string(),
                version: Version::new(1, 0, 20),
                condition: Condition::always_true(),
            }
        );
        assert_eq!(
            dependencies[i].dependencies[2],
            DepOfDep {
                package_name: "syn".to_string(),
                use_name: "syn".to_string(),
                version: Version::new(1, 0, 98),
                condition: Condition::always_true(),
            }
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "syn");
        assert_eq!(dependencies[i].version, Version::new(1, 0, 98));
        assert!(!dependencies[i].is_toplevel_dep);
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["clone-impls", "default", "derive", "parsing", "printing", "proc-macro", "quote"]
        );
        assert_eq!(dependencies[i].dependencies.len(), 3);
        assert_eq!(dependencies[i].build_dependencies.len(), 0);
        assert_eq!(
            dependencies[i].dependencies[0],
            DepOfDep {
                package_name: "proc-macro2".to_string(),
                use_name: "proc_macro2".to_string(),
                version: Version::new(1, 0, 40),
                condition: Condition::always_true(),
            }
        );
        assert_eq!(
            dependencies[i].dependencies[1],
            DepOfDep {
                package_name: "quote".to_string(),
                use_name: "quote".to_string(),
                version: Version::new(1, 0, 20),
                condition: Condition::always_true(),
            }
        );
        assert_eq!(
            dependencies[i].dependencies[2],
            DepOfDep {
                package_name: "unicode-ident".to_string(),
                use_name: "unicode_ident".to_string(),
                version: Version::new(1, 0, 1),
                condition: Condition::always_true(),
            }
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "termcolor");
        assert_eq!(dependencies[i].version, Version::new(1, 1, 3));
        assert!(dependencies[i].is_toplevel_dep);
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            empty_str_slice
        );
        assert_eq!(dependencies[i].dependencies.len(), 1);
        assert_eq!(dependencies[i].build_dependencies.len(), 0);
        assert_eq!(dependencies[i].dependencies[0].package_name, "winapi-util");
        assert_eq!(dependencies[i].dependencies[0].use_name, "winapi_util");
        assert_eq!(dependencies[i].dependencies[0].version, Version::new(0, 1, 5));
        assert_eq!(
            dependencies[i].dependencies[0].condition.to_handlebars_value().unwrap(),
            Some("is_win".to_string()),
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "time");
        assert_eq!(dependencies[i].version, Version::new(0, 3, 14));
        // `time` is a dependency of `foo`, so should also get classified as `Test`:
        assert_eq!(dependencies[i].group, Group::Test);
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["alloc", "default", "std"]
        );
        assert_eq!(dependencies[i].dependencies.len(), 2);
        assert_eq!(dependencies[i].dependencies[0].package_name, "libc");
        assert_eq!(dependencies[i].dependencies[0].version, Version::new(0, 2, 133));
        assert_eq!(
            dependencies[i].dependencies[0].condition.to_handlebars_value().unwrap(),
            Some("!is_win".to_string()),
        );
        assert_eq!(dependencies[i].dependencies[1].package_name, "num_threads");
        assert_eq!(dependencies[i].dependencies[1].version, Version::new(0, 1, 6));
        assert_eq!(
            dependencies[i].dependencies[1].condition.to_handlebars_value().unwrap(),
            Some("!is_win".to_string()),
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "unicode-ident");
        assert_eq!(dependencies[i].version, Version::new(1, 0, 1));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            empty_str_slice
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "winapi");
        assert_eq!(dependencies[i].version, Version::new(0, 3, 9));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &[
                "consoleapi",
                "errhandlingapi",
                "fileapi",
                "minwindef",
                "processenv",
                "std",
                "winbase",
                "wincon",
                "winerror",
                "winnt"
            ]
        );
        assert_eq!(dependencies[i].dependencies.len(), 0);
        assert_eq!(dependencies[i].build_dependencies.len(), 0);

        i += 1;

        assert_eq!(dependencies[i].package_name, "winapi-util");
        assert_eq!(dependencies[i].version, Version::new(0, 1, 5));
        assert!(dependencies[i].dependency_kinds.get(&DependencyKind::Normal).is_some_and(|d| {
            assert_eq!(d.features, empty_str_slice);
            assert_eq!(d.condition.to_handlebars_value().unwrap(), Some("is_win".to_string()),);
            true
        }));
        assert_eq!(dependencies[i].dependencies.len(), 1);
        assert_eq!(dependencies[i].build_dependencies.len(), 0);
        assert_eq!(dependencies[i].dependencies[0].package_name, "winapi");
        assert_eq!(dependencies[i].dependencies[0].version, Version::new(0, 3, 9));
        assert_eq!(
            dependencies[i].dependencies[0].condition.to_handlebars_value().unwrap(),
            Some("is_win".to_string()),
        );

        i += 1;
        assert_eq!(dependencies.len(), i);
    }

    #[test]
    fn dependencies_for_workspace_member() {
        let config = BuildConfig::default();
        let metadata = PackageGraph::from_json(SAMPLE_CARGO_METADATA).unwrap();

        // Start from "foo" workspace member.
        let dependencies = collect_dependencies(&metadata, "foo", &config).unwrap();

        let mut i = 0;

        assert_eq!(dependencies[i].package_name, "bar");
        assert_eq!(dependencies[i].version, Version::new(0, 1, 0));

        i += 1;

        assert_eq!(dependencies[i].package_name, "libc");
        assert_eq!(dependencies[i].version, Version::new(0, 2, 133));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["default", "std"]
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "num_threads");
        assert_eq!(dependencies[i].version, Version::new(0, 1, 6));

        i += 1;

        assert_eq!(dependencies[i].package_name, "time");
        assert_eq!(dependencies[i].version, Version::new(0, 3, 14));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["alloc", "default", "std"]
        );

        i += 1;
        assert_eq!(dependencies.len(), i);
    }

    #[test]
    fn dependencies_removed_by_config_file() {
        // Use a config with `remove_crates` and `remove_deps` entries.
        let mut config = BuildConfig::default();
        config.resolve.remove_crates.insert("num_threads".to_string());
        config.per_crate_config.insert(
            "time".to_string(),
            CrateConfig { remove_deps: vec!["num_threads".to_string()], ..CrateConfig::default() },
        );

        // Collect dependencies.
        let metadata = PackageGraph::from_json(SAMPLE_CARGO_METADATA).unwrap();
        let dependencies = collect_dependencies(&metadata, "sample_package", &config).unwrap();

        // Verify that `num_threads` got removed.
        for package in dependencies.iter() {
            assert_ne!(package.package_name, "num_threads");
            assert!(!package
                .build_dependencies
                .iter()
                .any(|dep| dep.package_name == "num_threads"));
            assert!(!package.dependencies.iter().any(|dep| dep.package_name == "num_threads"));
        }
    }

    // test_metadata.json contains the output of "cargo metadata" run in
    // sample_package. The dependency graph is relatively simple but includes
    // transitive deps and a workspace member.
    static SAMPLE_CARGO_METADATA: &str = include_str!("test_metadata.json");

    /// This test spot-checks that the trimming down of the dependency graph
    /// from https://crrev.com/c/6259145 didn't regress.
    #[test]
    fn collect_dependencies_on_sample_output2() {
        let config = BuildConfig::default();
        let metadata = PackageGraph::from_json(SAMPLE_CARGO_METADATA2).unwrap();
        let dependencies = collect_dependencies(&metadata, "sample_package2", &config).unwrap();
        let dependencies = dependencies
            .into_iter()
            .map(|package| (package.package_name.to_string(), package))
            .collect::<HashMap<_, _>>();

        let icu_capi = &dependencies["icu_capi"];
        assert_eq!(
            icu_capi.dependency_kinds[&DependencyKind::Normal].features,
            &["calendar", "compiled_data", "experimental"],
        );
        assert_eq!(
            icu_capi.dependencies.iter().map(|d| d.package_name.clone()).sorted().collect_vec(),
            &[
                "diplomat",
                "diplomat-runtime",
                "icu_calendar",
                "icu_experimental",
                "icu_locale_core",
                "icu_provider",
                "icu_provider_adapters",
                "icu_time",
                "potential_utf",
                "tinystr",
                "writeable",
                "zerovec",
            ],
        );
        assert!(icu_capi.build_dependencies.is_empty());

        let icu_properties = &dependencies["icu_properties"];
        assert_eq!(
            icu_properties.dependency_kinds[&DependencyKind::Normal].features,
            &["alloc", "compiled_data"],
        );
        assert_eq!(
            icu_properties
                .dependencies
                .iter()
                .map(|d| d.package_name.clone())
                .sorted()
                .collect_vec(),
            &[
                "displaydoc",
                "icu_collections",
                "icu_locale_core",
                "icu_properties_data",
                "icu_provider",
                "potential_utf",
                "zerotrie",
                "zerovec",
            ],
        );
        assert!(icu_properties.build_dependencies.is_empty());

        let smallvec = &dependencies["smallvec"];
        assert_eq!(
            smallvec.dependency_kinds[&DependencyKind::Normal].features,
            &["const_generics"],
        );
        assert!(smallvec.dependencies.is_empty());
        assert!(smallvec.build_dependencies.is_empty());
    }

    // `test_metadata2.json` contains the output of `cargo metadata` run in
    // `gnrt/sample_package2` directory.  See the `Cargo.toml` for more
    // information.
    static SAMPLE_CARGO_METADATA2: &str = include_str!("test_metadata2.json");

    #[test]
    fn collect_dependencies_on_sample_output3() {
        let config = BuildConfig::default();
        let metadata = PackageGraph::from_json(SAMPLE_CARGO_METADATA3).unwrap();
        let dependencies = collect_dependencies(&metadata, "sample_package3", &config).unwrap();
        let dependencies = dependencies
            .into_iter()
            .map(|package| (package.package_name.to_string(), package))
            .collect::<HashMap<_, _>>();
        assert!(!dependencies.contains_key("windows_aarch64_gnullvm"));
        assert!(dependencies.contains_key("windows_aarch64_msvc"));
        assert!(!dependencies.contains_key("windows_i686_gnu"));
        assert!(!dependencies.contains_key("windows_i686_gnullvm"));
        assert!(dependencies.contains_key("windows_i686_msvc"));
        assert!(!dependencies.contains_key("windows_x86_64_gnu"));
        assert!(!dependencies.contains_key("windows_x86_64_gnullvm"));
        assert!(dependencies.contains_key("windows_x86_64_msvc"));
    }

    // `test_metadata3.json` contains the output of `cargo metadata` run in
    // `gnrt/sample_package3` directory.  See the `Cargo.toml` for more
    // information.
    static SAMPLE_CARGO_METADATA3: &str = include_str!("test_metadata3.json");

    #[test]
    fn collect_dependencies_on_sample_output4() {
        let config = BuildConfig::default();
        let metadata = PackageGraph::from_json(SAMPLE_CARGO_METADATA4).unwrap();
        let dependencies = collect_dependencies(&metadata, "sample_package4", &config).unwrap();
        let dependencies = dependencies
            .into_iter()
            .map(|package| (package.package_name.to_string(), package))
            .collect::<HashMap<_, _>>();

        // When compiling for arm64 **target**, there is a foo => prost-derive
        // dependency.
        let foo = &dependencies["foo"];
        assert_eq!(foo.dependencies.len(), 1);
        assert_eq!(foo.dependencies[0].package_name, "prost-derive");
        assert_eq!(
            foo.dependencies[0].condition.to_handlebars_value().unwrap(),
            Some("current_cpu == \"arm64\"".to_string()),
        );

        // We can cross-compile for arm64 **target** when building on x86 **host**.
        // Therefore the arm64 condition should *not* propagate 1) into when
        // `prost` is enabled via its reverse dependencies, nor 2) into transitive
        // dependnecies of `prost`.
        let prost = &dependencies["prost-derive"];
        assert_eq!(prost.dependencies.len(), 5);
        assert_eq!(prost.dependencies[0].package_name, "anyhow");
        assert!(prost.dependencies[0].condition.is_always_true());
        assert!(prost.dependency_kinds[&DependencyKind::Normal].condition.is_always_true());
    }

    // `test_metadata4.json` contains the output of `cargo metadata` run in
    // `gnrt/sample_package4` directory.  See the `Cargo.toml` for more
    // information.
    static SAMPLE_CARGO_METADATA4: &str = include_str!("test_metadata4.json");
}
