// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "relative_include_helper.h"
#include "third_party/relative_include_vendored.h"

// Both headers are reachable only through the relative
// `-I relative_include`, as in a standalone ANGLE build.

void TestRelativeInclude(int index) {
  // Positive control: always rewritten, so an empty tool run fails.
  // Expected rewrite:
  // std::array<int, 4> control = {1, 2, 3, 4};
  std::array<int, 4> control = {1, 2, 3, 4};
  control[index] = 10;

  // RelativeIncludeFunction() is first-party ANGLE code. No .data() here
  // means its header was rewritten; without the fix, it gets one.
  // Expected rewrite:
  // std::array<int, 4> arr = {1, 2, 3, 4};
  std::array<int, 4> arr = {1, 2, 3, 4};
  arr[index] = 10;
  // No expected rewrite:
  // RelativeIncludeFunction(arr);
  RelativeIncludeFunction(arr);

  // RelativeIncludeVendoredFunction() lives under a nested third_party/, so
  // it stays excluded once its path is resolved, and the call is a frontier.
  // Expected rewrite:
  // std::array<int, 4> vendored_arr = {1, 2, 3, 4};
  std::array<int, 4> vendored_arr = {1, 2, 3, 4};
  vendored_arr[index] = 10;
  // Expected rewrite:
  // RelativeIncludeVendoredFunction(vendored_arr.data());
  RelativeIncludeVendoredFunction(vendored_arr.data());
}
