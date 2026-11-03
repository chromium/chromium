// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests for the Crubit-generated `//url:url_rs_api` bindings.

chromium::import! {
    "//url:url_rs_api" as url;
}

use ctor::{Assign, Ctor, CtorNew, Emplace};
use rust_gtest_interop::prelude::*;
use std::convert::Infallible;
use std::pin::Pin;
use url::{Origin, GURL};

/// Returns a `GURL` by value.
///
/// `GURL` is not Rust-movable (Rust can't move it with `memcpy`, because it
/// has a non-trivial move constructor).  Therefore the result is a `Ctor` -
/// a lazy constructor that the caller runs in place (e.g. with `Box::emplace`
/// or `ctor::emplace!`).
//
// TODO(b/559065915#comment9): Make `GURL` Rust-movable when Crubit supports
// this.  Today, its `std::string` field prevents this.
fn gurl_ctor(spec: &str) -> impl Ctor<Output = GURL, Error = Infallible> + use<'_> {
    GURL::ctor_new(cpp_std::string_view::from(spec))
}

fn new_gurl(spec: &str) -> Pin<Box<GURL>> {
    Box::emplace(gurl_ctor(spec))
}

/// Takes a `GURL` by value (e.g. `ctor::mov!(some_gurl)` or `gurl_ctor(...)`).
fn host_of(url: impl Ctor<Output = GURL, Error = Infallible>) -> String {
    let url = ctor::emplace!(url);
    to_str(url.host()).to_string()
}

fn new_origin(spec: &str) -> Pin<Box<Origin>> {
    Box::emplace(Origin::Create(&new_gurl(spec)))
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

#[gtest(UrlRsApiTest, GurlMove)]
fn test_gurl_move() {
    // A `GURL` returned by value is constructed in place in a new binding.
    let mut a = ctor::emplace!(gurl_ctor("https://a.com/"));
    expect_eq!(to_str(a.host()), "a.com");

    // `ctor::mov!` (the Rust equivalent of `std::move`) moves the `GURL` into a
    // new binding.  This calls the C++ move constructor, and `a` stays a valid,
    // moved-from `GURL`.
    let mut b = ctor::emplace!(GURL::ctor_new(ctor::mov!(a)));
    expect_eq!(to_str(b.host()), "a.com");
    expect_false!(a.is_valid());

    // `ctor::mov!` also moves the `GURL` into a function argument.
    expect_eq!(host_of(ctor::mov!(b)), "a.com");
    expect_false!(b.is_valid());

    // A `GURL` returned by value can be passed directly to such a function.
    expect_eq!(host_of(gurl_ctor("https://c.com/")), "c.com");

    // `assign` + `ctor::mov!` calls the C++ move assignment operator.
    let mut d = new_gurl("https://d.com/");
    a.as_mut().assign(ctor::mov!(d));
    expect_eq!(to_str(a.host()), "d.com");
    expect_false!(d.is_valid());
}

#[gtest(UrlRsApiTest, OriginFromGurl)]
fn test_origin_from_gurl() {
    // `GURL` goes from Rust to C++ (by reference), and `Origin` goes from C++
    // to Rust (by value).
    let url = new_gurl("https://example.com:8080/path");
    let origin = Box::emplace(Origin::Create(&url));
    expect_false!(origin.opaque());
    expect_eq!(origin.port(), 8080);
    expect_true!(origin.DomainIs(cpp_std::string_view::from("example.com")));
    expect_false!(origin.DomainIs(cpp_std::string_view::from("other.com")));

    // `Origin` goes from Rust to C++ (by reference), and `GURL` goes from C++
    // to Rust (by value).
    let origin_url = Box::emplace(origin.GetURL());
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
    let opaque1_copy = Box::emplace(Origin::ctor_new(&*opaque1));
    expect_true!(opaque1 == opaque1_copy);
    expect_true!(opaque1 != opaque2);
    expect_eq!(opaque1.cmp(&opaque1_copy), std::cmp::Ordering::Equal);
    expect_ne!(opaque1.cmp(&opaque2), std::cmp::Ordering::Equal);
}
