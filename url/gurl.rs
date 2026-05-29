// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(https://crbug.com/40226863): In the long-term, `gurl_bridge.rs` should
// be removed and replaced with directly using the C++ `GURL` type using
// Crubit-generated `rs_bindings_from_cc`-based bindings.

use std::fmt::Debug;

#[cxx::bridge]
pub mod ffi {
    unsafe extern "C++" {
        include!("url/gurl.h");
        include!("url/gurl_shim.h");

        pub type GURL;

        #[namespace = "url"]
        #[rust_name = "possibly_invalid_spec"]
        fn GURLPossiblyInvalidSpec(gurl: &GURL) -> &str;
    }

    impl UniquePtr<GURL> {}
}

// SAFETY: `GURL` does not utilize shared or thread-bound data so moving it
// between threads is safe.
unsafe impl Send for ffi::GURL {}

// Since Rust is only aware of the interface and is unaware of the inner
// workings of GURL, it needs to duplicate the operator== implementation from
// C++ into Rust.
// TODO: This can be removed once https://github.com/dtolnay/cxx/pull/1689 lands
// and CXX has support for calling overload operators.
impl PartialEq for ffi::GURL {
    fn eq(&self, other: &Self) -> bool {
        ffi::possibly_invalid_spec(self) == ffi::possibly_invalid_spec(other)
    }
}
impl Eq for ffi::GURL {}

impl Debug for ffi::GURL {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("GURL").field("spec", &ffi::possibly_invalid_spec(self)).finish()
    }
}
