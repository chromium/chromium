// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

void InvalidateBeforeSwapContainers(std::vector<int> v1, std::vector<int> v2) {
  auto it1 = std::begin(v1);
  auto it2 = std::begin(v2);
  if (it1 == std::end(v1) || it2 == std::end(v2)) {
    return;
  }
  // Both iterators are valid before the swap.
  *it1 = 0;
  *it2 = 0;

  // This invalidates `it1`.
  v1.clear();
  *it1 = 0;

  std::swap(v1, v2);

  // After the swap, `it1` is not valid and `it2` is valid.
  *it1 = 0;
  *it2 = 0;
}
