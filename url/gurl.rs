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

        fn is_valid(self: &GURL) -> bool;
        #[rust_name = "is_about_blank"]
        fn IsAboutBlank(self: &GURL) -> bool;
        #[rust_name = "is_about_srcdoc"]
        fn IsAboutSrcdoc(self: &GURL) -> bool;
        #[rust_name = "scheme_is_blob"]
        fn SchemeIsBlob(self: &GURL) -> bool;
        #[rust_name = "scheme_is_file"]
        fn SchemeIsFile(self: &GURL) -> bool;
        #[rust_name = "scheme_is_filesystem"]
        fn SchemeIsFileSystem(self: &GURL) -> bool;

        #[namespace = "url"]
        #[rust_name = "create_gurl_from_str"]
        fn CreateGURLFromStr(spec: &str) -> UniquePtr<GURL>;

        #[namespace = "url"]
        #[rust_name = "possibly_invalid_spec"]
        fn GURLPossiblyInvalidSpec(gurl: &GURL) -> &str;

        #[namespace = "url"]
        #[rust_name = "gurl_content"]
        fn GURLContent(gurl: &GURL) -> &str;

        #[namespace = "url"]
        #[rust_name = "gurl_scheme"]
        fn GURLScheme(gurl: &GURL) -> &str;

        #[namespace = "url"]
        #[rust_name = "gurl_scheme_is"]
        fn GURLSchemeIs(gurl: &GURL, lower_ascii_scheme: &str) -> bool;
    }

    impl UniquePtr<GURL> {}
}

impl ffi::GURL {
    pub fn possibly_invalid_spec(&self) -> &str {
        ffi::possibly_invalid_spec(self)
    }

    pub fn content(&self) -> &str {
        ffi::gurl_content(self)
    }

    pub fn scheme(&self) -> &str {
        ffi::gurl_scheme(self)
    }

    pub fn scheme_is(&self, lower_ascii_scheme: &str) -> bool {
        ffi::gurl_scheme_is(self, lower_ascii_scheme)
    }
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
        self.possibly_invalid_spec() == other.possibly_invalid_spec()
    }
}
impl Eq for ffi::GURL {}

impl PartialOrd for ffi::GURL {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for ffi::GURL {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.possibly_invalid_spec().cmp(other.possibly_invalid_spec())
    }
}

impl Debug for ffi::GURL {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("GURL").field("spec", &self.possibly_invalid_spec()).finish()
    }
}
