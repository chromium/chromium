// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <vector>

#include "common/span.h"
#include "common/unsafe_buffers.h"

void f() {
  std::vector<int> ctn = {1, 2, 3, 4};
  // Expected rewrite:
  // angle::Span<int> ptr = ctn;
  angle::Span<int> ptr = ctn;
  ptr[0] = 0;
}

#define CUSTOM_MACRO(expr) (expr)

void test_macro() {
  auto buf = std::to_array<int>({1, 2, 3});
  // Buffer access leading to buf to be spanified:
  buf[0] = 0;

  // Expected rewrite:
  // ANGLE_UNSAFE_TODO(CUSTOM_MACRO(buf.data() + 1));
  ANGLE_UNSAFE_TODO(CUSTOM_MACRO(buf.data() + 1));
}
