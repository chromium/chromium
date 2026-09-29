// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <tuple>

#include "base/containers/auto_spanification_helper.h"

int UnsafeIndex();  // This function might return an out-of-bound index.

// Unrelated array: still rewritten. Checks that the tool ran at all.
//
// Expected rewrite:
// auto positive_control_buf = std::to_array<int>({1, 2, 3});
auto positive_control_buf = std::to_array<int>({1, 2, 3});

int positive_control() {
  return positive_control_buf[UnsafeIndex()];
}

// A const char array used in `sizeof` would become a `std::string_view`,
// whose `sizeof` is not the buffer size. Not rewritten.
//
// No rewrite expected.
constexpr char kIndent[] = "                    ";  // 10x2 spaces
constexpr int kIndentWidth = 2;

int constexpr_case() {
  std::ignore = kIndent[UnsafeIndex()];
  return sizeof(kIndent) / kIndentWidth;
}

// Same, with `const` instead of `constexpr`.
//
// No rewrite expected.
const char kName[] = "0123456789";

int const_case() {
  std::ignore = kName[UnsafeIndex()];
  return sizeof(kName);
}

// A non-const char array becomes a `std::array`, where `sizeof` is fine.
// Still rewritten.
//
// Expected rewrite:
// std::array<char, 4> buf{"abc"};
// std::ignore = base::SpanificationSizeofForStdArray(buf);
void std_array_case() {
  std::array<char, 4> buf{"abc"};
  std::ignore = buf[UnsafeIndex()];
  std::ignore = base::SpanificationSizeofForStdArray(buf);
}
