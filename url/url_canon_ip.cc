// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "url/url_canon_ip.h"

#include <stdint.h>
#include <stdlib.h>

#include <limits>

#include "base/check.h"
#include "url/url_canon_internal.h"
#include "url/url_features.h"

namespace url {

namespace {

// Return true if we've made a final IPV4/BROKEN decision, false if the result
// is NEUTRAL, and we could use a second opinion.
template<typename CHAR, typename UCHAR>
bool DoCanonicalizeIPv4Address(const CHAR* spec,
                               const Component& host,
                               CanonOutput* output,
                               CanonHostInfo* host_info) {
  host_info->family = IPv4AddressToNumber(
      spec, host, host_info->address, &host_info->num_ipv4_components);

  switch (host_info->family) {
    case CanonHostInfo::IPV4:
      // Definitely an IPv4 address.
      host_info->out_host.begin = output->length();
      AppendIPv4Address(host_info->address, output);
      host_info->out_host.len = output->length() - host_info->out_host.begin;
      return true;
    case CanonHostInfo::BROKEN:
      // Definitely broken.
      return true;
    default:
      // Could be IPv6 or a hostname.
      return false;
  }
}

// Searches for the longest sequence of zeros in |address|, and writes the
// range into |contraction_range|. The run of zeros must be at least 16 bits,
// and if there is a tie the first is chosen.
void ChooseIPv6ContractionRange(const unsigned char address[16],
                                Component* contraction_range) {
  // The longest run of zeros in |address| seen so far.
  Component max_range;

  // The current run of zeros in |address| being iterated over.
  Component cur_range;

  for (int i = 0; i < 16; i += 2) {
    // Test for 16 bits worth of zero.
    bool is_zero = (address[i] == 0 && address[i + 1] == 0);

    if (is_zero) {
      // Add the zero to the current range (or start a new one).
      if (!cur_range.is_valid())
        cur_range = Component(i, 0);
      cur_range.len += 2;
    }

    if (!is_zero || i == 14) {
      // Just completed a run of zeros. If the run is greater than 16 bits,
      // it is a candidate for the contraction.
      if (cur_range.len > 2 && cur_range.len > max_range.len) {
        max_range = cur_range;
      }
      cur_range.reset();
    }
  }
  *contraction_range = max_range;
}

// Return true if we've made a final IPV6/BROKEN decision, false if the result
// is NEUTRAL, and we could use a second opinion.
template<typename CHAR, typename UCHAR>
bool DoCanonicalizeIPv6Address(const CHAR* spec,
                               const Component& host,
                               CanonOutput* output,
                               CanonHostInfo* host_info) {
  // Turn the IP address into a 128 bit number.
  if (!IPv6AddressToNumber(spec, host, host_info->address)) {
    // If it's not an IPv6 address, scan for characters that should *only*
    // exist in an IPv6 address.
    for (int i = host.begin; i < host.end(); i++) {
      switch (spec[i]) {
        case '[':
        case ']':
        case ':':
          host_info->family = CanonHostInfo::BROKEN;
          return true;
      }
    }

    // No invalid characters. Could still be IPv4 or a hostname.
    host_info->family = CanonHostInfo::NEUTRAL;
    return false;
  }

  host_info->out_host.begin = output->length();
  output->push_back('[');
  AppendIPv6Address(host_info->address, output);
  output->push_back(']');
  host_info->out_host.len = output->length() - host_info->out_host.begin;

  host_info->family = CanonHostInfo::IPV6;
  return true;
}

}  // namespace

void AppendIPv4Address(const unsigned char address[4], CanonOutput* output) {
  for (int i = 0; i < 4; i++) {
    char str[16];
    _itoa_s(address[i], str, 10);

    for (int ch = 0; str[ch] != 0; ch++)
      output->push_back(str[ch]);

    if (i != 3)
      output->push_back('.');
  }
}

void AppendIPv6Address(const unsigned char address[16], CanonOutput* output) {
  // We will output the address according to the rules in:
  // http://tools.ietf.org/html/draft-kawamura-ipv6-text-representation-01#section-4

  // Start by finding where to place the "::" contraction (if any).
  Component contraction_range;
  ChooseIPv6ContractionRange(address, &contraction_range);

  for (int i = 0; i <= 14;) {
    // We check 2 bytes at a time, from bytes (0, 1) to (14, 15), inclusive.
    DCHECK(i % 2 == 0);
    if (i == contraction_range.begin && contraction_range.len > 0) {
      // Jump over the contraction.
      if (i == 0)
        output->push_back(':');
      output->push_back(':');
      i = contraction_range.end();
    } else {
      // Consume the next 16 bits from |address|.
      int x = address[i] << 8 | address[i + 1];

      i += 2;

      // Stringify the 16 bit number (at most requires 4 hex digits).
      char str[5];
      _itoa_s(x, str, 16);
      for (int ch = 0; str[ch] != 0; ++ch)
        output->push_back(str[ch]);

      // Put a colon after each number, except the last.
      if (i < 16)
        output->push_back(':');
    }
  }
}

void CanonicalizeIPAddress(const char* spec,
                           const Component& host,
                           CanonOutput* output,
                           CanonHostInfo* host_info) {
  if (DoCanonicalizeIPv4Address<char, unsigned char>(
          spec, host, output, host_info))
    return;
  if (DoCanonicalizeIPv6Address<char, unsigned char>(
          spec, host, output, host_info))
    return;
}

void CanonicalizeIPAddress(const char16_t* spec,
                           const Component& host,
                           CanonOutput* output,
                           CanonHostInfo* host_info) {
  if (DoCanonicalizeIPv4Address<char16_t, char16_t>(spec, host, output,
                                                    host_info))
    return;
  if (DoCanonicalizeIPv6Address<char16_t, char16_t>(spec, host, output,
                                                    host_info))
    return;
}

void CanonicalizeIPv6Address(const char* spec,
                             const Component& host,
                             CanonOutput& output,
                             CanonHostInfo& host_info) {
  DoCanonicalizeIPv6Address<char, unsigned char>(spec, host, &output,
                                                 &host_info);
}

void CanonicalizeIPv6Address(const char16_t* spec,
                             const Component& host,
                             CanonOutput& output,
                             CanonHostInfo& host_info) {
  DoCanonicalizeIPv6Address<char16_t, char16_t>(spec, host, &output,
                                                &host_info);
}

}  // namespace url
