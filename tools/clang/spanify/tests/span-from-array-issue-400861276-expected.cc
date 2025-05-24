// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "base/containers/span.h"

int UnsafeIndex();

// Expected rewrite:
// void UseBufferUnsafely(base::span<int> int_buffer) {
void UseBufferUnsafely(base::span<int> int_buffer) {
  int_buffer[UnsafeIndex()] = 13;
}

void function() {
  // Expected rewrite:
  // std::array<int, 10> int_array;
  std::array<int, 10> int_array;

  // No rewrite expected. `base::span` is directly constructible
  // from this expression as written.
  UseBufferUnsafely(int_array);

  // Expected rewrite:
  // UseBufferUnsafely(base::span<int>(int_array).subspan(1));
  UseBufferUnsafely(base::span<int>(int_array).subspan(1));
}
