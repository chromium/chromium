// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Increments spelled in a macro argument are rewritten in place when the
// macro is an unsafe-buffers macro or an allow-listed one (EXPECT_ family).
// Otherwise the pointer is excluded, as `++` can't be applied to a span.

#include <tuple>
#include <utility>

#include "base/containers/span.h"

#define UNSAFE_BUFFERS(...) __VA_ARGS__
#define UNSAFE_TODO(...) UNSAFE_BUFFERS(__VA_ARGS__)

// Mock implementation of EXPECT_EQ.
#define EXPECT_EQ(lhs, rhs) \
  if (lhs == rhs) {         \
  } else {                  \
  }

#define NEXT(ptr) (*ptr++)

int UnsafeIndex();  // This function might return an out-of-bound index.

// Expected rewrite:
// void allow_listed_macros(base::span<const int> values) {
void allow_listed_macros(base::span<const int> values) {
  // Expected rewrite:
  // EXPECT_EQ(std::exchange(values, values.subspan(1u))[0], 1);
  EXPECT_EQ(std::exchange(values, values.subspan(1u))[0], 1);
  // Expected rewrite:
  // UNSAFE_TODO(EXPECT_EQ(std::exchange(values, values.subspan(1u))[0], 2));
  UNSAFE_TODO(EXPECT_EQ(std::exchange(values, values.subspan(1u))[0], 2));
  // Expected rewrite:
  // UNSAFE_TODO(values = values.subspan(1u));
  UNSAFE_TODO(values = values.subspan(1u));
  // Expected rewrite:
  // std::ignore = UNSAFE_TODO(std::exchange(values, values.subspan(1u))[0]);
  std::ignore = UNSAFE_TODO(std::exchange(values, values.subspan(1u))[0]);
  // Expected rewrite:
  // EXPECT_EQ(UNSAFE_TODO(std::exchange(values, values.subspan(1u))[0]), 3);
  EXPECT_EQ(UNSAFE_TODO(std::exchange(values, values.subspan(1u))[0]), 3);
  std::ignore = values[UnsafeIndex()];
}

// No rewrite expected.
int other_macro(const int* values) {
  std::ignore = values[UnsafeIndex()];
  return NEXT(values);
}
