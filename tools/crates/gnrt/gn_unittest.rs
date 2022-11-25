// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use gnrt_lib::gn::*;

use std::borrow::Borrow;

use gnrt_lib::crates::Epoch;

#[gtest(GnTest, FormatBuildFile)]
fn test() {
    // A simple lib rule.
    let build_file = BuildFile {
        rules: vec![(
            "lib".to_string(),
            Rule::Concrete {
                common: RuleCommon { testonly: false, public_visibility: true },
                details: RuleConcrete {
                    crate_name: Some("foo".to_string()),
                    epoch: Some(Epoch::Major(1)),
                    crate_type: "rlib".to_string(),
                    crate_root: "crate/src/lib.rs".to_string(),
                    edition: "2021".to_string(),
                    cargo_pkg_version: "1.2.3".to_string(),
                    cargo_pkg_authors: Some("Somebody <somebody@foo.org>".to_string()),
                    cargo_pkg_name: "foo".to_string(),
                    cargo_pkg_description: Some(
                        "A generic framework for foo\nNewline\"".to_string(),
                    ),
                    deps: vec![RuleDep::construct_for_testing(
                        Condition::Always,
                        "//third_party/rust/bar:lib".to_string(),
                    )],
                    // dev_deps should *not* show up in the output currently.
                    dev_deps: vec![RuleDep::construct_for_testing(
                        Condition::Always,
                        "//third_party/rust/rstest:lib".to_string(),
                    )],
                    build_deps: vec![RuleDep::construct_for_testing(
                        Condition::Always,
                        "//third_party/rust/bindgen:lib".to_string(),
                    )],
                    features: vec!["std".to_string()],
                    build_root: Some("crate/build.rs".to_string()),
                    build_script_outputs: vec!["binding.rs".to_string()],
                    gn_variables_lib: "variables = []".to_string(),
                },
            },
        )],
    };
    expect_eq_diff(
        format!("{}", build_file.display()),
        r#"# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import("//build/rust/cargo_crate.gni")

cargo_crate("lib") {
crate_name = "foo"
epoch = "1"
crate_type = "rlib"
crate_root = "crate/src/lib.rs"

# Unit tests skipped. Generate with --with-tests to include them.
build_native_rust_unit_tests = false
sources = [ "crate/src/lib.rs" ]
edition = "2021"
cargo_pkg_version = "1.2.3"
cargo_pkg_authors = "Somebody <somebody@foo.org>"
cargo_pkg_name = "foo"
cargo_pkg_description = "A generic framework for fooNewline'"
library_configs -= [ "//build/config/compiler:chromium_code" ]
library_configs += [ "//build/config/compiler:no_chromium_code" ]
executable_configs -= [ "//build/config/compiler:chromium_code" ]
executable_configs += [ "//build/config/compiler:no_chromium_code" ]
deps = [
"//third_party/rust/bar:lib",
]
build_deps = [
"//third_party/rust/bindgen:lib",
]
features = [
"std",
]
build_root = "crate/build.rs"
build_sources = [ "crate/build.rs" ]
build_script_outputs = [
"binding.rs",
]
variables = []
}
"#,
    );

    // Third-party only visibility, two rules in a file.
    let build_file = BuildFile {
        rules: vec![
            (
                "lib".to_string(),
                Rule::Concrete {
                    common: RuleCommon { testonly: false, public_visibility: false },
                    details: RuleConcrete {
                        crate_name: Some("foo".to_string()),
                        epoch: Some(Epoch::Major(1)),
                        crate_type: "rlib".to_string(),
                        crate_root: "crate/src/lib.rs".to_string(),
                        edition: "2021".to_string(),
                        cargo_pkg_version: "1.2.3".to_string(),
                        cargo_pkg_authors: None,
                        cargo_pkg_name: "foo".to_string(),
                        cargo_pkg_description: None,
                        deps: vec![],
                        // dev_deps should *not* show up in the output currently.
                        dev_deps: vec![],
                        build_deps: vec![],
                        features: vec![],
                        build_root: None,
                        build_script_outputs: vec![],
                        gn_variables_lib: String::new(),
                    },
                },
            ),
            (
                "test_support".to_string(),
                Rule::Group {
                    concrete_target: "lib".to_string(),
                    common: RuleCommon { testonly: true, public_visibility: true },
                },
            ),
        ],
    };
    expect_eq_diff(
        format!("{}", build_file.display()),
        r#"# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import("//build/rust/cargo_crate.gni")

cargo_crate("lib") {
crate_name = "foo"
epoch = "1"
crate_type = "rlib"

# Only for usage from third-party crates. Add the crate to
# third_party.toml to use it from first-party code.
visibility = [ "//third_party/rust/*" ]
crate_root = "crate/src/lib.rs"

# Unit tests skipped. Generate with --with-tests to include them.
build_native_rust_unit_tests = false
sources = [ "crate/src/lib.rs" ]
edition = "2021"
cargo_pkg_version = "1.2.3"
cargo_pkg_name = "foo"
library_configs -= [ "//build/config/compiler:chromium_code" ]
library_configs += [ "//build/config/compiler:no_chromium_code" ]
executable_configs -= [ "//build/config/compiler:chromium_code" ]
executable_configs += [ "//build/config/compiler:no_chromium_code" ]
}
group("test_support") {
public_deps = [ ":lib" ]
testonly = true
}
"#,
    );

    // A lib rule with conditional deps.
    let build_file = BuildFile {
        rules: vec![(
            "lib".to_string(),
            Rule::Concrete {
                common: RuleCommon { testonly: false, public_visibility: true },
                details: RuleConcrete {
                    crate_name: Some("foo".to_string()),
                    epoch: Some(Epoch::Major(1)),
                    crate_type: "rlib".to_string(),
                    crate_root: "crate/src/lib.rs".to_string(),
                    edition: "2021".to_string(),
                    cargo_pkg_version: "1.2.3".to_string(),
                    cargo_pkg_authors: None,
                    cargo_pkg_name: "foo".to_string(),
                    cargo_pkg_description: None,
                    deps: vec![
                        RuleDep::construct_for_testing(
                            Condition::Always,
                            "//third_party/rust/bar:lib".to_string(),
                        ),
                        RuleDep::construct_for_testing(
                            Condition::If("foo".to_string()),
                            "//third_party/rust/dep1:lib".to_string(),
                        ),
                        RuleDep::construct_for_testing(
                            Condition::If("foo".to_string()),
                            "//third_party/rust/dep2:lib".to_string(),
                        ),
                        RuleDep::construct_for_testing(
                            Condition::If("bar".to_string()),
                            "//third_party/rust/dep3:lib".to_string(),
                        ),
                    ],
                    // dev_deps should *not* show up in the output currently.
                    dev_deps: vec![RuleDep::construct_for_testing(
                        Condition::Always,
                        "//third_party/rust/rstest:lib".to_string(),
                    )],
                    build_deps: vec![RuleDep::construct_for_testing(
                        Condition::Always,
                        "//third_party/rust/bindgen:lib".to_string(),
                    )],
                    features: vec!["std".to_string()],
                    build_root: Some("crate/build.rs".to_string()),
                    build_script_outputs: vec!["binding.rs".to_string()],
                    gn_variables_lib: String::new(),
                },
            },
        )],
    };
    expect_eq_diff(
        format!("{}", build_file.display()),
        r#"# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import("//build/rust/cargo_crate.gni")

cargo_crate("lib") {
crate_name = "foo"
epoch = "1"
crate_type = "rlib"
crate_root = "crate/src/lib.rs"

# Unit tests skipped. Generate with --with-tests to include them.
build_native_rust_unit_tests = false
sources = [ "crate/src/lib.rs" ]
edition = "2021"
cargo_pkg_version = "1.2.3"
cargo_pkg_name = "foo"
library_configs -= [ "//build/config/compiler:chromium_code" ]
library_configs += [ "//build/config/compiler:no_chromium_code" ]
executable_configs -= [ "//build/config/compiler:chromium_code" ]
executable_configs += [ "//build/config/compiler:no_chromium_code" ]
deps = [
"//third_party/rust/bar:lib",
]
if (bar) {
deps += [
"//third_party/rust/dep3:lib",
]
}
if (foo) {
deps += [
"//third_party/rust/dep1:lib",
"//third_party/rust/dep2:lib",
]
}
build_deps = [
"//third_party/rust/bindgen:lib",
]
features = [
"std",
]
build_root = "crate/build.rs"
build_sources = [ "crate/build.rs" ]
build_script_outputs = [
"binding.rs",
]
}
"#,
    );
}

/// Expect two strings are equal, printing a human-readable diff when they're
/// different. Logs a gtest failure if they're not equal.
fn expect_eq_diff<T: Borrow<str>, U: Borrow<str>>(actual: T, expected: U) {
    let actual = actual.borrow();
    let expected = expected.borrow();

    expect_eq!(actual, expected);

    // Do not invoke `diff` if they're equal.
    if actual == expected {
        return;
    }

    use std::io::BufWriter;
    use std::io::Write;
    use std::process::*;

    // Closure to invoke diff on the inputs. This is wrapped in a closure so
    // that we can fail softly if `diff` could not be run.
    let inner = || {
        // One of the inputs must be a temporary file since we don't have a way
        // to pass two inputs through pipes.
        let expected_file = tempfile::NamedTempFile::new()?;
        let expected_file_path = expected_file.path().to_string_lossy().into_owned();
        write!(BufWriter::new(&expected_file), "{expected}")?;

        let mut diff = Command::new("diff")
            .args(["-U", "3", "--color=auto", &expected_file_path, "-"])
            .stdin(Stdio::piped())
            .spawn()?;

        // Write the second input to `diff`'s stdin then wait for the result.
        let stdin = diff.stdin.take().unwrap();
        write!(BufWriter::new(stdin), "{actual}")?;
        diff.wait()
    };

    // Print warning message if running `diff` failed, but don't panic. The test
    // already failed, we just don't get a pretty failure message.
    match inner() {
        Ok(exit_status) => {
            if !exit_status.success() {
                eprintln!("diff failed: {exit_status}");
            }
        }
        Err(err) => eprintln!("could not run diff: {err}"),
    }
}

#[gtest(GnTest, PlatformToCondition)]
fn test() {
    use cargo_platform::CfgExpr;
    use gnrt_lib::platforms::{Platform, PlatformSet};
    use std::convert::From;
    use std::str::FromStr;

    // Try an unconditional filter.
    expect_eq!(Condition::from(PlatformSet::one(None)), Condition::Always);

    // Try a target triple.
    expect_eq!(
        Condition::from(PlatformSet::one(Some(Platform::Name(
            "x86_64-pc-windows-msvc".to_string()
        ))))
        .get_if()
        .unwrap(),
        "(is_win && target_cpu == \"x64\")"
    );

    // Try a cfg expression.
    expect_eq!(
        Condition::from(PlatformSet::one(Some(Platform::Cfg(
            CfgExpr::from_str("any(windows, target_os = \"android\")").unwrap()
        ))))
        .get_if()
        .unwrap(),
        "((is_win) || (is_android))"
    );

    // Try a PlatformSet with multiple filters.
    let mut platform_set = PlatformSet::empty();
    platform_set.add(Some(Platform::Name("armv7-linux-android".to_string())));
    platform_set.add(Some(Platform::Cfg(CfgExpr::from_str("windows").unwrap())));
    expect_eq!(
        Condition::from(platform_set).get_if().unwrap(),
        "(is_android && target_cpu == \"arm\") || (is_win)"
    );
}

#[gtest(GnTest, StringEscaping)]
fn test() {
    fn escaped(s: &str) -> String {
        let mut out = String::new();
        write_str_escaped_for_testing(&mut out, s).unwrap();
        out
    }

    expect_eq!("foo bar", escaped("foo bar"));
    expect_eq!("foobar", escaped("foo\nbar"));
    expect_eq!(r#"foo 'bar'"#, escaped(r#"foo "bar""#));
    expect_eq!("foo 'bar'", escaped("foo 'bar'"));
}
