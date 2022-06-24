// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use gnrt_lib::deps::*;

#[gtest(DepsTest, CollectDependenciesOnSampleOutput)]
fn test() {
    use gnrt_lib::crates::Epoch;

    let metadata: cargo_metadata::Metadata = serde_json::from_str(SAMPLE_CARGO_METADATA).unwrap();
    let mut dependencies = collect_dependencies(&metadata);
    dependencies.sort_by(|left, right| {
        left.package_name.cmp(&right.package_name).then(left.epoch.cmp(&right.epoch))
    });

    let empty_str_slice: &'static [&'static str] = &[];

    expect_eq!(dependencies[0].package_name, "cc");
    expect_eq!(dependencies[0].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[0].dependency_kinds.get(&DependencyKind::Build).unwrap().features,
        empty_str_slice
    );

    expect_eq!(dependencies[1].package_name, "more-asserts");
    expect_eq!(dependencies[1].epoch, Epoch::Minor(3));
    expect_eq!(
        dependencies[1].dependency_kinds.get(&DependencyKind::Development).unwrap().features,
        empty_str_slice
    );

    expect_eq!(dependencies[2].package_name, "once_cell");
    expect_eq!(dependencies[2].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[2].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["alloc", "default", "race", "std"]
    );

    expect_eq!(dependencies[3].package_name, "proc-macro2");
    expect_eq!(dependencies[3].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[3].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["default", "proc-macro"]
    );

    expect_eq!(dependencies[4].package_name, "quote");
    expect_eq!(dependencies[4].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[4].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["default", "proc-macro"]
    );

    expect_eq!(dependencies[5].package_name, "serde");
    expect_eq!(dependencies[5].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[5].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["default", "derive", "serde_derive", "std"]
    );

    expect_eq!(dependencies[6].package_name, "serde_derive");
    expect_eq!(dependencies[6].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[6].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["default"]
    );

    expect_eq!(dependencies[7].package_name, "syn");
    expect_eq!(dependencies[7].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[7].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        &["clone-impls", "default", "derive", "parsing", "printing", "proc-macro", "quote"]
    );

    expect_eq!(dependencies[8].package_name, "termcolor");
    expect_eq!(dependencies[8].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[8].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        empty_str_slice
    );

    expect_eq!(dependencies[9].package_name, "unicode-ident");
    expect_eq!(dependencies[9].epoch, Epoch::Major(1));
    expect_eq!(
        dependencies[9].dependency_kinds.get(&DependencyKind::Normal).unwrap().features,
        empty_str_slice
    );
}

// test_metadata.json contains the output of "cargo metadata" run in
// sample_package. The dependency graph is relatively simple but includes
// transitive deps.
static SAMPLE_CARGO_METADATA: &'static str = include_str!("test_metadata.json");
