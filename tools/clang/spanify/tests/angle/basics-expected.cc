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
  std::array<int, 3> buf = {1, 2, 3};
  // Buffer access leading to buf to be spanified:
  buf[0] = 0;

  // Expected rewrite:
  // ANGLE_UNSAFE_TODO(CUSTOM_MACRO(buf.data() + 1));
  ANGLE_UNSAFE_TODO(CUSTOM_MACRO(buf.data() + 1));
}

struct Point {
  int x;
  int y;
};

void test_struct_with_braces(int index) {
  // Implicitly sized struct array with nested braces (elide_braces = false)
  // Expected rewrite:
  // std::array<Point, 2> points = {{{1, 2}, {3, 4}}};
  std::array<Point, 2> points = {{{1, 2}, {3, 4}}};
  points[index].x = 0;
}

void test_static_const_implicit_array(int index) {
  // Implicitly sized static const array
  // Expected rewrite:
  // static const std::array<int, 3> kValues = {10, 20, 30};
  static const std::array<int, 3> kValues = {10, 20, 30};
  int val = kValues[index];
}
