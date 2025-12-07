// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string>

#include "base/containers/span.h"

unsigned UnsafeIndex();

unsigned char CastStringToUnsignedChar(const std::string& arg) {
  // Expected rewrite:
  // base::span<const unsigned char> offset = base::as_byte_span(arg);
  base::span<const unsigned char> offset = base::as_byte_span(arg);
  return offset[UnsafeIndex()];
}

uint8_t CastStringToUintEightTee(const std::string& arg) {
  // Expected rewrite:
  // base::span<const uint8_t> offset = base::as_byte_span(arg);
  base::span<const uint8_t> offset = base::as_byte_span(arg);
  return offset[UnsafeIndex()];
}
