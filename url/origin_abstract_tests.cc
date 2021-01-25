// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/origin_abstract_tests.h"

namespace url {

// static
Origin UrlOriginTestTraits::CreateOriginFromString(base::StringPiece s) {
  return Origin::Create(GURL(s));
}

// static
Origin UrlOriginTestTraits::CreateUniqueOpaqueOrigin() {
  return Origin();
}

// static
Origin UrlOriginTestTraits::CreateWithReferenceOrigin(
    base::StringPiece url,
    const Origin& reference_origin) {
  return Origin::Resolve(GURL(url), reference_origin);
}

// static
Origin UrlOriginTestTraits::DeriveNewOpaqueOrigin(
    const Origin& reference_origin) {
  return reference_origin.DeriveNewOpaqueOrigin();
}

// static
bool UrlOriginTestTraits::IsOpaque(const Origin& origin) {
  return origin.opaque();
}

// static
std::string UrlOriginTestTraits::GetScheme(const Origin& origin) {
  return origin.scheme();
}

// static
std::string UrlOriginTestTraits::GetHost(const Origin& origin) {
  return origin.host();
}

// static
uint16_t UrlOriginTestTraits::GetPort(const Origin& origin) {
  return origin.port();
}

// static
SchemeHostPort UrlOriginTestTraits::GetTupleOrPrecursorTupleIfOpaque(
    const Origin& origin) {
  return origin.GetTupleOrPrecursorTupleIfOpaque();
}

// static
bool UrlOriginTestTraits::IsSameOrigin(const Origin& a, const Origin& b) {
  return a.IsSameOriginWith(b);
}

// static
std::string UrlOriginTestTraits::Serialize(const Origin& origin) {
  return origin.Serialize();
}

// static
bool UrlOriginTestTraits::IsValidUrl(base::StringPiece str) {
  return GURL(str).is_valid();
}

}  // namespace url
