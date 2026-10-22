// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//url:url_rust";
}

use rust_gtest_interop::prelude::*;
use url_rust::gurl::ffi::create_gurl_from_str;

#[gtest(GURLRustTest, CreationAndValidity)]
fn test_creation_and_validity() {
    let valid = create_gurl_from_str("https://example.com/path");
    expect_true!(valid.is_valid());
    expect_eq!(valid.possibly_invalid_spec(), "https://example.com/path");

    let empty = create_gurl_from_str("");
    expect_false!(empty.is_valid());
    expect_eq!(empty.possibly_invalid_spec(), "");

    let invalid = create_gurl_from_str("http://:invalid");
    expect_false!(invalid.is_valid());
    expect_eq!(invalid.possibly_invalid_spec(), "http://:invalid/");
}

#[gtest(GURLRustTest, SchemeAndSchemeIs)]
fn test_scheme_and_scheme_is() {
    let https_url = create_gurl_from_str("HTTPS://example.com/foo");
    expect_eq!(https_url.scheme(), "https");
    expect_true!(https_url.scheme_is("https"));
    expect_false!(https_url.scheme_is("http"));

    let custom_url = create_gurl_from_str("externalfile:drive/root/file.txt");
    expect_eq!(custom_url.scheme(), "externalfile");
    expect_true!(custom_url.scheme_is("externalfile"));

    let invalid_url = create_gurl_from_str("not a url");
    expect_eq!(invalid_url.scheme(), "");
    expect_false!(invalid_url.scheme_is("http"));

    let blob_url =
        create_gurl_from_str("blob:https://example.com/550e8400-e29b-41d4-a716-446655440000");
    expect_true!(blob_url.scheme_is_blob());
    expect_false!(blob_url.scheme_is_file());
    expect_false!(blob_url.scheme_is_filesystem());

    let file_url = create_gurl_from_str("file:///tmp/test.txt");
    expect_true!(file_url.scheme_is_file());
    expect_false!(file_url.scheme_is_blob());
    expect_false!(file_url.scheme_is_filesystem());

    let filesystem_url = create_gurl_from_str("filesystem:https://example.com/temporary/test.txt");
    expect_true!(filesystem_url.scheme_is_filesystem());
    expect_false!(filesystem_url.scheme_is_blob());
    expect_false!(filesystem_url.scheme_is_file());
}

#[gtest(GURLRustTest, IsAboutBlankAndSrcdoc)]
fn test_is_about_blank_and_srcdoc() {
    let about_blank = create_gurl_from_str("about:blank?query#ref");
    expect_true!(about_blank.is_about_blank());
    expect_false!(about_blank.is_about_srcdoc());

    let about_srcdoc = create_gurl_from_str("about:srcdoc#ref");
    expect_true!(about_srcdoc.is_about_srcdoc());
    expect_false!(about_srcdoc.is_about_blank());

    let about_version = create_gurl_from_str("about:version");
    expect_false!(about_version.is_about_blank());
    expect_false!(about_version.is_about_srcdoc());
}
