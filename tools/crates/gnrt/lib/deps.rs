// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Utilities to process `cargo metadata` dependency graph.

use crate::config::BuildConfig;
use crate::crates;
use crate::gn::{target_platform_to_condition, Condition};
use crate::group::Group;
use crate::inherit::find_inherited_privilege_group;

use std::collections::{hash_map::Entry, HashMap, HashSet};
use std::path::PathBuf;

use anyhow::{anyhow, Result};
pub use cargo_metadata::DependencyKind;
use itertools::Itertools;
pub use semver::Version;

/// Uniquely identifies a `Package` in a particular set of dependencies. The
/// representation is an implementation detail and may not be unique between
/// different sets of metadata.
pub use cargo_metadata::PackageId;

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
    /// A Rust dynamic library. See
    /// https://doc.rust-lang.org/reference/linkage.html for details and the
    /// distinction between dylib and cdylib.
    Dylib,
    /// A C-compatible dynamic library. See
    /// https://doc.rust-lang.org/reference/linkage.html for details and the
    /// distinction between dylib and cdylib.
    Cdylib,
    /// A procedural macro.
    ProcMacro,
}

impl std::fmt::Display for LibType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match *self {
            Self::Rlib => f.write_str("rlib"),
            Self::Dylib => f.write_str("dylib"),
            Self::Cdylib => f.write_str("cdylib"),
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
/// `roots` optionally specifies from which packages to traverse the dependency
/// graph (likely the root packages to generate build files for). This overrides
/// the usual behavior, which traverses from all workspace members and the root
/// workspace package. The package names in `roots` should still only contain
/// workspace members.
pub fn collect_dependencies(
    metadata: &cargo_metadata::Metadata,
    root_package_name: &str,
    extra_config: &BuildConfig,
) -> Result<Vec<Package>> {
    // The metadata is split into two parts:
    // 1. A list of packages and associated info: targets (e.g. lib, bin, tests),
    //    source path, etc. This includes all workspace members and all transitive
    //    dependencies. Deps are not filtered based on platform or features: it is
    //    the maximal set of dependencies.
    // 2. Resolved dependency graph. There is a node for each package pointing to
    //    its dependencies in each configuration (normal, build, dev), and the
    //    resolved feature set. This includes platform-specific info so one can
    //    filter based on target platform. Nodes include an ID that uniquely refers
    //    to a package in both (1) and (2).
    //
    // We need info from both parts. Traversing the graph tells us exactly which
    // crates are needed for a given configuration and platform. In the process,
    // we must collect package IDs then look up other data in (1).
    //
    // Note the difference between "packages" and "crates" as described in
    // https://doc.rust-lang.org/book/ch07-01-packages-and-crates.html

    // `metadata`'s structures are flattened into lists. Make it easy to index
    // by package ID.
    let dep_graph: MetadataGraph = build_graph(metadata);

    // `cargo metadata`'s resolved dependency graph.
    let resolved_graph: &cargo_metadata::Resolve = metadata.resolve.as_ref().unwrap();

    // The ID of the fake root package. Do not include it in the dependency list
    // since it is not actually built.
    let fake_root: &cargo_metadata::PackageId = resolved_graph.root.as_ref().unwrap();

    // `explore_node`, our recursive depth-first traversal function, needs to
    // share state between stack frames. Construct the shared state.
    let mut traversal_state = TraversalState {
        dep_graph: &dep_graph,
        root: fake_root,
        visited: HashSet::new(),
        dependencies: HashMap::new(),
    };

    // Do a depth-first traversal of the graph to find all relevant
    // dependencies. Start from the root package given as a parameter.
    let traversal_root_id = metadata
        .packages
        .iter()
        .filter(|pkg| pkg.name == root_package_name)
        .map(|pkg| pkg.id.clone())
        .exactly_one()
        .map_err(|exactly_one_error| {
            let found_ids = exactly_one_error.collect_vec();
            if found_ids.is_empty() {
                anyhow!(
                    "Couldn't find the root package: {root_package_name}. \
                     No package with this name."
                )
            } else {
                anyhow!(
                    "Couldn't find the root package: {root_package_name}. \
                     More than one package with this name: {}",
                    found_ids.into_iter().join(", ")
                )
            }
        })?;
    let node = dep_graph.nodes.get(&traversal_root_id).unwrap();
    explore_node(&mut traversal_state, node);

    // TODO(danakj): Throw an error if any `safe` crate depends on a `sandbox`
    // crate.

    // `traversal_state.dependencies` is the output of `explore_node`. Pull it
    // out for processing.
    let mut dependencies = traversal_state.dependencies;

    // Fill in the per-package data for each dependency.
    for (id, dep) in dependencies.iter_mut() {
        let node: &cargo_metadata::Node = traversal_state.dep_graph.nodes.get(id).unwrap();
        let package: &cargo_metadata::Package = traversal_state.dep_graph.packages.get(id).unwrap();

        dep.package_name = package.name.clone();
        dep.description = package.description.clone();
        dep.authors = package.authors.clone();
        dep.edition = package.edition.to_string();
        // TODO(danakj): It would be nice to store the `manifest_dir` here and
        // change all gnrt_config.toml relative paths to be relative to the
        // manifest instead of relative to the crate root, to eliminate the
        // chance for there being a different relative path from a lib root vs a
        // bin root. It can be grabbed like:
        //
        // dep.manifest_dir = package
        //     .manifest_path
        //     .parent()
        //     .expect("manifest_path has no directory?")
        //     .to_path_buf()
        //     .into_std_path_buf();

        // TODO(crbug.com/40212956): Resolve features independently per kind
        // and platform. This may require using the unstable unit-graph feature:
        // https://doc.rust-lang.org/cargo/reference/unstable.html#unit-graph
        for (_, kind_info) in dep.dependency_kinds.iter_mut() {
            kind_info.features = node.features.clone();
            // Remove "default" feature to match behavior of crates.py. Note
            // that this is technically not correct since a crate's code may
            // choose to check "default" directly, but virtually none actually
            // do this.
            //
            // TODO(crbug.com/40212956): Revisit this behavior and maybe keep
            // "default" features.
            if let Some(pos) = kind_info.features.iter().position(|x| x == "default") {
                kind_info.features.remove(pos);
            }
        }

        let allowed_bin_targets: HashSet<&str> =
            extra_config.get_combined_set(&package.name, |crate_cfg| &crate_cfg.bin_targets);
        for target in package.targets.iter() {
            let src_root = target.src_path.clone().into_std_path_buf();
            let target_type = match target.kind.iter().find_map(|s| TargetType::from_name(s)) {
                Some(target_type) => target_type,
                // Skip other targets, such as test, example, etc.
                None => continue,
            };

            match target_type {
                TargetType::Lib(lib_type) => {
                    // There can only be one lib target.
                    assert!(
                        dep.lib_target.is_none(),
                        "found duplicate lib target:\n{:?}\n{:?}",
                        dep.lib_target,
                        target
                    );
                    dep.lib_target = Some(LibTarget { root: src_root, lib_type });
                }
                TargetType::Bin => {
                    if allowed_bin_targets.contains(target.name.as_str()) {
                        dep.bin_targets
                            .push(BinTarget { root: src_root, name: target.name.clone() });
                    }
                }
                TargetType::BuildScript => {
                    assert_eq!(
                        dep.build_script, None,
                        "found duplicate build script target {target:?}"
                    );
                    dep.build_script = Some(src_root);
                }
            }
        }

        dep.version = package.version.clone();

        // Collect this package's list of resolved dependencies which will be
        // needed for build file generation later.
        for node_dep in iter_node_deps(node) {
            let dep_pkg = dep_graph.packages.get(node_dep.pkg).unwrap();
            let dep_of_dep = DepOfDep {
                package_name: dep_pkg.name.clone(),
                use_name: node_dep.lib_name.to_string(),
                version: dep_pkg.version.clone(),
                condition: node_dep.condition.clone(),
            };

            match node_dep.kind {
                DependencyKind::Normal => dep.dependencies.push(dep_of_dep),
                DependencyKind::Build => dep.build_dependencies.push(dep_of_dep),
                DependencyKind::Development | DependencyKind::Unknown => unreachable!(),
            }
        }

        dep.group = find_inherited_privilege_group(
            id,
            &dep_graph.nodes.get(fake_root).unwrap().id,
            &dep_graph.packages,
            &dep_graph.nodes,
            extra_config,
        );

        // Make sure the package comes from our vendored source. If not, report
        // the error for later.
        dep.is_local = package.source.is_none();

        // Determine whether it's a direct or transitive dependency.
        dep.is_toplevel_dep = {
            let fake_root_node = dep_graph.nodes.get(fake_root).unwrap();
            fake_root_node.dependencies.contains(id)
        };
    }

    // Filter out dependencies removed by config file.
    let mut dependencies = dependencies.into_values().collect::<Vec<_>>();
    for dep in dependencies.iter_mut() {
        let combined: HashSet<&str> =
            extra_config.get_combined_set(&dep.package_name, |crate_cfg| &crate_cfg.remove_deps);
        if combined.is_empty() {
            continue;
        }

        for kind in [&mut dep.dependencies, &mut dep.build_dependencies] {
            kind.retain(|dep_of_dep| !combined.contains(dep_of_dep.package_name.as_str()));
        }
    }
    dependencies
        .retain(|dep| !extra_config.resolve.remove_crates.iter().any(|r| **r == dep.package_name));

    // Return a flat list of dependencies.
    dependencies.sort_unstable_by(|a, b| {
        a.package_name.cmp(&b.package_name).then(a.version.cmp(&b.version))
    });
    Ok(dependencies)
}

/// Graph traversal state shared by recursive calls of `explore_node`.
struct TraversalState<'a> {
    /// The graph from "cargo metadata", processed for indexing by package id.
    dep_graph: &'a MetadataGraph<'a>,
    /// The fake root package that we exclude from `dependencies`.
    root: &'a cargo_metadata::PackageId,
    /// Set of packages already visited by `explore_node`.
    visited: HashSet<&'a cargo_metadata::PackageId>,
    /// The final set of dependencies.
    dependencies: HashMap<&'a cargo_metadata::PackageId, Package>,
}

/// Recursively explore a particular node in the dependency graph. Fills data in
/// `state`. The final output is in `state.dependencies`.
fn explore_node<'a>(state: &mut TraversalState<'a>, node: &'a cargo_metadata::Node) {
    // Mark the node as visited, or continue if it's already visited.
    if !state.visited.insert(&node.id) {
        return;
    }

    // Helper to insert a placeholder `Dependency` into a map. We fill in the
    // fields later.
    let init_dep = || Package {
        package_name: String::new(),
        version: Version::new(0, 0, 0),
        description: None,
        authors: Vec::new(),
        edition: String::new(),
        dependencies: Vec::new(),
        build_dependencies: Vec::new(),
        dependency_kinds: HashMap::new(),
        lib_target: None,
        bin_targets: Vec::new(),
        build_script: None,
        group: Group::Safe,
        is_local: false,
        is_toplevel_dep: false,
    };

    // Each node contains a list of enabled features plus a list of
    // dependencies. Each dependency has a platform filter if applicable.
    for dep_edge in iter_node_deps(node) {
        // Explore the target of this edge next. Note that we may visit the same
        // node multiple times, but this is OK since we'll skip it in the
        // recursive call.
        let target_node: &cargo_metadata::Node = state.dep_graph.nodes.get(&dep_edge.pkg).unwrap();

        explore_node(state, target_node);

        // Merge this with the existing entry for the dep.
        let dep: &mut Package = state.dependencies.entry(dep_edge.pkg).or_insert_with(&init_dep);
        let info: &mut PerKindInfo = dep
            .dependency_kinds
            .entry(dep_edge.kind)
            .or_insert(PerKindInfo { condition: Condition::AlwaysFalse, features: Vec::new() });
        info.condition = Condition::or(info.condition.clone(), dep_edge.condition);
    }

    // Initialize the dependency entry for this node's package if it's not our
    // fake root.
    if &node.id != state.root {
        state.dependencies.entry(&node.id).or_insert_with(&init_dep);
    }
}

struct DependencyEdge<'a> {
    pkg: &'a cargo_metadata::PackageId,
    lib_name: &'a str,
    kind: DependencyKind,
    condition: Condition,
}

/// Iterates over the dependencies of `node`, filtering out platforms we don't
/// support.
fn iter_node_deps(node: &cargo_metadata::Node) -> impl Iterator<Item = DependencyEdge<'_>> + '_ {
    node.deps.iter().flat_map(|node_dep| {
        // Each NodeDep has information about the package depended on, as
        // well as the kinds of dependence: as a normal, build script, or
        // test dependency. For each kind there is an optional platform
        // filter.
        //
        // Filter out kinds for unsupported platforms while mapping the
        // dependency edges to our own type.
        //
        // Cargo may also have duplicates in the dep_kinds list, which may
        // or may not be a Cargo bug, but we want to filter them out too.
        // See crbug.com/1393600.
        let mut seen = HashSet::new();
        node_dep.dep_kinds.iter().filter_map(move |dep_kind_info| {
            let condition = match dep_kind_info.target.as_ref() {
                None => Condition::AlwaysTrue,
                Some(platform) => target_platform_to_condition(platform),
            };

            // Filter if it's for a platform we don't support.
            if condition == Condition::AlwaysFalse {
                return None;
            };

            // At this point we don't support building dev dependencies
            // (e.g. native Rust unit tests) for packages from crates.io.
            if dep_kind_info.kind == DependencyKind::Development {
                return None;
            }

            if seen.contains(&(&dep_kind_info.kind, &dep_kind_info.target)) {
                return None;
            }
            seen.insert((&dep_kind_info.kind, &dep_kind_info.target));

            Some(DependencyEdge {
                pkg: &node_dep.pkg,
                lib_name: &node_dep.name,
                kind: dep_kind_info.kind,
                condition,
            })
        })
    })
}

/// Indexable representation of the `cargo_metadata::Metadata` fields we need.
struct MetadataGraph<'a> {
    nodes: HashMap<&'a cargo_metadata::PackageId, &'a cargo_metadata::Node>,
    packages: HashMap<&'a cargo_metadata::PackageId, &'a cargo_metadata::Package>,
}

/// Convert the flat lists in `metadata` to maps indexable by PackageId.
fn build_graph(metadata: &cargo_metadata::Metadata) -> MetadataGraph<'_> {
    // `metadata` always has `resolve` unless cargo was explicitly asked not to
    // output the dependency graph.
    let resolve = metadata.resolve.as_ref().unwrap();
    let mut graph = HashMap::new();
    for node in resolve.nodes.iter() {
        match graph.entry(&node.id) {
            Entry::Vacant(e) => e.insert(node),
            Entry::Occupied(_) => panic!("duplicate entries in dependency graph"),
        };
    }

    let packages = metadata.packages.iter().map(|p| (&p.id, p)).collect();

    MetadataGraph { nodes: graph, packages }
}

/// A crate target type we support.
#[derive(Clone, Copy, Debug)]
enum TargetType {
    Lib(LibType),
    Bin,
    BuildScript,
}

impl TargetType {
    fn from_name(name: &str) -> Option<Self> {
        match name {
            "lib" | "rlib" => Some(Self::Lib(LibType::Rlib)),
            "dylib" => Some(Self::Lib(LibType::Dylib)),
            "cdylib" => Some(Self::Lib(LibType::Cdylib)),
            "bin" => Some(Self::Bin),
            "custom-build" => Some(Self::BuildScript),
            "proc-macro" => Some(Self::Lib(LibType::ProcMacro)),
            _ => None,
        }
    }
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

        let metadata: cargo_metadata::Metadata =
            serde_json::from_str(SAMPLE_CARGO_METADATA).unwrap();
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

        assert_eq!(dependencies[i].package_name, "cc");
        assert_eq!(dependencies[i].version, Version::new(1, 0, 73));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Build).unwrap().features,
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
                condition: Condition::AlwaysTrue,
            }
        );
        assert_eq!(
            dependencies[i].dependencies[1],
            DepOfDep {
                package_name: "time".to_string(),
                use_name: "time".to_string(),
                version: Version::new(0, 3, 14),
                condition: Condition::AlwaysTrue,
            }
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "libc");
        assert_eq!(dependencies[i].version, Version::new(0, 2, 133));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["std"],
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "num-traits");
        assert_eq!(dependencies[i].version, Version::new(0, 2, 15));
        assert!(dependencies[i].is_toplevel_dep);
        assert_eq!(dependencies[i].group, Group::Safe);
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["std"]
        );
        assert_eq!(dependencies[i].build_dependencies.len(), 1);
        assert_eq!(
            dependencies[i].build_dependencies[0],
            DepOfDep {
                package_name: "autocfg".to_string(),
                use_name: "autocfg".to_string(),
                version: Version::new(1, 1, 0),
                condition: Condition::AlwaysTrue,
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
            &["alloc", "race", "std"]
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "proc-macro2");
        assert_eq!(dependencies[i].version, Version::new(1, 0, 40));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["proc-macro"]
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
            &["proc-macro"]
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "serde");
        assert_eq!(dependencies[i].version, Version::new(1, 0, 139));
        assert!(dependencies[i].is_toplevel_dep);
        assert_eq!(dependencies[i].group, Group::Safe);
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["derive", "serde_derive", "std"]
        );
        assert_eq!(dependencies[i].dependencies.len(), 1);
        assert_eq!(dependencies[i].build_dependencies.len(), 0);
        assert_eq!(
            dependencies[i].dependencies[0],
            DepOfDep {
                package_name: "serde_derive".to_string(),
                use_name: "serde_derive".to_string(),
                version: Version::new(1, 0, 139),
                condition: Condition::AlwaysTrue,
            }
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "serde_derive");
        assert_eq!(dependencies[i].version, Version::new(1, 0, 139));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            empty_str_slice
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
                condition: Condition::AlwaysTrue,
            }
        );
        assert_eq!(
            dependencies[i].dependencies[1],
            DepOfDep {
                package_name: "quote".to_string(),
                use_name: "quote".to_string(),
                version: Version::new(1, 0, 20),
                condition: Condition::AlwaysTrue,
            }
        );
        assert_eq!(
            dependencies[i].dependencies[2],
            DepOfDep {
                package_name: "syn".to_string(),
                use_name: "syn".to_string(),
                version: Version::new(1, 0, 98),
                condition: Condition::AlwaysTrue,
            }
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "syn");
        assert_eq!(dependencies[i].version, Version::new(1, 0, 98));
        assert!(!dependencies[i].is_toplevel_dep);
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["clone-impls", "derive", "parsing", "printing", "proc-macro", "quote"]
        );
        assert_eq!(dependencies[i].dependencies.len(), 3);
        assert_eq!(dependencies[i].build_dependencies.len(), 0);
        assert_eq!(
            dependencies[i].dependencies[0],
            DepOfDep {
                package_name: "proc-macro2".to_string(),
                use_name: "proc_macro2".to_string(),
                version: Version::new(1, 0, 40),
                condition: Condition::AlwaysTrue,
            }
        );
        assert_eq!(
            dependencies[i].dependencies[1],
            DepOfDep {
                package_name: "quote".to_string(),
                use_name: "quote".to_string(),
                version: Version::new(1, 0, 20),
                condition: Condition::AlwaysTrue,
            }
        );
        assert_eq!(
            dependencies[i].dependencies[2],
            DepOfDep {
                package_name: "unicode-ident".to_string(),
                use_name: "unicode_ident".to_string(),
                version: Version::new(1, 0, 1),
                condition: Condition::AlwaysTrue,
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
        assert_eq!(
            dependencies[i].dependencies[0],
            DepOfDep {
                package_name: "winapi-util".to_string(),
                use_name: "winapi_util".to_string(),
                version: Version::new(0, 1, 5),
                condition: Condition::Expr("is_win".to_string()),
            }
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "time");
        assert_eq!(dependencies[i].version, Version::new(0, 3, 14));
        // `time` is a dependency of `foo`, so should also get classified as `Test`:
        assert_eq!(dependencies[i].group, Group::Test);
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["alloc", "std"]
        );
        assert_eq!(dependencies[i].dependencies.len(), 2);
        assert_eq!(
            dependencies[i].dependencies[0],
            DepOfDep {
                package_name: "libc".to_string(),
                use_name: "libc".to_string(),
                version: Version::new(0, 2, 133),
                condition: Condition::Expr("!is_win".to_string()),
            }
        );
        assert_eq!(
            dependencies[i].dependencies[1],
            DepOfDep {
                package_name: "num_threads".to_string(),
                use_name: "num_threads".to_string(),
                version: Version::new(0, 1, 6),
                condition: Condition::Expr("!is_win".to_string()),
            }
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
            assert_eq!(d.condition, Condition::Expr("is_win".to_string()));
            true
        }));
        assert_eq!(dependencies[i].dependencies.len(), 1);
        assert_eq!(dependencies[i].build_dependencies.len(), 0);
        assert_eq!(
            dependencies[i].dependencies[0],
            DepOfDep {
                package_name: "winapi".to_string(),
                use_name: "winapi".to_string(),
                version: Version::new(0, 3, 9),
                condition: Condition::Expr("is_win".to_string()),
            }
        );

        i += 1;
        assert_eq!(dependencies.len(), i);
    }

    #[test]
    fn dependencies_for_workspace_member() {
        let config = BuildConfig::default();
        let metadata: cargo_metadata::Metadata =
            serde_json::from_str(SAMPLE_CARGO_METADATA).unwrap();

        // Start from "foo" workspace member.
        let dependencies = collect_dependencies(&metadata, "foo", &config).unwrap();

        let mut i = 0;

        assert_eq!(dependencies[i].package_name, "bar");
        assert_eq!(dependencies[i].version, Version::new(0, 1, 0));

        i += 1;

        assert_eq!(dependencies[i].package_name, "foo");
        assert_eq!(dependencies[i].version, Version::new(0, 1, 0));

        i += 1;

        assert_eq!(dependencies[i].package_name, "libc");
        assert_eq!(dependencies[i].version, Version::new(0, 2, 133));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["std"]
        );

        i += 1;

        assert_eq!(dependencies[i].package_name, "num_threads");
        assert_eq!(dependencies[i].version, Version::new(0, 1, 6));

        i += 1;

        assert_eq!(dependencies[i].package_name, "time");
        assert_eq!(dependencies[i].version, Version::new(0, 3, 14));
        assert_eq!(
            dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
            &["alloc", "std"]
        );

        i += 1;
        assert_eq!(dependencies.len(), i);
    }

    #[test]
    fn dependencies_removed_by_config_file() {
        // Use a config with `remove_crates` and `remove_deps` entries.
        let mut config = BuildConfig::default();
        config.resolve.remove_crates.push("num_threads".to_string());
        config.per_crate_config.insert(
            "time".to_string(),
            CrateConfig { remove_deps: vec!["num_threads".to_string()], ..CrateConfig::default() },
        );

        // Collect dependencies.
        let metadata: cargo_metadata::Metadata =
            serde_json::from_str(SAMPLE_CARGO_METADATA).unwrap();
        let dependencies = collect_dependencies(&metadata, "sample_package", &config).unwrap();

        // Verify that `num_threads` got removed.
        for package in dependencies.iter() {
            dbg!(&package.package_name);
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
}
