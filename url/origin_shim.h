// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_ORIGIN_SHIM_H_
#define URL_ORIGIN_SHIM_H_

#include <stdint.h>

#include "third_party/rust/cxx/v1/cxx.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

namespace url {

// Shim for Rust-side code to determine the order of `Origin` objects in
// tree-based maps.
int8_t OriginCompare(const Origin& a, const Origin& b);

// Shim for FFI to call SchemeHostPort::scheme() and get a Rust `str`.
rust::Str SchemeHostPortScheme(const SchemeHostPort& tuple);

// Shim for FFI to call SchemeHostPort::host() and get a Rust `str`.
rust::Str SchemeHostPortHost(const SchemeHostPort& tuple);

// Shim to clone an origin (since Rust cannot clone opaque C++ types).
std::unique_ptr<Origin> CloneOrigin(const Origin& origin);

// Shim for `url::Origin::Create(const GURL&)`, returning a unique_ptr for Rust.
// See the warnings on `url::Origin::Create()` before using.
std::unique_ptr<Origin> CreateOriginFromGURL(const GURL& url);

}  // namespace url

#endif  // URL_ORIGIN_SHIM_H_
