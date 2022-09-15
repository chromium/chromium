// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use gnrt_lib::deps::*;

use std::str::FromStr;

use cargo_platform::Platform;
use rust_gtest_interop::prelude::*;

#[gtest(DepsTest, CollectDependenciesOnSampleOutput)]
fn test() {
    use gnrt_lib::crates::{Epoch, NormalizedName};

    let metadata: cargo_metadata::Metadata = serde_json::from_str(SAMPLE_CARGO_METADATA).unwrap();
    let mut dependencies = collect_dependencies(&metadata);
    dependencies.sort_by(|left, right| {
        left.package_name.cmp(&right.package_name).then(left.epoch.cmp(&right.epoch))
    });

    let empty_str_slice: &'static [&'static str] = &[];

    expect_eq!(dependencies.len(), 14);

    let mut i = 0;

    expect_eq!(dependencies[i].package_name, "autocfg");
    expect_eq!(dependencies[i].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Build).unwrap().features,
        empty_str_slice
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "cc");
    expect_eq!(dependencies[i].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Build).unwrap().features,
        empty_str_slice
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "more-asserts");
    expect_eq!(dependencies[i].epoch, Epoch::Minor(3));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Development).unwrap().features,
        empty_str_slice
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "num-traits");
    expect_eq!(dependencies[i].epoch, Epoch::Minor(2));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["std"]
    );
    expect_eq!(dependencies[i].build_dependencies.len(), 1);
    expect_eq!(
        dependencies[i].build_dependencies[0],
        DepOfDep {
            normalized_name: NormalizedName::new("autocfg").unwrap(),
            epoch: Epoch::Major(1),
            platform: None,
        }
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "once_cell");
    expect_eq!(dependencies[i].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["alloc", "race", "std"]
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "proc-macro2");
    expect_eq!(dependencies[i].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["proc-macro"]
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "quote");
    expect_eq!(dependencies[i].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["proc-macro"]
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "serde");
    expect_eq!(dependencies[i].epoch, Epoch::Major(1));
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
            normalized_name: NormalizedName::new("serde_derive").unwrap(),
            epoch: Epoch::Major(1),
            platform: None,
        }
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "serde_derive");
    expect_eq!(dependencies[i].epoch, Epoch::Major(1));
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
            normalized_name: NormalizedName::new("proc_macro2").unwrap(),
            epoch: Epoch::Major(1),
            platform: None,
        }
    );
    expect_eq!(
        dependencies[i].dependencies[1],
        DepOfDep {
            normalized_name: NormalizedName::new("quote").unwrap(),
            epoch: Epoch::Major(1),
            platform: None,
        }
    );
    expect_eq!(
        dependencies[i].dependencies[2],
        DepOfDep {
            normalized_name: NormalizedName::new("syn").unwrap(),
            epoch: Epoch::Major(1),
            platform: None,
        }
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "syn");
    expect_eq!(dependencies[i].epoch, Epoch::Major(1));
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
            normalized_name: NormalizedName::new("proc_macro2").unwrap(),
            epoch: Epoch::Major(1),
            platform: None,
        }
    );
    expect_eq!(
        dependencies[i].dependencies[1],
        DepOfDep {
            normalized_name: NormalizedName::new("quote").unwrap(),
            epoch: Epoch::Major(1),
            platform: None,
        }
    );
    expect_eq!(
        dependencies[i].dependencies[2],
        DepOfDep {
            normalized_name: NormalizedName::new("unicode_ident").unwrap(),
            epoch: Epoch::Major(1),
            platform: None,
        }
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "termcolor");
    expect_eq!(dependencies[i].epoch, Epoch::Major(1));
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
            normalized_name: NormalizedName::new("winapi_util").unwrap(),
            epoch: Epoch::Minor(1),
            platform: Some(Platform::from_str("cfg(windows)").unwrap()),
        }
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "unicode-ident");
    expect_eq!(dependencies[i].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[i].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        empty_str_slice
    );

    i += 1;

    expect_eq!(dependencies[i].package_name, "winapi");
    expect_eq!(dependencies[i].epoch, Epoch::Minor(3));
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
    expect_eq!(dependencies[i].epoch, Epoch::Minor(1));
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
            normalized_name: NormalizedName::new("winapi").unwrap(),
            epoch: Epoch::Minor(3),
            platform: Some(Platform::from_str("cfg(windows)").unwrap()),
        }
    );
}

// test_metadata.json contains the output of "cargo metadata" run in
// sample_package. The dependency graph is relatively simple but includes
// transitive deps.
static SAMPLE_CARGO_METADATA: &'static str = include_str!("test_metadata.json");
