// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pointer arithmetic that already sits inside an unsafe-buffers macro must not
// be wrapped in one a second time; only `.data()` is appended. See
// AdaptBinaryOpInMacro() in tools/clang/spanify/Spanifier.cpp for why.

#include <array>

// The macros are defined here, with no effect, because the header that defines
// them is not on this corpus' include path. Both levels are needed to mirror
// how the real macros are defined: the rewrite has to identify the outermost
// one and avoid generating a second one.
#define ANGLE_UNSAFE_BUFFERS(...) (__VA_ARGS__)
#define ANGLE_UNSAFE_TODO(...) ANGLE_UNSAFE_BUFFERS(__VA_ARGS__)

void test_already_inside_unsafe_todo() {
  std::array<int, 3> buf = {1, 2, 3};
  // Buffer access leading to buf to be spanified:
  buf[0] = 0;

  ANGLE_UNSAFE_TODO(buf.data() + 1);
}

// Same, but with the macro that ANGLE_UNSAFE_TODO itself expands to. It opens
// the same pragma, so it has to be recognized too.
void test_already_inside_unsafe_buffers() {
  std::array<int, 3> buf = {1, 2, 3};
  // Buffer access leading to buf to be spanified:
  buf[0] = 0;

  ANGLE_UNSAFE_BUFFERS(buf.data() + 1);
}
