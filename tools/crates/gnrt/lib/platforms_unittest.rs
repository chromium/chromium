// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use gnrt_lib::platforms::*;

use cargo_platform::{CfgExpr, Platform};
use std::str::FromStr;

#[gtest(PlatformTest, PlatformIsSupported)]
fn test() {
    for named_platform in supported_named_platforms_for_testing() {
        expect_true!(matches_supported_target(&Platform::Name(named_platform.to_string())));
    }

    expect_false!(matches_supported_target(&Platform::Name("x86_64-unknown-redox".to_string())));
    expect_false!(matches_supported_target(&Platform::Name("wasm32-wasi".to_string())));

    for os in supported_os_cfgs_for_testing() {
        expect_true!(matches_supported_target(&Platform::Cfg(CfgExpr::Value(os.clone()))));
    }

    expect_false!(matches_supported_target(&Platform::Cfg(
        CfgExpr::from_str("target_os = \"redox\"").unwrap()
    )));
    expect_false!(matches_supported_target(&Platform::Cfg(
        CfgExpr::from_str("target_os = \"haiku\"").unwrap()
    )));
    expect_false!(matches_supported_target(&Platform::Cfg(
        CfgExpr::from_str("target_arch = \"sparc\"").unwrap()
    )));

    expect_true!(matches_supported_target(&Platform::Cfg(
        CfgExpr::from_str("any(unix, target_os = \"wasi\")").unwrap()
    )));

    expect_false!(matches_supported_target(&Platform::Cfg(
        CfgExpr::from_str("all(unix, target_os = \"wasi\")").unwrap()
    )));
}

#[gtest(PlatformTest, FilterUnsupported)]
fn test() {
    expect_eq!(
        filter_unsupported_platform_terms(Platform::Cfg(
            CfgExpr::from_str("any(unix, target_os = \"wasi\")").unwrap()
        )),
        Some(Platform::Cfg(CfgExpr::from_str("unix").unwrap()))
    );

    expect_eq!(
        filter_unsupported_platform_terms(Platform::Cfg(
            CfgExpr::from_str("all(not(unix), not(target_os = \"wasi\"))").unwrap()
        )),
        Some(Platform::Cfg(CfgExpr::from_str("not(unix)").unwrap()))
    );

    expect_eq!(
        filter_unsupported_platform_terms(Platform::Cfg(
            CfgExpr::from_str("not(target_os = \"wasi\")").unwrap()
        )),
        None
    );
}
