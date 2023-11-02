// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_CANON_IP_H_
#define URL_URL_CANON_IP_H_

#include "base/component_export.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"

namespace url {

// Writes the given IPv4 address to |output|.
COMPONENT_EXPORT(URL)
void AppendIPv4Address(const unsigned char address[4], CanonOutput* output);

// Writes the given IPv6 address to |output|.
COMPONENT_EXPORT(URL)
void AppendIPv6Address(const unsigned char address[16], CanonOutput* output);

// Converts an IPv4 address to a 32-bit number (network byte order).
//
// Possible return values:
//   IPV4    - IPv4 address was successfully parsed.
//   BROKEN  - Input was formatted like an IPv4 address, but overflow occurred
//             during parsing.
//   NEUTRAL - Input couldn't possibly be interpreted as an IPv4 address.
//             It might be an IPv6 address, or a hostname.
//
// On success, |num_ipv4_components| will be populated with the number of
// components in the IPv4 address.
COMPONENT_EXPORT(URL)
CanonHostInfo::Family IPv4AddressToNumber(const char* spec,
                                          const Component& host,
                                          unsigned char address[4],
                                          int* num_ipv4_components);
COMPONENT_EXPORT(URL)
CanonHostInfo::Family IPv4AddressToNumber(const char16_t* spec,
                                          const Component& host,
                                          unsigned char address[4],
                                          int* num_ipv4_components);

// Converts an IPv6 address to a 128-bit number (network byte order), returning
// true on success. False means that the input was not a valid IPv6 address.
//
// NOTE that |host| is expected to be surrounded by square brackets.
// i.e. "[::1]" rather than "::1".
COMPONENT_EXPORT(URL)
bool IPv6AddressToNumber(const char* spec,
                         const Component& host,
                         unsigned char address[16]);
COMPONENT_EXPORT(URL)
bool IPv6AddressToNumber(const char16_t* spec,
                         const Component& host,
                         unsigned char address[16]);

}  // namespace url

#endif  // URL_URL_CANON_IP_H_
