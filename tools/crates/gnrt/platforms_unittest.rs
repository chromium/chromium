// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use gnrt_lib::platforms::*;

#[gtest(PlatformTest, PlatformIsSupported)]
fn test() {
    use cargo_platform::{CfgExpr, Platform};
    use std::str::FromStr;

    for named_platform in supported_named_platforms_for_testing() {
        expect_true!(is_supported(&Platform::Name(named_platform.to_string())));
    }

    expect_false!(is_supported(&Platform::Name("x86_64-unknown-redox".to_string())));
    expect_false!(is_supported(&Platform::Name("wasm32-wasi".to_string())));

    for os in supported_os_cfgs_for_testing() {
        expect_true!(is_supported(&Platform::Cfg(CfgExpr::Value(os.clone()))));
    }

    expect_false!(is_supported(&Platform::Cfg(
        CfgExpr::from_str("target_os = \"redox\"").unwrap()
    )));
    expect_false!(is_supported(&Platform::Cfg(
        CfgExpr::from_str("target_os = \"haiku\"").unwrap()
    )));
    expect_false!(is_supported(&Platform::Cfg(
        CfgExpr::from_str("target_arch = \"sparc\"").unwrap()
    )));
}
