// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! # URL & Origin Rust FFI Bridge
//!
//! This crate provides Rust bindings for Chromium's `GURL` and `url::Origin`
//! types.
//!
//! A naive approach would be to implement a parallel Rust-only URL and Origin
//! library. However, URL parsing is notoriously complex and filled with
//! edge-cases. Any slight parsing divergence between a Rust-only parser and the
//! C++ parser (such as how non-standard schemes, IPv6 hosts, or invalid
//! percent-encoding are handled) could lead to security-critical bypasses where
//! C++ and Rust code disagree on whether two origins or URLs are identical.
//!
//! To achieve absolute security parity and eliminate divergence risks, we wrap
//! the existing, battle-tested C++ implementations in `cxx::UniquePtr`
//! wrappers.
//!
//! Because both C++ and Rust utilize the exact same code paths, there is zero
//! possibility of parser divergence or behavior mismatches. This also reduces
//! complexity because there is no need to keep two highly complex codebases in
//! sync.
//!
//! There are tradeoffs to this approach. Creating or manipulating these types
//! from Rust requires crossing the FFI boundary and managing heap allocations
//! via `cxx::UniquePtr`. Calling methods on `GURL` or `Origin` also incurs some
//! FFI call overhead.

pub mod gurl;
pub use gurl::ffi::GURL;

pub mod origin;
pub use origin::ffi::Origin;
