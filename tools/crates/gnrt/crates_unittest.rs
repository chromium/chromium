// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use gnrt_lib::crates::*;

use std::str::FromStr;

use Epoch::*;

#[gtest(EpochTest, FromStr)]
fn test() {
    use EpochParseError::*;
    expect_eq!(Epoch::from_str("v1"), Ok(Major(1)));
    expect_eq!(Epoch::from_str("v2"), Ok(Major(2)));
    expect_eq!(Epoch::from_str("v0_3"), Ok(Minor(3)));
    expect_eq!(Epoch::from_str("0_1"), Err(BadFormat));
    expect_eq!(Epoch::from_str("v1_9"), Err(BadVersion));
    expect_eq!(Epoch::from_str("v0_0"), Err(BadVersion));
    expect_eq!(Epoch::from_str("v0_1_2"), Err(BadFormat));
    expect_eq!(Epoch::from_str("v1_0"), Err(BadVersion));
    expect_true!(matches!(Epoch::from_str("v1_0foo"), Err(InvalidInt(_))));
    expect_true!(matches!(Epoch::from_str("vx_1"), Err(InvalidInt(_))));
}

#[gtest(EpochTest, ToString)]
fn test() {
    expect_eq!(Major(1).to_string(), "v1");
    expect_eq!(Major(2).to_string(), "v2");
    expect_eq!(Minor(3).to_string(), "v0_3");
}

#[gtest(EpochTest, FromVersion)]
fn test() {
    use semver::Version;

    expect_eq!(Epoch::from_version(&Version::new(0, 1, 0)), Minor(1));
    expect_eq!(Epoch::from_version(&Version::new(1, 2, 0)), Major(1));
}

#[gtest(EpochTest, FromVersionReqStr)]
fn test() {
    expect_eq!(Epoch::from_version_req_str("0.1.0"), Minor(1));
    expect_eq!(Epoch::from_version_req_str("1.0.0"), Major(1));
    expect_eq!(Epoch::from_version_req_str("2.3.0"), Major(2));
}
