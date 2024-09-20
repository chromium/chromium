// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

use testutil::ToolchainVersion;

#[test]
#[cfg_attr(miri, ignore)]
fn ui() {
    let version = ToolchainVersion::extract_from_pwd().unwrap();
    // See the doc comment on this method for an explanation of what this does
    // and why we store source files in different directories.
    let source_files_dirname = version.get_ui_source_files_dirname_and_maybe_print_warning();

    let t = trybuild::TestCases::new();
    t.compile_fail(format!("tests/{source_files_dirname}/*.rs"));
}

// The file `invalid-impls.rs` directly includes `src/macros.rs` in order to
// test the `impl_or_verify!` macro which is defined in that file. Specifically,
// it tests the verification portion of that macro, which is enabled when
// `cfg(any(feature = "derive", test))`. While `--cfg test` is of course passed
// to the code in the file you're reading right now, `trybuild` does not pass
// `--cfg test` when it invokes Cargo. As a result, this `trybuild` test only
// tests the correct behavior when the "derive" feature is enabled.
#[cfg(feature = "derive")]
#[test]
#[cfg_attr(miri, ignore)]
fn ui_invalid_impls() {
    let version = ToolchainVersion::extract_from_pwd().unwrap();
    // See the doc comment on this method for an explanation of what this does
    // and why we store source files in different directories.
    let source_files_dirname = version.get_ui_source_files_dirname_and_maybe_print_warning();

    let t = trybuild::TestCases::new();
    t.compile_fail(format!("tests/{source_files_dirname}/invalid-impls/*.rs"));
}
