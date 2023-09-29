// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/origin_abstract_tests.h"

namespace url {

void ExpectParsedUrlsEqual(const GURL& a, const GURL& b) {
  EXPECT_EQ(a, b);
  const Parsed& a_parsed = a.parsed_for_possibly_invalid_spec();
  const Parsed& b_parsed = b.parsed_for_possibly_invalid_spec();
  EXPECT_EQ(a_parsed.scheme.begin, b_parsed.scheme.begin);
  EXPECT_EQ(a_parsed.scheme.len, b_parsed.scheme.len);
  EXPECT_EQ(a_parsed.username.begin, b_parsed.username.begin);
  EXPECT_EQ(a_parsed.username.len, b_parsed.username.len);
  EXPECT_EQ(a_parsed.password.begin, b_parsed.password.begin);
  EXPECT_EQ(a_parsed.password.len, b_parsed.password.len);
  EXPECT_EQ(a_parsed.host.begin, b_parsed.host.begin);
  EXPECT_EQ(a_parsed.host.len, b_parsed.host.len);
  EXPECT_EQ(a_parsed.port.begin, b_parsed.port.begin);
  EXPECT_EQ(a_parsed.port.len, b_parsed.port.len);
  EXPECT_EQ(a_parsed.path.begin, b_parsed.path.begin);
  EXPECT_EQ(a_parsed.path.len, b_parsed.path.len);
  EXPECT_EQ(a_parsed.query.begin, b_parsed.query.begin);
  EXPECT_EQ(a_parsed.query.len, b_parsed.query.len);
  EXPECT_EQ(a_parsed.ref.begin, b_parsed.ref.begin);
  EXPECT_EQ(a_parsed.ref.len, b_parsed.ref.len);
}

// static
Origin UrlOriginTestTraits::CreateOriginFromString(std::string_view s) {
  return Origin::Create(GURL(s));
}

// static
Origin UrlOriginTestTraits::CreateUniqueOpaqueOrigin() {
  return Origin();
}

// static
Origin UrlOriginTestTraits::CreateWithReferenceOrigin(
    std::string_view url,
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
  std::string serialized = origin.Serialize();

  // Extra test assertion for GetURL (which doesn't have an equivalent in
  // blink::SecurityOrigin).
  ExpectParsedUrlsEqual(GURL(serialized), origin.GetURL());

  return serialized;
}

// static
bool UrlOriginTestTraits::IsValidUrl(std::string_view str) {
  return GURL(str).is_valid();
}

// This is an abstract test suite which is instantiated by each implementation.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AbstractOriginTest);

}  // namespace url
