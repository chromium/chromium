// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//url:url_rust";
}

use rust_gtest_interop::prelude::*;
use url_rust::gurl::ffi::{create_gurl_from_str, possibly_invalid_spec};

#[gtest(GURLRustTest, CreationAndValidity)]
fn test_creation_and_validity() {
    let valid = create_gurl_from_str("https://example.com/path");
    expect_true!(valid.is_valid());
    expect_eq!(possibly_invalid_spec(&valid), "https://example.com/path");

    let empty = create_gurl_from_str("");
    expect_false!(empty.is_valid());
    expect_eq!(possibly_invalid_spec(&empty), "");

    let invalid = create_gurl_from_str("http://:invalid");
    expect_false!(invalid.is_valid());
    expect_eq!(possibly_invalid_spec(&invalid), "http://:invalid/");
}
