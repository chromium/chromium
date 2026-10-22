// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_GURL_SHIM_H_
#define URL_GURL_SHIM_H_

#include <memory>

#include "third_party/rust/cxx/v1/cxx.h"
#include "url/gurl.h"

namespace url {

// TODO(https://crbug.com/40226863): Eventually, these shims should be removed
// and replaced by Crubit-generated bindings.

// Shim to construct a GURL from a string.
std::unique_ptr<GURL> CreateGURLFromStr(rust::Str spec);

// Shim for FFI to call GURL::PossiblyInvalidSpec() and get a Rust `str`.
rust::Str GURLPossiblyInvalidSpec(const GURL& gurl);

// Shim for FFI to call GURL::GetContentPiece() and get a Rust `str`.
rust::Str GURLContent(const GURL& gurl);

// Shim for FFI to call GURL::scheme() and get a Rust `str`.
rust::Str GURLScheme(const GURL& gurl);

// Shim for FFI to call GURL::SchemeIs(std::string_view).
bool GURLSchemeIs(const GURL& gurl, rust::Str lower_ascii_scheme);

}  // namespace url

#endif  // URL_GURL_SHIM_H_
