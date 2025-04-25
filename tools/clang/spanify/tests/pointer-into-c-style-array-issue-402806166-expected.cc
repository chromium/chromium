// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"

unsigned UnsafeIndex();

// Expected rewrite:
// void UseBufferUnsafely(base::span<int> buffer) {
void UseBufferUnsafely(base::span<int> buffer) {
  buffer[UnsafeIndex()] = 13;
}

void Func() {
  // No expected rewrite: nothing unsafe happens in this scope.
  int array[10];

  // Even if the underlying C-style array isn't rewritten, the array is
  // passed to a spanified function - this requires that we rewrite the
  // call expression.
  //
  // Expected rewrite:
  // UseBufferUnsafely(array);
  UseBufferUnsafely(array);

  // With an offset (subspan) in play, we need to also wrap the array
  // declref in a `base::span()`.
  // Expected rewrite:
  // UseBufferUnsafely(base::span<int>(array).subspan(1u));
  UseBufferUnsafely(base::span<int>(array).subspan(1u));
}
