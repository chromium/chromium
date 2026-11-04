// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! No-op `#[test]` attribute macro so `rustc` does not strip `#[test]`
//! functions in `src/tests/*.rs` when compiled without `--test`.
//!
//! Upstream `openh264rs` uses standard `#[test]` for standalone `cargo test`,
//! whereas Chromium does not run native `rustc --test` targets on bots (see
//! crbug.com/40809974) and instead links tests into GTest binaries via
//! `rust_gtest_interop`. Shadowing `#[test]` keeps those functions callable
//! from `#[gtest(...)]` wrappers in `unittests.rs`.

extern crate proc_macro;

use proc_macro::TokenStream;

#[proc_macro_attribute]
pub fn test(_attr: TokenStream, item: TokenStream) -> TokenStream {
    item
}
