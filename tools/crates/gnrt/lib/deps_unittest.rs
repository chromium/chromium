// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use gnrt_lib::deps::*;

use std::collections::HashSet;
use std::str::FromStr;

use cargo_platform::Platform;
use semver::Version;

#[gtest(DepsTest, CollectDependenciesOnSampleOutput)]
fn test() {
    let metadata: cargo_metadata::Metadata = serde_json::from_str(SAMPLE_CARGO_METADATA).unwrap();
    let mut dependencies = collect_dependencies(&metadata, None, None);
    dependencies.sort_by(|left, right| {
        left.package_name.cmp(&right.package_name).then(left.version.cmp(&right.version))
    });

    let empty_str_slice: &'static [&'static str] = &[];

    expect_eq!(dependencies.len(), 17);

    let mut i = 0;

    expect_eq!(dependencies[i].package_name, "autocfg");
    expect_eq!(dependencies[i].version, Version::new(1, 1, 0));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Build).unwrap().features,
        empty_str_slice
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "bar");
    expect_eq!(dependencies[i].version, Version::new(0, 1, 0));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        empty_str_slice
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "cc");
    expect_eq!(dependencies[i].version, Version::new(1, 0, 73));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Build).unwrap().features,
        empty_str_slice
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "foo");
    expect_eq!(dependencies[i].version, Version::new(0, 1, 0));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        empty_str_slice
    );
    expect_eq!(dependencies[i].dependencies.len(), 2);
    expect_eq!(
        dependencies[i].dependencies[0],
        DepOfDep {
            package_name: "bar".to_string(),
            use_name: "baz".to_string(),
            version: Version::new(0, 1, 0),
            platform: None,
        }
    );
    expect_eq!(
        dependencies[i].dependencies[1],
        DepOfDep {
            package_name: "time".to_string(),
            use_name: "time".to_string(),
            version: Version::new(0, 3, 14),
            platform: None,
        }
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "more-asserts");
    expect_eq!(dependencies[i].version, Version::new(0, 3, 0));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Development).unwrap().features,
        empty_str_slice
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "num-traits");
    expect_eq!(dependencies[i].version, Version::new(0, 2, 15));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["std"]
    );
    expect_eq!(dependencies[i].build_dependencies.len(), 1);
    expect_eq!(
        dependencies[i].build_dependencies[0],
        DepOfDep {
            package_name: "autocfg".to_string(),
            use_name: "autocfg".to_string(),
            version: Version::new(1, 1, 0),
            platform: None,
        }
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "once_cell");
    expect_eq!(dependencies[i].version, Version::new(1, 13, 0));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["alloc", "race", "std"]
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "proc-macro2");
    expect_eq!(dependencies[i].version, Version::new(1, 0, 40));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["proc-macro"]
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "quote");
    expect_eq!(dependencies[i].version, Version::new(1, 0, 20));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["proc-macro"]
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "serde");
    expect_eq!(dependencies[i].version, Version::new(1, 0, 139));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["derive", "serde_derive", "std"]
    );
    expect_eq!(dependencies[i].dependencies.len(), 1);
    expect_eq!(dependencies[i].build_dependencies.len(), 0);
    expect_eq!(dependencies[i].dev_dependencies.len(), 0);
    expect_eq!(
        dependencies[i].dependencies[0],
        DepOfDep {
            package_name: "serde_derive".to_string(),
            use_name: "serde_derive".to_string(),
            version: Version::new(1, 0, 139),
            platform: None,
        }
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "serde_derive");
    expect_eq!(dependencies[i].version, Version::new(1, 0, 139));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        empty_str_slice
    );
    expect_eq!(dependencies[i].dependencies.len(), 3);
    expect_eq!(dependencies[i].build_dependencies.len(), 0);
    expect_eq!(dependencies[i].dev_dependencies.len(), 0);
    expect_eq!(
        dependencies[i].dependencies[0],
        DepOfDep {
            package_name: "proc-macro2".to_string(),
            use_name: "proc_macro2".to_string(),
            version: Version::new(1, 0, 40),
            platform: None,
        }
    );
    expect_eq!(
        dependencies[i].dependencies[1],
        DepOfDep {
            package_name: "quote".to_string(),
            use_name: "quote".to_string(),
            version: Version::new(1, 0, 20),
            platform: None,
        }
    );
    expect_eq!(
        dependencies[i].dependencies[2],
        DepOfDep {
            package_name: "syn".to_string(),
            use_name: "syn".to_string(),
            version: Version::new(1, 0, 98),
            platform: None,
        }
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "syn");
    expect_eq!(dependencies[i].version, Version::new(1, 0, 98));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["clone-impls", "derive", "parsing", "printing", "proc-macro", "quote"]
    );
    expect_eq!(dependencies[i].dependencies.len(), 3);
    expect_eq!(dependencies[i].build_dependencies.len(), 0);
    expect_eq!(dependencies[i].dev_dependencies.len(), 0);
    expect_eq!(
        dependencies[i].dependencies[0],
        DepOfDep {
            package_name: "proc-macro2".to_string(),
            use_name: "proc_macro2".to_string(),
            version: Version::new(1, 0, 40),
            platform: None,
        }
    );
    expect_eq!(
        dependencies[i].dependencies[1],
        DepOfDep {
            package_name: "quote".to_string(),
            use_name: "quote".to_string(),
            version: Version::new(1, 0, 20),
            platform: None,
        }
    );
    expect_eq!(
        dependencies[i].dependencies[2],
        DepOfDep {
            package_name: "unicode-ident".to_string(),
            use_name: "unicode_ident".to_string(),
            version: Version::new(1, 0, 1),
            platform: None,
        }
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "termcolor");
    expect_eq!(dependencies[i].version, Version::new(1, 1, 3));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        empty_str_slice
    );
    expect_eq!(dependencies[i].dependencies.len(), 1);
    expect_eq!(dependencies[i].build_dependencies.len(), 0);
    expect_eq!(dependencies[i].dev_dependencies.len(), 0);
    expect_eq!(
        dependencies[i].dependencies[0],
        DepOfDep {
            package_name: "winapi-util".to_string(),
            use_name: "winapi_util".to_string(),
            version: Version::new(0, 1, 5),
            platform: Some(Platform::from_str("cfg(windows)").unwrap()),
        }
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "time");
    expect_eq!(dependencies[i].version, Version::new(0, 3, 14));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["alloc", "std"]
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "unicode-ident");
    expect_eq!(dependencies[i].version, Version::new(1, 0, 1));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        empty_str_slice
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "winapi");
    expect_eq!(dependencies[i].version, Version::new(0, 3, 9));
    expect_eq!(
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
    expect_eq!(dependencies[i].dependencies.len(), 0);
    expect_eq!(dependencies[i].build_dependencies.len(), 0);
    expect_eq!(dependencies[i].dev_dependencies.len(), 0);

    i += 1;

    expect_eq!(dependencies[i].package_name, "winapi-util");
    expect_eq!(dependencies[i].version, Version::new(0, 1, 5));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        empty_str_slice
    );
    expect_eq!(dependencies[i].dependencies.len(), 1);
    expect_eq!(dependencies[i].build_dependencies.len(), 0);
    expect_eq!(dependencies[i].dev_dependencies.len(), 0);
    expect_eq!(
        dependencies[i].dependencies[0],
        DepOfDep {
            package_name: "winapi".to_string(),
            use_name: "winapi".to_string(),
            version: Version::new(0, 3, 9),
            platform: Some(Platform::from_str("cfg(windows)").unwrap()),
        }
    );
}

#[gtest(DepsTest, DependenciesForWorkspaceMember)]
fn test() {
    let metadata: cargo_metadata::Metadata = serde_json::from_str(SAMPLE_CARGO_METADATA).unwrap();

    // Start from "foo" workspace member.
    let mut dependencies = collect_dependencies(&metadata, Some(vec!["foo".to_string()]), None);
    dependencies.sort_by(|left, right| {
        left.package_name.cmp(&right.package_name).then(left.version.cmp(&right.version))
    });

    expect_eq!(dependencies.len(), 3);

    let mut i = 0;

    expect_eq!(dependencies[i].package_name, "bar");
    expect_eq!(dependencies[i].version, Version::new(0, 1, 0));

    i += 1;

    expect_eq!(dependencies[i].package_name, "foo");
    expect_eq!(dependencies[i].version, Version::new(0, 1, 0));

    i += 1;

    expect_eq!(dependencies[i].package_name, "time");
    expect_eq!(dependencies[i].version, Version::new(0, 3, 14));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["alloc", "std"]
    );
}

#[gtest(DepsTest, ExcludeDependency)]
fn test() {
    let metadata: cargo_metadata::Metadata = serde_json::from_str(SAMPLE_CARGO_METADATA).unwrap();

    let deps_with_exclude =
        collect_dependencies(&metadata, None, Some(vec!["serde_derive".to_string()]));
    let deps_without_exclude = collect_dependencies(&metadata, None, None);

    let pkgs_with_exclude: HashSet<&str> =
        deps_with_exclude.iter().map(|dep| dep.package_name.as_str()).collect();
    let pkgs_without_exclude: HashSet<&str> =
        deps_without_exclude.iter().map(|dep| dep.package_name.as_str()).collect();
    let mut diff: Vec<&str> =
        pkgs_without_exclude.difference(&pkgs_with_exclude).copied().collect();
    diff.sort_unstable();
    expect_eq!(diff, ["proc-macro2", "quote", "serde_derive", "syn", "unicode-ident",]);
}

// test_metadata.json contains the output of "cargo metadata" run in
// sample_package. The dependency graph is relatively simple but includes
// transitive deps and a workspace member.
static SAMPLE_CARGO_METADATA: &'static str = include_str!("test_metadata.json");
