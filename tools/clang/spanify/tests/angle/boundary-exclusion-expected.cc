// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "autogen/autogen_helper.h"

// Out-of-line definition of AutogenFunction.
// Even though this definition is in a non-autogen source file, because its
// original declaration is in an autogen header, it must be excluded.
void AutogenFunction(const int* buffer) {
  int x = buffer[0];
}

void TestBoundaryExclusion(int index) {
  // Array passed to an autogen function and an autogen inline function.
  // Because their declarations are in autogen/, they are excluded,
  // making these frontier calls requiring .data().
  // Expected rewrite:
  // std::array<int, 4> arr = {1, 2, 3, 4};
  std::array<int, 4> arr = {1, 2, 3, 4};
  arr[index] = 10;
  // Expected rewrite:
  // AutogenFunction(arr.data());
  AutogenFunction(arr.data());
  // Expected rewrite:
  // AutogenInlineFunction(arr.data());
  AutogenInlineFunction(arr.data());
}
