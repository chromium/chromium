// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

// Many of our UI tests require the "derive" feature to function properly. Cargo
// enforces that requirement for this entire integration target. In particular:
// - Some tests directly include `zerocopy-derive/tests/include.rs`, which
//   derives traits on the `AU16` type.
// - The file `invalid-impls.rs` directly includes `src/util/macros.rs` in order
//   to test the `impl_or_verify!` macro which is defined in that file.
//   Specifically, it tests the verification portion of that macro, which is
//   enabled when `cfg(any(feature = "derive", test))`. While `--cfg test` is
//   passed to this integration test, the fixture compiler does not receive it.
//   The fixtures therefore require the real "derive" feature.

use testutil::UiTestRunner;

#[test]
#[cfg_attr(
    any(
        miri,
        coverage_nightly,
        not(any(
            __ZEROCOPY_INTERNAL_USE_ONLY_TOOLCHAIN = "msrv",
            __ZEROCOPY_INTERNAL_USE_ONLY_TOOLCHAIN = "stable",
            __ZEROCOPY_INTERNAL_USE_ONLY_TOOLCHAIN = "nightly",
        )),
    ),
    ignore
)]
fn test_ui() {
    UiTestRunner::new()
        .use_outer_features()
        .rustc_arg("-Wwarnings") // To reflect typical user experience in stderr
        .run();
}
