// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Increments are rewritten with the span's own `subspan()`, as in every other
// project.

#include "common/span.h"

int Sum(angle::Span<const int> values, int count) {
  int sum = 0;
  // Expected rewrite:
  // for (angle::Span<const int> it = values; count > 0;
  //      it = it.subspan(1u), --count) {
  for (angle::Span<const int> it = values; count > 0;
       it = it.subspan(1u), --count) {
    sum += it[0];
  }
  return sum;
}

int Parse(int argc, angle::Span<const char*> argv) {
  int total = 0;
  // The value of `argc--, argv++` is discarded, so `std::exchange()` (which is
  // [[nodiscard]]) must not be used for the right operand of the comma.
  // Expected rewrite:
  // for (; argc > 0; argc--, argv = argv.subspan(1u)) {
  for (; argc > 0; argc--, argv = argv.subspan(1u)) {
    total += argv[0][0];
  }
  return total;
}

#define ANGLE_UNSAFE_BUFFERS(...) (__VA_ARGS__)
#define ANGLE_UNSAFE_TODO(...) ANGLE_UNSAFE_BUFFERS(__VA_ARGS__)

int SumUnsafe(angle::Span<const int> values, int count) {
  int sum = 0;
  while (count-- > 0) {
    sum += values[0];
    // An increment spelled in an argument of the unsafe-buffers macro is
    // rewritten in place.
    // Expected rewrite:
    // ANGLE_UNSAFE_TODO(values = values.subspan(1u));
    ANGLE_UNSAFE_TODO(values = values.subspan(1u));
  }
  return sum;
}
