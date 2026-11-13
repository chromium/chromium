// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests for the Crubit-generated `//url:url_rs_api` bindings.

chromium::import! {
    "//url:url_rs_api" as url;
}

use rust_gtest_interop::prelude::*;
use url::{Origin, GURL};

/// Returns a `GURL` by value.
///
/// `GURL` is Rust-movable (because it is annotated with
/// `CRUBIT_UNSAFE_BITWISE_RELOCATABLE`), so it can be returned and passed by
/// value like any other Rust value.
fn new_gurl(spec: &str) -> GURL {
    GURL::from(cpp_std::string_view::from(spec))
}

/// Takes a `GURL` by value.
fn host_of(url: GURL) -> String {
    to_str(url.host()).to_string()
}

fn new_origin(spec: &str) -> Origin {
    Origin::Create(&new_gurl(spec))
}

/// Compile-time check that `T` implements `Eq` and `Ord`.
fn assert_eq_and_ord<T: Eq + Ord + ?Sized>() {}

/// The result borrows the same data as `s` (i.e. it has the same lifetime).
fn to_str<'a>(s: cpp_std::string_view<'a>) -> &'a str {
    s.to_str().unwrap()
}

#[gtest(UrlRsApiTest, GurlStringViewAccessors)]
fn test_gurl_string_view_accessors() {
    let url = new_gurl("HTTPS://user@Example.com:8080/path?query#ref");
    expect_true!(url.is_valid());

    // The accessors return a `cpp_std::string_view` that borrows from `url`.
    expect_eq!(to_str(url.scheme()), "https");
    expect_eq!(to_str(url.username()), "user");
    expect_eq!(to_str(url.host()), "example.com");
    expect_eq!(to_str(url.port()), "8080");
    expect_eq!(to_str(url.path()), "/path");
    expect_eq!(to_str(url.query()), "query");
    expect_eq!(to_str(url.r#ref()), "ref");

    // `Display` comes from `operator<<` in `gurl.h`.
    expect_eq!(url.to_string(), "https://user@example.com:8080/path?query#ref");

    let invalid = new_gurl("not a url");
    expect_false!(invalid.is_valid());
    expect_true!(invalid.host().is_empty());
}

#[gtest(UrlRsApiTest, GurlEqAndOrd)]
fn test_gurl_eq_and_ord() {
    assert_eq_and_ord::<GURL>();

    let a = new_gurl("https://a.com/");
    let a2 = new_gurl("HTTPS://A.COM");
    let b = new_gurl("https://b.com/");
    expect_true!(a == a2);
    expect_true!(a != b);
    expect_true!(a < b);
    expect_eq!(a.cmp(&b), std::cmp::Ordering::Less);

    // `Ord` makes it possible to sort `GURL`s.
    let mut urls = [new_gurl("https://c.com/"), b, a];
    urls.sort();
    let hosts: Vec<String> = urls.iter().map(|url| to_str(url.host()).to_string()).collect();
    expect_eq!(hosts, ["a.com", "b.com", "c.com"]);
}

#[gtest(UrlRsApiTest, GurlRustMove)]
fn test_gurl_rust_move() {
    // `GURL` stores the spec in a `std::string`.  Short strings are stored
    // inline (in the `GURL` object), and long strings are stored on the heap.
    let short_spec = "https://a.com/";
    let long_spec = format!("https://example.com/{}", "x".repeat(100));

    // Rust moves `GURL`s with `memcpy` - e.g. when moving into a new binding,
    // into a function argument, into a `Box`, or when a `Vec` grows.
    let a = new_gurl(short_spec);
    let b = a;
    expect_eq!(host_of(b), "a.com");
    let boxed = Box::new(new_gurl(&long_spec));
    expect_eq!(boxed.to_string(), long_spec);
    let mut urls = Vec::new();
    for _ in 0..100 {
        urls.push(new_gurl(short_spec));
        urls.push(new_gurl(&long_spec));
    }
    for pair in urls.chunks(2) {
        expect_eq!(pair[0].to_string(), short_spec);
        expect_eq!(pair[1].to_string(), long_spec);
    }

    // `filesystem:` URLs own an inner `GURL` (and an inner `url::Parsed`) on
    // the heap.
    let mut fs_url = new_gurl("filesystem:https://example.com/temporary/foo");
    let fs_url2 = std::mem::take(&mut fs_url);
    expect_false!(fs_url.is_valid());
    expect_true!(fs_url2.is_valid());
    expect_true!(fs_url2.SchemeIsFileSystem());
    expect_eq!(fs_url2.to_string(), "filesystem:https://example.com/temporary/foo");
    // For `filesystem:` URLs, `DomainIs` checks the inner `GURL`.
    expect_true!(fs_url2.DomainIs(cpp_std::string_view::from("example.com")));
}

#[gtest(UrlRsApiTest, OriginFromGurl)]
fn test_origin_from_gurl() {
    // `GURL` goes from Rust to C++ (by reference), and `Origin` goes from C++
    // to Rust (by value).
    let url = new_gurl("https://example.com:8080/path");
    let origin = Origin::Create(&url);
    expect_false!(origin.opaque());
    expect_eq!(origin.port(), 8080);
    expect_true!(origin.DomainIs(cpp_std::string_view::from("example.com")));
    expect_false!(origin.DomainIs(cpp_std::string_view::from("other.com")));

    // `Origin` goes from Rust to C++ (by reference), and `GURL` goes from C++
    // to Rust (by value).
    let origin_url = origin.GetURL();
    expect_eq!(to_str(origin_url.host()), "example.com");
    expect_eq!(to_str(origin_url.path()), "/");
}

#[gtest(UrlRsApiTest, OriginEqAndOrd)]
fn test_origin_eq_and_ord() {
    assert_eq_and_ord::<Origin>();

    let a = new_origin("https://a.com/path");
    let a2 = new_origin("https://a.com/other/path");
    let b = new_origin("https://b.com/");
    expect_true!(a == a2);
    expect_true!(a != b);
    expect_true!(a < b);
    expect_eq!(b.cmp(&a), std::cmp::Ordering::Greater);

    // An opaque origin is only equal to its copies.
    let opaque1 = new_origin("data:text/plain,hello");
    let opaque2 = new_origin("data:text/plain,hello");
    expect_true!(opaque1.opaque());
    let opaque1_copy = opaque1.clone();
    expect_true!(opaque1 == opaque1_copy);
    expect_true!(opaque1 != opaque2);
    expect_eq!(opaque1.cmp(&opaque1_copy), std::cmp::Ordering::Equal);
    expect_ne!(opaque1.cmp(&opaque2), std::cmp::Ordering::Equal);
}

#[gtest(UrlRsApiTest, OriginRustMove)]
fn test_origin_rust_move() {
    // Rust moves `Origin`s with `memcpy`.  This keeps the identity (i.e. the
    // nonce) of opaque origins - also when the nonce is not yet initialized
    // (`Origin::Nonce` initializes it lazily, e.g. when copying the `Origin`).
    let opaque = new_origin("data:text/plain,hello");
    let mut origins = vec![opaque, new_origin("https://example.com/")];
    origins.reserve(100);
    let opaque_copy = origins[0].clone();
    origins.reserve(1000);
    expect_true!(origins[0] == opaque_copy);
    expect_true!(origins[0].opaque());
    expect_false!(origins[1].opaque());
    expect_true!(origins[1].DomainIs(cpp_std::string_view::from("example.com")));
}
