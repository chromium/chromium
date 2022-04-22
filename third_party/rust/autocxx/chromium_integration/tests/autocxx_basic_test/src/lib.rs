// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use autocxx::prelude::*;
use cxx::{CxxString, UniquePtr};

include_cpp! {
    #include "base/cpu.h"
    #include "base/strings/string_piece.h"
    #include "base/strings/string_piece_rust.h"
    #include "url/gurl.h"
    #include "url/origin.h"
    safety!(unsafe_ffi) // see https://google.github.io/autocxx/safety.html
    generate!("base::CPU")
    generate!("url::Origin")
    generate!("GURL")
    generate!("base::StringPiece")
    generate!("base::RustStrToStringPiece")
}

use ffi::*;

pub fn serialize_url(scheme: &str, host: &str, port: u16) -> String {
    moveit! {
        let o = url::Origin::CreateFromNormalizedTuple(scheme, host, port);
    }
    let serialized: UniquePtr<CxxString> = o.Serialize();
    serialized.to_str().unwrap().to_string()
}

pub fn get_cpu_vendor() -> String {
    moveit! {
        let cpu = base::CPU::new();
    }
    cpu.vendor_name().to_string_lossy().to_string()
}

#[test]
fn test_get_cpu_vendor() {
    assert!(!get_cpu_vendor().is_empty())
}

#[test]
fn test_gurl_parse() {
    // Papercuts to fix here:
    // 1. Allow allocation of this StringPiece on the stack (currently it's
    //    in a UniquePtr). This is relatively hard because we need to ask C++
    //    the size of the concrete templated type BasicStringPiece<char>,
    //    but until we parse the rest of the C++ we don't know we even _care_
    //    about BasicStringPiece<char>. This can be resolved with an explicit
    //    'concrete!' directive in due course -
    //    https://github.com/google/autocxx/issues/1009 - though a closer
    //    coupling with libclang/libtooling would enable us to iteratively
    //    figure these things out without the need for that.
    // 2. The annoying overloaded constructor 'new3'.
    // 3. The awkward need to unwrap a c_int.
    // 4. It would be good to provide transparent conversion into a StringPiece
    //    for any Rust slice, just as we do for full std::strings.
    let sp = ffi::base::RustStrToStringPiece("https://test.com");
    moveit! {
        let uri = ffi::GURL::new3(sp);
    }
    assert!(uri.is_valid());
    let port = uri.EffectiveIntPort();
    assert_eq!(port, 443.into());
}