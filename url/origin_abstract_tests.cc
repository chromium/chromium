// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/origin_abstract_tests.h"

namespace url {

Origin UrlOriginTestTraits::CreateOriginFromString(base::StringPiece s) {
  return Origin::Create(GURL(s));
}

Origin UrlOriginTestTraits::CreateUniqueOpaqueOrigin() {
  return Origin();
}

Origin UrlOriginTestTraits::CreateWithReferenceOrigin(
    base::StringPiece url,
    const Origin& reference_origin) {
  return Origin::Resolve(GURL(url), reference_origin);
}

Origin UrlOriginTestTraits::DeriveNewOpaqueOrigin(
    const Origin& reference_origin) {
  return reference_origin.DeriveNewOpaqueOrigin();
}

bool UrlOriginTestTraits::IsOpaque(const Origin& origin) {
  return origin.opaque();
}

std::string UrlOriginTestTraits::GetScheme(const Origin& origin) {
  return origin.scheme();
}

std::string UrlOriginTestTraits::GetHost(const Origin& origin) {
  return origin.host();
}

uint16_t UrlOriginTestTraits::GetPort(const Origin& origin) {
  return origin.port();
}

SchemeHostPort UrlOriginTestTraits::GetTupleOrPrecursorTupleIfOpaque(
    const Origin& origin) {
  return origin.GetTupleOrPrecursorTupleIfOpaque();
}

bool UrlOriginTestTraits::IsSameOrigin(const Origin& a, const Origin& b) {
  return a.IsSameOriginWith(b);
}

std::string UrlOriginTestTraits::Serialize(const Origin& origin) {
  return origin.Serialize();
}

bool UrlOriginTestTraits::IsValidUrl(base::StringPiece str) {
  return GURL(str).is_valid();
}

}  // namespace url
