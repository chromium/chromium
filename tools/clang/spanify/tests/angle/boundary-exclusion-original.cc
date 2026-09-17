// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autogen/autogen_helper.h"
#include "src/common/third_party/angle/nested_angle.h"
#include "tools/clang/spanify/tests/common/external_chromium_core.h"

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
  int arr[4] = {1, 2, 3, 4};
  arr[index] = 10;
  // Expected rewrite:
  // AutogenFunction(arr.data());
  AutogenFunction(arr);
  // Expected rewrite:
  // AutogenInlineFunction(arr.data());
  AutogenInlineFunction(arr);

  // Array passed to a function declared in a nested third-party header,
  // src/common/third_party/angle/nested_angle.h. A third_party/ directory
  // follows the project root, so the header is excluded and the call becomes
  // a frontier.
  // Expected rewrite:
  // std::array<int, 4> nested_arr = {1, 2, 3, 4};
  int nested_arr[4] = {1, 2, 3, 4};
  nested_arr[index] = 10;
  // Expected rewrite:
  // NestedAngleThirdPartyFunction(nested_arr.data());
  NestedAngleThirdPartyFunction(nested_arr);

  // Array passed to a function that lives outside the ANGLE submodule
  // altogether; tools/clang/spanify/tests/common/ stands in for a Chromium
  // core directory such as base/ or gpu/. Its path contains no "third_party/"
  // at all, so a predicate keyed on that substring would fail open and rewrite
  // the signature. Enclosure in the submodule is what must be required, making
  // this a frontier that has to emit .data().
  // Expected rewrite:
  // std::array<int, 4> core_arr = {1, 2, 3, 4};
  int core_arr[4] = {1, 2, 3, 4};
  core_arr[index] = 10;
  // Expected rewrite:
  // ChromiumCoreFunction(core_arr.data());
  ChromiumCoreFunction(core_arr);
}
