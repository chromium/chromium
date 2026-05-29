// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(https://crbug.com/40226863): In the long-term, this `origin.rs` wrapper
// should be removed and replaced by directly using the C++ `url::Origin` type
// via Crubit-generated `rs_bindings_from_cc`-based bindings.

use std::fmt::Debug;

#[cxx::bridge]
pub mod ffi {
    unsafe extern "C++" {
        include!("url/origin.h");
        include!("url/scheme_host_port.h");
        include!("url/origin_shim.h");

        #[namespace = "url"]
        pub type Origin;

        #[namespace = "url"]
        type SchemeHostPort;

        fn opaque(self: &Origin) -> bool;
        #[rust_name = "is_same_origin_with"]
        fn IsSameOriginWith(self: &Origin, other: &Origin) -> bool;
        #[rust_name = "get_tuple_or_precursor_tuple_if_opaque"]
        fn GetTupleOrPrecursorTupleIfOpaque(self: &Origin) -> &SchemeHostPort;

        // We use an FFI shim for comparison to avoid exposing a Compare member
        // function to general C++ callers in `url::Origin`.
        #[namespace = "url"]
        #[rust_name = "origin_compare"]
        fn OriginCompare(a: &Origin, b: &Origin) -> i8;

        #[namespace = "url"]
        #[rust_name = "scheme"]
        fn SchemeHostPortScheme(tuple: &SchemeHostPort) -> &str;
        #[namespace = "url"]
        #[rust_name = "host"]
        fn SchemeHostPortHost(tuple: &SchemeHostPort) -> &str;

        fn port(self: &SchemeHostPort) -> u16;
    }

    impl UniquePtr<Origin> {}
}

// SAFETY: `Origin` does not utilize shared or thread-bound data so moving it
// between threads is safe. Note that `Origin` is not `Sync` because it contains
// a lazily-initialized nonce value.
unsafe impl Send for ffi::Origin {}

// Since Rust is only aware of the interface and is unaware of the inner
// workings of url::Origin, it needs to defer to C++ to check things like
// equality and order.
impl PartialEq for ffi::Origin {
    fn eq(&self, other: &Self) -> bool {
        self.is_same_origin_with(other)
    }
}
impl Eq for ffi::Origin {}

impl PartialOrd for ffi::Origin {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for ffi::Origin {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        // `origin_compare` returns -1/0/1 for less/equal/greater (mapped from <=>).
        // Comparing this FFI result against reference `&0` using `i8::cmp` maps
        // the sign (negative, zero, or positive) directly into Rust's `Ordering`.
        ffi::origin_compare(self, other).cmp(&0)
    }
}

impl Debug for ffi::Origin {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let tuple = self.get_tuple_or_precursor_tuple_if_opaque();
        f.debug_struct("Origin")
            .field("scheme", &ffi::scheme(tuple))
            .field("host", &ffi::host(tuple))
            .field("port", &tuple.port())
            .field("opaque", &self.opaque())
            .finish()
    }
}
