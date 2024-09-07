// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

void CopyConstructorTest(std::vector<int>& v) {
  auto it = std::begin(v);
  auto copy_it = it;

  // Both invalid because they were not checked against `end`.
  *it;
  *copy_it;

  if (it == std::end(v)) {
    return;
  }

  // Valid because it was checked against `end`.
  *it;

  // Invalid because it was not checked against `end`.
  *copy_it;
}
