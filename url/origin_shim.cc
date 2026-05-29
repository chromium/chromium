// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/origin_shim.h"

namespace url {

int8_t OriginCompare(const Origin& a, const Origin& b) {
  std::strong_ordering cmp = a <=> b;
  if (cmp == std::strong_ordering::less) {
    return -1;
  }
  if (cmp == std::strong_ordering::greater) {
    return 1;
  }
  return 0;
}

rust::Str SchemeHostPortScheme(const SchemeHostPort& tuple) {
  return rust::Str(tuple.scheme());
}

rust::Str SchemeHostPortHost(const SchemeHostPort& tuple) {
  return rust::Str(tuple.host());
}

}  // namespace url
