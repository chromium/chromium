// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/containers/span.h"

unsigned UnsafeIndex();

// Writes a pointer into `*out`, like ANGLE's `mapForReadAccessOnly()`.
void Map(void** out);

uint8_t ReinterpretCast() {
  // No rewrite expected: a span here would get a pointer written into its
  // storage.
  uint8_t* src = nullptr;
  Map(reinterpret_cast<void**>(&src));
  return src[UnsafeIndex()];
}

uint8_t CStyleCast() {
  // No rewrite expected: same as above, with a C-style cast.
  uint8_t* src = nullptr;
  Map((void**)&src);
  return src[UnsafeIndex()];
}

struct Field {
  uint8_t Get() {
    Map(reinterpret_cast<void**>(&src));
    return src[UnsafeIndex()];
  }

  // No rewrite expected: same as above, for a field.
  uint8_t* src = nullptr;
};

uint8_t Control() {
  // Expected rewrite:
  // base::span<uint8_t> src = {};
  base::span<uint8_t> src = {};
  return src[UnsafeIndex()];
}
