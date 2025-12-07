// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

void ImplicitCastIteratorTest(std::vector<int>& v) {
  std::vector<int>::const_iterator it = std::begin(v);

  // Invalid because it was not checked against `end`.
  *it;

  if (it == std::end(v)) {
    return;
  }

  // Valid because it was checked against `end`.
  *it;
}
